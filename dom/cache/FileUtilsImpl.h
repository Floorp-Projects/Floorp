/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_FileUtilsImpl_h
#define mozilla_dom_cache_FileUtilsImpl_h

#include "mozilla/dom/cache/FileUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

template <typename Func>
nsresult BodyTraverseFiles(const QuotaInfo& aQuotaInfo, nsIFile* aBodyDir,
                           const Func& aHandleFileFunc,
                           const bool aCanRemoveFiles, const bool aTrackQuota) {
  MOZ_DIAGNOSTIC_ASSERT(aBodyDir);

  nsresult rv;
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  nsCOMPtr<nsIFile> parentFile;
  rv = aBodyDir->GetParent(getter_AddRefs(parentFile));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  MOZ_DIAGNOSTIC_ASSERT(parentFile);

  nsAutoCString nativeLeafName;
  rv = parentFile->GetNativeLeafName(nativeLeafName);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

  MOZ_DIAGNOSTIC_ASSERT(
      StringEndsWith(nativeLeafName, NS_LITERAL_CSTRING("morgue")));
#endif

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = aBodyDir->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isEmpty = true;
  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(rv = entries->GetNextFile(getter_AddRefs(file))) &&
         file) {
    bool isDir = false;
    rv = file->IsDirectory(&isDir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // If it's a directory somehow, try to remove it and move on
    if (NS_WARN_IF(isDir)) {
      DebugOnly<nsresult> result =
          RemoveNsIFileRecursively(aQuotaInfo, file, /* aTrackQuota */ false);
      MOZ_ASSERT(NS_SUCCEEDED(result));
      continue;
    }

    nsAutoCString leafName;
    rv = file->GetNativeLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Delete all tmp files regardless of known bodies. These are all
    // considered orphans.
    if (StringEndsWith(leafName, NS_LITERAL_CSTRING(".tmp"))) {
      if (aCanRemoveFiles) {
        DebugOnly<nsresult> result =
            RemoveNsIFile(aQuotaInfo, file, aTrackQuota);
        MOZ_ASSERT(NS_SUCCEEDED(result));
        continue;
      }
    } else if (NS_WARN_IF(
                   !StringEndsWith(leafName, NS_LITERAL_CSTRING(".final")))) {
      // Otherwise, it must be a .final file.  If its not, then try to remove it
      // and move on
      DebugOnly<nsresult> result =
          RemoveNsIFile(aQuotaInfo, file, /* aTrackQuota */ false);
      MOZ_ASSERT(NS_SUCCEEDED(result));
      continue;
    }

    bool fileDeleted;
    rv = aHandleFileFunc(file, leafName, fileDeleted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (fileDeleted) {
      continue;
    }

    isEmpty = false;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isEmpty && aCanRemoveFiles) {
    DebugOnly<nsresult> result =
        RemoveNsIFileRecursively(aQuotaInfo, aBodyDir, /* aTrackQuota */ false);
    MOZ_ASSERT(NS_SUCCEEDED(result));
  }

  return rv;
}

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_FileUtilsImpl_h
