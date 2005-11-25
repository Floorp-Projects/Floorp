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
#include "nsILocalFileMac.h"

#include <Carbon/Carbon.h>

// Defines

#define APP_REGISTRY_NAME   NS_LITERAL_CSTRING("Application.regs")

//*****************************************************************************
// AppDirServiceProvider::Constructor/Destructor
//*****************************************************************************   

AppDirServiceProvider::AppDirServiceProvider(const nsACString& productDirName)
{
  mProductDirName.Assign(productDirName);
}

AppDirServiceProvider::~AppDirServiceProvider()
{
}

//*****************************************************************************
// AppDirServiceProvider::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(AppDirServiceProvider, nsIDirectoryServiceProvider)

//*****************************************************************************
// AppDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************   

NS_IMETHODIMP
AppDirServiceProvider::GetFile(const char *prop, PRBool *persistent, nsIFile **_retval)
{    
  nsCOMPtr<nsILocalFile>  localFile;
  nsresult rv = NS_ERROR_FAILURE;
  nsCAutoString strBuf;

  *_retval = nsnull;
  *persistent = PR_TRUE;
  
  if (strcmp(prop, NS_APP_APPLICATION_REGISTRY_DIR) == 0)
  {
    rv = GetProductDirectory(getter_AddRefs(localFile));
  }
  else if (strcmp(prop, NS_APP_APPLICATION_REGISTRY_FILE) == 0)
  {
    rv = GetProductDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(APP_REGISTRY_NAME);
  }
  else if (strcmp(prop, NS_APP_USER_PROFILES_ROOT_DIR) == 0)
  {
    rv = GetProductDirectory(getter_AddRefs(localFile));
  }
  else if (strcmp(prop, NS_APP_CACHE_PARENT_DIR) == 0)
  {
    rv = GetCacheDirectory(getter_AddRefs(localFile));
  }
    
  if (localFile && NS_SUCCEEDED(rv))
    return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
    
  return rv;
}

//*****************************************************************************
// AppDirServiceProvider::AppDirServiceProvider
//*****************************************************************************   

NS_METHOD
AppDirServiceProvider::GetProductDirectory(nsILocalFile **outLocalFile)
{
  return EnsureFolder(kApplicationSupportFolderType, outLocalFile);
}

nsresult
AppDirServiceProvider::GetCacheDirectory(nsILocalFile** outCacheFolder)
{
  return EnsureFolder(kCachedDataFolderType, outCacheFolder);
}

nsresult
AppDirServiceProvider::EnsureFolder(OSType inFolderType, nsILocalFile** outFolder)
{
  NS_ENSURE_ARG_POINTER(outFolder);
  *outFolder = nsnull;
  
  nsresult rv;
  FSRef   foundRef;
  
  OSErr err = ::FSFindFolder(kUserDomain, inFolderType, kCreateFolder, &foundRef);
  if (err != noErr)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsILocalFileMac> localDir(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  if (!localDir)
    return NS_ERROR_FAILURE;
  rv = localDir->InitWithFSRef(&foundRef);
  if (NS_FAILED(rv))
    return rv;
  rv = localDir->AppendNative(mProductDirName);
  if (NS_FAILED(rv))
    return rv;
  
  PRBool exists;
  rv = localDir->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists)
    rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
  if (NS_FAILED(rv))
    return rv;
  
  *outFolder = localDir;
  NS_ADDREF(*outFolder);
  
  return rv; 
}