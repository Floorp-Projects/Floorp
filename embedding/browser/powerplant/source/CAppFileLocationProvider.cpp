/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#include "CAppFileLocationProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIAtom.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"

#include <Folders.h>
#include <Script.h>
#include <Processes.h>
#include "nsILocalFileMac.h"

#include "ApplIDs.h"  

//*****************************************************************************
// CAppFileLocationProvider::Constructor/Destructor
//*****************************************************************************   

CAppFileLocationProvider::CAppFileLocationProvider(const nsAString& aAppDataDirName) :
    mAppDataDirName(aAppDataDirName)
{
}

CAppFileLocationProvider::~CAppFileLocationProvider()
{
}


//*****************************************************************************
// CAppFileLocationProvider::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CAppFileLocationProvider, nsIDirectoryServiceProvider)

//*****************************************************************************
// CAppFileLocationProvider::nsIDirectoryServiceProvider
//*****************************************************************************   

NS_IMETHODIMP
CAppFileLocationProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{    
    nsCOMPtr<nsILocalFile>  localFile;
    nsresult rv = NS_ERROR_FAILURE;

    *_retval = nsnull;
    *persistant = PR_TRUE;
	
    if (strcmp(prop, NS_APP_APPLICATION_REGISTRY_DIR) == 0)
    {
        rv = GetAppDataDirectory(getter_AddRefs(localFile));
    }
    else if (strcmp(prop, NS_APP_APPLICATION_REGISTRY_FILE) == 0)
    {
        rv = GetAppDataDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendNative(nsDependentCString("Application Registry"));
    }
    else if (strcmp(prop, NS_APP_USER_PROFILES_ROOT_DIR) == 0)
    {
        rv = GetAppDataDirectory(getter_AddRefs(localFile));
        if (NS_FAILED(rv))
          return rv;
        rv = localFile->AppendNative(nsDependentCString("Profiles"));
        if (NS_FAILED(rv))
          return rv;
          
        PRBool exists;
        rv = localFile->Exists(&exists);
        if (NS_SUCCEEDED(rv) && !exists)
          rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv))
          return rv;
    }
    
    if (localFile && NS_SUCCEEDED(rv))
      rv = localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
		
    return rv;
}

//----------------------------------------------------------------------------------------
// GetAppDataDirectory - Gets the directory which contains the application data folder
//----------------------------------------------------------------------------------------
NS_METHOD CAppFileLocationProvider::GetAppDataDirectory(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);
    NS_ENSURE_TRUE(!mAppDataDirName.IsEmpty(), NS_ERROR_NOT_INITIALIZED);
    
    nsresult rv;
    PRBool exists;
    nsCOMPtr<nsILocalFile> localDir;

    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    OSErr   err;
 
#if defined(XP_MACOSX)
    FSRef fsRef;
    err = ::FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &fsRef);
    if (err) return NS_ERROR_FAILURE;
    NS_NewLocalFile(EmptyString(), PR_TRUE, getter_AddRefs(localDir));
    if (!localDir) return NS_ERROR_FAILURE;
    nsCOMPtr<nsILocalFileMac> localDirMac(do_QueryInterface(localDir));
    rv = localDirMac->InitWithFSRef(&fsRef);
    if (NS_FAILED(rv)) return rv;
#else   
    long    response;
    err = ::Gestalt(gestaltSystemVersion, &response);
    const char *prop = (!err && response >= 0x00001000) ? NS_MAC_USER_LIB_DIR : NS_MAC_DOCUMENTS_DIR;
    rv = directoryService->Get(prop, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;   
#endif

    rv = localDir->Append(mAppDataDirName);
    if (NS_FAILED(rv))
        return rv;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv))
        return rv;

    *aLocalFile = localDir;
    NS_ADDREF(*aLocalFile);

    return rv; 
}
