/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemQuotaClient.h"

#include "ResultStatement.h"
#include "datamodel/FileSystemDatabaseManager.h"
#include "datamodel/FileSystemFileManager.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/quota/Assertions.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsIFile.h"

namespace mozilla::dom::fs {

namespace {

auto toNSResult = [](const auto& aRv) { return ToNSResult(aRv); };

Result<ResultConnection, QMResult> GetStorageConnection(
    const quota::OriginMetadata& aOriginMetadata) {
  QM_TRY_INSPECT(const nsCOMPtr<nsIFile>& databaseFile,
                 data::GetDatabaseFile(aOriginMetadata));

  QM_TRY_INSPECT(
      const auto& storageService,
      QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_GET_TYPED(
          nsCOMPtr<mozIStorageService>, MOZ_SELECT_OVERLOAD(do_GetService),
          MOZ_STORAGE_SERVICE_CONTRACTID)));

  QM_TRY_UNWRAP(
      auto connection,
      QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
          nsCOMPtr<mozIStorageConnection>, storageService, OpenDatabase,
          databaseFile, mozIStorageService::CONNECTION_DEFAULT)));

  ResultConnection result(connection);

  return result;
}

}  // namespace

FileSystemQuotaClient::FileSystemQuotaClient() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

quota::Client::Type FileSystemQuotaClient::GetType() {
  return quota::Client::Type::FILESYSTEM;
}

Result<quota::UsageInfo, nsresult> FileSystemQuotaClient::InitOrigin(
    quota::PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata, const AtomicBool& aCanceled) {
  quota::AssertIsOnIOThread();

  {
    QM_TRY_INSPECT(const nsCOMPtr<nsIFile>& databaseFile,
                   data::GetDatabaseFile(aOriginMetadata).mapErr(toNSResult));

    bool exists = false;
    QM_TRY(MOZ_TO_RESULT(databaseFile->Exists(&exists)));
    // If database doesn't already exist, we do not create it
    if (!exists) {
      return quota::UsageInfo();
    }
  }

  QM_TRY_INSPECT(const ResultConnection& conn,
                 GetStorageConnection(aOriginMetadata).mapErr(toNSResult));

  QM_TRY(MOZ_TO_RESULT(
      data::FileSystemDatabaseManager::RescanUsages(conn, aOriginMetadata)));

  return data::FileSystemDatabaseManager::GetUsage(conn, aOriginMetadata)
      .mapErr(toNSResult);
}

nsresult FileSystemQuotaClient::InitOriginWithoutTracking(
    quota::PersistenceType /* aPersistenceType */,
    const quota::OriginMetadata& /* aOriginMetadata */,
    const AtomicBool& /* aCanceled */) {
  quota::AssertIsOnIOThread();

  // This is called when a storage/permanent/${origin}/fs directory exists. Even
  // though this shouldn't happen with a "good" profile, we shouldn't return an
  // error here, since that would cause origin initialization to fail. We just
  // warn and otherwise ignore that.
  UNKNOWN_FILE_WARNING(
      NS_LITERAL_STRING_FROM_CSTRING(FILESYSTEM_DIRECTORY_NAME));

  return NS_OK;
}

Result<quota::UsageInfo, nsresult> FileSystemQuotaClient::GetUsageForOrigin(
    quota::PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata,
    const AtomicBool& /* aCanceled */) {
  quota::AssertIsOnIOThread();

  MOZ_ASSERT(aPersistenceType ==
             quota::PersistenceType::PERSISTENCE_TYPE_DEFAULT);

  quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // We can't open the database at this point because the quota manager may not
  // allow it. Use the cached value instead.
  return quotaManager->GetUsageForClient(aPersistenceType, aOriginMetadata,
                                         quota::Client::FILESYSTEM);
}

void FileSystemQuotaClient::OnOriginClearCompleted(
    quota::PersistenceType aPersistenceType, const nsACString& aOrigin) {
  quota::AssertIsOnIOThread();
}

void FileSystemQuotaClient::OnRepositoryClearCompleted(
    quota::PersistenceType aPersistenceType) {
  quota::AssertIsOnIOThread();
}

void FileSystemQuotaClient::ReleaseIOThreadObjects() {
  quota::AssertIsOnIOThread();
}

void FileSystemQuotaClient::AbortOperationsForLocks(
    const DirectoryLockIdTable& aDirectoryLockIds) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  data::FileSystemDataManager::AbortOperationsForLocks(aDirectoryLockIds);
}

void FileSystemQuotaClient::AbortOperationsForProcess(
    ContentParentId aContentParentId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

void FileSystemQuotaClient::AbortAllOperations() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

void FileSystemQuotaClient::StartIdleMaintenance() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

void FileSystemQuotaClient::StopIdleMaintenance() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

void FileSystemQuotaClient::InitiateShutdown() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  data::FileSystemDataManager::InitiateShutdown();
}

nsCString FileSystemQuotaClient::GetShutdownStatus() const {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  return "Not implemented"_ns;
}

bool FileSystemQuotaClient::IsShutdownCompleted() const {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  return data::FileSystemDataManager::IsShutdownCompleted();
}

void FileSystemQuotaClient::ForceKillActors() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  // Hopefully not needed.
}

void FileSystemQuotaClient::FinalizeShutdown() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  // Empty for now.
}

}  // namespace mozilla::dom::fs
