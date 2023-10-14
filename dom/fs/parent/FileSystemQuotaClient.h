/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMQUOTACLIENT_H_
#define DOM_FS_PARENT_FILESYSTEMQUOTACLIENT_H_

#include "mozilla/dom/quota/Client.h"

namespace mozilla::dom::fs {

class FileSystemQuotaClient : public quota::Client {
 public:
  FileSystemQuotaClient();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::fs::FileSystemQuotaClient,
                                        override)

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

  void OnRepositoryClearCompleted(
      quota::PersistenceType aPersistenceType) override;

  void ReleaseIOThreadObjects() override;

  void AbortOperationsForLocks(
      const DirectoryLockIdTable& aDirectoryLockIds) override;

  void AbortOperationsForProcess(ContentParentId aContentParentId) override;

  void AbortAllOperations() override;

  void StartIdleMaintenance() override;

  void StopIdleMaintenance() override;

 protected:
  ~FileSystemQuotaClient() = default;

  void InitiateShutdown() override;
  bool IsShutdownCompleted() const override;
  nsCString GetShutdownStatus() const override;
  void ForceKillActors() override;
  void FinalizeShutdown() override;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_PARENT_FILESYSTEMQUOTACLIENT_H_
