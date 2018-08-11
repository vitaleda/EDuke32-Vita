//
// Common non-engine code/data for EDuke32 and Mapster32
//

#include "compat.h"
#include "build.h"
#include "baselayer.h"

#include "grpscan.h"

#ifdef _WIN32
# include "winbits.h"
# include <shlwapi.h>
# include <winnt.h>
# ifndef KEY_WOW64_32KEY
#  define KEY_WOW64_32KEY 0x0200
# endif
#elif defined __APPLE__
# include "osxbits.h"
#endif

#include <vitasdk.h>

#include "common.h"
#include "common_game.h"

struct grpfile_t const *g_selectedGrp;

int32_t g_gameType = GAMEFLAG_DUKE;

const char* s_buildInfo = "(32-bit) build";

int32_t g_usingAddon = 0;

// g_gameNamePtr can point to one of: grpfiles[].name (string literal), string
// literal, malloc'd block (XXX: possible leak)
const char *g_gameNamePtr = NULL;
char *g_defNamePtr = NULL;

char **g_defModules = NULL;
int32_t g_defModulesNum = 0;
// grp/con handling

char **g_clipMapFiles = NULL;
int32_t g_clipMapFilesNum = 0;

static const char *defaultgamegrp[GAMECOUNT] = { "DUKE3D.GRP", "NAM.GRP", "NAPALM.GRP", "WW2GI.GRP" };
static const char *defaultdeffilename[GAMECOUNT] = { "duke3d.def", "nam.def", "napalm.def", "ww2gi.def" };
static const char *defaultconfilename = "GAME.CON";
static const char *defaultgameconfilename[GAMECOUNT] = { "EDUKE.CON", "NAM.CON", "NAPALM.CON", "WW2GI.CON" };

// g_grpNamePtr can ONLY point to a malloc'd block (length BMAX_PATH)
char *g_grpNamePtr = NULL;
// g_scriptNamePtr can ONLY point to a malloc'd block (length BMAX_PATH)
char *g_scriptNamePtr = NULL;

void clearGrpNamePtr(void)
{
    Bfree(g_grpNamePtr);
    // g_grpNamePtr assumed to be assigned to right after
}

void clearScriptNamePtr(void)
{
    Bfree(g_scriptNamePtr);
    // g_scriptNamePtr assumed to be assigned to right after
}

const char *G_DefaultGrpFile(void)
{
    if (DUKE)
        return defaultgamegrp[GAME_DUKE];
    // order is important for the following three because GAMEFLAG_NAM overlaps all
    else if (NAPALM)
        return defaultgamegrp[GAME_NAPALM];
    else if (WW2GI)
        return defaultgamegrp[GAME_WW2GI];
    else if (NAM)
        return defaultgamegrp[GAME_NAM];

    return defaultgamegrp[0];
}
const char *G_DefaultDefFile(void)
{
    if (DUKE)
        return defaultdeffilename[GAME_DUKE];
    else if (WW2GI)
        return defaultdeffilename[GAME_WW2GI];
    else if (NAPALM)
    {
        if (!testkopen(defaultdeffilename[GAME_NAPALM],0) && testkopen(defaultdeffilename[GAME_NAM],0))
            return defaultdeffilename[GAME_NAM]; // NAM/NAPALM Sharing
        else
            return defaultdeffilename[GAME_NAPALM];
    }
    else if (NAM)
    {
        if (!testkopen(defaultdeffilename[GAME_NAM],0) && testkopen(defaultdeffilename[GAME_NAPALM],0))
            return defaultdeffilename[GAME_NAPALM]; // NAM/NAPALM Sharing
        else
            return defaultdeffilename[GAME_NAM];
    }

    return defaultdeffilename[0];
}

void G_AddDef(const char *buffer)
{
    clearDefNamePtr();
    g_defNamePtr = dup_filename(buffer);
    initprintf("Using DEF file \"%s\".\n",g_defNamePtr);
}

void G_AddDefModule(const char *buffer)
{
    g_defModules = (char **) Xrealloc (g_defModules, (g_defModulesNum+1) * sizeof(char *));
    g_defModules[g_defModulesNum] = Xstrdup(buffer);
    ++g_defModulesNum;
}

void G_AddClipMap(const char *buffer)
{
    g_clipMapFiles = (char **) Xrealloc (g_clipMapFiles, (g_clipMapFilesNum+1) * sizeof(char *));
    g_clipMapFiles[g_clipMapFilesNum] = Xstrdup(buffer);
    ++g_clipMapFilesNum;
}

