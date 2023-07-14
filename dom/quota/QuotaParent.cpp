/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaParent.h"

#include <mozilla/Assertions.h>
#include "mozilla/RefPtr.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/PQuota.h"
#include "mozilla/dom/quota/PQuotaRequestParent.h"
#include "mozilla/dom/quota/PQuotaUsageRequestParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsDebug.h"
#include "nsError.h"
#include "OriginOperations.h"
#include "QuotaRequestBase.h"
#include "QuotaUsageRequestBase.h"

namespace mozilla::dom::quota {

using namespace mozilla::ipc;

PQuotaParent* AllocPQuotaParent() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

  auto actor = MakeRefPtr<Quota>();

  return actor.forget().take();
}

bool DeallocPQuotaParent(PQuotaParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Quota> actor = dont_AddRef(static_cast<Quota*>(aActor));
  return true;
}

Quota::Quota()
#ifdef DEBUG
    : mActorDestroyed(false)
#endif
{
}

Quota::~Quota() { MOZ_ASSERT(mActorDestroyed); }

bool Quota::VerifyRequestParams(const UsageRequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  switch (aParams.type()) {
    case UsageRequestParams::TAllUsageParams:
      break;

    case UsageRequestParams::TOriginUsageParams: {
      const OriginUsageParams& params = aParams.get_OriginUsageParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

bool Quota::VerifyRequestParams(const RequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  switch (aParams.type()) {
    case RequestParams::TStorageNameParams:
    case RequestParams::TStorageInitializedParams:
    case RequestParams::TTemporaryStorageInitializedParams:
    case RequestParams::TInitParams:
    case RequestParams::TInitTemporaryStorageParams:
      break;

    case RequestParams::TInitializePersistentOriginParams: {
      const InitializePersistentOriginParams& params =
          aParams.get_InitializePersistentOriginParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TInitializeTemporaryOriginParams: {
      const InitializeTemporaryOriginParams& params =
          aParams.get_InitializeTemporaryOriginParams();

      if (NS_WARN_IF(!IsBestEffortPersistenceType(params.persistenceType()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TGetFullOriginMetadataParams: {
      const GetFullOriginMetadataParams& params =
          aParams.get_GetFullOriginMetadataParams();
      if (NS_WARN_IF(!IsBestEffortPersistenceType(params.persistenceType()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TClearOriginParams: {
      const ClearResetOriginParams& params =
          aParams.get_ClearOriginParams().commonParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      if (params.persistenceTypeIsExplicit()) {
        if (NS_WARN_IF(!IsValidPersistenceType(params.persistenceType()))) {
          MOZ_CRASH_UNLESS_FUZZING();
          return false;
        }
      }

      if (params.clientTypeIsExplicit()) {
        if (NS_WARN_IF(!Client::IsValidType(params.clientType()))) {
          MOZ_CRASH_UNLESS_FUZZING();
          return false;
        }
      }

      break;
    }

    case RequestParams::TResetOriginParams: {
      const ClearResetOriginParams& params =
          aParams.get_ResetOriginParams().commonParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      if (params.persistenceTypeIsExplicit()) {
        if (NS_WARN_IF(!IsValidPersistenceType(params.persistenceType()))) {
          MOZ_CRASH_UNLESS_FUZZING();
          return false;
        }
      }

      if (params.clientTypeIsExplicit()) {
        if (NS_WARN_IF(!Client::IsValidType(params.clientType()))) {
          MOZ_CRASH_UNLESS_FUZZING();
          return false;
        }
      }

      break;
    }

    case RequestParams::TClearDataParams: {
      if (BackgroundParent::IsOtherProcessActor(Manager())) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TClearAllParams:
    case RequestParams::TResetAllParams:
    case RequestParams::TListOriginsParams:
      break;

    case RequestParams::TPersistedParams: {
      const PersistedParams& params = aParams.get_PersistedParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TPersistParams: {
      const PersistParams& params = aParams.get_PersistParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TEstimateParams: {
      const EstimateParams& params = aParams.get_EstimateParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

void Quota::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
#ifdef DEBUG
  MOZ_ASSERT(!mActorDestroyed);
  mActorDestroyed = true;
#endif
}

PQuotaUsageRequestParent* Quota::AllocPQuotaUsageRequestParent(
    const UsageRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  if (!trustParams && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    MOZ_CRASH_UNLESS_FUZZING();
    return nullptr;
  }

  QM_TRY(QuotaManager::EnsureCreated(), nullptr);

  MOZ_ASSERT(QuotaManager::Get());

  auto actor = [&]() -> RefPtr<QuotaUsageRequestBase> {
    switch (aParams.type()) {
      case UsageRequestParams::TAllUsageParams:
        return CreateGetUsageOp(aParams);

      case UsageRequestParams::TOriginUsageParams:
        return CreateGetOriginUsageOp(aParams);

      default:
        MOZ_CRASH("Should never get here!");
    }
  }();

  MOZ_ASSERT(actor);

  QuotaManager::Get()->RegisterNormalOriginOp(*actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Quota::RecvPQuotaUsageRequestConstructor(
    PQuotaUsageRequestParent* aActor, const UsageRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  auto* op = static_cast<QuotaUsageRequestBase*>(aActor);

  op->RunImmediately();
  return IPC_OK();
}

bool Quota::DeallocPQuotaUsageRequestParent(PQuotaUsageRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaUsageRequestBase> actor =
      dont_AddRef(static_cast<QuotaUsageRequestBase*>(aActor));
  return true;
}

PQuotaRequestParent* Quota::AllocPQuotaRequestParent(
    const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  if (!trustParams && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    MOZ_CRASH_UNLESS_FUZZING();
    return nullptr;
  }

  QM_TRY(QuotaManager::EnsureCreated(), nullptr);

  MOZ_ASSERT(QuotaManager::Get());

  auto actor = [&]() -> RefPtr<QuotaRequestBase> {
    switch (aParams.type()) {
      case RequestParams::TStorageNameParams:
        return CreateStorageNameOp();

      case RequestParams::TStorageInitializedParams:
        return CreateStorageInitializedOp();

      case RequestParams::TTemporaryStorageInitializedParams:
        return CreateTemporaryStorageInitializedOp();

      case RequestParams::TInitParams:
        return CreateInitOp();

      case RequestParams::TInitTemporaryStorageParams:
        return CreateInitTemporaryStorageOp();

      case RequestParams::TInitializePersistentOriginParams:
        return CreateInitializePersistentOriginOp(aParams);

      case RequestParams::TInitializeTemporaryOriginParams:
        return CreateInitializeTemporaryOriginOp(aParams);

      case RequestParams::TGetFullOriginMetadataParams:
        return CreateGetFullOriginMetadataOp(
            aParams.get_GetFullOriginMetadataParams());

      case RequestParams::TClearOriginParams:
        return CreateClearOriginOp(aParams);

      case RequestParams::TResetOriginParams:
        return CreateResetOriginOp(aParams);

      case RequestParams::TClearDataParams:
        return CreateClearDataOp(aParams);

      case RequestParams::TClearAllParams:
        return CreateResetOrClearOp(/* aClear */ true);

      case RequestParams::TResetAllParams:
        return CreateResetOrClearOp(/* aClear */ false);

      case RequestParams::TPersistedParams:
        return CreatePersistedOp(aParams);

      case RequestParams::TPersistParams:
        return CreatePersistOp(aParams);

      case RequestParams::TEstimateParams:
        return CreateEstimateOp(aParams.get_EstimateParams());

      case RequestParams::TListOriginsParams:
        return CreateListOriginsOp();

      default:
        MOZ_CRASH("Should never get here!");
    }
  }();

  MOZ_ASSERT(actor);

  QuotaManager::Get()->RegisterNormalOriginOp(*actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Quota::RecvPQuotaRequestConstructor(
    PQuotaRequestParent* aActor, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  auto* op = static_cast<QuotaRequestBase*>(aActor);

  op->RunImmediately();
  return IPC_OK();
}

bool Quota::DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaRequestBase> actor =
      dont_AddRef(static_cast<QuotaRequestBase*>(aActor));
  return true;
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForPrivateBrowsing(
    ClearStoragesForPrivateBrowsingResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    MOZ_CRASH_UNLESS_FUZZING();
    return IPC_FAIL(this, "Wrong actor");
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(), ([&aResolver](const auto rv) {
                  aResolver(rv);
                  return IPC_OK();
                }));

  quotaManager->ClearPrivateRepository()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, resolver = std::move(aResolver)](
          const BoolPromise::ResolveOrRejectValue& aValue) {
        if (!self->CanSend()) {
          return;
        }

        if (aValue.IsResolve()) {
          resolver(aValue.ResolveValue());
        } else {
          resolver(aValue.RejectValue());
        }
      });

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvStartIdleMaintenance() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    MOZ_CRASH_UNLESS_FUZZING();
    return IPC_FAIL(this, "Wrong actor");
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QM_TRY(QuotaManager::EnsureCreated(), IPC_OK());

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  quotaManager->StartIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvStopIdleMaintenance() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    MOZ_CRASH_UNLESS_FUZZING();
    return IPC_FAIL(this, "Wrong actor");
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->StopIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvAbortOperationsForProcess(
    const ContentParentId& aContentParentId) {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    MOZ_CRASH_UNLESS_FUZZING();
    return IPC_FAIL(this, "Wrong actor");
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->AbortOperationsForProcess(aContentParentId);

  return IPC_OK();
}

}  // namespace mozilla::dom::quota
