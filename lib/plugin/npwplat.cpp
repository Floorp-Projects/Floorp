/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// npwplat.cpp
#include "stdafx.h"
#include <afxole.h>
#include <afxpriv.h>
#include <afxwin.h>
#include <errno.h>
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef WIN32
    #include <io.h>
    #include <winver.h>
#else
    #include <dos.h>
    #include <ver.h>
#endif
#include "npwplat.h"
#include "nppg.h"
#include "plgindll.h"
#include "np.h"
#include "helper.h"
#include "java.h"
#include "edt.h"
#include "npglue.h"

NPPMgtBlk* g_pRegisteredPluginList = NULL;

// Tests that the directory specified by "dir" exists.
// Returns true if it does.

static XP_Bool wfe_CheckDir(const char * dir)
{
    int ret;
    XP_StatStruct statinfo; 

    ret = _stat((char *) dir, &statinfo);
    if(ret == -1)
        return(FALSE);

    return(TRUE);

}

// Get the name of the directory where plugins live.  "csDirname" is where the
// name of the dir is written to, and the trailing slash is removed.
// It always returns NPERR_NO_ERROR, since the Nav had to be started to get
// here, and the plugin dir is under where Nav started from.
NPError wfe_GetPluginsDirectory(CString& csDirname)
{
    char ca_default[_MAX_PATH];
    ::GetModuleFileName(AfxGetApp()->m_hInstance, ca_default, _MAX_PATH);
    char *cp_lastslash = ::strrchr(ca_default, '\\');
    *cp_lastslash = NULL;
    csDirname = ca_default;
    csDirname += "\\plugins";
    return NPERR_NO_ERROR;
}

// Fetches the "MIME type" string from the DLL VERSIONINFO structure.
// "pVersionInfo" is ptr to the VERSIONINFO data, and "pNPMgtBlk" is a handle
// to a plugin management data structure created during fe_RegisterPlugin().
// The string is copied from the VERSIONINFO data into the
// "pNPMgtBlk->szMIMEType" member.
// An error is returned if out of memory.  The MIME type string IS
// required, so an error is returned if the string does not exist.
NPError wfe_GetPluginMIMEType(VS_FIXEDFILEINFO* pVersionInfo,
                              NPPMgtBlk* pNPMgtBlk)
{
    // get the mime type of this plugin
    static char szMimeTypeBlock[] = "StringFileInfo\\040904E4\\MIMEType";
    void* lpBuffer = NULL;
    UINT  uValueSize = 0;
    if(::VerQueryValue(pVersionInfo, szMimeTypeBlock,
                       &lpBuffer, &uValueSize) == 0) // couldn't find MIME type
        return NPERR_GENERIC_ERROR;

    pNPMgtBlk->szMIMEType = new char[uValueSize];
    if(pNPMgtBlk->szMIMEType == NULL)
        return NPERR_OUT_OF_MEMORY_ERROR;
     
    strcpy(pNPMgtBlk->szMIMEType, (const char*)lpBuffer);

    return NPERR_NO_ERROR;
}

// Fetches the "File Extents" string from the DLL VERSIONINFO structure.
// "pVersionInfo" is ptr to the VERSIONINFO data, and "pNPMgtBlk" is a handle
// to a plugin management data structure created during fe_RegisterPlugin().
// The string is copied from the VERSIONINFO data into the
// "pNPMgtBlk->szFileExtents" member.
// An error is returned if out of memory, but the extents string is not
// required, so it is not an error if the string does not exist.
NPError wfe_GetPluginExtents(VS_FIXEDFILEINFO* pVersionInfo,
                             NPPMgtBlk* pNPMgtBlk)
{   // get the file extent list of this plugin
    pNPMgtBlk->szFileExtents = NULL;
    
    static char szFileExtentsBlock[] = "StringFileInfo\\040904E4\\FileExtents";
    void* lpBuffer = NULL;
    UINT  uValueSize = 0;

    if(::VerQueryValue(pVersionInfo, szFileExtentsBlock,
                       &lpBuffer, &uValueSize) == 0)
        // couldn't find file extent
        return NPERR_NO_ERROR;
    
    if((*((char*)lpBuffer) == '\0') || (uValueSize == 0))   // no extents
        return NPERR_NO_ERROR;

    // alloc char array
    pNPMgtBlk->szFileExtents = new char[uValueSize];
    if(pNPMgtBlk->szFileExtents == NULL)
        return NPERR_OUT_OF_MEMORY_ERROR;
    strcpy(pNPMgtBlk->szFileExtents, (const char*)lpBuffer);
        
    return NPERR_NO_ERROR;
}

