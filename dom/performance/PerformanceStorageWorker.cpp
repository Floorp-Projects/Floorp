/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceStorageWorker.h"
#include "mozilla/dom/WorkerHolder.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla {
namespace dom {

class PerformanceProxyData
{
public:
  PerformanceProxyData(UniquePtr<PerformanceTimingData>&& aData,
                       const nsAString& aInitiatorType,
                       const nsAString& aEntryName)
    : mData(Move(aData))
    , mInitiatorType(aInitiatorType)
    , mEntryName(aEntryName)
  {}

  UniquePtr<PerformanceTimingData> mData;
  nsString mInitiatorType;
  nsString mEntryName;
};

namespace {

// This runnable calls InitializeOnWorker() on the worker thread. Here a
// workerHolder is used to monitor when the worker thread is starting the
// shutdown procedure.
// Here we use control runnable because this code must be executed also when in
// a sync event loop.
class PerformanceStorageInitializer final : public WorkerControlRunnable
{
  RefPtr<PerformanceStorageWorker> mStorage;

public:
  PerformanceStorageInitializer(WorkerPrivate* aWorkerPrivate,
                                PerformanceStorageWorker* aStorage)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mStorage(aStorage)
  {}

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mStorage->InitializeOnWorker();
    return true;
  }

  nsresult
  Cancel() override
  {
    mStorage->ShutdownOnWorker();
    return WorkerRunnable::Cancel();
  }

  bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    return true;
  }

  void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {}
};

// Here we use control runnable because this code must be executed also when in
// a sync event loop
class PerformanceEntryAdder final : public WorkerControlRunnable
{
public:
  PerformanceEntryAdder(WorkerPrivate* aWorkerPrivate,
                        PerformanceStorageWorker* aStorage,
                        UniquePtr<PerformanceProxyData>&& aData)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mStorage(aStorage)
    , mData(Move(aData))
  {}

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mStorage->AddEntryOnWorker(Move(mData));
    return true;
  }

  nsresult
  Cancel() override
  {
    mStorage->ShutdownOnWorker();
    return WorkerRunnable::Cancel();
  }

  bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    return true;
  }

  void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {}

private:
  RefPtr<PerformanceStorageWorker> mStorage;
  UniquePtr<PerformanceProxyData> mData;
};

class PerformanceStorageWorkerHolder final : public WorkerHolder
{
  RefPtr<PerformanceStorageWorker> mStorage;

public:
  explicit PerformanceStorageWorkerHolder(PerformanceStorageWorker* aStorage)
    : WorkerHolder("PerformanceStorageWorkerHolder",
                   WorkerHolder::AllowIdleShutdownStart)
    , mStorage(aStorage)
  {}

  bool
  Notify(WorkerStatus aStatus) override
  {
    if (mStorage) {
      RefPtr<PerformanceStorageWorker> storage;
      storage.swap(mStorage);
      storage->ShutdownOnWorker();
    }

    return true;
  }
};

} // anonymous

/* static */ already_AddRefed<PerformanceStorageWorker>
PerformanceStorageWorker::Create(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<PerformanceStorageWorker> storage =
    new PerformanceStorageWorker(aWorkerPrivate);

  RefPtr<PerformanceStorageInitializer> r =
    new PerformanceStorageInitializer(aWorkerPrivate, storage);
  if (NS_WARN_IF(!r->Dispatch())) {
    return nullptr;
  }

  return storage.forget();
}

PerformanceStorageWorker::PerformanceStorageWorker(WorkerPrivate* aWorkerPrivate)
  : mMutex("PerformanceStorageWorker::mMutex")
  , mWorkerPrivate(aWorkerPrivate)
  , mState(eInitializing)
{
}

PerformanceStorageWorker::~PerformanceStorageWorker() = default;

void
PerformanceStorageWorker::AddEntry(nsIHttpChannel* aChannel,
                                   nsITimedChannel* aTimedChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);

  if (mState == eTerminated) {
    return;
  }

  nsAutoString initiatorType;
  nsAutoString entryName;

  UniquePtr<PerformanceTimingData> performanceTimingData(
    PerformanceTimingData::Create(aTimedChannel, aChannel, 0, initiatorType,
                                  entryName));
  if (!performanceTimingData) {
    return;
  }

  UniquePtr<PerformanceProxyData> data(
    new PerformanceProxyData(Move(performanceTimingData), initiatorType,
                             entryName));

  RefPtr<PerformanceEntryAdder> r =
    new PerformanceEntryAdder(mWorkerPrivate, this, Move(data));
  Unused << NS_WARN_IF(!r->Dispatch());
}

void
PerformanceStorageWorker::InitializeOnWorker()
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mState == eInitializing);
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  mWorkerHolder.reset(new PerformanceStorageWorkerHolder(this));
  if (!mWorkerHolder->HoldWorker(mWorkerPrivate, Canceling)) {
    MutexAutoUnlock lock(mMutex);
    ShutdownOnWorker();
    return;
  }

  // We are ready to accept entries.
  mState = eReady;
}

void
PerformanceStorageWorker::ShutdownOnWorker()
{
  MutexAutoLock lock(mMutex);

  if (mState == eTerminated) {
    return;
  }

  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  mState = eTerminated;
  mWorkerHolder = nullptr;
  mWorkerPrivate = nullptr;
}

void
PerformanceStorageWorker::AddEntryOnWorker(UniquePtr<PerformanceProxyData>&& aData)
{
  RefPtr<Performance> performance;
  UniquePtr<PerformanceProxyData> data = Move(aData);

  {
    MutexAutoLock lock(mMutex);

    if (mState == eTerminated) {
      return;
    }

    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    MOZ_ASSERT(mState == eReady);

    WorkerGlobalScope* scope = mWorkerPrivate->GlobalScope();
    performance = scope->GetPerformance();
  }

  if (NS_WARN_IF(!performance)) {
    return;
  }

  RefPtr<PerformanceResourceTiming> performanceEntry =
    new PerformanceResourceTiming(Move(data->mData), performance,
                                 data->mEntryName);
  performanceEntry->SetInitiatorType(data->mInitiatorType);

  performance->InsertResourceEntry(performanceEntry);
}

} // namespace dom
} // namespace mozilla
