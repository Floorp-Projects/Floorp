/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemFileManager.h"

#include "FileSystemDataManager.h"
#include "FileSystemHashSource.h"
#include "GetDirectoryForOrigin.h"
#include "mozilla/Assertions.h"
#include "mozilla/NotNull.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIFileProtocolHandler.h"
#include "nsIFileURL.h"
#include "nsIURIMutator.h"
#include "nsXPCOM.h"

namespace mozilla::dom::fs::data {

namespace {

constexpr nsLiteralString kDatabaseFileName = u"metadata.sqlite"_ns;

Result<nsCOMPtr<nsIFile>, QMResult> GetFileDestination(
    const nsCOMPtr<nsIFile>& aTopDirectory, const EntryId& aEntryId) {
  MOZ_ASSERT(32u == aEntryId.Length());

  nsCOMPtr<nsIFile> destination;

  // nsIFile Clone is not a constant method
  QM_TRY(QM_TO_RESULT(aTopDirectory->Clone(getter_AddRefs(destination))));

  QM_TRY_UNWRAP(Name encoded, FileSystemHashSource::EncodeHash(aEntryId));

  MOZ_ALWAYS_TRUE(IsAscii(encoded));

  nsString relativePath;
  relativePath.Append(Substring(encoded, 0, 2));

  QM_TRY(QM_TO_RESULT(destination->AppendRelativePath(relativePath)));

  QM_TRY(QM_TO_RESULT(destination->AppendRelativePath(encoded)));

  return destination;
}

Result<nsCOMPtr<nsIFile>, QMResult> GetOrCreateFileImpl(
    const nsAString& aFilePath) {
  MOZ_ASSERT(!aFilePath.IsEmpty());

  nsCOMPtr<nsIFile> result;
  QM_TRY(QM_TO_RESULT(NS_NewLocalFile(aFilePath,
                                      /* aFollowLinks */ false,
                                      getter_AddRefs(result))));

  bool exists = true;
  QM_TRY(QM_TO_RESULT(result->Exists(&exists)));

  if (!exists) {
    QM_TRY(QM_TO_RESULT(result->Create(nsIFile::NORMAL_FILE_TYPE, 0644)));

    return result;
  }

  bool isDirectory = true;
  QM_TRY(QM_TO_RESULT(result->IsDirectory(&isDirectory)));
  QM_TRY(OkIf(!isDirectory), Err(QMResult(NS_ERROR_FILE_IS_DIRECTORY)));

  return result;
}

Result<nsCOMPtr<nsIFile>, QMResult> GetFile(
    const nsCOMPtr<nsIFile>& aTopDirectory, const EntryId& aEntryId) {
  MOZ_ASSERT(!aEntryId.IsEmpty());

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> pathObject,
                GetFileDestination(aTopDirectory, aEntryId));

  nsString desiredPath;
  QM_TRY(QM_TO_RESULT(pathObject->GetPath(desiredPath)));

  nsCOMPtr<nsIFile> result;
  QM_TRY(QM_TO_RESULT(NS_NewLocalFile(desiredPath,
                                      /* aFollowLinks */ false,
                                      getter_AddRefs(result))));

  return result;
}

Result<nsCOMPtr<nsIFile>, QMResult> GetOrCreateFile(
    const nsCOMPtr<nsIFile>& aTopDirectory, const EntryId& aEntryId) {
  MOZ_ASSERT(!aEntryId.IsEmpty());

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> pathObject,
                GetFileDestination(aTopDirectory, aEntryId));

  nsString desiredPath;
  QM_TRY(QM_TO_RESULT(pathObject->GetPath(desiredPath)));

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> result, GetOrCreateFileImpl(desiredPath));

  return result;
}

}  // namespace

Result<nsCOMPtr<nsIFile>, QMResult> GetFileSystemDirectory(
    const Origin& aOrigin) {
  quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

#if FS_QUOTA_MANAGEMENT_ENABLED
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> fileSystemDirectory,
                QM_TO_RESULT_TRANSFORM(quotaManager->GetDirectoryForOrigin(
                    quota::PERSISTENCE_TYPE_DEFAULT, aOrigin)));
#else
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> fileSystemDirectory,
                GetDirectoryForOrigin(*quotaManager, aOrigin));
#endif

  QM_TRY(QM_TO_RESULT(fileSystemDirectory->AppendRelativePath(
      NS_LITERAL_STRING_FROM_CSTRING(FILESYSTEM_DIRECTORY_NAME))));

  return fileSystemDirectory;
}

