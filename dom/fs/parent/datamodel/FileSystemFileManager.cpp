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

#define FS_QUOTA_MANAGEMENT_ENABLED 0

namespace mozilla::dom::fs::data {

namespace {

Result<nsCOMPtr<nsIFile>, QMResult> GetFileDestination(
    nsIFile& aTopDirectory,  // non-const for nsIFile Clone
    const EntryId& aEntryId) {
  MOZ_ASSERT(32u == aEntryId.Length());

  nsCOMPtr<nsIFile> destination;

  // nsIFile Clone is not a constant method
  QM_TRY(QM_TO_RESULT(aTopDirectory.Clone(getter_AddRefs(destination))));

  QM_TRY_UNWRAP(Name encoded, FileSystemHashSource::EncodeHash(aEntryId));

  MOZ_ALWAYS_TRUE(IsAscii(encoded));

  nsString relativePath;
  relativePath.Append(Substring(encoded, 0, 2));

  QM_TRY(QM_TO_RESULT(destination->AppendRelativePath(relativePath)));

  QM_TRY(QM_TO_RESULT(destination->AppendRelativePath(encoded)));

  return destination;
}

Result<nsCOMPtr<nsIFile>, QMResult> GetOrCreateEntry(
    const nsAString& aDesiredFilePath, bool& aExists,
    decltype(nsIFile::NORMAL_FILE_TYPE) aKind, uint32_t aPermissions) {
  MOZ_ASSERT(!aDesiredFilePath.IsEmpty());

  nsCOMPtr<nsIFile> result;
  QM_TRY(QM_TO_RESULT(NS_NewLocalFile(aDesiredFilePath,
                                      /* aFollowLinks */ false,
                                      getter_AddRefs(result))));

  QM_TRY(QM_TO_RESULT(result->Exists(&aExists)));
  if (aExists) {
    return result;
  }

  QM_TRY(QM_TO_RESULT(result->Create(aKind, aPermissions)));

  return result;
}

Result<nsCOMPtr<nsIFile>, QMResult> GetFileSystemDirectory(
    const Origin& aOrigin) {
  quota::QuotaManager* qm = quota::QuotaManager::Get();
  MOZ_ASSERT(qm);

#if FS_QUOTA_MANAGEMENT_ENABLED
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> fileSystemDirectory,
                QM_TO_RESULT_TRANSFORM(qm->GetDirectoryForOrigin(
                    quota::PERSISTENCE_TYPE_DEFAULT, aOrigin)));
#else
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> fileSystemDirectory,
                GetDirectoryForOrigin(*qm, aOrigin));
#endif

  QM_TRY(QM_TO_RESULT(fileSystemDirectory->AppendRelativePath(u"fs"_ns)));

  return fileSystemDirectory;
}

Result<nsString, QMResult> GetFileSystemDatabaseFile(const Origin& aOrigin) {
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> directoryPath,
                GetFileSystemDirectory(aOrigin));

  QM_TRY(
      QM_TO_RESULT(directoryPath->AppendRelativePath(u"metadata.sqlite"_ns)));

  nsString databaseFile;
  QM_TRY(QM_TO_RESULT(directoryPath->GetPath(databaseFile)));

  return databaseFile;
}

}  // namespace

Result<nsCOMPtr<nsIFile>, QMResult> GetDatabasePath(const Origin& aOrigin,
                                                    bool& aExists) {
  QM_TRY_UNWRAP(nsString databaseFilePath, GetFileSystemDatabaseFile(aOrigin))

  MOZ_ASSERT(!databaseFilePath.IsEmpty());

  return GetOrCreateEntry(databaseFilePath, aExists, nsIFile::NORMAL_FILE_TYPE,
                          0644);
}

/**
 * TODO: This is almost identical to the corresponding function of IndexedDB
 */
Result<nsCOMPtr<nsIFileURL>, QMResult> GetDatabaseFileURL(
    const nsCOMPtr<nsIFile>& aDatabaseFile, const int64_t aDirectoryLockId) {
  MOZ_ASSERT(aDirectoryLockId >= 0);

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
                     aDatabaseFile)));

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

Result<FileSystemFileManager, QMResult>
FileSystemFileManager::CreateFileSystemFileManager(
    nsCOMPtr<nsIFile>&& topDirectory) {
  return FileSystemFileManager(std::move(topDirectory));
}

Result<FileSystemFileManager, QMResult>
FileSystemFileManager::CreateFileSystemFileManager(const Origin& aOrigin) {
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> topDirectory,
                GetFileSystemDirectory(aOrigin));

  return FileSystemFileManager(std::move(topDirectory));
}

FileSystemFileManager::FileSystemFileManager(nsCOMPtr<nsIFile>&& aTopDirectory)
    : mTopDirectory(std::move(aTopDirectory)) {}

Result<nsCOMPtr<nsIFile>, QMResult> FileSystemFileManager::GetOrCreateFile(
    const EntryId& aEntryId) {
  MOZ_ASSERT(!aEntryId.IsEmpty());

  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> pathObject,
                GetFileDestination(*mTopDirectory, aEntryId));

  nsString desiredPath;
  QM_TRY(QM_TO_RESULT(pathObject->GetPath(desiredPath)));

  bool exists = false;
  QM_TRY_UNWRAP(
      nsCOMPtr<nsIFile> result,
      GetOrCreateEntry(desiredPath, exists, nsIFile::NORMAL_FILE_TYPE, 0644));

  return result;
}

nsresult FileSystemFileManager::RemoveFile(const EntryId& aEntryId) {
  MOZ_ASSERT(!aEntryId.IsEmpty());
  QM_TRY_UNWRAP(nsCOMPtr<nsIFile> pathObject,
                GetFileDestination(*mTopDirectory, aEntryId));

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
