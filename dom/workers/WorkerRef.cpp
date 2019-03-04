/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerRef.h"

#include "mozilla/Unused.h"
#include "WorkerHolder.h"
#include "WorkerRunnable.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

namespace {

// This runnable is used to release the StrongWorkerRef on the worker thread
// when a ThreadSafeWorkerRef is released.
class ReleaseRefControlRunnable final : public WorkerControlRunnable {
 public:
  ReleaseRefControlRunnable(WorkerPrivate* aWorkerPrivate,
                            already_AddRefed<StrongWorkerRef> aRef)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
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
// WorkerRef::Holder

class WorkerRef::Holder final : public mozilla::dom::WorkerHolder {
 public:
  Holder(const char* aName, WorkerRef* aWorkerRef, Behavior aBehavior)
      : mozilla::dom::WorkerHolder(aName, aBehavior), mWorkerRef(aWorkerRef) {}

  bool Notify(WorkerStatus aStatus) override {
    MOZ_ASSERT(mWorkerRef);

    if (aStatus < Canceling) {
      return true;
    }

    // Let's keep this object alive for the whole Notify() execution.
    RefPtr<WorkerRef> workerRef;
    workerRef = mWorkerRef;

    workerRef->Notify();
    return true;
  }

 public:
  WorkerRef* mWorkerRef;
};

// ----------------------------------------------------------------------------
// WorkerRef

WorkerRef::WorkerRef(WorkerPrivate* aWorkerPrivate)
    : mWorkerPrivate(aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
}

WorkerRef::~WorkerRef() { NS_ASSERT_OWNINGTHREAD(WorkerRef); }

void WorkerRef::Notify() {
  MOZ_ASSERT(mHolder);
  NS_ASSERT_OWNINGTHREAD(WorkerRef);

  if (!mCallback) {
    return;
  }

  std::function<void()> callback = std::move(mCallback);
  MOZ_ASSERT(!mCallback);

  callback();
}

// ----------------------------------------------------------------------------
// WeakWorkerRef

/* static */ already_AddRefed<WeakWorkerRef> WeakWorkerRef::Create(
    WorkerPrivate* aWorkerPrivate, std::function<void()>&& aCallback) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<WeakWorkerRef> ref = new WeakWorkerRef(aWorkerPrivate);

  // This holder doesn't keep the worker alive.
  UniquePtr<Holder> holder(new Holder("WeakWorkerRef::Holder", ref,
                                      WorkerHolder::AllowIdleShutdownStart));
  if (NS_WARN_IF(!holder->HoldWorker(aWorkerPrivate, Canceling))) {
    return nullptr;
  }

  ref->mHolder = std::move(holder);
  ref->mCallback = std::move(aCallback);

  return ref.forget();
}

WeakWorkerRef::WeakWorkerRef(WorkerPrivate* aWorkerPrivate)
    : WorkerRef(aWorkerPrivate) {}

WeakWorkerRef::~WeakWorkerRef() = default;

void WeakWorkerRef::Notify() {
  WorkerRef::Notify();

  mHolder = nullptr;
  mWorkerPrivate = nullptr;
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

/* static */ already_AddRefed<StrongWorkerRef> StrongWorkerRef::Create(
    WorkerPrivate* const aWorkerPrivate, const char* const aName,
    std::function<void()>&& aCallback) {
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

  RefPtr<StrongWorkerRef> ref = new StrongWorkerRef(aWorkerPrivate);

  // The worker is kept alive by this holder.
  UniquePtr<Holder> holder(
      new Holder(aName, ref, WorkerHolder::PreventIdleShutdownStart));
  if (NS_WARN_IF(!holder->HoldWorker(aWorkerPrivate, aFailStatus))) {
    return nullptr;
  }

  ref->mHolder = std::move(holder);

  return ref.forget();
}

StrongWorkerRef::StrongWorkerRef(WorkerPrivate* aWorkerPrivate)
    : WorkerRef(aWorkerPrivate) {}

StrongWorkerRef::~StrongWorkerRef() { NS_ASSERT_OWNINGTHREAD(StrongWorkerRef); }

WorkerPrivate* StrongWorkerRef::Private() const {
  MOZ_ASSERT(mHolder);
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

}  // namespace dom
}  // namespace mozilla