const char *G_DefaultConFile(void)
{
    if (DUKE && testkopen(defaultgameconfilename[GAME_DUKE],0))
        return defaultgameconfilename[GAME_DUKE];
    else if (WW2GI && testkopen(defaultgameconfilename[GAME_WW2GI],0))
        return defaultgameconfilename[GAME_WW2GI];
    else if (NAPALM)
    {
        if (!testkopen(defaultgameconfilename[GAME_NAPALM],0))
        {
            if (testkopen(defaultgameconfilename[GAME_NAM],0))
                return defaultgameconfilename[GAME_NAM]; // NAM/NAPALM Sharing
        }
        else
            return defaultgameconfilename[GAME_NAPALM];
    }
    else if (NAM)
    {
        if (!testkopen(defaultgameconfilename[GAME_NAM],0))
        {
            if (testkopen(defaultgameconfilename[GAME_NAPALM],0))
                return defaultgameconfilename[GAME_NAPALM]; // NAM/NAPALM Sharing
        }
        else
            return defaultgameconfilename[GAME_NAM];
    }

    return defaultconfilename;
}

//// FILE NAME / DIRECTORY LISTS ////
void fnlist_clearnames(fnlist_t *fnl)
{
    klistfree(fnl->finddirs);
    klistfree(fnl->findfiles);

    fnl->finddirs = fnl->findfiles = NULL;
    fnl->numfiles = fnl->numdirs = 0;
}

// dirflags, fileflags:
//  -1 means "don't get dirs/files",
//  otherwise ORed to flags for respective klistpath
int32_t fnlist_getnames(fnlist_t *fnl, const char *dirname, const char *pattern,
                        int32_t dirflags, int32_t fileflags)
{
    CACHE1D_FIND_REC *r;

    fnlist_clearnames(fnl);

    if (dirflags != -1)
        fnl->finddirs = klistpath(dirname, "*", CACHE1D_FIND_DIR|dirflags);
    if (fileflags != -1)
        fnl->findfiles = klistpath(dirname, pattern, CACHE1D_FIND_FILE|fileflags);

    for (r=fnl->finddirs; r; r=r->next)
        fnl->numdirs++;
    for (r=fnl->findfiles; r; r=r->next)
        fnl->numfiles++;

    return(0);
}

void clearDefNamePtr(void)
{
    Bfree(g_defNamePtr);
    // g_defNamePtr assumed to be assigned to right after
}

int32_t FindDistance3D(int32_t x, int32_t y, int32_t z)
{
    return sepdist(x, y, z);
}

// returns: 1 if file could be opened, 0 else
int32_t testkopen(const char *filename, char searchfirst)
{
    int32_t fd = kopen4load(filename, searchfirst);
    if (fd >= 0)
        kclose(fd);
    return (fd >= 0);
}

const char *G_GrpFile(void)
{
    if (g_grpNamePtr == NULL)
        return G_DefaultGrpFile();
    else
        return g_grpNamePtr;
}

const char *G_DefFile(void)
{
    if (g_defNamePtr == NULL)
        return G_DefaultDefFile();
    else
        return g_defNamePtr;
}

const char *G_ConFile(void)
{
    if (g_scriptNamePtr == NULL)
        return G_DefaultConFile();
    else
        return g_scriptNamePtr;
}

//////////

// Set up new-style multi-psky handling.
void G_InitMultiPsky(int32_t const CLOUDYOCEAN__DYN, int32_t const MOONSKY1__DYN, int32_t const BIGORBIT1__DYN, int32_t const LA__DYN)
{
    // When adding other multi-skies, take care that the tileofs[] values are
    // <= PSKYOFF_MAX. (It can be increased up to MAXPSKYTILES, but should be
    // set as tight as possible.)

    // The default sky properties (all others are implicitly zero):
    psky_t * const defaultsky = E_DefinePsky(DEFAULTPSKY);
    defaultsky->lognumtiles = 3;
    defaultsky->horizfrac = 32768;

    // CLOUDYOCEAN
    // Aligns with the drawn scene horizon because it has one itself.
    psky_t * const oceansky = E_DefinePsky(CLOUDYOCEAN__DYN);
    oceansky->lognumtiles = 3;
    oceansky->horizfrac = 65536;

    // MOONSKY1
    //        earth          mountain   mountain         sun
    psky_t * const moonsky = E_DefinePsky(MOONSKY1__DYN);
    moonsky->lognumtiles = 3;
    moonsky->horizfrac = 32768;
    moonsky->tileofs[6] = 1;
    moonsky->tileofs[1] = 2;
    moonsky->tileofs[4] = 2;
    moonsky->tileofs[2] = 3;

    // BIGORBIT1   // orbit
    //       earth1         2           3           moon/sun
    psky_t * const spacesky = E_DefinePsky(BIGORBIT1__DYN);
    spacesky->lognumtiles = 3;
    spacesky->horizfrac = 32768;
    spacesky->tileofs[5] = 1;
    spacesky->tileofs[6] = 2;
    spacesky->tileofs[7] = 3;
    spacesky->tileofs[2] = 4;

    // LA // la city
    //       earth1         2           3           moon/sun
    psky_t * const citysky = E_DefinePsky(LA__DYN);
    citysky->lognumtiles = 3;
    citysky->horizfrac = 16384+1024;
    citysky->tileofs[0] = 1;
    citysky->tileofs[1] = 2;
    citysky->tileofs[2] = 1;
    citysky->tileofs[3] = 3;
    citysky->tileofs[4] = 4;
    citysky->tileofs[5] = 0;
    citysky->tileofs[6] = 2;
    citysky->tileofs[7] = 3;

#if 0
    // This assertion should hold. See note above.
    for (int i=0; i<pskynummultis; ++i)
        for (int j=0; j<(1<<multipsky[i].lognumtiles); ++j)
            Bassert(multipsky[i].tileofs[j] <= PSKYOFF_MAX);
#endif
}