nsresult EnsureFileSystemDirectory(
    const quota::OriginMetadata& aOriginMetadata) {
  quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  QM_TRY(MOZ_TO_RESULT(quotaManager->EnsureStorageIsInitialized()));

  QM_TRY(MOZ_TO_RESULT(quotaManager->EnsureTemporaryStorageIsInitialized()));

#if FS_QUOTA_MANAGEMENT_ENABLED
  QM_TRY_INSPECT(const auto& fileSystemDirectory,
                 quotaManager
                     ->EnsureTemporaryOriginIsInitialized(
                         quota::PERSISTENCE_TYPE_DEFAULT, aOriginMetadata)
                     .map([](const auto& aPair) { return aPair.first; }));
#else
  QM_TRY_INSPECT(const auto& fileSystemDirectory,
                 GetDirectoryForOrigin(*quotaManager, aOriginMetadata.mOrigin));
#endif

  QM_TRY(QM_TO_RESULT(fileSystemDirectory->AppendRelativePath(
      NS_LITERAL_STRING_FROM_CSTRING(FILESYSTEM_DIRECTORY_NAME))));

  bool exists = true;
  QM_TRY(QM_TO_RESULT(fileSystemDirectory->Exists(&exists)));

  if (!exists) {
    QM_TRY(QM_TO_RESULT(
        fileSystemDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755)));

    return NS_OK;
  }

  bool isDirectory = true;
  QM_TRY(QM_TO_RESULT(fileSystemDirectory->IsDirectory(&isDirectory)));
  QM_TRY(OkIf(isDirectory), NS_ERROR_FILE_NOT_DIRECTORY);

  return NS_OK;
}

Result<nsCOMPtr<nsIFile>, QMResult> GetDatabaseFile(const Origin& aOrigin) {
  MOZ_ASSERT(!aOrigin.IsEmpty());

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> databaseFile,
                GetFileSystemDirectory(aOrigin));

  QM_TRY(QM_TO_RESULT(databaseFile->AppendRelativePath(kDatabaseFileName)));

  return databaseFile;
}

/**
 * TODO: This is almost identical to the corresponding function of IndexedDB
 */
Result<nsCOMPtr<nsIFileURL>, QMResult> GetDatabaseFileURL(
    const Origin& aOrigin, const int64_t aDirectoryLockId) {
  MOZ_ASSERT(aDirectoryLockId >= 0);

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> databaseFile, GetDatabaseFile(aOrigin));

  QM_TRY_INSPECT(
      const auto& protocolHandler,
      QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_GET_TYPED(
          nsCOMPtr<nsIProtocolHandler>, MOZ_SELECT_OVERLOAD(do_GetService),
          NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "file")));

  QM_TRY_INSPECT(const auto& fileHandler,
                 QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_GET_TYPED(
                     nsCOMPtr<nsIFileProtocolHandler>,
                     MOZ_SELECT_OVERLOAD(do_QueryInterface), protocolHandler)));

  QM_TRY_INSPECT(const auto& mutator,
                 QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                     nsCOMPtr<nsIURIMutator>, fileHandler, NewFileURIMutator,
                     databaseFile)));

#if FS_QUOTA_MANAGEMENT_ENABLED
  nsCString directoryLockIdClause = "&directoryLockId="_ns;
  directoryLockIdClause.AppendInt(aDirectoryLockId);
#else
  nsCString directoryLockIdClause;
#endif

  nsCOMPtr<nsIFileURL> result;
  QM_TRY(QM_TO_RESULT(
      NS_MutateURI(mutator).SetQuery(directoryLockIdClause).Finalize(result)));

  return result;
}

/* static */
Result<FileSystemFileManager, QMResult>
FileSystemFileManager::CreateFileSystemFileManager(
    nsCOMPtr<nsIFile>&& topDirectory) {
  return FileSystemFileManager(std::move(topDirectory));
}

/* static */
Result<FileSystemFileManager, QMResult>
FileSystemFileManager::CreateFileSystemFileManager(const Origin& aOrigin) {
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> topDirectory,
                GetFileSystemDirectory(aOrigin));

  return FileSystemFileManager(std::move(topDirectory));
}

FileSystemFileManager::FileSystemFileManager(nsCOMPtr<nsIFile>&& aTopDirectory)
    : mTopDirectory(std::move(aTopDirectory)) {}

Result<nsCOMPtr<nsIFile>, QMResult> FileSystemFileManager::GetFile(
    const EntryId& aEntryId) const {
  return data::GetFile(mTopDirectory, aEntryId);
}

Result<nsCOMPtr<nsIFile>, QMResult> FileSystemFileManager::GetOrCreateFile(
    const EntryId& aEntryId) {
  return data::GetOrCreateFile(mTopDirectory, aEntryId);
}

nsresult FileSystemFileManager::RemoveFile(const EntryId& aEntryId) {
  MOZ_ASSERT(!aEntryId.IsEmpty());
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> pathObject,
                GetFileDestination(mTopDirectory, aEntryId));

  bool exists = false;
  QM_TRY(MOZ_TO_RESULT(pathObject->Exists(&exists)));

  if (!exists) {
    return NS_OK;
  }

  bool isFile = false;
  QM_TRY(MOZ_TO_RESULT(pathObject->IsFile(&isFile)));

  if (!isFile) {
    return NS_ERROR_FILE_IS_DIRECTORY;
  }

  QM_TRY(MOZ_TO_RESULT(pathObject->Remove(/* recursive */ false)));

  return NS_OK;
}

}  // namespace mozilla::dom::fs::data
