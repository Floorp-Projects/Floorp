/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIMemory.h"
#include "nsPluginsDir.h"
#include "nsPluginsDirUtils.h"
#include "prmem.h"
#include "prenv.h"
#include "prerror.h"
#include "prio.h"
#include <sys/stat.h>
#include "nsString.h"
#include "nsIFile.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#define LOCAL_PLUGIN_DLL_SUFFIX ".so"
#if defined(__hpux)
#define DEFAULT_X11_PATH "/usr/lib/X11R6/"
#undef LOCAL_PLUGIN_DLL_SUFFIX
#define LOCAL_PLUGIN_DLL_SUFFIX ".sl"
#define LOCAL_PLUGIN_DLL_ALT_SUFFIX ".so"
#elif defined(_AIX)
#define DEFAULT_X11_PATH "/usr/lib"
#define LOCAL_PLUGIN_DLL_ALT_SUFFIX ".a"
#elif defined(SOLARIS)
#define DEFAULT_X11_PATH "/usr/openwin/lib/"
#elif defined(LINUX)
#define DEFAULT_X11_PATH "/usr/X11R6/lib/"
#elif defined(__APPLE__)
#define DEFAULT_X11_PATH "/usr/X11R6/lib"
#undef LOCAL_PLUGIN_DLL_SUFFIX
#define LOCAL_PLUGIN_DLL_SUFFIX ".dylib"
#define LOCAL_PLUGIN_DLL_ALT_SUFFIX ".so"
#else
#define DEFAULT_X11_PATH ""
#endif

#if (MOZ_WIDGET_GTK == 2)

#define PLUGIN_MAX_LEN_OF_TMP_ARR 512

static void DisplayPR_LoadLibraryErrorMessage(const char *libName)
{
    char errorMsg[PLUGIN_MAX_LEN_OF_TMP_ARR] = "Cannot get error from NSPR.";
    if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
        PR_GetErrorText(errorMsg);

    fprintf(stderr, "LoadPlugin: failed to initialize shared library %s [%s]\n",
        libName, errorMsg);
}

static void SearchForSoname(const char* name, char** soname)
{
    if (!(name && soname))
        return;
    PRDir *fdDir = PR_OpenDir(DEFAULT_X11_PATH);
    if (!fdDir)
        return;       

    int n = strlen(name);
    PRDirEntry *dirEntry;
    while ((dirEntry = PR_ReadDir(fdDir, PR_SKIP_BOTH))) {
        if (!PL_strncmp(dirEntry->name, name, n)) {
            if (dirEntry->name[n] == '.' && dirEntry->name[n+1] && !dirEntry->name[n+2]) {
                // name.N, wild guess this is what we need
                char out[PLUGIN_MAX_LEN_OF_TMP_ARR] = DEFAULT_X11_PATH;
                PL_strcat(out, dirEntry->name);
                *soname = PL_strdup(out);
               break;
            }
        }
    }

    PR_CloseDir(fdDir);
}

static bool LoadExtraSharedLib(const char *name, char **soname, bool tryToGetSoname)
{
    bool ret = true;
    PRLibSpec tempSpec;
    PRLibrary *handle;
    tempSpec.type = PR_LibSpec_Pathname;
    tempSpec.value.pathname = name;
    handle = PR_LoadLibraryWithFlags(tempSpec, PR_LD_NOW|PR_LD_GLOBAL);
    if (!handle) {
        ret = false;
        DisplayPR_LoadLibraryErrorMessage(name);
        if (tryToGetSoname) {
            SearchForSoname(name, soname);
            if (*soname) {
                ret = LoadExtraSharedLib((const char *) *soname, nullptr, false);
            }
        }
    }
    return ret;
}

#define PLUGIN_MAX_NUMBER_OF_EXTRA_LIBS 32
#define PREF_PLUGINS_SONAME "plugin.soname.list"
#if defined(SOLARIS) || defined(HPUX)
#define DEFAULT_EXTRA_LIBS_LIST "libXt" LOCAL_PLUGIN_DLL_SUFFIX ":libXext" LOCAL_PLUGIN_DLL_SUFFIX ":libXm" LOCAL_PLUGIN_DLL_SUFFIX
#else
#define DEFAULT_EXTRA_LIBS_LIST "libXt" LOCAL_PLUGIN_DLL_SUFFIX ":libXext" LOCAL_PLUGIN_DLL_SUFFIX
#endif
/*
 this function looks for
 user_pref("plugin.soname.list", "/usr/X11R6/lib/libXt.so.6:libXext.so");
 in user's pref.js
 and loads all libs in specified order
*/