void G_SetupGlobalPsky(void)
{
    int32_t i, mskyidx=0;

    // NOTE: Loop must be running backwards for the same behavior as the game
    // (greatest sector index with matching parallaxed sky takes precedence).
    for (i=numsectors-1; i>=0; i--)
    {
        if (sector[i].ceilingstat & 1)
        {
            mskyidx = getpskyidx(sector[i].ceilingpicnum);
            if (mskyidx > 0)
                break;
        }
    }

    g_pskyidx = mskyidx;
}

//////////

static char g_rootDir[BMAX_PATH];
char g_modDir[BMAX_PATH] = "ux0:data/EDuke32";

int32_t kopen4loadfrommod(const char *filename, char searchfirst)
{
    int32_t r=-1;

    if (g_modDir[0]!='/' || g_modDir[1]!=0)
    {
        static char fn[BMAX_PATH];

        Bsnprintf(fn, sizeof(fn), "%s/%s",g_modDir,filename);
        r = kopen4load(fn,searchfirst);
    }

    if (r < 0)
        r = kopen4load(filename,searchfirst);

    return r;
}

int32_t ldist(const void *s1, const void *s2)
{
    tspritetype const *const sp1 = (tspritetype *)s1;
    tspritetype const *const sp2 = (tspritetype *)s2;
    return sepldist(sp1->x - sp2->x, sp1->y - sp2->y);
}

int32_t dist(const void *s1, const void *s2)
{
    tspritetype const *const sp1 = (tspritetype *)s1;
    tspritetype const *const sp2 = (tspritetype *)s2;
    return sepdist(sp1->x - sp2->x, sp1->y - sp2->y, sp1->z - sp2->z);
}

int32_t usecwd;
static void G_LoadAddon(void);
int32_t g_groupFileHandle;

void G_ExtPreInit(int32_t argc,const char **argv)
{
    usecwd = G_CheckCmdSwitch(argc, argv, "-usecwd");

#ifdef _WIN32
    GetModuleFileName(NULL,g_rootDir,BMAX_PATH);
    Bcorrectfilename(g_rootDir,1);
    //chdir(g_rootDir);
#else
    strcpy(g_rootDir,"ux0:data/Duke32");
    strcat(g_rootDir,"/");
#endif
}

void G_ExtInit(void)
{
    char cwd[BMAX_PATH];

#ifdef EDUKE32_OSX
    char *appdir = Bgetappdir();
    addsearchpath(appdir);
    Bfree(appdir);
#endif

    if (Bstrcmp(cwd,"/") != 0)
        addsearchpath(cwd);

    if (CommandPaths)
    {
        int32_t i;
        struct strllist *s;
        while (CommandPaths)
        {
            s = CommandPaths->next;
            i = addsearchpath(CommandPaths->str);
            if (i < 0)
            {
                initprintf("Failed adding %s for game data: %s\n", CommandPaths->str,
                           i==-1 ? "not a directory" : "no such directory");
            }

            Bfree(CommandPaths->str);
            Bfree(CommandPaths);
            CommandPaths = s;
        }
    }

#if defined(_WIN32)
    if (!access("user_profiles_enabled", F_OK))
#else
    if (usecwd == 0 && access("user_profiles_disabled", F_OK))
#endif
    {
        char *homedir;
        int32_t asperr;

        if ((homedir = Bgethomedir()))
        {
            Bsnprintf(cwd,sizeof(cwd),"%s/"
#if defined(_WIN32)
                      "EDuke32 Settings"
#elif defined(GEKKO)
                      "apps/eduke32"
#else
                      ".eduke32"
#endif
                      ,homedir);
            asperr = addsearchpath(cwd);
            if (asperr == -2)
            {
                if (sceIoMkdir(cwd,0777) == 0) asperr = addsearchpath(cwd);
                else asperr = -1;
            }
            /*if (asperr == 0)
                Bchdir(cwd);*/
            Bfree(homedir);
        }
    }

    // JBF 20031220: Because it's annoying renaming GRP files whenever I want to test different game data
    if (g_grpNamePtr == NULL)
    {
        const char *cp = getenv("DUKE3DGRP");
        if (cp)
        {
            clearGrpNamePtr();
            g_grpNamePtr = dup_filename(cp);
            initprintf("Using \"%s\" as main GRP file\n", g_grpNamePtr);
        }
    }
}

int32_t G_CheckCmdSwitch(int32_t argc, const char **argv, const char *str)
{
    int32_t i;
    for (i=0; i<argc; i++)
    {
        if (str && !Bstrcasecmp(argv[i], str))
            return 1;
    }

    return 0;
}