// Fetches the "File Open Template" string from the DLL VERSIONINFO structure.
// "pVersionInfo" is ptr to the VERSIONINFO data, and "pNPMgtBlk" is a handle
// to a plugin management data structure created during fe_RegisterPlugin().
// The string is copied from the VERSIONINFO data into the
// "pNPMgtBlk->szFileOpenName" member.
// An error is returned if out of memory, but the template string is not
// required, so it is not an error if the string does not exist.
NPError wfe_GetPluginFileOpenTemplate(VS_FIXEDFILEINFO* pVersionInfo,
                                      NPPMgtBlk* pNPMgtBlk)
{   // get the file open template this plugin
    pNPMgtBlk->szFileOpenName = NULL;
    
    static char szFileOpenBlock[] = "StringFileInfo\\040904E4\\FileOpenName";
    void* lpBuffer = NULL;
    UINT  uValueSize = 0;

    if(::VerQueryValue(pVersionInfo, szFileOpenBlock,
                       &lpBuffer, &uValueSize) == 0)
        // couldn't find file extent
        return NPERR_NO_ERROR;
    
    if((*((char*)lpBuffer) == '\0') || (uValueSize == 0))   // no extents
        return NPERR_NO_ERROR;

    // alloc char array
    pNPMgtBlk->szFileOpenName = new char[uValueSize];
    if(pNPMgtBlk->szFileOpenName == NULL)
        return NPERR_OUT_OF_MEMORY_ERROR;
    strcpy(pNPMgtBlk->szFileOpenName, (const char*)lpBuffer);

    return NPERR_NO_ERROR;
}

// Fetches the plugin name and description from the resource info.
// Uses predefined version information strings
NPError wfe_GetPluginNameAndDescription(VS_FIXEDFILEINFO* pVersionInfo,
                                        CString&		  strName,
                                        CString&		  strDescription)
{
    static char szNameBlock[] = "StringFileInfo\\040904E4\\ProductName";
    static char szDescBlock[] = "StringFileInfo\\040904E4\\FileDescription";
    void* lpBuffer = NULL;
    UINT  uValueSize = 0;

    // Get the product name
    if(::VerQueryValue(pVersionInfo, szNameBlock, &lpBuffer, &uValueSize) > 0)
        strName = (LPSTR)lpBuffer;

    // Get the file description
    if(::VerQueryValue(pVersionInfo, szDescBlock, &lpBuffer, &uValueSize) > 0)
        strDescription = (LPSTR)lpBuffer;

    return NPERR_NO_ERROR;
}

