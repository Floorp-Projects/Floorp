/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemQuotaClient.h"

#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom::fs {

namespace {

class QuotaClient final : public mozilla::dom::quota::Client {
 public:
  QuotaClient();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::fs::QuotaClient, override)

  Type GetType() override;

  Result<quota::UsageInfo, nsresult> InitOrigin(
      quota::PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) override;

  nsresult InitOriginWithoutTracking(
      quota::PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) override;

  Result<quota::UsageInfo, nsresult> GetUsageForOrigin(
      quota::PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) override;

  void OnOriginClearCompleted(quota::PersistenceType aPersistenceType,
                              const nsACString& aOrigin) override;

  void ReleaseIOThreadObjects() override;

  void AbortOperationsForLocks(
      const DirectoryLockIdTable& aDirectoryLockIds) override;

  void AbortOperationsForProcess(ContentParentId aContentParentId) override;

  void AbortAllOperations() override;

  void StartIdleMaintenance() override;

  void StopIdleMaintenance() override;

 private:
  ~QuotaClient() = default;

  void InitiateShutdown() override;
  bool IsShutdownCompleted() const override;
  nsCString GetShutdownStatus() const override;
  void ForceKillActors() override;
  void FinalizeShutdown() override;
};

}  // namespace

QuotaClient::QuotaClient() { ::mozilla::ipc::AssertIsOnBackgroundThread(); }

mozilla::dom::quota::Client::Type QuotaClient::GetType() {
  return quota::Client::Type::FILESYSTEM;
}

Result<quota::UsageInfo, nsresult> QuotaClient::InitOrigin(
    quota::PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata, const AtomicBool& aCanceled) {
  quota::AssertIsOnIOThread();

  return quota::UsageInfo{};
}

nsresult QuotaClient::InitOriginWithoutTracking(
    quota::PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata, const AtomicBool& aCanceled) {
  quota::AssertIsOnIOThread();

  return NS_OK;
}

Result<quota::UsageInfo, nsresult> QuotaClient::GetUsageForOrigin(
    quota::PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata, const AtomicBool& aCanceled) {
  quota::AssertIsOnIOThread();

  return quota::UsageInfo{};
}

void QuotaClient::OnOriginClearCompleted(
    quota::PersistenceType aPersistenceType, const nsACString& aOrigin) {
  quota::AssertIsOnIOThread();
}

void QuotaClient::ReleaseIOThreadObjects() { quota::AssertIsOnIOThread(); }

void QuotaClient::AbortOperationsForLocks(
    const DirectoryLockIdTable& aDirectoryLockIds) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  data::FileSystemDataManager::AbortOperationsForLocks(aDirectoryLockIds);
}

void QuotaClient::AbortOperationsForProcess(ContentParentId aContentParentId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

void QuotaClient::AbortAllOperations() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

void QuotaClient::StartIdleMaintenance() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

void QuotaClient::StopIdleMaintenance() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

void QuotaClient::InitiateShutdown() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  data::FileSystemDataManager::InitiateShutdown();
}

bool QuotaClient::IsShutdownCompleted() const {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  return data::FileSystemDataManager::IsShutdownCompleted();
}

nsCString QuotaClient::GetShutdownStatus() const {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  return "Not implemented"_ns;
}

void QuotaClient::ForceKillActors() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  // Hopefully not needed.
}

void QuotaClient::FinalizeShutdown() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  // Empty for now.
}

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  RefPtr<QuotaClient> client = new fs::QuotaClient();
  return client.forget();
}

}  // namespace mozilla::dom::fs
