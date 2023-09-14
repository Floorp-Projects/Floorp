/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemFileManager.h"

#include "FileSystemDataManager.h"
#include "FileSystemHashSource.h"
#include "FileSystemParentTypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/NotNull.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIFile.h"
#include "nsIFileProtocolHandler.h"
#include "nsIFileURL.h"
#include "nsIURIMutator.h"
#include "nsTHashMap.h"
#include "nsXPCOM.h"

namespace mozilla::dom::fs::data {

namespace {

constexpr nsLiteralString kDatabaseFileName = u"metadata.sqlite"_ns;

Result<nsCOMPtr<nsIFile>, QMResult> GetFileDestination(
    const nsCOMPtr<nsIFile>& aTopDirectory, const FileId& aFileId) {
  MOZ_ASSERT(32u == aFileId.Value().Length());

  nsCOMPtr<nsIFile> destination;

  // nsIFile Clone is not a constant method
  QM_TRY(QM_TO_RESULT(aTopDirectory->Clone(getter_AddRefs(destination))));

  QM_TRY_UNWRAP(Name encoded, FileSystemHashSource::EncodeHash(aFileId));

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

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> result,
                QM_TO_RESULT_TRANSFORM(quota::QM_NewLocalFile(aFilePath)));

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
    const nsCOMPtr<nsIFile>& aTopDirectory, const FileId& aFileId) {
  MOZ_ASSERT(!aFileId.IsEmpty());

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> pathObject,
                GetFileDestination(aTopDirectory, aFileId));

  nsString desiredPath;
  QM_TRY(QM_TO_RESULT(pathObject->GetPath(desiredPath)));

  QM_TRY_RETURN(QM_TO_RESULT_TRANSFORM(quota::QM_NewLocalFile(desiredPath)));
}

Result<nsCOMPtr<nsIFile>, QMResult> GetOrCreateFile(
    const nsCOMPtr<nsIFile>& aTopDirectory, const FileId& aFileId) {
  MOZ_ASSERT(!aFileId.IsEmpty());

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> pathObject,
                GetFileDestination(aTopDirectory, aFileId));

  nsString desiredPath;
  QM_TRY(QM_TO_RESULT(pathObject->GetPath(desiredPath)));

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> result, GetOrCreateFileImpl(desiredPath));

  return result;
}

nsresult RemoveFileObject(const nsCOMPtr<nsIFile>& aFilePtr) {
  // If we cannot tell whether the object is file or directory, or it is a
  // directory, it is abandoned as an unknown object. If an attempt is made to
  // create a new object with the same path on disk, we regenerate the FileId
  // until the collision is resolved.

  bool isFile = false;
  QM_TRY(MOZ_TO_RESULT(aFilePtr->IsFile(&isFile)));

  QM_TRY(OkIf(isFile), NS_ERROR_FILE_IS_DIRECTORY);

  QM_TRY(QM_TO_RESULT(aFilePtr->Remove(/* recursive */ false)));

  return NS_OK;
}

#ifdef DEBUG
// Unused in release builds
Result<Usage, QMResult> GetFileSize(const nsCOMPtr<nsIFile>& aFileObject) {
  bool exists = false;
  QM_TRY(QM_TO_RESULT(aFileObject->Exists(&exists)));

  if (!exists) {
    return 0;
  }

  bool isFile = false;
  QM_TRY(QM_TO_RESULT(aFileObject->IsFile(&isFile)));

  // We never create directories with this path: this is an unknown object
  // and the file does not exist
  QM_TRY(OkIf(isFile), 0);

  QM_TRY_UNWRAP(Usage fileSize,
                QM_TO_RESULT_INVOKE_MEMBER(aFileObject, GetFileSize));

  return fileSize;
}
#endif

}  // namespace

Result<nsCOMPtr<nsIFile>, QMResult> GetFileSystemDirectory(
    const quota::OriginMetadata& aOriginMetadata) {
  MOZ_ASSERT(aOriginMetadata.mPersistenceType ==
             quota::PERSISTENCE_TYPE_DEFAULT);

  quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> fileSystemDirectory,
                QM_TO_RESULT_TRANSFORM(
                    quotaManager->GetOriginDirectory(aOriginMetadata)));

  QM_TRY(QM_TO_RESULT(fileSystemDirectory->AppendRelativePath(
      NS_LITERAL_STRING_FROM_CSTRING(FILESYSTEM_DIRECTORY_NAME))));

  return fileSystemDirectory;
}

nsresult EnsureFileSystemDirectory(
    const quota::OriginMetadata& aOriginMetadata) {
  quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  QM_TRY(MOZ_TO_RESULT(quotaManager->EnsureTemporaryStorageIsInitialized()));

  QM_TRY_INSPECT(const auto& fileSystemDirectory,
                 quotaManager
                     ->EnsureTemporaryOriginIsInitialized(
                         quota::PERSISTENCE_TYPE_DEFAULT, aOriginMetadata)
                     .map([](const auto& aPair) { return aPair.first; }));

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

Result<nsCOMPtr<nsIFile>, QMResult> GetDatabaseFile(
    const quota::OriginMetadata& aOriginMetadata) {
  MOZ_ASSERT(!aOriginMetadata.mOrigin.IsEmpty());

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> databaseFile,
                GetFileSystemDirectory(aOriginMetadata));

  QM_TRY(QM_TO_RESULT(databaseFile->AppendRelativePath(kDatabaseFileName)));

  return databaseFile;
}

