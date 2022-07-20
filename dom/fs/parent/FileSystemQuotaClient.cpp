/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemQuotaClient.h"
#include "FileSystemQuotaClientImpl.h"

#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom::fs {

QuotaClient::QuotaClient() { ::mozilla::ipc::AssertIsOnBackgroundThread(); }

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  RefPtr<QuotaClient> client = new fs::QuotaClient();
  return client.forget();
}

mozilla::dom::quota::Client::Type QuotaClient::GetType() {
  return quota::Client::Type::FILESYSTEM;
}

Result<quota::UsageInfo, nsresult> QuotaClient::InitOrigin(
    quota::PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata, const AtomicBool& aCanceled) {
  quota::AssertIsOnIOThread();

  return GetUsageForOrigin(aPersistenceType, aOriginMetadata, aCanceled);
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

  return usage::GetUsageForOrigin(aPersistenceType, aOriginMetadata, aCanceled);
}

nsresult QuotaClient::AboutToClearOrigins(
    const Nullable<quota::PersistenceType>& aPersistenceType,
    const quota::OriginScope& aOriginScope) {
  quota::AssertIsOnIOThread();

  return usage::AboutToClearOrigins(aPersistenceType, aOriginScope);

  return NS_OK;
}

void QuotaClient::OnOriginClearCompleted(
    quota::PersistenceType aPersistenceType, const nsACString& aOrigin) {
  quota::AssertIsOnIOThread();
}

void QuotaClient::ReleaseIOThreadObjects() { quota::AssertIsOnIOThread(); }

void QuotaClient::AbortOperationsForLocks(
    const DirectoryLockIdTable& aDirectoryLockIds) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
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

  mIsShutdown.exchange(true);
}

bool QuotaClient::IsShutdownCompleted() const { return mIsShutdown; }

void QuotaClient::ForceKillActors() {
  // Hopefully not needed.
}

nsCString QuotaClient::GetShutdownStatus() const {
  return mIsShutdown ? "Done"_ns : "Not done"_ns;
}

void QuotaClient::FinalizeShutdown() {
  // Empty for now.
}

}  // namespace mozilla::dom::fs
