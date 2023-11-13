/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WorkerRef.h"

#include "nsDebug.h"
#include "WorkerRunnable.h"
#include "WorkerPrivate.h"

namespace mozilla::dom {

namespace {

// This runnable is used to release the StrongWorkerRef on the worker thread
// when a ThreadSafeWorkerRef is released.
class ReleaseRefControlRunnable final : public WorkerControlRunnable {
 public:
  ReleaseRefControlRunnable(WorkerPrivate* aWorkerPrivate,
                            already_AddRefed<StrongWorkerRef> aRef)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThread),
        mRef(std::move(aRef)) {
    MOZ_ASSERT(mRef);
  }

  bool PreDispatch(WorkerPrivate* aWorkerPrivate) override { return true; }

  void PostDispatch(WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult) override {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mRef = nullptr;
    return true;
  }

 private:
  RefPtr<StrongWorkerRef> mRef;
};

}  // namespace

// ----------------------------------------------------------------------------
// WorkerRef

WorkerRef::WorkerRef(WorkerPrivate* aWorkerPrivate, const char* aName,
                     bool aIsPreventingShutdown)
    : mWorkerPrivate(aWorkerPrivate),
      mName(aName),
      mIsPreventingShutdown(aIsPreventingShutdown),
      mHolding(false) {
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aName);

  aWorkerPrivate->AssertIsOnWorkerThread();
}

WorkerRef::~WorkerRef() {
  NS_ASSERT_OWNINGTHREAD(WorkerRef);
  ReleaseWorker();
}

void WorkerRef::ReleaseWorker() {
  if (mHolding) {
    MOZ_ASSERT(mWorkerPrivate);

    if (mIsPreventingShutdown) {
      mWorkerPrivate->AssertIsNotPotentiallyLastGCCCRunning();
    }
    mWorkerPrivate->RemoveWorkerRef(this);
    mWorkerPrivate = nullptr;

    mHolding = false;
  }
}

bool WorkerRef::HoldWorker(WorkerStatus aStatus) {
  MOZ_ASSERT(mWorkerPrivate);
  MOZ_ASSERT(!mHolding);

  if (NS_WARN_IF(!mWorkerPrivate->AddWorkerRef(this, aStatus))) {
    return false;
  }

  mHolding = true;
  return true;
}

void WorkerRef::Notify() {
  NS_ASSERT_OWNINGTHREAD(WorkerRef);

  if (!mCallback) {
    return;
  }

  MoveOnlyFunction<void()> callback = std::move(mCallback);
  MOZ_ASSERT(!mCallback);

  callback();
}

// ----------------------------------------------------------------------------
// WeakWorkerRef

/* static */
already_AddRefed<WeakWorkerRef> WeakWorkerRef::Create(
    WorkerPrivate* aWorkerPrivate, MoveOnlyFunction<void()>&& aCallback) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<WeakWorkerRef> ref = new WeakWorkerRef(aWorkerPrivate);
  if (!ref->HoldWorker(Canceling)) {
    return nullptr;
  }

  ref->mCallback = std::move(aCallback);

  return ref.forget();
}

WeakWorkerRef::WeakWorkerRef(WorkerPrivate* aWorkerPrivate)
    : WorkerRef(aWorkerPrivate, "WeakWorkerRef", false) {}

WeakWorkerRef::~WeakWorkerRef() = default;

void WeakWorkerRef::Notify() {
  MOZ_ASSERT(mHolding);
  MOZ_ASSERT(mWorkerPrivate);

  // Notify could drop the last reference to this object. We must keep it alive
  // in order to call ReleaseWorker() immediately after.
  RefPtr<WeakWorkerRef> kungFuGrip = this;

  WorkerRef::Notify();
  ReleaseWorker();
}

WorkerPrivate* WeakWorkerRef::GetPrivate() const {
  NS_ASSERT_OWNINGTHREAD(WeakWorkerRef);
  return mWorkerPrivate;
}

WorkerPrivate* WeakWorkerRef::GetUnsafePrivate() const {
  return mWorkerPrivate;
}

// ----------------------------------------------------------------------------
// StrongWorkerRef