static void LoadExtraSharedLibs()
{
    // check out if user's prefs.js has libs name
    nsresult res;
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &res));
    if (NS_SUCCEEDED(res) && (prefs != nullptr)) {
        char *sonameList = nullptr;
        bool prefSonameListIsSet = true;
        res = prefs->GetCharPref(PREF_PLUGINS_SONAME, &sonameList);
        if (!sonameList) {
            // pref is not set, lets use hardcoded list
            prefSonameListIsSet = false;
            sonameList = PL_strdup(DEFAULT_EXTRA_LIBS_LIST);
        }
        if (sonameList) {
            char *arrayOfLibs[PLUGIN_MAX_NUMBER_OF_EXTRA_LIBS] = {0};
            int numOfLibs = 0;
            char *nextToken;
            char *p = nsCRT::strtok(sonameList,":",&nextToken);
            if (p) {
                while (p && numOfLibs < PLUGIN_MAX_NUMBER_OF_EXTRA_LIBS) {
                    arrayOfLibs[numOfLibs++] = p;
                    p = nsCRT::strtok(nextToken,":",&nextToken);
                }
            } else // there is just one lib
                arrayOfLibs[numOfLibs++] = sonameList;

            char sonameListToSave[PLUGIN_MAX_LEN_OF_TMP_ARR] = "";
            for (int i=0; i<numOfLibs; i++) {
                // trim out head/tail white spaces (just in case)
                bool head = true;
                p = arrayOfLibs[i];
                while (*p) {
                    if (*p == ' ' || *p == '\t') {
                        if (head) {
                            arrayOfLibs[i] = ++p;
                        } else {
                            *p = 0;
                        }
                    } else {
                        head = false;
                        p++;
                    }
                }
                if (!arrayOfLibs[i][0]) {
                    continue; // null string
                }
                bool tryToGetSoname = true;
                if (PL_strchr(arrayOfLibs[i], '/')) {
                    //assuming it's real name, try to stat it
                    struct stat st;
                    if (stat((const char*) arrayOfLibs[i], &st)) {
                        //get just a file name
                        arrayOfLibs[i] = PL_strrchr(arrayOfLibs[i], '/') + 1;
                    } else
                        tryToGetSoname = false;
                }
                char *soname = nullptr;
                if (LoadExtraSharedLib(arrayOfLibs[i], &soname, tryToGetSoname)) {
                    //construct soname's list to save in prefs
                    p = soname ? soname : arrayOfLibs[i];
                    int n = PLUGIN_MAX_LEN_OF_TMP_ARR -
                        (strlen(sonameListToSave) + strlen(p));
                    if (n > 0) {
                        PL_strcat(sonameListToSave, p);
                        PL_strcat(sonameListToSave,":");
                    }
                    if (soname) {
                        PL_strfree(soname); // it's from strdup
                    }
                    if (numOfLibs > 1)
                        arrayOfLibs[i][strlen(arrayOfLibs[i])] = ':'; //restore ":" in sonameList
                }
            }

            // Check whether sonameListToSave is a empty String, Bug: 329205
            if (sonameListToSave[0]) 
                for (p = &sonameListToSave[strlen(sonameListToSave) - 1]; *p == ':'; p--)
                    *p = 0; //delete tail ":" delimiters

            if (!prefSonameListIsSet || PL_strcmp(sonameList, sonameListToSave)) {
                // if user specified some bogus soname I overwrite it here,
                // otherwise it'll decrease performance by calling popen() in SearchForSoname
                // every time for each bogus name
                prefs->SetCharPref(PREF_PLUGINS_SONAME, (const char *)sonameListToSave);
            }
            PL_strfree(sonameList);
        }
    }
}
#endif //MOZ_WIDGET_GTK == 2

/* nsPluginsDir implementation */

bool nsPluginsDir::IsPluginFile(nsIFile* file)
{
    nsAutoCString filename;
    if (NS_FAILED(file->GetNativeLeafName(filename)))
        return false;

#ifdef ANDROID
    // It appears that if you load
    // 'libstagefright_honeycomb.so' on froyo, or
    // 'libstagefright_froyo.so' on honeycomb, we will abort.
    // Since these are just helper libs, we can ignore.
    const char *cFile = filename.get();
    if (strstr(cFile, "libstagefright") != nullptr)
        return false;
#endif

    NS_NAMED_LITERAL_CSTRING(dllSuffix, LOCAL_PLUGIN_DLL_SUFFIX);
    if (filename.Length() > dllSuffix.Length() &&
        StringEndsWith(filename, dllSuffix))
        return true;
    
#ifdef LOCAL_PLUGIN_DLL_ALT_SUFFIX
    NS_NAMED_LITERAL_CSTRING(dllAltSuffix, LOCAL_PLUGIN_DLL_ALT_SUFFIX);
    if (filename.Length() > dllAltSuffix.Length() &&
        StringEndsWith(filename, dllAltSuffix))
        return true;
#endif
    return false;
}

/* nsPluginFile implementation */

nsPluginFile::nsPluginFile(nsIFile* file)
: mPlugin(file)
{
}

nsPluginFile::~nsPluginFile()
{
}

