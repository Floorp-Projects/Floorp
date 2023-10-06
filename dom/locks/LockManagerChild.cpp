/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LockManagerChild.h"
#include "LockRequestChild.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"

namespace mozilla::dom::locks {

LockManagerChild::LockManagerChild(nsIGlobalObject* aOwner) : mOwner(aOwner) {
  if (!NS_IsMainThread()) {
    mWorkerRef = IPCWorkerRef::Create(GetCurrentThreadWorkerPrivate(),
                                      "LockManagerChild");
  }
}

void LockManagerChild::NotifyBFCacheOnMainThread(nsPIDOMWindowInner* aInner,
                                                 bool aCreated) {
  AssertIsOnMainThread();
  if (!aInner) {
    return;
  }
  if (aCreated) {
    aInner->RemoveFromBFCacheSync();
  }

  uint32_t count = aInner->UpdateLockCount(aCreated);
  // It's okay for WindowGlobalChild to not exist, as it should mean it already
  // is destroyed and can't enter bfcache anyway.
  if (WindowGlobalChild* child = aInner->GetWindowGlobalChild()) {
    if (aCreated && count == 1) {
      // The first lock is active.
      child->BlockBFCacheFor(BFCacheStatus::ACTIVE_LOCK);
    } else if (count == 0) {
      child->UnblockBFCacheFor(BFCacheStatus::ACTIVE_LOCK);
    }
  }
}

class BFCacheNotifyLockRunnable final : public WorkerProxyToMainThreadRunnable {
 public:
  explicit BFCacheNotifyLockRunnable(bool aCreated) : mCreated(aCreated) {}

  void RunOnMainThread(WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    AssertIsOnMainThread();
    if (aWorkerPrivate->IsDedicatedWorker()) {
      LockManagerChild::NotifyBFCacheOnMainThread(
          aWorkerPrivate->GetAncestorWindow(), mCreated);
      return;
    }
    if (aWorkerPrivate->IsSharedWorker()) {
      aWorkerPrivate->GetRemoteWorkerController()->NotifyLock(mCreated);
      return;
    }
    MOZ_ASSERT_UNREACHABLE("Unexpected worker type");
  }

  void RunBackOnWorkerThreadForCleanup(WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

 private:
  bool mCreated;
};

void LockManagerChild::RequestLock(const LockRequest& aRequest,
                                   const LockOptions& aOptions) {
  auto requestActor = MakeRefPtr<LockRequestChild>(aRequest, aOptions.mSignal);
  requestActor->MaybeSetWorkerRef();
  SendPLockRequestConstructor(
      requestActor, IPCLockRequest(nsString(aRequest.mName), aOptions.mMode,
                                   aOptions.mIfAvailable, aOptions.mSteal));
  NotifyToWindow(true);
}

void LockManagerChild::NotifyRequestDestroy() const { NotifyToWindow(false); }

void LockManagerChild::NotifyToWindow(bool aCreated) const {
  if (NS_IsMainThread()) {
    NotifyBFCacheOnMainThread(GetParentObject()->GetAsInnerWindow(), aCreated);
    return;
  }

  WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
  if (wp->IsDedicatedWorker() || wp->IsSharedWorker()) {
    RefPtr<BFCacheNotifyLockRunnable> runnable =
        new BFCacheNotifyLockRunnable(aCreated);

    runnable->Dispatch(wp);
  }
};

}  // namespace mozilla::dom::locks
