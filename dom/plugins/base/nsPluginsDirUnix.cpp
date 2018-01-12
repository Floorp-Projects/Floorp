/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIMemory.h"
#include "nsPluginsDir.h"
#include "nsPluginsDirUtils.h"
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

/* nsPluginsDir implementation */

bool nsPluginsDir::IsPluginFile(nsIFile* file)
{
    nsAutoCString filename;
    if (NS_FAILED(file->GetNativeLeafName(filename)))
        return false;

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

    *outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
    pLibrary = *outLibrary;

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

    free(info.fMimeTypeArray);
    info.fMimeTypeArray = nullptr;
    free(info.fMimeDescriptionArray);
    info.fMimeDescriptionArray = nullptr;
    free(info.fExtensionArray);
    info.fExtensionArray = nullptr;

    if (info.fFullPath != nullptr)
        PL_strfree(info.fFullPath);

    if (info.fFileName != nullptr)
        PL_strfree(info.fFileName);

    if (info.fVersion != nullptr)
        PL_strfree(info.fVersion);

    return NS_OK;
}