void G_ScanGroups(void)
{
    ScanGroups();

    g_selectedGrp = NULL;

    char const * const currentGrp = G_GrpFile();

    for (grpfile_t const *fg = foundgrps; fg; fg=fg->next)
    {
        if (!Bstrcasecmp(fg->filename, currentGrp))
        {
            g_selectedGrp = fg;
            break;
        }
    }
}

static int32_t G_TryLoadingGrp(char const * const grpfile)
{
    int32_t i;

    if ((i = initgroupfile(grpfile)) == -1)
        initprintf("Warning: could not find main data file \"%s\"!\n", grpfile);
    else
        initprintf("Using \"%s\" as main game data file.\n", grpfile);

    return i;
}

static int32_t G_LoadGrpDependencyChain(grpfile_t const * const grp)
{
    if (!grp)
        return -1;

    if (grp->type->dependency && grp->type->dependency != grp->type->crcval)
        G_LoadGrpDependencyChain(FindGroup(grp->type->dependency));

    int32_t const i = G_TryLoadingGrp(grp->filename);

    if (grp->type->postprocessing)
        grp->type->postprocessing(grp->type->crcval);

    return i;
}

// checks from path and in ZIPs, returns 1 if NOT found
int32_t check_file_exist(const char *fn)
{
    int32_t opsm = pathsearchmode;
    char *tfn;

    pathsearchmode = 1;
    if (findfrompath(fn,&tfn) < 0)
    {
        char buf[BMAX_PATH];

        Bstrcpy(buf,fn);
        kzfindfilestart(buf);
        if (!kzfindfile(buf))
        {
            initprintf("Error: file \"%s\" does not exist\n",fn);
            pathsearchmode = opsm;
            return 1;
        }
    }
    else Bfree(tfn);
    pathsearchmode = opsm;

    return 0;
}

// Copy FN to WBUF and append an extension if it's not there, which is checked
// case-insensitively.
// Returns: 1 if not all characters could be written to WBUF, 0 else.
int32_t maybe_append_ext(char *wbuf, int32_t wbufsiz, const char *fn, const char *ext)
{
    const int32_t slen=Bstrlen(fn), extslen=Bstrlen(ext);
    const int32_t haveext = (slen>=extslen && Bstrcasecmp(&fn[slen-extslen], ext)==0);

    Bassert((intptr_t)wbuf != (intptr_t)fn);  // no aliasing

    // If 'fn' has no extension suffixed, append one.
    return (Bsnprintf(wbuf, wbufsiz, "%s%s", fn, haveext ? "" : ext) >= wbufsiz);
}

void G_LoadGroups(int32_t autoload)
{
    if (g_modDir[0] != '/')
    {
        char cwd[BMAX_PATH];

        Bstrcat(g_rootDir, g_modDir);
        addsearchpath(g_rootDir);
        //        addsearchpath(mod_dir);

        //if (getcwd(cwd, BMAX_PATH))
        {
            Bsprintf(cwd, "%s/%s", cwd, g_modDir);
            if (!Bstrcmp(g_rootDir, cwd))
            {
                if (addsearchpath(cwd) == -2)
                    if (sceIoMkdir(cwd, 0777) == 0)
                        addsearchpath(cwd);
            }
        }

#ifdef USE_OPENGL
        Bsprintf(cwd, "%s/%s", g_modDir, TEXCACHEFILE);
        Bstrcpy(TEXCACHEFILE, cwd);
#endif
    }

    if (g_usingAddon)
        G_LoadAddon();

    const char *grpfile;
    int32_t i;

    if ((i = G_LoadGrpDependencyChain(g_selectedGrp)) != -1)
    {
        grpfile = g_selectedGrp->filename;

        clearGrpNamePtr();
        g_grpNamePtr = dup_filename(grpfile);

        grpinfo_t const * const type = g_selectedGrp->type;

        g_gameType = type->game;
        g_gameNamePtr = type->name;

        if (type->scriptname && g_scriptNamePtr == NULL)
            g_scriptNamePtr = dup_filename(type->scriptname);

        if (type->defname && g_defNamePtr == NULL)
            g_defNamePtr = dup_filename(type->defname);
    }
    else
    {
        grpfile = G_GrpFile();
        i = G_TryLoadingGrp(grpfile);
    }

    if (autoload)
    {
        G_LoadGroupsInDir("autoload");

        if (i != -1)
            G_DoAutoload(grpfile);
    }

    if (g_modDir[0] != '/')
        G_LoadGroupsInDir(g_modDir);

    if (g_defNamePtr == NULL)
    {
        const char *tmpptr = getenv("DUKE3DDEF");
        if (tmpptr)
        {
            clearDefNamePtr();
            g_defNamePtr = dup_filename(tmpptr);
            initprintf("Using \"%s\" as definitions file\n", g_defNamePtr);
        }
    }

    loaddefinitions_game(G_DefFile(), TRUE);

    struct strllist *s;

    int const bakpathsearchmode = pathsearchmode;
    pathsearchmode = 1;

    while (CommandGrps)
    {
        int32_t j;

        s = CommandGrps->next;

        if ((j = initgroupfile(CommandGrps->str)) == -1)
            initprintf("Could not find file \"%s\".\n", CommandGrps->str);
        else
        {
            g_groupFileHandle = j;
            initprintf("Using file \"%s\" as game data.\n", CommandGrps->str);
            if (autoload)
                G_DoAutoload(CommandGrps->str);
        }

        Bfree(CommandGrps->str);
        Bfree(CommandGrps);
        CommandGrps = s;
    }
    pathsearchmode = bakpathsearchmode;
}

