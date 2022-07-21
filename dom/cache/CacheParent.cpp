/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheParent.h"

#include "mozilla/dom/cache/CacheOpParent.h"
#include "nsCOMPtr.h"

namespace mozilla::dom::cache {

// Declared in ActorUtils.h
void DeallocPCacheParent(PCacheParent* aActor) { delete aActor; }

CacheParent::CacheParent(SafeRefPtr<cache::Manager> aManager, CacheId aCacheId)
    : mManager(std::move(aManager)), mCacheId(aCacheId) {
  MOZ_COUNT_CTOR(cache::CacheParent);
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  mManager->AddRefCacheId(mCacheId);
}

CacheParent::~CacheParent() {
  MOZ_COUNT_DTOR(cache::CacheParent);
  MOZ_DIAGNOSTIC_ASSERT(!mManager);
}

void CacheParent::ActorDestroy(ActorDestroyReason aReason) {
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  mManager->ReleaseCacheId(mCacheId);
  mManager = nullptr;
}

already_AddRefed<PCacheOpParent> CacheParent::AllocPCacheOpParent(
    const CacheOpArgs& aOpArgs) {
  if (aOpArgs.type() != CacheOpArgs::TCacheMatchArgs &&
      aOpArgs.type() != CacheOpArgs::TCacheMatchAllArgs &&
      aOpArgs.type() != CacheOpArgs::TCachePutAllArgs &&
      aOpArgs.type() != CacheOpArgs::TCacheDeleteArgs &&
      aOpArgs.type() != CacheOpArgs::TCacheKeysArgs) {
    MOZ_CRASH("Invalid operation sent to Cache actor!");
  }

  return MakeAndAddRef<CacheOpParent>(Manager(), mCacheId, aOpArgs);
}

mozilla::ipc::IPCResult CacheParent::RecvPCacheOpConstructor(
    PCacheOpParent* aActor, const CacheOpArgs& aOpArgs) {
  auto actor = static_cast<CacheOpParent*>(aActor);
  actor->Execute(mManager.clonePtr());
  return IPC_OK();
}

mozilla::ipc::IPCResult CacheParent::RecvTeardown() {
  // If child process is gone, warn and allow actor to clean up normally
  QM_WARNONLY_TRY(OkIf(Send__delete__(this)));
  return IPC_OK();
}

}  // namespace mozilla::dom::cache