/* static */
already_AddRefed<StrongWorkerRef> StrongWorkerRef::Create(
    WorkerPrivate* const aWorkerPrivate, const char* const aName,
    MoveOnlyFunction<void()>&& aCallback) {
  if (RefPtr<StrongWorkerRef> ref =
          CreateImpl(aWorkerPrivate, aName, Canceling)) {
    ref->mCallback = std::move(aCallback);
    return ref.forget();
  }
  return nullptr;
}

/* static */
already_AddRefed<StrongWorkerRef> StrongWorkerRef::CreateForcibly(
    WorkerPrivate* const aWorkerPrivate, const char* const aName) {
  return CreateImpl(aWorkerPrivate, aName, Killing);
}

/* static */
already_AddRefed<StrongWorkerRef> StrongWorkerRef::CreateImpl(
    WorkerPrivate* const aWorkerPrivate, const char* const aName,
    WorkerStatus const aFailStatus) {
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aName);

  RefPtr<StrongWorkerRef> ref = new StrongWorkerRef(aWorkerPrivate, aName);
  if (!ref->HoldWorker(aFailStatus)) {
    return nullptr;
  }

  return ref.forget();
}

StrongWorkerRef::StrongWorkerRef(WorkerPrivate* aWorkerPrivate,
                                 const char* aName)
    : WorkerRef(aWorkerPrivate, aName, true) {}

StrongWorkerRef::~StrongWorkerRef() = default;

WorkerPrivate* StrongWorkerRef::Private() const {
  NS_ASSERT_OWNINGTHREAD(StrongWorkerRef);
  return mWorkerPrivate;
}

// ----------------------------------------------------------------------------
// ThreadSafeWorkerRef

ThreadSafeWorkerRef::ThreadSafeWorkerRef(StrongWorkerRef* aRef) : mRef(aRef) {
  MOZ_ASSERT(aRef);
  aRef->Private()->AssertIsOnWorkerThread();
}

ThreadSafeWorkerRef::~ThreadSafeWorkerRef() {
  // Let's release the StrongWorkerRef on the correct thread.
  if (!mRef->mWorkerPrivate->IsOnWorkerThread()) {
    WorkerPrivate* workerPrivate = mRef->mWorkerPrivate;
    RefPtr<ReleaseRefControlRunnable> r =
        new ReleaseRefControlRunnable(workerPrivate, mRef.forget());
    r->Dispatch();
    return;
  }
}

WorkerPrivate* ThreadSafeWorkerRef::Private() const {
  return mRef->mWorkerPrivate;
}

// ----------------------------------------------------------------------------
// IPCWorkerRef

/* static */
already_AddRefed<IPCWorkerRef> IPCWorkerRef::Create(
    WorkerPrivate* aWorkerPrivate, const char* aName,
    MoveOnlyFunction<void()>&& aCallback) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<IPCWorkerRef> ref = new IPCWorkerRef(aWorkerPrivate, aName);
  if (!ref->HoldWorker(Canceling)) {
    return nullptr;
  }
  ref->mWorkerPrivate->AdjustNonblockingCCBackgroundActorCount(1);
  ref->mCallback = std::move(aCallback);

  return ref.forget();
}

IPCWorkerRef::IPCWorkerRef(WorkerPrivate* aWorkerPrivate, const char* aName)
    : WorkerRef(aWorkerPrivate, aName, false), mActorCount(1) {}

IPCWorkerRef::~IPCWorkerRef() {
  NS_ASSERT_OWNINGTHREAD(IPCWorkerRef);
  // explicit type convertion to avoid undefined behavior of uint32_t overflow.
  mWorkerPrivate->AdjustNonblockingCCBackgroundActorCount(
      (int32_t)-mActorCount);
  ReleaseWorker();
};

WorkerPrivate* IPCWorkerRef::Private() const {
  NS_ASSERT_OWNINGTHREAD(IPCWorkerRef);
  return mWorkerPrivate;
}

void IPCWorkerRef::SetActorCount(uint32_t aCount) {
  NS_ASSERT_OWNINGTHREAD(IPCWorkerRef);
  // explicit type convertion to avoid undefined behavior of uint32_t overflow.
  mWorkerPrivate->AdjustNonblockingCCBackgroundActorCount((int32_t)aCount -
                                                          (int32_t)mActorCount);
  mActorCount = aCount;
}

}  // namespace mozilla::dom
