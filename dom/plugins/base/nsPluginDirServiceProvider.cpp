/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginDirServiceProvider.h"

#include "nsCRT.h"
#include "nsIFile.h"

#include <windows.h>
#include "nsIWindowsRegKey.h"

using namespace mozilla;

/* static */ nsresult GetPLIDDirectories(nsTArray<nsCOMPtr<nsIFile>>& aDirs) {
  GetPLIDDirectoriesWithRootKey(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, aDirs);
  GetPLIDDirectoriesWithRootKey(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                                aDirs);
  return NS_OK;
}

/* static */ nsresult GetPLIDDirectoriesWithRootKey(
    uint32_t aKey, nsTArray<nsCOMPtr<nsIFile>>& aDirs) {
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_ENSURE_TRUE(regKey, NS_ERROR_FAILURE);

  nsresult rv = regKey->Open(aKey, u"Software\\MozillaPlugins"_ns,
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
        rv = childKey->ReadStringValue(u"Path"_ns, path);
        if (NS_SUCCEEDED(rv)) {
          // We deliberately do not do any further checks here on whether
          // these are actually directories, whether they even exist, or
          // whether they are duplicates. The pluginhost code will do them.
          // This allows the whole process to avoid mainthread IO.
          nsCOMPtr<nsIFile> localFile;
          rv = NS_NewLocalFile(path, true, getter_AddRefs(localFile));
          if (NS_SUCCEEDED(rv) && localFile) {
            aDirs.AppendElement(localFile);
          }
        }
      }
    }
  }
  return NS_OK;
}