// Loads plugin properties from the DLL VERSIONINFO data structure.
// MIME type and version are required, but file extents and string
// file open templates are optional.  "pPluginFilespec" is a string
// containing the file name of a DLL.  "pNPMgtBlk" is a handle to a
// plugin management data structure created during fe_RegisterPlugin().  
// Returns an error if "pPluginFilespec" did not have the required MIME
// type and version fields.
//
// Also returns the plugin name and description
NPError wfe_GetPluginProperties(char* 		pPluginFilespec,
                                NPPMgtBlk* 	pNPMgtBlk,
                                CString&	strPluginName,
                                CString&	strPluginDescription)
{
    // prepare to read the version info tags
    DWORD dwHandle = NULL;
    DWORD dwVerInfoSize = ::GetFileVersionInfoSize(pPluginFilespec, &dwHandle);
    if(dwVerInfoSize == NULL)
        return NPERR_GENERIC_ERROR;

    VS_FIXEDFILEINFO* pVersionInfo =
        (VS_FIXEDFILEINFO*)new char[dwVerInfoSize];
    if(pVersionInfo == NULL)
        return NPERR_OUT_OF_MEMORY_ERROR;

    if(::GetFileVersionInfo(pPluginFilespec, dwHandle,
                            dwVerInfoSize, pVersionInfo) == 0)
    {
        delete [] (char*)pVersionInfo;
        return NPERR_GENERIC_ERROR;
    }

    NPError result;
    // get the mime type of this plugin
    if((result = wfe_GetPluginMIMEType(pVersionInfo, pNPMgtBlk))
       != NPERR_NO_ERROR)
    {
        delete [] (char*)pVersionInfo;
        return result;
    }

    // get the file extent list of this plugin
    if((result = wfe_GetPluginExtents(pVersionInfo, pNPMgtBlk))
       != NPERR_NO_ERROR)
    
    {
        delete [] pNPMgtBlk->szMIMEType;
        delete [] (char*)pVersionInfo;
        return result;
    }

    // get the file type template string of this plugin
    if((result = wfe_GetPluginFileOpenTemplate(pVersionInfo, pNPMgtBlk))
       != NPERR_NO_ERROR)
    
    {
        delete [] pNPMgtBlk->szFileExtents;
        delete [] pNPMgtBlk->szMIMEType;
        delete [] (char*)pVersionInfo;
        return result;
    }

    // get the version of this plugin
    void* lpBuffer = NULL;
    UINT  uValueSize = 0;
    if(::VerQueryValue(pVersionInfo, "\\", &lpBuffer, &uValueSize) == 0)
    {   // couldn't find versioninfo
        delete [] pNPMgtBlk->szFileExtents;
        delete [] pNPMgtBlk->szMIMEType;
        delete [] (char*)pVersionInfo;
        return NPERR_GENERIC_ERROR;
    }
    pNPMgtBlk->versionMS  = ((VS_FIXEDFILEINFO*)lpBuffer)->dwProductVersionMS;
    pNPMgtBlk->versionLS  = ((VS_FIXEDFILEINFO*)lpBuffer)->dwProductVersionLS;

    // get the plugin name and description. It's not fatal if these fail
    wfe_GetPluginNameAndDescription(pVersionInfo, strPluginName, strPluginDescription);

    delete [] (char*)pVersionInfo;

    return NPERR_NO_ERROR;
}

// Given a file open filter, e.g. Text Documents(*.txt), extracts the
// description and returns a copy of it
static LPSTR
ExtractDescription(LPCSTR lpszFileOpenFilter)
{
    ASSERT(lpszFileOpenFilter);

    LPSTR lpszDescription = XP_STRDUP(lpszFileOpenFilter);

    // Strip off an filter pattern at the end
    LPSTR lpszPattern = strrchr(lpszDescription, '(');

    if (lpszPattern)
        *lpszPattern = '\0';

    return lpszDescription;
}

