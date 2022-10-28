/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INDEXEDDB_DATABASEFILEMANAGERIMPL_H_
#define DOM_INDEXEDDB_DATABASEFILEMANAGERIMPL_H_

#include "DatabaseFileManager.h"

// local includes
#include "ActorsParentCommon.h"

// global includes
#include "mozilla/dom/quota/Assertions.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "nsIFile.h"
#include "nsString.h"

namespace mozilla::dom::indexedDB {

template <typename KnownDirEntryOp, typename UnknownDirEntryOp>
Result<Ok, nsresult> DatabaseFileManager::TraverseFiles(
    nsIFile& aDirectory, KnownDirEntryOp&& aKnownDirEntryOp,
    UnknownDirEntryOp&& aUnknownDirEntryOp) {
  quota::AssertIsOnIOThread();

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(aDirectory, Exists));

  if (!exists) {
    return Ok{};
  }

  QM_TRY(quota::CollectEachFile(
      aDirectory,
      [&aKnownDirEntryOp, &aUnknownDirEntryOp](
          const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
        QM_TRY_INSPECT(const auto& dirEntryKind, quota::GetDirEntryKind(*file));

        switch (dirEntryKind) {
          case quota::nsIFileKind::ExistsAsDirectory: {
            QM_TRY_INSPECT(
                const auto& leafName,
                MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsString, file, GetLeafName));

            if (leafName.Equals(kJournalDirectoryName)) {
              QM_TRY(std::forward<KnownDirEntryOp>(aKnownDirEntryOp)(
                  *file, /* isDirectory */ true));

              break;
            }

            Unused << WARN_IF_FILE_IS_UNKNOWN(*file);

            QM_TRY(std::forward<UnknownDirEntryOp>(aUnknownDirEntryOp)(
                *file, /* isDirectory */ true));

            break;
          }

          case quota::nsIFileKind::ExistsAsFile: {
            QM_TRY_INSPECT(
                const auto& leafName,
                MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsString, file, GetLeafName));

            nsresult rv;
            leafName.ToInteger64(&rv);
            if (NS_SUCCEEDED(rv)) {
              QM_TRY(std::forward<KnownDirEntryOp>(aKnownDirEntryOp)(
                  *file, /* isDirectory */ false));

              break;
            }

            Unused << WARN_IF_FILE_IS_UNKNOWN(*file);

            QM_TRY(std::forward<UnknownDirEntryOp>(aUnknownDirEntryOp)(
                *file, /* isDirectory */ false));

            break;
          }

          case quota::nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return Ok{};
      }));

  return Ok{};
}

}  // namespace mozilla::dom::indexedDB

#endif  // DOM_INDEXEDDB_DATABASEFILEMANAGERIMPL_H_
