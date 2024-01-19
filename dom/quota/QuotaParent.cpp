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
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsDebug.h"
#include "nsError.h"
#include "OriginOperations.h"
#include "QuotaRequestBase.h"
#include "QuotaUsageRequestBase.h"

// CUF == CRASH_UNLESS_FUZZING
#define QM_CUF_AND_IPC_FAIL(actor)                           \
  [&_actor = *actor](const auto& aFunc, const auto& aExpr) { \
    MOZ_CRASH_UNLESS_FUZZING();                              \
    return QM_IPC_FAIL(&_actor)(aFunc, aExpr);               \
  }

namespace mozilla::dom::quota {

using namespace mozilla::ipc;

namespace {

class ResolveBoolResponseAndReturn {
 public:
  using ResolverType = BoolResponseResolver;

  explicit ResolveBoolResponseAndReturn(const ResolverType& aResolver)
      : mResolver(aResolver) {}

  mozilla::ipc::IPCResult operator()(const nsresult rv) {
    mResolver(rv);
    return IPC_OK();
  }

 private:
  const ResolverType& mResolver;
};

class BoolPromiseResolveOrRejectCallback {
 public:
  using PromiseType = BoolPromise;
  using ResolverType = BoolResponseResolver;

  BoolPromiseResolveOrRejectCallback(RefPtr<Quota> aQuota,
                                     ResolverType&& aResolver)
      : mQuota(std::move(aQuota)), mResolver(std::move(aResolver)) {}

  void operator()(const PromiseType::ResolveOrRejectValue& aValue) {
    if (!mQuota->CanSend()) {
      return;
    }

    if (aValue.IsResolve()) {
      mResolver(aValue.ResolveValue());
    } else {
      mResolver(aValue.RejectValue());
    }
  }

 private:
  RefPtr<Quota> mQuota;
  ResolverType mResolver;
};

}  // namespace

already_AddRefed<PQuotaParent> AllocPQuotaParent() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

  auto actor = MakeRefPtr<Quota>();

  return actor.forget();
}

Quota::Quota()
#ifdef DEBUG
    : mActorDestroyed(false)
#endif
{
}

Quota::~Quota() { MOZ_ASSERT(mActorDestroyed); }

bool Quota::TrustParams() const {
#ifdef DEBUG
  // Never trust parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  return trustParams;
}

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

  if (!TrustParams() && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    MOZ_CRASH_UNLESS_FUZZING();
    return nullptr;
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(), nullptr);

  auto actor = [&]() -> RefPtr<QuotaUsageRequestBase> {
    switch (aParams.type()) {
      case UsageRequestParams::TAllUsageParams:
        return CreateGetUsageOp(quotaManager, aParams);

      case UsageRequestParams::TOriginUsageParams:
        return CreateGetOriginUsageOp(quotaManager, aParams);

      default:
        MOZ_CRASH("Should never get here!");
    }
  }();

  MOZ_ASSERT(actor);

  quotaManager->RegisterNormalOriginOp(*actor);

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

  if (!TrustParams() && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    MOZ_CRASH_UNLESS_FUZZING();
    return nullptr;
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(), nullptr);

  auto actor = [&]() -> RefPtr<QuotaRequestBase> {
    switch (aParams.type()) {
      case RequestParams::TStorageNameParams:
        return CreateStorageNameOp(quotaManager);

      case RequestParams::TInitializePersistentOriginParams:
        return CreateInitializePersistentOriginOp(quotaManager, aParams);

      case RequestParams::TInitializeTemporaryOriginParams:
        return CreateInitializeTemporaryOriginOp(quotaManager, aParams);

      case RequestParams::TGetFullOriginMetadataParams:
        return CreateGetFullOriginMetadataOp(
            quotaManager, aParams.get_GetFullOriginMetadataParams());

      case RequestParams::TResetOriginParams:
        return CreateResetOriginOp(quotaManager, aParams);

      case RequestParams::TPersistedParams:
        return CreatePersistedOp(quotaManager, aParams);

      case RequestParams::TPersistParams:
        return CreatePersistOp(quotaManager, aParams);

      case RequestParams::TEstimateParams:
        return CreateEstimateOp(quotaManager, aParams.get_EstimateParams());

      case RequestParams::TListOriginsParams:
        return CreateListOriginsOp(quotaManager);

      default:
        MOZ_CRASH("Should never get here!");
    }
  }();

  MOZ_ASSERT(actor);

  quotaManager->RegisterNormalOriginOp(*actor);

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

mozilla::ipc::IPCResult Quota::RecvStorageInitialized(
    StorageInitializedResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->StorageInitialized()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvTemporaryStorageInitialized(
    TemporaryStorageInitializedResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->TemporaryStorageInitialized()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializeStorage(
    InitializeStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->InitializeStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializePersistentClient(
    const PrincipalInfo& aPrincipalInfo, const Type& aClientType,
    InitializeTemporaryClientResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(QuotaManager::IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(Client::IsValidType(aClientType)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  quotaManager->InitializePersistentClient(aPrincipalInfo, aClientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializeTemporaryClient(
    const PersistenceType& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, const Type& aClientType,
    InitializeTemporaryClientResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(aPersistenceType)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(QuotaManager::IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(Client::IsValidType(aClientType)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  quotaManager
      ->InitializeTemporaryClient(aPersistenceType, aPrincipalInfo, aClientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializeTemporaryStorage(
    InitializeTemporaryStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->InitializeTemporaryStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForOrigin(
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, const Maybe<Type>& aClientType,
    ClearStoragesForOriginResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    if (aPersistenceType) {
      QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(*aPersistenceType)),
             QM_CUF_AND_IPC_FAIL(this));
    }

    QM_TRY(MOZ_TO_RESULT(QuotaManager::IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));

    if (aClientType) {
      QM_TRY(MOZ_TO_RESULT(Client::IsValidType(*aClientType)),
             QM_CUF_AND_IPC_FAIL(this));
    }
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager
      ->ClearStoragesForOrigin(aPersistenceType, aPrincipalInfo, aClientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForOriginPrefix(
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo,
    ClearStoragesForOriginResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    if (aPersistenceType) {
      QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(*aPersistenceType)),
             QM_CUF_AND_IPC_FAIL(this));
    }

    QM_TRY(MOZ_TO_RESULT(QuotaManager::IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearStoragesForOriginPrefix(aPersistenceType, aPrincipalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForOriginAttributesPattern(
    const OriginAttributesPattern& aPattern,
    ClearStoragesForOriginAttributesPatternResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(!BackgroundParent::IsOtherProcessActor(Manager())),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearStoragesForOriginAttributesPattern(aPattern)->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForPrivateBrowsing(
    ClearStoragesForPrivateBrowsingResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(!BackgroundParent::IsOtherProcessActor(Manager())),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearPrivateRepository()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStorage(
    ShutdownStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvShutdownStorage(
    ShutdownStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ShutdownStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

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
