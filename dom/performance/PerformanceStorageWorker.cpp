/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceStorageWorker.h"
#include "Performance.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceTiming.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"

namespace mozilla::dom {

class PerformanceProxyData {
 public:
  PerformanceProxyData(UniquePtr<PerformanceTimingData>&& aData,
                       const nsAString& aInitiatorType,
                       const nsAString& aEntryName)
      : mData(std::move(aData)),
        mInitiatorType(aInitiatorType),
        mEntryName(aEntryName) {
    MOZ_RELEASE_ASSERT(mData);
  }

  UniquePtr<PerformanceTimingData> mData;  // always non-null
  nsString mInitiatorType;
  nsString mEntryName;
};

namespace {

// Here we use control runnable because this code must be executed also when in
// a sync event loop
class PerformanceEntryAdder final : public WorkerControlRunnable {
 public:
  PerformanceEntryAdder(WorkerPrivate* aWorkerPrivate,
                        PerformanceStorageWorker* aStorage,
                        UniquePtr<PerformanceProxyData>&& aData)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mStorage(aStorage),
        mData(std::move(aData)) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mStorage->AddEntryOnWorker(std::move(mData));
    return true;
  }

  nsresult Cancel() override {
    mStorage->ShutdownOnWorker();
    return WorkerRunnable::Cancel();
  }

  bool PreDispatch(WorkerPrivate* aWorkerPrivate) override { return true; }

  void PostDispatch(WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult) override {}

 private:
  RefPtr<PerformanceStorageWorker> mStorage;
  UniquePtr<PerformanceProxyData> mData;
};

}  // namespace

/* static */
already_AddRefed<PerformanceStorageWorker> PerformanceStorageWorker::Create(
    WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<PerformanceStorageWorker> storage = new PerformanceStorageWorker();

  storage->mWorkerRef = WeakWorkerRef::Create(
      aWorkerPrivate, [storage]() { storage->ShutdownOnWorker(); });

  // PerformanceStorageWorker is created at the creation time of the worker.
  MOZ_ASSERT(storage->mWorkerRef);

  return storage.forget();
}

PerformanceStorageWorker::PerformanceStorageWorker()
    : mMutex("PerformanceStorageWorker::mMutex") {}

PerformanceStorageWorker::~PerformanceStorageWorker() = default;

void PerformanceStorageWorker::AddEntry(nsIHttpChannel* aChannel,
                                        nsITimedChannel* aTimedChannel) {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);

  if (!mWorkerRef) {
    return;
  }

  // If we have mWorkerRef, we haven't received the WorkerRef notification and
  // we haven't yet call ShutdownOnWorker, which uses the mutex.
  WorkerPrivate* workerPrivate = mWorkerRef->GetUnsafePrivate();
  MOZ_ASSERT(workerPrivate);

  nsAutoString initiatorType;
  nsAutoString entryName;

  UniquePtr<PerformanceTimingData> performanceTimingData(
      PerformanceTimingData::Create(aTimedChannel, aChannel, 0, initiatorType,
                                    entryName));
  if (!performanceTimingData) {
    return;
  }

  UniquePtr<PerformanceProxyData> data(new PerformanceProxyData(
      std::move(performanceTimingData), initiatorType, entryName));

  RefPtr<PerformanceEntryAdder> r =
      new PerformanceEntryAdder(workerPrivate, this, std::move(data));
  Unused << NS_WARN_IF(!r->Dispatch());
}

void PerformanceStorageWorker::AddEntry(
    const nsString& aEntryName, const nsString& aInitiatorType,
    UniquePtr<PerformanceTimingData>&& aData) {
  MOZ_ASSERT(!NS_IsMainThread());
  if (!aData) {
    return;
  }

  UniquePtr<PerformanceProxyData> data = MakeUnique<PerformanceProxyData>(
      std::move(aData), aInitiatorType, aEntryName);

  AddEntryOnWorker(std::move(data));
}

void PerformanceStorageWorker::ShutdownOnWorker() {
  MutexAutoLock lock(mMutex);

  if (!mWorkerRef) {
    return;
  }

  MOZ_ASSERT(!NS_IsMainThread());

  mWorkerRef = nullptr;
}

void PerformanceStorageWorker::AddEntryOnWorker(
    UniquePtr<PerformanceProxyData>&& aData) {
  RefPtr<Performance> performance;
  UniquePtr<PerformanceProxyData> data = std::move(aData);

  {
    MutexAutoLock lock(mMutex);

    if (!mWorkerRef) {
      return;
    }

    // We must have the workerPrivate because it is available until a
    // notification is received by WorkerRef and we use mutex to make the code
    // protected.
    WorkerPrivate* workerPrivate = mWorkerRef->GetPrivate();
    MOZ_ASSERT(workerPrivate);

    WorkerGlobalScope* scope = workerPrivate->GlobalScope();
    performance = scope->GetPerformance();
  }

  if (NS_WARN_IF(!performance)) {
    return;
  }

  RefPtr<PerformanceResourceTiming> performanceEntry =
      new PerformanceResourceTiming(std::move(data->mData), performance,
                                    data->mEntryName);
  performanceEntry->SetInitiatorType(data->mInitiatorType);

  performance->InsertResourceEntry(performanceEntry);
}

}  // namespace mozilla::dom
