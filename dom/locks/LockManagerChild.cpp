/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LockManagerChild.h"
#include "LockRequestChild.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"

namespace mozilla::dom::locks {

void NotifyImpl(nsPIDOMWindowInner* inner, Document& aDoc, bool aCreated) {
  uint32_t count = aDoc.UpdateLockCount(aCreated);
  if (WindowGlobalChild* child = inner->GetWindowGlobalChild()) {
    if (aCreated && count == 1) {
      // The first lock is active.
      child->BlockBFCacheFor(BFCacheStatus::ACTIVE_LOCK);
    } else if (count == 0) {
      child->UnblockBFCacheFor(BFCacheStatus::ACTIVE_LOCK);
    }
  }
  // window actor is dead, so we should be just
  // destroying things
  MOZ_ASSERT_IF(!inner->GetWindowGlobalChild(), !aCreated);
}

class BFCacheNotifyLockRunnable final : public WorkerProxyToMainThreadRunnable {
 public:
  explicit BFCacheNotifyLockRunnable(bool aCreated) : mCreated(aCreated) {}

  void RunOnMainThread(WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    AssertIsOnMainThread();
    Document* doc = aWorkerPrivate->GetDocument();
    if (!doc) {
      return;
    }
    nsPIDOMWindowInner* inner = doc->GetInnerWindow();
    if (!inner) {
      return;
    }
    if (mCreated) {
      inner->RemoveFromBFCacheSync();
    }
    NotifyImpl(inner, *doc, mCreated);
  }

  void RunBackOnWorkerThreadForCleanup(WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

 private:
  bool mCreated;
};

NS_IMPL_CYCLE_COLLECTION(LockManagerChild, mOwner)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(LockManagerChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(LockManagerChild, Release)

void LockManagerChild::RequestLock(const LockRequest& aRequest,
                                   const LockOptions& aOptions) {
  auto requestActor = MakeRefPtr<LockRequestChild>(aRequest, aOptions.mSignal);
  SendPLockRequestConstructor(
      requestActor, IPCLockRequest(nsString(aRequest.mName), aOptions.mMode,
                                   aOptions.mIfAvailable, aOptions.mSteal));
  NotifyToWindow(true);
}

void LockManagerChild::NotifyRequestDestroy() const { NotifyToWindow(false); }

void LockManagerChild::NotifyToWindow(bool aCreated) const {
  if (NS_IsMainThread()) {
    nsPIDOMWindowInner* inner = GetParentObject()->AsInnerWindow();
    MOZ_ASSERT(inner);
    NotifyImpl(inner, *inner->GetExtantDoc(), aCreated);
    return;
  }

  WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
  if (!wp || !wp->IsDedicatedWorker()) {
    return;
  }

  RefPtr<BFCacheNotifyLockRunnable> runnable =
      new BFCacheNotifyLockRunnable(aCreated);

  runnable->Dispatch(wp);
};

}  // namespace mozilla::dom::locks
