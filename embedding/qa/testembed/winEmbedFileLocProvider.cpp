/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Conrad Carlen <conrad@ingress.com>
 */
 
#include "winEmbedFileLocProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsXPIDLString.h"


#include <windows.h>
#include <shlobj.h>

 
// WARNING: These hard coded names need to go away. They need to
// come from localizable resources
#define APP_REGISTRY_NAME "registry.dat"

#define PROFILE_ROOT_DIR_NAME       "Profiles"
#define DEFAULTS_DIR_NAME           "defaults"
#define DEFAULTS_PREF_DIR_NAME      "pref"
#define DEFAULTS_PROFILE_DIR_NAME   "profile"
#define RES_DIR_NAME                "res"
#define CHROME_DIR_NAME             "chrome"
#define PLUGINS_DIR_NAME            "plugins"
#define SEARCH_DIR_NAME             "searchplugins" 


//*****************************************************************************
// winEmbedFileLocProvider::Constructor/Destructor
//*****************************************************************************   

winEmbedFileLocProvider::winEmbedFileLocProvider(const char* productDirName)
{
    NS_INIT_ISUPPORTS();

    strncpy(mProductDirName, productDirName, sizeof(mProductDirName) - 1);
    mProductDirName[sizeof(mProductDirName) - 1] = '\0';
}

winEmbedFileLocProvider::~winEmbedFileLocProvider()
{
}


//*****************************************************************************
// winEmbedFileLocProvider::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(winEmbedFileLocProvider, nsIDirectoryServiceProvider)

//*****************************************************************************
// winEmbedFileLocProvider::nsIDirectoryServiceProvider
//*****************************************************************************   

NS_IMETHODIMP
winEmbedFileLocProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{    
	nsCOMPtr<nsILocalFile>  localFile;
	nsresult rv = NS_ERROR_FAILURE;

	*_retval = nsnull;
	*persistant = PR_TRUE;
	
    if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_DIR) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_FILE) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(APP_REGISTRY_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_PREF_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativePath(DEFAULTS_PREF_DIR_NAME);
        }
    }
    else if (nsCRT::strcmp(prop, NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR) == 0 ||
             nsCRT::strcmp(prop, NS_APP_PROFILE_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativePath(DEFAULTS_PROFILE_DIR_NAME);
        }
    }
    else if (nsCRT::strcmp(prop, NS_APP_USER_PROFILES_ROOT_DIR) == 0)
    {
        rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile));   
    }
    else if (nsCRT::strcmp(prop, NS_APP_RES_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(RES_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_CHROME_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(CHROME_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_PLUGINS_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(PLUGINS_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_SEARCH_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(SEARCH_DIR_NAME);
    }
    
	if (localFile && NS_SUCCEEDED(rv))
		return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
		
	return rv;
}


NS_METHOD winEmbedFileLocProvider::CloneMozBinDirectory(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);
    nsresult rv;
    
    if (!mMozBinDirectory)
    {        
        // Get the mozilla bin directory
        // 1. Check the directory service first for NS_XPCOM_CURRENT_PROCESS_DIR
        //    This will be set if a directory was passed to NS_InitXPCOM
        // 2. If that doesn't work, set it to be the current process directory
        
//        NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
	    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID,&rv));

        if (NS_FAILED(rv))
            return rv;
        
        rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));
        if (NS_FAILED(rv)) {
            rv = directoryService->Get(NS_OS_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));
            if (NS_FAILED(rv))
                return rv;
        }
    }
    
    nsCOMPtr<nsIFile> aFile;
    rv = mMozBinDirectory->Clone(getter_AddRefs(aFile));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsILocalFile> lfile = do_QueryInterface (aFile);
    if (!lfile)
        return NS_ERROR_FAILURE;
    
    NS_IF_ADDREF(*aLocalFile = lfile);
    return NS_OK;
}


//----------------------------------------------------------------------------------------
// GetProductDirectory - Gets the directory which contains the application data folder
//
// WIN    : <Application Data folder on user's machine>\Mozilla 
//----------------------------------------------------------------------------------------
NS_METHOD winEmbedFileLocProvider::GetProductDirectory(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);
    
    nsresult rv;
    PRBool exists;
    nsCOMPtr<nsILocalFile> localDir;
   
    //NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID,&rv));

    if (NS_FAILED(rv)) return rv;
    rv = directoryService->Get(NS_WIN_APPDATA_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_SUCCEEDED(rv))
        rv = localDir->Exists(&exists);
    if (NS_FAILED(rv) || !exists)
    {
        // On some Win95 machines, NS_WIN_APPDATA_DIR does not exist - revert to NS_WIN_WINDOWS_DIR
        localDir = nsnull;
        rv = directoryService->Get(NS_WIN_WINDOWS_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    }
    if (NS_FAILED(rv)) return rv;

    rv = localDir->AppendRelativePath(mProductDirName);
    if (NS_FAILED(rv)) return rv;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;

    *aLocalFile = localDir;
    NS_ADDREF(*aLocalFile);

   return rv; 
}


//----------------------------------------------------------------------------------------
// GetDefaultUserProfileRoot - Gets the directory which contains each user profile dir
//
// WIN    : <Application Data folder on user's machine>\Mozilla\Users50 
//----------------------------------------------------------------------------------------
NS_METHOD winEmbedFileLocProvider::GetDefaultUserProfileRoot(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);
    
    nsresult rv;
    PRBool exists;
    nsCOMPtr<nsILocalFile> localDir;
   
    rv = GetProductDirectory(getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;

    // These 3 platforms share this part of the path - do them as one
    rv = localDir->AppendRelativePath(PROFILE_ROOT_DIR_NAME);
    if (NS_FAILED(rv)) return rv;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;

    *aLocalFile = localDir;
    NS_ADDREF(*aLocalFile);

   return rv; 
}