#ifdef _WIN32
const char * G_GetInstallPath(int32_t insttype)
{
    static char spath[NUMINSTPATHS][BMAX_PATH];
    static int32_t success[NUMINSTPATHS] = { -1, -1, -1, -1, -1 };
    int32_t siz = BMAX_PATH;

    if (success[insttype] == -1)
    {
        HKEY HKLM32;
        LONG keygood = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NULL, 0, KEY_READ | KEY_WOW64_32KEY, &HKLM32);
        // KEY_WOW64_32KEY gets us around Wow6432Node on 64-bit builds

        if (keygood == ERROR_SUCCESS)
        {
            switch (insttype)
            {
            case INSTPATH_STEAM_DUKE3D_MEGATON:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 225140", "InstallLocation", NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_STEAM_DUKE3D_3DR:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 359850", "InstallLocation", NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_GOG_DUKE3D:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\GOG.com\\GOGDUKE3D", "PATH", NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_3DR_DUKE3D:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\3DRealms\\Duke Nukem 3D", NULL, NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_3DR_ANTH:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\3DRealms\\Anthology", NULL, NULL, spath[insttype], (LPDWORD)&siz);
                break;
            case INSTPATH_STEAM_NAM:
                success[insttype] = SHGetValueA(HKLM32, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 329650", "InstallLocation", NULL, spath[insttype], (LPDWORD)&siz);
                break;
            }

            RegCloseKey(HKLM32);
        }
    }

    if (success[insttype] == ERROR_SUCCESS)
        return spath[insttype];

    return NULL;
}
#endif

static void G_LoadAddon(void)
{
    int32_t crc = 0;  // compiler-happy

    switch (g_usingAddon)
    {
    case ADDON_DUKEDC:
        crc = DUKEDC_CRC;
        break;
    case ADDON_NWINTER:
        crc = DUKENW_CRC;
        break;
    case ADDON_CARIBBEAN:
        crc = DUKECB_CRC;
        break;
    }

    if (!crc) return;

    grpfile_t const * const grp = FindGroup(crc);

    if (grp)
        g_selectedGrp = grp;
}

#if defined EDUKE32_OSX || defined __linux__ || defined EDUKE32_BSD
static void G_AddSteamPaths(const char *basepath)
{
    char buf[BMAX_PATH];

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot", basepath);
    addsearchpath_user(buf, SEARCHPATH_REMOVE);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/addons/dc", basepath);
    addsearchpath_user(buf, SEARCHPATH_REMOVE);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/addons/nw", basepath);
    addsearchpath_user(buf, SEARCHPATH_REMOVE);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/addons/vacation", basepath);
    addsearchpath_user(buf, SEARCHPATH_REMOVE);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/music", basepath);
    addsearchpath(buf);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/music/nwinter", basepath);
    addsearchpath_user(buf, SEARCHPATH_NWINTER);

    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/gameroot/music/vacation", basepath);
    addsearchpath(buf);

#if defined EDUKE32_OSX
    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Duke Nukem 3D/Duke Nukem 3D.app/drive_c/Program Files/Duke Nukem 3D", basepath);
    addsearchpath_user(buf, SEARCHPATH_REMOVE);
#endif

#if defined EDUKE32_OSX
    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Nam/Nam.app/Contents/Resources/Nam.boxer/C.harddisk/NAM", basepath);
#else
    Bsnprintf(buf, sizeof(buf), "%s/steamapps/common/Nam/NAM", basepath);
#endif
    addsearchpath_user(buf, SEARCHPATH_NAM);
}