/**
 * TODO: This is almost identical to the corresponding function of IndexedDB
 */
Result<nsCOMPtr<nsIFileURL>, QMResult> GetDatabaseFileURL(
    const quota::OriginMetadata& aOriginMetadata,
    const int64_t aDirectoryLockId) {
  MOZ_ASSERT(aDirectoryLockId >= 0);

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> databaseFile,
                GetDatabaseFile(aOriginMetadata));

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

  nsCString directoryLockIdClause = "&directoryLockId="_ns;
  directoryLockIdClause.AppendInt(aDirectoryLockId);

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
Result<UniquePtr<FileSystemFileManager>, QMResult>
FileSystemFileManager::CreateFileSystemFileManager(
    const quota::OriginMetadata& aOriginMetadata) {
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> topDirectory,
                GetFileSystemDirectory(aOriginMetadata));

  return MakeUnique<FileSystemFileManager>(
      FileSystemFileManager(std::move(topDirectory)));
}

FileSystemFileManager::FileSystemFileManager(nsCOMPtr<nsIFile>&& aTopDirectory)
    : mTopDirectory(std::move(aTopDirectory)) {}

Result<nsCOMPtr<nsIFile>, QMResult> FileSystemFileManager::GetFile(
    const FileId& aFileId) const {
  return data::GetFile(mTopDirectory, aFileId);
}

Result<nsCOMPtr<nsIFile>, QMResult> FileSystemFileManager::GetOrCreateFile(
    const FileId& aFileId) {
  return data::GetOrCreateFile(mTopDirectory, aFileId);
}

Result<nsCOMPtr<nsIFile>, QMResult> FileSystemFileManager::CreateFileFrom(
    const FileId& aDestinationFileId, const FileId& aSourceFileId) {
  MOZ_ASSERT(!aDestinationFileId.IsEmpty());
  MOZ_ASSERT(!aSourceFileId.IsEmpty());

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> original, GetFile(aSourceFileId));

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> destination,
                GetFileDestination(mTopDirectory, aDestinationFileId));

  nsAutoString leafName;
  QM_TRY(QM_TO_RESULT(destination->GetLeafName(leafName)));

  nsCOMPtr<nsIFile> destParent;
  QM_TRY(QM_TO_RESULT(destination->GetParent(getter_AddRefs(destParent))));

  QM_TRY(QM_TO_RESULT(original->CopyTo(destParent, leafName)));

#ifdef DEBUG
  bool exists = false;
  QM_TRY(QM_TO_RESULT(destination->Exists(&exists)));
  MOZ_ASSERT(exists);

  int64_t destSize = 0;
  QM_TRY(QM_TO_RESULT(destination->GetFileSize(&destSize)));

  int64_t origSize = 0;
  QM_TRY(QM_TO_RESULT(original->GetFileSize(&origSize)));

  MOZ_ASSERT(destSize == origSize);
#endif

  return destination;
}

Result<Usage, QMResult> FileSystemFileManager::RemoveFile(
    const FileId& aFileId) {
  MOZ_ASSERT(!aFileId.IsEmpty());
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> pathObject,
                GetFileDestination(mTopDirectory, aFileId));

  bool exists = false;
  QM_TRY(QM_TO_RESULT(pathObject->Exists(&exists)));

  if (!exists) {
    return 0;
  }

  bool isFile = false;
  QM_TRY(QM_TO_RESULT(pathObject->IsFile(&isFile)));

  // We could handle this also as a nonexistent file.
  if (!isFile) {
    return Err(QMResult(NS_ERROR_FILE_IS_DIRECTORY));
  }

  Usage totalUsage = 0;
#ifdef DEBUG
  QM_TRY_UNWRAP(totalUsage,
                QM_TO_RESULT_INVOKE_MEMBER(pathObject, GetFileSize));
#endif

  QM_TRY(QM_TO_RESULT(pathObject->Remove(/* recursive */ false)));

  return totalUsage;
}

Result<DebugOnly<Usage>, QMResult> FileSystemFileManager::RemoveFiles(
    const nsTArray<FileId>& aFileIds, nsTArray<FileId>& aFailedRemovals) {
  if (aFileIds.IsEmpty()) {
    return DebugOnly<Usage>(0);
  }

  CheckedInt64 totalUsage = 0;
  for (const auto& someId : aFileIds) {
    QM_WARNONLY_TRY_UNWRAP(Maybe<nsCOMPtr<nsIFile>> maybeFile,
                           GetFileDestination(mTopDirectory, someId));
    if (!maybeFile) {
      aFailedRemovals.AppendElement(someId);
      continue;
    }
    nsCOMPtr<nsIFile> fileObject = maybeFile.value();

// Size recorded at close is checked to be equal to the sum of sizes on disk
#ifdef DEBUG
    QM_WARNONLY_TRY_UNWRAP(Maybe<Usage> fileSize, GetFileSize(fileObject));
    if (!fileSize) {
      aFailedRemovals.AppendElement(someId);
      continue;
    }
    totalUsage += fileSize.value();
#endif

    QM_WARNONLY_TRY_UNWRAP(Maybe<Ok> ok,
                           MOZ_TO_RESULT(RemoveFileObject(fileObject)));
    if (!ok) {
      aFailedRemovals.AppendElement(someId);
    }
  }

  MOZ_ASSERT(totalUsage.isValid());

  return DebugOnly<Usage>(totalUsage.value());
}

}  // namespace mozilla::dom::fs::data