// Registers one plugin.  "pPluginFilespec" is a string containing the file
// name of a DLL.
// This function can be called more than once so a plugin which has copied
// into the plugins subdir AFTER the Nav has started can be registered
// and loaded.
// The DLL is a plugin candidate, and is qualified during
// this function, by checking the DLL VERSIONINFO data structure for a
// MIME type string.  If this string is found successfully, the handle
// of the management block is used to register the plugin for its MIME type.
// The extents which are to be associated with this MIME type are also grokked
// from the VERSIONINFO data structure, once the candidate is accepted.
// This function also takes the opportunity to overrided any helper app, by
// setting a helper app data structure member to allow the plugin to handle
// the MIME type instead of the helper, (pApp->how_handle = HANDLE_VIA_PLUGIN).
// Returns an error if "pPluginFilespec" was not registered.
NPError fe_RegisterPlugin(char* pPluginFilespec)
{
    CString	strName, strDescription;

    // initialize a new plugin list mgt block
    NPPMgtBlk* pNPMgtBlk = new NPPMgtBlk;
    if(pNPMgtBlk == NULL)   // fatal, can't continue
        return NPERR_OUT_OF_MEMORY_ERROR;
        
    pNPMgtBlk->pPluginFuncs = NULL;
    pNPMgtBlk->pLibrary		= NULL;
    pNPMgtBlk->uRefCount    = 0;
    pNPMgtBlk->next         = NULL;
    
    // determine the MIME type and version of this plugin
    if(wfe_GetPluginProperties(pPluginFilespec, pNPMgtBlk, strName, strDescription) != NPERR_NO_ERROR)
    {
        delete pNPMgtBlk;
        return NPERR_GENERIC_ERROR;
    }

    // if a plugin is already registered for this MIME type, return.  this
    // allows downloading and registering new plugins without exiting the nav
    for(NPPMgtBlk* pListBlk = g_pRegisteredPluginList; pListBlk != NULL;
        pListBlk = pListBlk->next) {
        BOOL bSameFile =
            (strcmp(pListBlk->pPluginFilename, pPluginFilespec) == 0);
        BOOL bSameMIMEtype =
            (strstr(pNPMgtBlk->szMIMEType, pListBlk->szMIMEType) != NULL);
        if(bSameFile && bSameMIMEtype) {
            // the plugin DLL's filename and the MIME type it's registering for
            // are the same, don't reregister
            delete pNPMgtBlk;
            return NPERR_GENERIC_ERROR;
        }
    }    

    pNPMgtBlk->pPluginFilename = XP_STRDUP(pPluginFilespec);
    if(pNPMgtBlk->pPluginFilename == NULL) // fatal, can't continue
    {
        delete pNPMgtBlk;
        return NPERR_OUT_OF_MEMORY_ERROR;
    }

    // Register the plugin file with the XP plugin code
    NPL_RegisterPluginFile(strName, pNPMgtBlk->pPluginFilename, strDescription,
                           pNPMgtBlk);

    // Iterate through the filename extensions.  These are a list which are
    // delimited via the '|' character.  The list of MIME Types,File Extents, 
    // and Open Names must all coordinate
    char *pStartMIME, *pEndMIME, *pStartExt, *pEndExt, *pStartName, *pEndName;

    pStartMIME = pNPMgtBlk->szMIMEType;
    pStartExt = pNPMgtBlk->szFileExtents;
    pStartName = pNPMgtBlk->szFileOpenName;

    pEndMIME = strchr(pStartMIME ,'|');
    while (pEndMIME) {
        pEndExt = strchr(pStartExt,'|');
        pEndName = strchr(pStartName,'|');
        if (pEndMIME) *pEndMIME = 0;
        else return NPERR_GENERIC_ERROR;
        if (pEndExt)  *pEndExt = 0;
        else return NPERR_GENERIC_ERROR;
        if (pEndName) *pEndName = 0;
        else return NPERR_GENERIC_ERROR;

        // Register the MIME type with the XP plugin code. We need to pass in
        // a description. If there's a file open template specified, then use the
        // description from there. Otherwise use the MIME type
        LPSTR	lpszDescription = NULL;

        if (pStartName)
            lpszDescription = ExtractDescription(pStartName);

        NPL_RegisterPluginType(pStartMIME, (LPCSTR)pStartExt, lpszDescription ?
                               lpszDescription : pStartMIME, (void *)pStartName, pNPMgtBlk, TRUE);

        if (lpszDescription)
            XP_FREE(lpszDescription);

        CHelperApp *pApp;
        if(theApp.m_HelperListByType.Lookup(pStartMIME, (CObject *&)pApp))  {
            //  We've a match.
            //  Make sure the app is marked to handle the load
            //      via a plugin
            pApp->how_handle = HANDLE_VIA_PLUGIN;
        }

        if ((pEndMIME+1) && (pEndExt+1) && (pEndName+1)) {
            pStartMIME = pEndMIME+1;
            pStartExt = pEndExt+1;
            pStartName = pEndName+1;
        }
        pEndMIME = strchr(pStartMIME ,'|');    
    }

    // Register the MIME type with the XP plugin code. We need to pass in
    // a description. If there's a file open template specified, then use the
    // description from there. Otherwise use the MIME type
    LPSTR	lpszDescription = NULL;

    if (pStartName)
        lpszDescription = ExtractDescription(pStartName);

    NPL_RegisterPluginType(pStartMIME, (LPCSTR)pStartExt, lpszDescription ?
                           lpszDescription : pStartMIME, (void *)pStartName, pNPMgtBlk, TRUE);

    if (lpszDescription)
        XP_FREE(lpszDescription);

    CHelperApp *pApp;
    if(theApp.m_HelperListByType.Lookup(pStartMIME, (CObject *&)pApp))  {
        //  We've a match.
        //  Make sure the app is marked to handle the load
        //      via a plugin
        pApp->how_handle = HANDLE_VIA_PLUGIN;
    }

    // insert the plugin mgt blk at the head of the list of registered plugins.
    // this means they are listed in the reverse order from which they were
    // created, which doesn't matter.
    pNPMgtBlk->next = g_pRegisteredPluginList;
    g_pRegisteredPluginList = pNPMgtBlk;

    return NPERR_NO_ERROR;
}