nsresult nsPluginFile::LoadPlugin(PRLibrary **outLibrary)
{
    PRLibSpec libSpec;
    libSpec.type = PR_LibSpec_Pathname;
    bool exists = false;
    mPlugin->Exists(&exists);
    if (!exists)
        return NS_ERROR_FILE_NOT_FOUND;

    nsresult rv;
    nsAutoCString path;
    rv = mPlugin->GetNativePath(path);
    if (NS_FAILED(rv))
        return rv;

    libSpec.value.pathname = path.get();

#if (MOZ_WIDGET_GTK == 2)

    // Normally, Mozilla isn't linked against libXt and libXext
    // since it's a Gtk/Gdk application.  On the other hand,
    // legacy plug-ins expect the libXt and libXext symbols
    // to already exist in the global name space.  This plug-in
    // wrapper is linked against libXt and libXext, but since
    // we never call on any of these libraries, plug-ins still
    // fail to resolve Xt symbols when trying to do a dlopen
    // at runtime.  Explicitly opening Xt/Xext into the global
    // namespace before attempting to load the plug-in seems to
    // work fine.


#if defined(SOLARIS) || defined(HPUX)
    // Acrobat/libXm: Lazy resolving might cause crash later (bug 211587)
    *outLibrary = PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW);
    pLibrary = *outLibrary;
#else
    // Some dlopen() doesn't recover from a failed PR_LD_NOW (bug 223744)
    *outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
    pLibrary = *outLibrary;
#endif
    if (!pLibrary) {
        LoadExtraSharedLibs();
        // try reload plugin once more
        *outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
        pLibrary = *outLibrary;
        if (!pLibrary) {
            DisplayPR_LoadLibraryErrorMessage(libSpec.value.pathname);
            return NS_ERROR_FAILURE;
        }
    }
#else
    *outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
    pLibrary = *outLibrary;
#endif  // MOZ_WIDGET_GTK == 2

#ifdef DEBUG
    printf("LoadPlugin() %s returned %lx\n", 
           libSpec.value.pathname, (unsigned long)pLibrary);
#endif

    if (!pLibrary) {
        return NS_ERROR_FAILURE;
    }
    
    return NS_OK;
}

nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info, PRLibrary **outLibrary)
{
    *outLibrary = nullptr;

    info.fVersion = nullptr;

    // Sadly we have to load the library for this to work.
    nsresult rv = LoadPlugin(outLibrary);
    if (NS_FAILED(rv))
        return rv;
  
    const char* (*npGetPluginVersion)() =
        (const char* (*)()) PR_FindFunctionSymbol(pLibrary, "NP_GetPluginVersion");
    if (npGetPluginVersion) {
        info.fVersion = PL_strdup(npGetPluginVersion());
    }

    const char* (*npGetMIMEDescription)() =
        (const char* (*)()) PR_FindFunctionSymbol(pLibrary, "NP_GetMIMEDescription");
    if (!npGetMIMEDescription) {
        return NS_ERROR_FAILURE;
    }

    const char* mimedescr = npGetMIMEDescription();
    if (!mimedescr) {
        return NS_ERROR_FAILURE;
    }

    rv = ParsePluginMimeDescription(mimedescr, info);
    if (NS_FAILED(rv)) {
        return rv;
    }

    nsAutoCString path;
    if (NS_FAILED(rv = mPlugin->GetNativePath(path)))
        return rv;
    info.fFullPath = PL_strdup(path.get());

    nsAutoCString fileName;
    if (NS_FAILED(rv = mPlugin->GetNativeLeafName(fileName)))
        return rv;
    info.fFileName = PL_strdup(fileName.get());

    NP_GetValueFunc npGetValue = (NP_GetValueFunc)PR_FindFunctionSymbol(pLibrary, "NP_GetValue");
    if (!npGetValue) {
        return NS_ERROR_FAILURE;
    }

    const char *name = nullptr;
    npGetValue(nullptr, NPPVpluginNameString, &name);
    if (name) {
        info.fName = PL_strdup(name);
    }
    else {
        info.fName = PL_strdup(fileName.get());
    }

    const char *description = nullptr;
    npGetValue(nullptr, NPPVpluginDescriptionString, &description);
    if (description) {
        info.fDescription = PL_strdup(description);
    }
    else {
        info.fDescription = PL_strdup("");
    }

    return NS_OK;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
    if (info.fName != nullptr)
        PL_strfree(info.fName);

    if (info.fDescription != nullptr)
        PL_strfree(info.fDescription);

    for (uint32_t i = 0; i < info.fVariantCount; i++) {
        if (info.fMimeTypeArray[i] != nullptr)
            PL_strfree(info.fMimeTypeArray[i]);

        if (info.fMimeDescriptionArray[i] != nullptr)
            PL_strfree(info.fMimeDescriptionArray[i]);

        if (info.fExtensionArray[i] != nullptr)
            PL_strfree(info.fExtensionArray[i]);
    }

    PR_FREEIF(info.fMimeTypeArray);
    PR_FREEIF(info.fMimeDescriptionArray);
    PR_FREEIF(info.fExtensionArray);

    if (info.fFullPath != nullptr)
        PL_strfree(info.fFullPath);

    if (info.fFileName != nullptr)
        PL_strfree(info.fFileName);

    if (info.fVersion != nullptr)
        PL_strfree(info.fVersion);

    return NS_OK;
}