// A bare-bones "parser" for Valve's KeyValues VDF format.
// There is no guarantee this will function properly with ill-formed files.
static void KeyValues_SkipWhitespace(char **vdfbuf, char * const vdfbufend)
{
    while (((*vdfbuf)[0] == ' ' || (*vdfbuf)[0] == '\n' || (*vdfbuf)[0] == '\r' || (*vdfbuf)[0] == '\t' || (*vdfbuf)[0] == '\0') && *vdfbuf < vdfbufend)
        (*vdfbuf)++;

    // comments
    if ((*vdfbuf) + 2 < vdfbufend && (*vdfbuf)[0] == '/' && (*vdfbuf)[1] == '/')
    {
        while ((*vdfbuf)[0] != '\n' && (*vdfbuf)[0] != '\r' && *vdfbuf < vdfbufend)
            (*vdfbuf)++;

        KeyValues_SkipWhitespace(vdfbuf, vdfbufend);
    }
}
static void KeyValues_SkipToEndOfQuotedToken(char **vdfbuf, char * const vdfbufend)
{
    (*vdfbuf)++;
    while ((*vdfbuf)[0] != '\"' && (*vdfbuf)[-1] != '\\' && *vdfbuf < vdfbufend)
        (*vdfbuf)++;
}
static void KeyValues_SkipToEndOfUnquotedToken(char **vdfbuf, char * const vdfbufend)
{
    while ((*vdfbuf)[0] != ' ' && (*vdfbuf)[0] != '\n' && (*vdfbuf)[0] != '\r' && (*vdfbuf)[0] != '\t' && (*vdfbuf)[0] != '\0' && *vdfbuf < vdfbufend)
        (*vdfbuf)++;
}
static void KeyValues_SkipNextWhatever(char **vdfbuf, char * const vdfbufend)
{
    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);

    if (*vdfbuf == vdfbufend)
        return;

    if ((*vdfbuf)[0] == '{')
    {
        (*vdfbuf)++;
        do
        {
            KeyValues_SkipNextWhatever(vdfbuf, vdfbufend);
        }
        while ((*vdfbuf)[0] != '}');
        (*vdfbuf)++;
    }
    else if ((*vdfbuf)[0] == '\"')
        KeyValues_SkipToEndOfQuotedToken(vdfbuf, vdfbufend);
    else if ((*vdfbuf)[0] != '}')
        KeyValues_SkipToEndOfUnquotedToken(vdfbuf, vdfbufend);

    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);
}
static char* KeyValues_NormalizeToken(char **vdfbuf, char * const vdfbufend)
{
    char *token = *vdfbuf;

    if ((*vdfbuf)[0] == '\"' && *vdfbuf < vdfbufend)
    {
        token++;

        KeyValues_SkipToEndOfQuotedToken(vdfbuf, vdfbufend);
        (*vdfbuf)[0] = '\0';

        // account for escape sequences
        char *writeseeker = token, *readseeker = token;
        while (readseeker <= *vdfbuf)
        {
            if (readseeker[0] == '\\')
                readseeker++;

            writeseeker[0] = readseeker[0];

            writeseeker++;
            readseeker++;
        }

        return token;
    }

    KeyValues_SkipToEndOfUnquotedToken(vdfbuf, vdfbufend);
    (*vdfbuf)[0] = '\0';

    return token;
}
static void KeyValues_FindKey(char **vdfbuf, char * const vdfbufend, const char *token)
{
    char *ParentKey = KeyValues_NormalizeToken(vdfbuf, vdfbufend);
    if (token != NULL) // pass in NULL to find the next key instead of a specific one
        while (Bstrcmp(ParentKey, token) != 0 && *vdfbuf < vdfbufend)
        {
            KeyValues_SkipNextWhatever(vdfbuf, vdfbufend);
            ParentKey = KeyValues_NormalizeToken(vdfbuf, vdfbufend);
        }

    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);
}
static int32_t KeyValues_FindParentKey(char **vdfbuf, char * const vdfbufend, const char *token)
{
    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);

    // end of scope
    if ((*vdfbuf)[0] == '}')
        return 0;

    KeyValues_FindKey(vdfbuf, vdfbufend, token);

    // ignore the wrong type
    while ((*vdfbuf)[0] != '{' && *vdfbuf < vdfbufend)
    {
        KeyValues_SkipNextWhatever(vdfbuf, vdfbufend);
        KeyValues_FindKey(vdfbuf, vdfbufend, token);
    }

    if (*vdfbuf == vdfbufend)
        return 0;

    return 1;
}
static char* KeyValues_FindKeyValue(char **vdfbuf, char * const vdfbufend, const char *token)
{
    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);

    // end of scope
    if ((*vdfbuf)[0] == '}')
        return NULL;

    KeyValues_FindKey(vdfbuf, vdfbufend, token);

    // ignore the wrong type
    while ((*vdfbuf)[0] == '{' && *vdfbuf < vdfbufend)
    {
        KeyValues_SkipNextWhatever(vdfbuf, vdfbufend);
        KeyValues_FindKey(vdfbuf, vdfbufend, token);
    }

    KeyValues_SkipWhitespace(vdfbuf, vdfbufend);

    if (*vdfbuf == vdfbufend)
        return NULL;

    return KeyValues_NormalizeToken(vdfbuf, vdfbufend);
}

static void G_ParseSteamKeyValuesForPaths(const char *vdf)
{
    int32_t fd = Bopen(vdf, BO_RDONLY);
    int32_t size = Bfilelength(fd);
    char *vdfbufstart, *vdfbuf, *vdfbufend;

    if (size <= 0)
        return;

    vdfbufstart = vdfbuf = (char*)Bmalloc(size);
    size = (int32_t)Bread(fd, vdfbuf, size);
    Bclose(fd);
    vdfbufend = vdfbuf + size;

    if (KeyValues_FindParentKey(&vdfbuf, vdfbufend, "LibraryFolders"))
    {
        char *result;
        vdfbuf++;
        while ((result = KeyValues_FindKeyValue(&vdfbuf, vdfbufend, NULL)) != NULL)
            G_AddSteamPaths(result);
    }

    Bfree(vdfbufstart);
}
#endif

