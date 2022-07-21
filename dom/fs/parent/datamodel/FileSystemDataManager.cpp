/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDataManager.h"
#include "FileSystemDataManagerVersion001.h"
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

#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOM.h"

namespace mozilla::dom {

using FileSystemEntries = nsTArray<fs::FileSystemEntryMetadata>;

namespace fs::data {

namespace {

nsresult getOrCreateEntry(const nsAString& aDesiredFilePath, bool& aExists,
                          nsCOMPtr<nsIFile>& aFile,
                          decltype(nsIFile::NORMAL_FILE_TYPE) aKind,
                          uint32_t aPermissions) {
  MOZ_ASSERT(!aDesiredFilePath.IsEmpty());

  QM_TRY(MOZ_TO_RESULT(NS_NewLocalFile(aDesiredFilePath,
                                       /* aFollowLinks */ false,
                                       getter_AddRefs(aFile))));

  QM_TRY(MOZ_TO_RESULT(aFile->Exists(&aExists)));
  if (aExists) {
    return NS_OK;
  }

  QM_TRY(MOZ_TO_RESULT(aFile->Create(aKind, aPermissions)));

  return NS_OK;
}

nsresult getFileSystemDirectory(const Origin& aOrigin,
                                nsCOMPtr<nsIFile>& aDirectory) {
  QM_TRY_UNWRAP(RefPtr<quota::QuotaManager> quotaManager,
                quota::QuotaManager::GetOrCreate());

  QM_TRY_UNWRAP(aDirectory, quotaManager->GetDirectoryForOrigin(
                                quota::PERSISTENCE_TYPE_DEFAULT, aOrigin));

  QM_TRY(MOZ_TO_RESULT(aDirectory->AppendRelativePath(u"filesystem"_ns)));

  return NS_OK;
}

nsresult getFileSystemDatabaseFile(const Origin& aOrigin,
                                   nsString& aDatabaseFile) {
  nsCOMPtr<nsIFile> directoryPath;
  QM_TRY(MOZ_TO_RESULT(getFileSystemDirectory(aOrigin, directoryPath)));

  QM_TRY(
      MOZ_TO_RESULT(directoryPath->AppendRelativePath(u"metadata.sqlite"_ns)));

  QM_TRY(MOZ_TO_RESULT(directoryPath->GetPath(aDatabaseFile)));

  return NS_OK;
}

nsresult getDatabasePath(const Origin& aOrigin, bool& aExists,
                         nsCOMPtr<nsIFile>& aFile) {
  nsString databaseFilePath;
  QM_TRY(MOZ_TO_RESULT(getFileSystemDatabaseFile(aOrigin, databaseFilePath)));

  MOZ_ASSERT(!databaseFilePath.IsEmpty());

  return getOrCreateEntry(databaseFilePath, aExists, aFile,
                          nsIFile::NORMAL_FILE_TYPE, 0644);
}

Result<ResultConnection, QMResult> GetStorageConnection(const Origin& aOrigin) {
  bool exists = false;
  nsCOMPtr<nsIFile> databaseFile;
  QM_TRY(QM_TO_RESULT(getDatabasePath(aOrigin, exists, databaseFile)));

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
    return new FileSystemDataManagerVersion001(std::move(connection));
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