// FE_RegisterPlugins is called from navigator main via npglue's NP_Init(). Finds all
// plugins and begins tracking them using a NPPMgtBlk.  Uses the NPPMgtBlk
// block handle to register the plugin with the xp plugin glue. Looks
// in the directory under the netscape.exe dir, named "plugins" and all
// subdirectories in recursive way (see fe_RegisterPlugins).  If the
// directory doesn't exist, no warning dialog is shown.
// There are no input or return vals.
void fe_RegisterPlugins(char* pszPluginDir)
{
    CString csPluginSpec;
    csPluginSpec = pszPluginDir; 

#ifdef JAVA
    // add directory to the java path no matter what
    LJ_AddToClassPath(pszPluginDir);
#endif

    csPluginSpec += "\\*.*"; 

#ifndef _WIN32
    _find_t fileinfo;
    unsigned result = _dos_findfirst((LPSTR)((LPCSTR)csPluginSpec), _A_NORMAL | _A_SUBDIR, &fileinfo );

    if (result == 0) {

        result = _dos_findnext(&fileinfo); // skip "."
        result = _dos_findnext(&fileinfo); // skip ".."

        CString csFileSpec;

        while(result == 0)
        {
            csFileSpec = pszPluginDir;
            csFileSpec += "\\";
            csFileSpec += fileinfo.name;

            // we got a subdir, call recursively this function to load plugin
            if (fileinfo.attrib & _A_SUBDIR ) {
                fe_RegisterPlugins((LPSTR)(LPCSTR)csFileSpec);
            }
            else {
                size_t len = strlen(fileinfo.name);

                // it's a file, see if it can be a plugin file
                if ( len > 6 && // at least "np.dll"
                     (fileinfo.name[0] == 'n' || fileinfo.name[0] == 'N') && 
                     (fileinfo.name[1] == 'p' || fileinfo.name[1] == 'P') &&
                     !_stricmp(fileinfo.name + len - 4, ".dll"))
                    fe_RegisterPlugin((LPSTR)((LPCSTR)csFileSpec));
#ifdef EDITOR
                // If it's a cpXXX.zip file,register it as a composer plugin.
                else if ( len > 6 && // at least cp.zip
                          (fileinfo.name[0] == 'c' || fileinfo.name[0] == 'C') && 
                          (fileinfo.name[1] == 'p' || fileinfo.name[1] == 'P') &&
                          (!_stricmp(fileinfo.name + len - 4, ".zip")
                           || !_stricmp(fileinfo.name + len - 4, ".jar"))
                    )
                    EDT_RegisterPlugin((LPSTR)((LPCSTR)csFileSpec));
#endif /* EDITOR */
            }
            result = _dos_findnext(&fileinfo);
        }
    }
#else   /* _WIN32 */
    _finddata_t fileinfo;
    unsigned handle = _findfirst((LPSTR)((LPCSTR)csPluginSpec), &fileinfo );
    unsigned result = 0;

    if (handle != -1) {
        result = _findnext(handle, &fileinfo); // skip "."
        result = _findnext(handle, &fileinfo); // skip ".."
    

        CString csFileSpec;

        while((result != -1) && (handle != -1))
        {
            csFileSpec = pszPluginDir;
            csFileSpec += "\\";
            csFileSpec += fileinfo.name;

            // we got a subdir, call recursively this function to load plugin
            if (fileinfo.attrib & _A_SUBDIR ) {
                fe_RegisterPlugins((LPSTR)(LPCSTR)csFileSpec);
            }
            else {
                size_t len = strlen(fileinfo.name);

                // it's a file, see if it can be a plugin file
                if ( len > 6 && // at least "np.dll"
                     (fileinfo.name[0] == 'n' || fileinfo.name[0] == 'N') && 
                     (fileinfo.name[1] == 'p' || fileinfo.name[1] == 'P') &&
                     !_stricmp(fileinfo.name + len - 4, ".dll"))
                    fe_RegisterPlugin((LPSTR)((LPCSTR)csFileSpec));
#ifdef EDITOR
                // If it's a cpXXX.zip file, add it to the java class path.
                else if ( len > 6 && // at least cp.zip
                          (fileinfo.name[0] == 'c' || fileinfo.name[0] == 'C') && 
                          (fileinfo.name[1] == 'p' || fileinfo.name[1] == 'P') &&
                          (!_stricmp(fileinfo.name + len - 4, ".zip")
                           || !_stricmp(fileinfo.name + len - 4, ".jar"))
                    )
                    EDT_RegisterPlugin((LPSTR)((LPCSTR)csFileSpec));
#endif
            }
            result = _findnext(handle, &fileinfo);
        }
        _findclose(handle);
    }
#endif  /* _WIN32 */
}