void G_AddSearchPaths(void)
{
#if defined __linux__ || defined EDUKE32_BSD
    char buf[BMAX_PATH];
    char *homepath = Bgethomedir();

    Bsnprintf(buf, sizeof(buf), "%s/.steam/steam", homepath);
    G_AddSteamPaths(buf);

    Bsnprintf(buf, sizeof(buf), "%s/.steam/steam/steamapps/libraryfolders.vdf", homepath);
    G_ParseSteamKeyValuesForPaths(buf);

    Bfree(homepath);

    addsearchpath("/usr/share/games/jfduke3d");
    addsearchpath("/usr/local/share/games/jfduke3d");
    addsearchpath("/usr/share/games/eduke32");
    addsearchpath("/usr/local/share/games/eduke32");
#elif defined EDUKE32_OSX
    char buf[BMAX_PATH];
    int32_t i;
    char *applications[] = { osx_getapplicationsdir(0), osx_getapplicationsdir(1) };
    char *support[] = { osx_getsupportdir(0), osx_getsupportdir(1) };

    for (i = 0; i < 2; i++)
    {
        Bsnprintf(buf, sizeof(buf), "%s/Steam", support[i]);
        G_AddSteamPaths(buf);

        Bsnprintf(buf, sizeof(buf), "%s/Steam/steamapps/libraryfolders.vdf", support[i]);
        G_ParseSteamKeyValuesForPaths(buf);

        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D.app/Contents/Resources/Duke Nukem 3D.boxer/C.harddisk", applications[i]);
        addsearchpath_user(buf, SEARCHPATH_REMOVE);
    }

    for (i = 0; i < 2; i++)
    {
        Bsnprintf(buf, sizeof(buf), "%s/JFDuke3D", support[i]);
        addsearchpath(buf);
        Bsnprintf(buf, sizeof(buf), "%s/EDuke32", support[i]);
        addsearchpath(buf);
    }

    for (i = 0; i < 2; i++)
    {
        Bfree(applications[i]);
        Bfree(support[i]);
    }
#elif defined (_WIN32)
    char buf[BMAX_PATH];
    const char* instpath;

    if ((instpath = G_GetInstallPath(INSTPATH_STEAM_DUKE3D_MEGATON)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/gameroot", instpath);
        addsearchpath_user(buf, SEARCHPATH_REMOVE);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/dc", instpath);
        addsearchpath_user(buf, SEARCHPATH_REMOVE);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/nw", instpath);
        addsearchpath_user(buf, SEARCHPATH_REMOVE);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/addons/vacation", instpath);
        addsearchpath_user(buf, SEARCHPATH_REMOVE);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/music", instpath);
        addsearchpath(buf);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/music/nwinter", instpath);
        addsearchpath_user(buf, SEARCHPATH_NWINTER);

        Bsnprintf(buf, sizeof(buf), "%s/gameroot/music/vacation", instpath);
        addsearchpath(buf);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_STEAM_DUKE3D_3DR)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D", instpath);
        addsearchpath_user(buf, SEARCHPATH_REMOVE);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_GOG_DUKE3D)))
        addsearchpath_user(instpath, SEARCHPATH_REMOVE);

    if ((instpath = G_GetInstallPath(INSTPATH_3DR_DUKE3D)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D", instpath);
        addsearchpath_user(buf, SEARCHPATH_REMOVE);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_3DR_ANTH)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/Duke Nukem 3D", instpath);
        addsearchpath_user(buf, SEARCHPATH_REMOVE);
    }

    if ((instpath = G_GetInstallPath(INSTPATH_STEAM_NAM)))
    {
        Bsnprintf(buf, sizeof(buf), "%s/NAM", instpath);
        addsearchpath_user(buf, SEARCHPATH_NAM);
    }
#endif
}

void G_CleanupSearchPaths(void)
{
    removesearchpaths_withuser(SEARCHPATH_REMOVE);

    if (!(NAM || NAPALM))
        removesearchpaths_withuser(SEARCHPATH_NAM);

    if (!(NWINTER))
        removesearchpaths_withuser(SEARCHPATH_NWINTER);
}

//////////

struct strllist *CommandPaths, *CommandGrps;

char **g_scriptModules = NULL;
int32_t g_scriptModulesNum = 0;

void G_AddGroup(const char *buffer)
{
    char buf[BMAX_PATH];

    struct strllist *s = (struct strllist *)Xcalloc(1,sizeof(struct strllist));

    Bstrcpy(buf, buffer);

    if (Bstrchr(buf,'.') == 0)
        Bstrcat(buf,".grp");

    s->str = Xstrdup(buf);

    if (CommandGrps)
    {
        struct strllist *t;
        for (t = CommandGrps; t->next; t=t->next) ;
        t->next = s;
        return;
    }
    CommandGrps = s;
}

