/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAPARENT_H_
#define DOM_QUOTA_QUOTAPARENT_H_

#include "mozilla/dom/quota/PQuotaParent.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom::quota {

class Quota final : public PQuotaParent {
#ifdef DEBUG
  bool mActorDestroyed;
#endif

 public:
  Quota();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::quota::Quota, override)

 private:
  ~Quota();

  bool TrustParams() const;

  bool VerifyRequestParams(const UsageRequestParams& aParams) const;

  bool VerifyRequestParams(const RequestParams& aParams) const;

  // IPDL methods.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PQuotaUsageRequestParent* AllocPQuotaUsageRequestParent(
      const UsageRequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPQuotaUsageRequestConstructor(
      PQuotaUsageRequestParent* aActor,
      const UsageRequestParams& aParams) override;

  virtual bool DeallocPQuotaUsageRequestParent(
      PQuotaUsageRequestParent* aActor) override;

  virtual PQuotaRequestParent* AllocPQuotaRequestParent(
      const RequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPQuotaRequestConstructor(
      PQuotaRequestParent* aActor, const RequestParams& aParams) override;

  virtual bool DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvStorageInitialized(
      StorageInitializedResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult RecvTemporaryStorageInitialized(
      TemporaryStorageInitializedResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult RecvInitializeStorage(
      InitializeStorageResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult RecvInitializePersistentClient(
      const PrincipalInfo& aPrincipalInfo, const Type& aClientType,
      InitializePersistentClientResolver&& aResolve) override;

  virtual mozilla::ipc::IPCResult RecvInitializeTemporaryClient(
      const PersistenceType& aPersistenceType,
      const PrincipalInfo& aPrincipalInfo, const Type& aClientType,
      InitializeTemporaryClientResolver&& aResolve) override;

  virtual mozilla::ipc::IPCResult RecvInitializeTemporaryStorage(
      InitializeTemporaryStorageResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult RecvClearStoragesForOrigin(
      const Maybe<PersistenceType>& aPersistenceType,
      const PrincipalInfo& aPrincipalInfo, const Maybe<Type>& aClientType,
      ClearStoragesForOriginResolver&& aResolve) override;

  virtual mozilla::ipc::IPCResult RecvClearStoragesForOriginPrefix(
      const Maybe<PersistenceType>& aPersistenceType,
      const PrincipalInfo& aPrincipalInfo,
      ClearStoragesForOriginPrefixResolver&& aResolve) override;

  virtual mozilla::ipc::IPCResult RecvClearStoragesForOriginAttributesPattern(
      const OriginAttributesPattern& aPattern,
      ClearStoragesForOriginAttributesPatternResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult RecvClearStoragesForPrivateBrowsing(
      ClearStoragesForPrivateBrowsingResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult RecvClearStorage(
      ClearStorageResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult RecvShutdownStorage(
      ShutdownStorageResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult RecvStartIdleMaintenance() override;

  virtual mozilla::ipc::IPCResult RecvStopIdleMaintenance() override;

  virtual mozilla::ipc::IPCResult RecvAbortOperationsForProcess(
      const ContentParentId& aContentParentId) override;
};

already_AddRefed<PQuotaParent> AllocPQuotaParent();

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTAPARENT_H_