// called from the navigator main, dispatch to the previous function
void FE_RegisterPlugins()
{
    CString csPluginDir;
    // get the main plugins directory and start the process
    wfe_GetPluginsDirectory(csPluginDir);
    fe_RegisterPlugins((LPSTR)(LPCSTR)csPluginDir);
}
    
// saves the current path in the variable passed.  ppPathSave is the
// address of the ptr which stores the path string.  returns out of memory
// error if allocation for the string returned by _getcwd() fails.  returns
// generic error if the "one deep" stack is already full.
NPError wfe_PushPath(LPSTR *ppPathSave)
{
    // only allow "one deep" stack
    if(*ppPathSave != NULL)
        return NPERR_GENERIC_ERROR;

    // save the CWD
    *ppPathSave = _getcwd(NULL, 0);
    if(*ppPathSave == NULL)
        return NPERR_OUT_OF_MEMORY_ERROR;

    return NPERR_NO_ERROR;
}

// calculate the drive letter value for _chdrive().  ascii in, int out 
int wfe_MapAsciiToDriveNum(char cAsciiNum)
{
    int iDriveLetter = cAsciiNum;
    if(islower(cAsciiNum))
        iDriveLetter = _toupper(cAsciiNum);
    // plus one because for _chdrive(),  A = 1, B = 2, etc
    return iDriveLetter - 'A' + 1;
}

// restores the path from the variable passed.  pDirSave stores the pointer
// to the path string.  returns an error if a path has never been pushed.
NPError wfe_PopPath(LPSTR pPathSave)
{
    // only allows "one deep" stack
    if(pPathSave == NULL)
        return NPERR_GENERIC_ERROR;

    // restore the drive
    int chdriveErr = _chdrive(wfe_MapAsciiToDriveNum(pPathSave[0]));

    // restore the CWD
    int chdirErr = _chdir(pPathSave);
    free(pPathSave);

    return NPERR_NO_ERROR;
}

