/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001, 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *  Hakan Waara <hwaara@gmail.com>
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

#include "AppDirServiceProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFileMac.h"

#include <Carbon/Carbon.h>

// Defines
#define APP_REGISTRY_NAME        NS_LITERAL_CSTRING("Application.regs")
#define COMPONENT_REGISTRY_NAME  NS_LITERAL_CSTRING("compreg.dat")
#define XPTI_REGISTRY_NAME       NS_LITERAL_CSTRING("xpti.dat")


AppDirServiceProvider::AppDirServiceProvider(const char *inName, PRBool isCustomProfile)
{
  mIsCustomProfile = isCustomProfile;
  mName.Assign(inName);
}

AppDirServiceProvider::~AppDirServiceProvider()
{
}

NS_IMPL_ISUPPORTS1(AppDirServiceProvider, nsIDirectoryServiceProvider)


// nsIDirectoryServiceProvider implementation 

NS_IMETHODIMP
AppDirServiceProvider::GetFile(const char *prop, PRBool *persistent, nsIFile **_retval)
{    
  nsCOMPtr<nsILocalFile>  localFile;
  nsresult rv = NS_ERROR_FAILURE;

  *_retval = nsnull;
  *persistent = PR_TRUE;
  
  if (strcmp(prop, NS_APP_APPLICATION_REGISTRY_DIR) == 0 ||
      strcmp(prop, NS_APP_USER_PROFILES_ROOT_DIR)   == 0)
  {
    rv = GetProfileDirectory(getter_AddRefs(localFile));
  }
  else if (strcmp(prop, NS_APP_APPLICATION_REGISTRY_FILE) == 0)
  {
    rv = GetProfileDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(APP_REGISTRY_NAME);
  }
  else if (strcmp(prop, NS_XPCOM_COMPONENT_REGISTRY_FILE) == 0)
  {
    rv = GetProfileDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(COMPONENT_REGISTRY_NAME);
  }
  else if (strcmp(prop, NS_XPCOM_XPTI_REGISTRY_FILE) == 0)
  {
    rv = GetProfileDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(XPTI_REGISTRY_NAME);
  }
  else if (strcmp(prop, NS_APP_CACHE_PARENT_DIR) == 0)
  {
    rv = GetParentCacheDirectory(getter_AddRefs(localFile));
  }

  if (localFile && NS_SUCCEEDED(rv))
    return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
    
  return rv;
}

// Protected methods

nsresult
AppDirServiceProvider::GetProfileDirectory(nsILocalFile** outFolder)
{
  NS_ENSURE_ARG_POINTER(outFolder);
  *outFolder = nsnull;
  nsresult rv = NS_OK;
  
  // Init and cache the profile directory; we'll get queried for it a lot of times.
  if (!mProfileDir)
  {
    if (mIsCustomProfile)
    {
      rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(mName), PR_FALSE, getter_AddRefs(mProfileDir));
      
      if (NS_FAILED(rv))
      {
        NS_WARNING ("Couldn't use the specified custom path!");
        return rv;
      }
    }
    else
    {
      // if this is not a custom profile path, we have a product name, and we'll use
      // Application Support/<mName> as our profile dir.
      rv = GetSystemDirectory(kApplicationSupportFolderType, getter_AddRefs(mProfileDir));
      NS_ENSURE_SUCCESS(rv, rv);
      
      // if it's not a custom profile, mName is our product name.
      mProfileDir->AppendNative(mName);
    }
    
    rv = EnsureExists(mProfileDir);
    NS_ENSURE_SUCCESS(rv, rv);
  } // end lazy init
  
  nsCOMPtr<nsIFile> profileDir;
	rv = mProfileDir->Clone(getter_AddRefs(profileDir));
  NS_ENSURE_SUCCESS(rv, rv);
  
  return CallQueryInterface(profileDir, outFolder);
}

nsresult
AppDirServiceProvider::GetSystemDirectory(OSType inFolderType, nsILocalFile** outFolder)
{
  FSRef foundRef;
  *outFolder = nsnull;
  
  OSErr err = ::FSFindFolder(kUserDomain, inFolderType, kCreateFolder, &foundRef);
  if (err != noErr)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsILocalFile> localDir;
  NS_NewLocalFile(EmptyString(), PR_TRUE, getter_AddRefs(localDir));
  nsCOMPtr<nsILocalFileMac> localDirMac(do_QueryInterface(localDir));
  NS_ENSURE_STATE(localDirMac);
  
  nsresult rv = localDirMac->InitWithFSRef(&foundRef);
  NS_ENSURE_SUCCESS(rv, rv);
  
  NS_ADDREF(*outFolder = localDirMac);
  return NS_OK;
}

// Gets the parent directory to the Cache folder.
nsresult
AppDirServiceProvider::GetParentCacheDirectory(nsILocalFile** outFolder)
{
  *outFolder = nsnull;
  
  if (mIsCustomProfile)
    return GetProfileDirectory(outFolder);
  
  // we don't have a custom profile path, so use Caches/<product name>
  nsresult rv = GetSystemDirectory(kCachedDataFolderType, outFolder);
  if (NS_FAILED(rv) || !(*outFolder))
    return NS_ERROR_FAILURE;
  
  (*outFolder)->AppendNative(mName);
  rv = EnsureExists(*outFolder);

  return rv;
}

/* static */ 
nsresult
AppDirServiceProvider::EnsureExists(nsILocalFile* inFolder)
{
  PRBool exists;
  nsresult rv = inFolder->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists)
    rv = inFolder->Create(nsIFile::DIRECTORY_TYPE, 0775);
  return rv;
}

