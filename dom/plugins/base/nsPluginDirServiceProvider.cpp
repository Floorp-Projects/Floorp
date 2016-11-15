/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginDirServiceProvider.h"

#include "nsCRT.h"
#include "nsIFile.h"
#include "nsDependentString.h"
#include "nsArrayEnumerator.h"
#include "mozilla/Preferences.h"

#include <windows.h>
#include "nsIWindowsRegKey.h"

using namespace mozilla;

//*****************************************************************************
// nsPluginDirServiceProvider::Constructor/Destructor
//*****************************************************************************

nsPluginDirServiceProvider::nsPluginDirServiceProvider()
{
}

nsPluginDirServiceProvider::~nsPluginDirServiceProvider()
{
}

//*****************************************************************************
// nsPluginDirServiceProvider::nsISupports
//*****************************************************************************

NS_IMPL_ISUPPORTS(nsPluginDirServiceProvider,
                  nsIDirectoryServiceProvider)

//*****************************************************************************
// nsPluginDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsPluginDirServiceProvider::GetFile(const char *charProp, bool *persistant,
                                    nsIFile **_retval)
{
  NS_ENSURE_ARG(charProp);

  *_retval = nullptr;
  *persistant = false;

  return NS_ERROR_FAILURE;
}

nsresult
nsPluginDirServiceProvider::GetPLIDDirectories(nsISimpleEnumerator **aEnumerator)
{
  NS_ENSURE_ARG_POINTER(aEnumerator);
  *aEnumerator = nullptr;

  nsCOMArray<nsIFile> dirs;

  GetPLIDDirectoriesWithRootKey(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, dirs);
  GetPLIDDirectoriesWithRootKey(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, dirs);

  return NS_NewArrayEnumerator(aEnumerator, dirs);
}

nsresult
nsPluginDirServiceProvider::GetPLIDDirectoriesWithRootKey(uint32_t aKey, nsCOMArray<nsIFile> &aDirs)
{
  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_ENSURE_TRUE(regKey, NS_ERROR_FAILURE);

  nsresult rv = regKey->Open(aKey,
                             NS_LITERAL_STRING("Software\\MozillaPlugins"),
                             nsIWindowsRegKey::ACCESS_READ);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t childCount = 0;
  regKey->GetChildCount(&childCount);

  for (uint32_t index = 0; index < childCount; ++index) {
    nsAutoString childName;
    rv = regKey->GetChildName(index, childName);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIWindowsRegKey> childKey;
      rv = regKey->OpenChild(childName, nsIWindowsRegKey::ACCESS_QUERY_VALUE,
                             getter_AddRefs(childKey));
      if (NS_SUCCEEDED(rv) && childKey) {
        nsAutoString path;
        rv = childKey->ReadStringValue(NS_LITERAL_STRING("Path"), path);
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIFile> localFile;
          if (NS_SUCCEEDED(NS_NewLocalFile(path, true,
                                           getter_AddRefs(localFile))) &&
              localFile) {
            // Some vendors use a path directly to the DLL so chop off
            // the filename
            bool isDir = false;
            if (NS_SUCCEEDED(localFile->IsDirectory(&isDir)) && !isDir) {
              nsCOMPtr<nsIFile> temp;
              localFile->GetParent(getter_AddRefs(temp));
              if (temp)
                localFile = temp;
            }

            // Now we check to make sure it's actually on disk and
            // To see if we already have this directory in the array
            bool isFileThere = false;
            bool isDupEntry = false;
            if (NS_SUCCEEDED(localFile->Exists(&isFileThere)) && isFileThere) {
              int32_t c = aDirs.Count();
              for (int32_t i = 0; i < c; i++) {
                nsIFile *dup = static_cast<nsIFile*>(aDirs[i]);
                if (dup &&
                    NS_SUCCEEDED(dup->Equals(localFile, &isDupEntry)) &&
                    isDupEntry) {
                  break;
                }
              }

              if (!isDupEntry) {
                aDirs.AppendObject(localFile);
              }
            }
          }
        }
      }
    }
  }
  return NS_OK;
}
