/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDataManager.h"
#include "FileSystemDataManagerVersion001.h"
#include "FileSystemFileManager.h"
#include "FileSystemHashSource.h"
#include "ResultStatement.h"

#include "fs/FileSystemConstants.h"

#include "SchemaVersion001.h"
#include "mozilla/dom/BackgroundFileSystemParent.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/Result.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "nsString.h"

namespace mozilla::dom {

using FileSystemEntries = nsTArray<fs::FileSystemEntryMetadata>;

namespace fs::data {

namespace {

Result<ResultConnection, QMResult> GetStorageConnection(const Origin& aOrigin) {
  bool exists = false;
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> databaseFile,
                GetDatabasePath(aOrigin, exists));

  QM_TRY_INSPECT(
      const auto& storageService,
      QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_GET_TYPED(
          nsCOMPtr<mozIStorageService>, MOZ_SELECT_OVERLOAD(do_GetService),
          MOZ_STORAGE_SERVICE_CONTRACTID)));

  const auto flags = mozIStorageService::CONNECTION_DEFAULT;
  ResultConnection connection;
  QM_TRY(QM_TO_RESULT(storageService->OpenDatabase(
      databaseFile, flags, getter_AddRefs(connection))));

  return connection;
}

}  // namespace

Result<FileSystemDataManager*, QMResult>
FileSystemDataManager::CreateFileSystemDataManager(const Origin& aOrigin) {
  QM_TRY_UNWRAP(auto connection, fs::data::GetStorageConnection(aOrigin));
  QM_TRY_UNWRAP(DatabaseVersion version,
                SchemaVersion001::InitializeConnection(connection, aOrigin));

  if (1 == version) {
    QM_TRY_UNWRAP(FileSystemFileManager fmRes,
                  FileSystemFileManager::CreateFileSystemFileManager(aOrigin));

    return new FileSystemDataManagerVersion001(
        std::move(connection),
        MakeUnique<FileSystemFileManager>(std::move(fmRes)));
  }

  return Err(QMResult(NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR));
}

Result<EntryId, QMResult> GetRootHandle(const Origin& origin) {
  MOZ_ASSERT(!origin.IsEmpty());

  return FileSystemHashSource::GenerateHash(origin, kRootName);
}

Result<EntryId, QMResult> GetEntryHandle(
    const FileSystemChildMetadata& aHandle) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  return FileSystemHashSource::GenerateHash(aHandle.parentId(),
                                            aHandle.childName());
}

}  // namespace fs::data

}  // namespace mozilla::dom