// Load a plugin dll.  "pluginType" is a handle to a plugin management data
// structure created during FE_RegisterPlugins().  "pNavigatorFuncs" is
// a table of entry points into Navigator, which are the services provided
// by Navigator to plugins, such as requestread() and GetUrl().  These entry
// points are stored and called by the plugin when services are needed.
// The return val is a table of functions which are entry points to the
// newly loaded plugin, such as NewStream() and Write().
NPPluginFuncs* FE_LoadPlugin(void* pluginType, NPNetscapeFuncs* pNavigatorFuncs, np_handle* handle)
{
    if(pluginType == NULL)
        return NULL;

    NPPMgtBlk* pNPMgtBlock = (NPPMgtBlk*)pluginType;

    CString csPluginDir;
    wfe_GetPluginsDirectory(csPluginDir);

    LPSTR pPathSave = NULL;   // storage for one dir spec

    wfe_PushPath(&pPathSave);  // save the current drive and dir

    // change the default dir so that implib'd plugins won't fail
    if(_chdir(csPluginDir) != 0) {
        wfe_PopPath(pPathSave);  // restore the path
        return NULL;
    }

    // must change the drive as well as the dir path
    if(isalpha(csPluginDir[0]) && csPluginDir[1] == ':') {
        if(_chdrive(wfe_MapAsciiToDriveNum(csPluginDir[0])) != 0) {
            wfe_PopPath(pPathSave);  // restore the path
            return NULL;
        }
    }

    pNPMgtBlock->pLibrary = PR_LoadLibrary(pNPMgtBlock->pPluginFilename);

    // the cross platform code should take care of the 16/32 bit issues
    if(pNPMgtBlock->pLibrary == NULL)
        return NULL;
    
    NP_CREATEPLUGIN npCreatePlugin =
#ifndef NSPR20
        (NP_CREATEPLUGIN)PR_FindSymbol("NP_CreatePlugin", pNPMgtBlock->pLibrary);
#else
        (NP_CREATEPLUGIN)PR_FindSymbol(pNPMgtBlock->pLibrary, "NP_CreatePlugin");
#endif
    if (npCreatePlugin != NULL) {
        if (thePluginManager == NULL) {
            static NS_DEFINE_IID(kIPluginManagerIID, NP_IPLUGINMANAGER_IID);
            if (nsPluginManager::Create(NULL, kIPluginManagerIID, (void**)&thePluginManager) != NS_OK)
                return NULL;
        }
        NPIPlugin* plugin = NULL;
        NPPluginError err = npCreatePlugin(thePluginManager, &plugin);
        handle->userPlugin = plugin;
        if (err == NPPluginError_NoError && plugin != NULL) {
            pNPMgtBlock->pPluginFuncs = (NPPluginFuncs*)-1;   // something to say it's loaded, but != 0
        }
    }
    else {

        NP_GETENTRYPOINTS getentrypoints =
#ifndef NSPR20
            (NP_GETENTRYPOINTS)PR_FindSymbol("NP_GetEntryPoints", pNPMgtBlock->pLibrary);
#else
            (NP_GETENTRYPOINTS)PR_FindSymbol(pNPMgtBlock->pLibrary, "NP_GetEntryPoints");
#endif
        if(getentrypoints == NULL)
        {
            PR_UnloadLibrary(pNPMgtBlock->pLibrary);
            wfe_PopPath(pPathSave);  // restore the path
            return NULL;
        }
    
        if(pNPMgtBlock->pPluginFuncs == NULL)
        {
            pNPMgtBlock->pPluginFuncs = new NPPluginFuncs;
            pNPMgtBlock->pPluginFuncs->size = sizeof NPPluginFuncs;
            pNPMgtBlock->pPluginFuncs->javaClass = NULL;
            if(pNPMgtBlock->pPluginFuncs == NULL)   // fatal, can't continue
            {
                PR_UnloadLibrary(pNPMgtBlock->pLibrary);
                wfe_PopPath(pPathSave);  // restore the path
                return NULL;
            }
        }

        if(getentrypoints(pNPMgtBlock->pPluginFuncs) != NPERR_NO_ERROR)
        {
            PR_UnloadLibrary(pNPMgtBlock->pLibrary);
            delete pNPMgtBlock->pPluginFuncs;
            pNPMgtBlock->pPluginFuncs = NULL;
            wfe_PopPath(pPathSave);  // restore the path
            return NULL;
        }

        // if the plugin's major ver level is lower than the Navigator's,
        // then they are incompatible, and should return an error
        if(HIBYTE(pNPMgtBlock->pPluginFuncs->version) < NP_VERSION_MAJOR)
        {
            PR_UnloadLibrary(pNPMgtBlock->pLibrary);
            delete pNPMgtBlock->pPluginFuncs;
            pNPMgtBlock->pPluginFuncs = NULL;
            wfe_PopPath(pPathSave);  // restore the path
            return NULL;
        }

        NP_PLUGININIT npinit = NULL;

        // if this DLL is not already loaded, call its initialization entry point
        if(pNPMgtBlock->uRefCount == 0) {
            // the NP_Initialize entry point was misnamed as NP_PluginInit, early
            // in plugin project development.  Its correct name is documented
            // now, and new developers expect it to work.  However, I don't want
            // to  break the plugins already in the field, so I'll accept either
            // name
            npinit =
#ifndef NSPR20
                (NP_PLUGININIT)PR_FindSymbol("NP_Initialize", pNPMgtBlock->pLibrary);
#else
                (NP_PLUGININIT)PR_FindSymbol(pNPMgtBlock->pLibrary, "NP_Initialize");
#endif
            if(!npinit) {
                npinit =
#ifndef NSPR20
                    (NP_PLUGININIT)PR_FindSymbol("NP_PluginInit", pNPMgtBlock->pLibrary);
#else
                    (NP_PLUGININIT)PR_FindSymbol(pNPMgtBlock->pLibrary, "NP_PluginInit");
#endif
            }
        }

        if(npinit == NULL) {
            PR_UnloadLibrary(pNPMgtBlock->pLibrary);
            delete pNPMgtBlock->pPluginFuncs;
            pNPMgtBlock->pPluginFuncs = NULL;
            wfe_PopPath(pPathSave);  // restore the path
            return NULL;
        }

        if (npinit(pNavigatorFuncs) != NPERR_NO_ERROR) {
            PR_UnloadLibrary(pNPMgtBlock->pLibrary);
            delete pNPMgtBlock->pPluginFuncs;
            pNPMgtBlock->pPluginFuncs = NULL;
            wfe_PopPath(pPathSave);  // restore the path
            return NULL;
        }
    }
    
    pNPMgtBlock->uRefCount++;   // all is well, remember it was loaded
    
    // can't pop the path until plugins have been completely loaded/init'd
    wfe_PopPath(pPathSave);  // restore the path
    return pNPMgtBlock->pPluginFuncs;
}

