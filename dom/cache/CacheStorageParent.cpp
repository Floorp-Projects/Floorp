/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStorageParent.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/cache/CacheOpParent.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/PBackgroundParent.h"

namespace mozilla::dom::cache {

using mozilla::dom::quota::QuotaManager;
using mozilla::ipc::PBackgroundParent;
using mozilla::ipc::PrincipalInfo;

// declared in ActorUtils.h
already_AddRefed<PCacheStorageParent> AllocPCacheStorageParent(
    PBackgroundParent* aManagingActor, Namespace aNamespace,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  if (NS_WARN_IF(!QuotaManager::IsPrincipalInfoValid(aPrincipalInfo))) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  return MakeAndAddRef<CacheStorageParent>(aManagingActor, aNamespace,
                                           aPrincipalInfo);
}

// declared in ActorUtils.h
void DeallocPCacheStorageParent(PCacheStorageParent* aActor) { delete aActor; }

CacheStorageParent::CacheStorageParent(PBackgroundParent* aManagingActor,
                                       Namespace aNamespace,
                                       const PrincipalInfo& aPrincipalInfo)
    : mNamespace(aNamespace), mVerifiedStatus(NS_OK) {
  MOZ_COUNT_CTOR(cache::CacheStorageParent);
  MOZ_DIAGNOSTIC_ASSERT(aManagingActor);

  // Start the async principal verification process immediately.
  mVerifier = PrincipalVerifier::CreateAndDispatch(*this, aManagingActor,
                                                   aPrincipalInfo);
  MOZ_DIAGNOSTIC_ASSERT(mVerifier);
}

CacheStorageParent::~CacheStorageParent() {
  MOZ_COUNT_DTOR(cache::CacheStorageParent);
  MOZ_DIAGNOSTIC_ASSERT(!mVerifier);
}

void CacheStorageParent::ActorDestroy(ActorDestroyReason aReason) {
  if (mVerifier) {
    mVerifier->RemoveListener(*this);
    mVerifier = nullptr;
  }
}

PCacheOpParent* CacheStorageParent::AllocPCacheOpParent(
    const CacheOpArgs& aOpArgs) {
  if (aOpArgs.type() != CacheOpArgs::TStorageMatchArgs &&
      aOpArgs.type() != CacheOpArgs::TStorageHasArgs &&
      aOpArgs.type() != CacheOpArgs::TStorageOpenArgs &&
      aOpArgs.type() != CacheOpArgs::TStorageDeleteArgs &&
      aOpArgs.type() != CacheOpArgs::TStorageKeysArgs) {
    MOZ_CRASH("Invalid operation sent to CacheStorage actor!");
  }

  return new CacheOpParent(Manager(), mNamespace, aOpArgs);
}

bool CacheStorageParent::DeallocPCacheOpParent(PCacheOpParent* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult CacheStorageParent::RecvPCacheOpConstructor(
    PCacheOpParent* aActor, const CacheOpArgs& aOpArgs) {
  auto actor = static_cast<CacheOpParent*>(aActor);

  if (mVerifier) {
    MOZ_DIAGNOSTIC_ASSERT(!mManagerId);
    actor->WaitForVerification(mVerifier);
    return IPC_OK();
  }

  if (NS_WARN_IF(NS_FAILED(mVerifiedStatus))) {
    ErrorResult result(mVerifiedStatus);

    QM_WARNONLY_TRY(OkIf(
        CacheOpParent::Send__delete__(actor, std::move(result), void_t())));
    return IPC_OK();
  }

  MOZ_DIAGNOSTIC_ASSERT(mManagerId);
  actor->Execute(mManagerId);
  return IPC_OK();
}

mozilla::ipc::IPCResult CacheStorageParent::RecvTeardown() {
  // If child process is gone, warn and allow actor to clean up normally
  QM_WARNONLY_TRY(OkIf(Send__delete__(this)));
  return IPC_OK();
}

void CacheStorageParent::OnPrincipalVerified(
    nsresult aRv, const SafeRefPtr<ManagerId>& aManagerId) {
  MOZ_DIAGNOSTIC_ASSERT(mVerifier);
  MOZ_DIAGNOSTIC_ASSERT(!mManagerId);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(mVerifiedStatus));

  if (NS_WARN_IF(NS_FAILED(aRv))) {
    mVerifiedStatus = aRv;
  }

  mManagerId = aManagerId.clonePtr();
  mVerifier->RemoveListener(*this);
  mVerifier = nullptr;
}

}  // namespace mozilla::dom::cache