void G_AddPath(const char *buffer)
{
    struct strllist *s = (struct strllist *)Xcalloc(1,sizeof(struct strllist));
    s->str = Xstrdup(buffer);

    if (CommandPaths)
    {
        struct strllist *t;
        for (t = CommandPaths; t->next; t=t->next) ;
        t->next = s;
        return;
    }
    CommandPaths = s;
}

void G_AddCon(const char *buffer)
{
    clearScriptNamePtr();
    g_scriptNamePtr = dup_filename(buffer);
    initprintf("Using CON file \"%s\".\n",g_scriptNamePtr);
}

void G_AddConModule(const char *buffer)
{
    g_scriptModules = (char **) Xrealloc (g_scriptModules, (g_scriptModulesNum+1) * sizeof(char *));
    g_scriptModules[g_scriptModulesNum] = Xstrdup(buffer);
    ++g_scriptModulesNum;
}

//////////

int32_t getatoken(scriptfile *sf, const tokenlist *tl, int32_t ntokens)
{
    char *tok;
    int32_t i;

    if (!sf) return T_ERROR;
    tok = scriptfile_gettoken(sf);
    if (!tok) return T_EOF;

    for (i=ntokens-1; i>=0; i--)
    {
        if (!Bstrcasecmp(tok, tl[i].text))
            return tl[i].tokenid;
    }
    return T_ERROR;
}

int32_t FindDistance2D(int32_t x, int32_t y)
{
    return sepldist(x, y);
}

// loads all group (grp, zip, pk3/4) files in the given directory
void G_LoadGroupsInDir(const char *dirname)
{
    static const char *extensions[4] = { "*.grp", "*.zip", "*.pk3", "*.pk4" };

    char buf[BMAX_PATH];
    int32_t i;

    fnlist_t fnlist = FNLIST_INITIALIZER;

    for (i=0; i<4; i++)
    {
        CACHE1D_FIND_REC *rec;

        fnlist_getnames(&fnlist, dirname, extensions[i], -1, 0);

        for (rec=fnlist.findfiles; rec; rec=rec->next)
        {
            Bsnprintf(buf, sizeof(buf), "%s/%s", dirname, rec->name);
            initprintf("Using group file \"%s\".\n", buf);
            initgroupfile(buf);
        }

        fnlist_clearnames(&fnlist);
    }
}

void G_DoAutoload(const char *dirname)
{
    char buf[BMAX_PATH];

    Bsnprintf(buf, sizeof(buf), "autoload/%s", dirname);
    G_LoadGroupsInDir(buf);
}

//////////

void G_LoadLookups(void)
{
    int32_t fp, j;

    if ((fp=kopen4loadfrommod("lookup.dat",0)) == -1)
        if ((fp=kopen4loadfrommod("lookup.dat",1)) == -1)
            return;

    j = loadlookups(fp);

    if (j < 0)
    {
        if (j == -1)
            initprintf("ERROR loading \"lookup.dat\": failed reading enough data.\n");

        return kclose(fp);
    }

    uint8_t paldata[768];

    for (j=1; j<=5; j++)
    {
        // Account for TITLE and REALMS swap between basepal number and on-disk order.
        int32_t basepalnum = (j == 3 || j == 4) ? 4+3-j : j;

        if (kread_and_test(fp, paldata, 768))
            return kclose(fp);

        for (int k = 0; k < 768; k++)
            paldata[k] <<= 2;

        setbasepal(basepalnum, paldata);
    }

    kclose(fp);
}

#if defined HAVE_FLAC || defined HAVE_VORBIS
int32_t S_UpgradeFormat(const char *fn, char searchfirst)
{
    char *testfn, *extension;
    int32_t fp = -1;

    testfn = (char *)Xmalloc(Bstrlen(fn) + 6);
    Bstrcpy(testfn, fn);
    extension = Bstrrchr(testfn, '.');

    if (extension != NULL)
    {
        char * const fn_end = Bstrrchr(testfn, '\0');
        *extension = '_';

#ifdef HAVE_FLAC
        char const * const extFLAC = ".flac";
        Bstrcpy(fn_end, extFLAC);
        fp = kopen4loadfrommod(testfn, searchfirst);
        if (fp >= 0)
        {
            Bfree(testfn);
            return fp;
        }
#endif

#ifdef HAVE_VORBIS
        char const * const extOGG = ".ogg";
        Bstrcpy(fn_end, extOGG);
        fp = kopen4loadfrommod(testfn, searchfirst);
        if (fp >= 0)
        {
            Bfree(testfn);
            return fp;
        }
#endif

#ifdef HAVE_FLAC
        Bstrcpy(extension, extFLAC);
        fp = kopen4loadfrommod(testfn, searchfirst);
        if (fp >= 0)
        {
            Bfree(testfn);
            return fp;
        }
#endif

#ifdef HAVE_VORBIS
        Bstrcpy(extension, extOGG);
        fp = kopen4loadfrommod(testfn, searchfirst);
        if (fp >= 0)
        {
            Bfree(testfn);
            return fp;
        }
#endif
    }

    Bfree(testfn);
    return -1;
}
#endif