// Unloads a plugin DLL.  "pluginType" is a handle to a plugin management data
// structure created during FE_RegisterPlugins().  There is no return val.
void FE_UnloadPlugin(void* pluginType, struct _np_handle* handle)
{
    NPPMgtBlk* pNPMgtBlk = (NPPMgtBlk*)pluginType;
    if (pNPMgtBlk == NULL)
        return;

    // XXX Note that we're double refcounting here. If we were rewriting this 
    // from scratch, we could eliminate this pNPMgtBlk and its refcount and just
    // rely on the one in the userPlugin.

    pNPMgtBlk->uRefCount--;   // another one bites the dust

    // if this DLL is the last one, call its shutdown entry point
    if (pNPMgtBlk->uRefCount == 0) {
        if (handle->userPlugin) {
            handle->userPlugin->Release();
        }
        else {
            // the NP_Shutdown entry point was misnamed as NP_PluginShutdown, early
            // in plugin project development.  Its correct name is documented
            // now, and new developers expect it to work.  However, I don't want
            // to  break the plugins already in the field, so I'll accept either
            // name
            NP_PLUGINSHUTDOWN npshutdown =
#ifndef NSPR20
                (NP_PLUGINSHUTDOWN)PR_FindSymbol("NP_Shutdown", pNPMgtBlk->pLibrary);
#else
                (NP_PLUGINSHUTDOWN)PR_FindSymbol(pNPMgtBlk->pLibrary, "NP_Shutdown");
#endif
            if (!npshutdown) {
                npshutdown =
#ifndef NSPR20
                    (NP_PLUGINSHUTDOWN)PR_FindSymbol("NP_PluginShutdown", pNPMgtBlk->pLibrary);
#else
                    (NP_PLUGINSHUTDOWN)PR_FindSymbol(pNPMgtBlk->pLibrary, "NP_PluginShutdown");
#endif
            }

            if (npshutdown != NULL) {
                NPError result = npshutdown();
            }
        }
    }

    // XXX Shouldn't the rest of this be inside the if statement, above?...

    PR_UnloadLibrary(pNPMgtBlk->pLibrary);
    pNPMgtBlk->pLibrary = NULL;
    
    if (handle->userPlugin) {
        PR_ASSERT(pNPMgtBlk->pPluginFuncs == (NPPluginFuncs*)-1);     // set in FE_LoadPlugin
        handle->userPlugin = NULL;
    }
    else {
        delete pNPMgtBlk->pPluginFuncs;
    }
    pNPMgtBlk->pPluginFuncs = NULL;
}

// Removes a plugin DLL from memory, and frees its NPPMgtBlk management block.
// "pluginType" is a handle to the plugin management data structure created
// during FE_RegisterPlugins().  There is no return val.
void FE_UnregisterPlugin(void* pluginType)
{
    NPPMgtBlk* pNPMgtBlk = (NPPMgtBlk*)pluginType;
    if(pNPMgtBlk == NULL)
        return;
    
    if(pNPMgtBlk->pLibrary != NULL)
		PR_UnloadLibrary(pNPMgtBlk->pLibrary);
    
    delete [] (char*)*pNPMgtBlk->pPluginFilename;
    delete [] pNPMgtBlk->pPluginFilename;
    delete [] pNPMgtBlk->szMIMEType;
    delete pNPMgtBlk->pPluginFuncs;
    delete pNPMgtBlk;
}

// Provides the entry point used by xp plugin code, (NPGLUE.C), to control
// scroll bars.  The inputs are obvious, and there is no return val.
void FE_ShowScrollBars(MWContext* pContext, XP_Bool show)
{
    if(ABSTRACTCX(pContext)->IsWindowContext())	{
        CPaneCX *pPaneCX = PANECX(pContext);

        pPaneCX->ShowScrollBars(SB_BOTH, show);
    }
}

#ifdef XP_WIN32
void*
WFE_BeginSetModuleState()
{
    return AfxSetModuleState(AfxGetAppModuleState());	
}

void
WFE_EndSetModuleState(void* pPrevState)
{
    AfxSetModuleState((AFX_MODULE_STATE*)pPrevState);	
}
#endif


