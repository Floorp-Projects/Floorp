/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataStore.h"
#include "DataStoreCursor.h"

#include "mozilla/dom/DataStore.h"
#include "mozilla/dom/DataStoreCursor.h"
#include "mozilla/dom/DataStoreBinding.h"
#include "mozilla/dom/DataStoreImplBinding.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/ErrorResult.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

BEGIN_WORKERS_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WorkerDataStoreCursor, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WorkerDataStoreCursor, Release)

NS_IMPL_CYCLE_COLLECTION(WorkerDataStoreCursor, mWorkerStore)

WorkerDataStoreCursor::WorkerDataStoreCursor(WorkerDataStore* aWorkerStore)
  : mWorkerStore(aWorkerStore)
{
  MOZ_ASSERT(!NS_IsMainThread());
}

already_AddRefed<WorkerDataStoreCursor>
WorkerDataStoreCursor::Constructor(GlobalObject& aGlobal, ErrorResult& aRv)
{
  // We don't allow Gecko to create WorkerDataStoreCursor through JS codes like
  // window.DataStoreCursor() on the worker, so disable this for now.
  NS_NOTREACHED("Cannot use the chrome constructor on the worker!");
  return nullptr;
}

JSObject*
WorkerDataStoreCursor::WrapObject(JSContext* aCx)
{
  return DataStoreCursorBinding_workers::Wrap(aCx, this);
}

// A WorkerMainThreadRunnable which holds a reference to DataStoreCursor.
class DataStoreCursorRunnable : public WorkerMainThreadRunnable
{
protected:
  nsMainThreadPtrHandle<DataStoreCursor> mBackingCursor;

public:
  DataStoreCursorRunnable(WorkerPrivate* aWorkerPrivate,
                          const nsMainThreadPtrHandle<DataStoreCursor>& aBackingCursor)
    : WorkerMainThreadRunnable(aWorkerPrivate)
    , mBackingCursor(aBackingCursor)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }
};

// A DataStoreCursorRunnable to run DataStoreCursor::Next(...) on the main
// thread.
class DataStoreCursorNextRunnable MOZ_FINAL : public DataStoreCursorRunnable
{
  nsRefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  ErrorResult& mRv;

public:
  DataStoreCursorNextRunnable(WorkerPrivate* aWorkerPrivate,
                              const nsMainThreadPtrHandle<DataStoreCursor>& aBackingCursor,
                              Promise* aWorkerPromise,
                              ErrorResult& aRv)
    : DataStoreCursorRunnable(aWorkerPrivate, aBackingCursor)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    mPromiseWorkerProxy =
      new PromiseWorkerProxy(aWorkerPrivate, aWorkerPromise);
  }

protected:
  virtual bool
  MainThreadRun() MOZ_OVERRIDE
  {
    AssertIsOnMainThread();

    nsRefPtr<Promise> promise = mBackingCursor->Next(mRv);
    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreCursorRunnable to run DataStoreCursor::Close(...) on the main
// thread.
class DataStoreCursorCloseRunnable MOZ_FINAL : public DataStoreCursorRunnable
{
  ErrorResult& mRv;

public:
  DataStoreCursorCloseRunnable(WorkerPrivate* aWorkerPrivate,
                               const nsMainThreadPtrHandle<DataStoreCursor>& aBackingCursor,
                               ErrorResult& aRv)
    : DataStoreCursorRunnable(aWorkerPrivate, aBackingCursor)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() MOZ_OVERRIDE
  {
    AssertIsOnMainThread();

    mBackingCursor->Close(mRv);
    return true;
  }
};

already_AddRefed<WorkerDataStore>
WorkerDataStoreCursor::GetStore(JSContext* aCx, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  // We should direcly return the existing WorkerDataStore which owns this
  // WorkerDataStoreCursor, so that the WorkerDataStoreCursor.store can be
  // tested as equal to the WorkerDataStore owning this WorkerDataStoreCursor.
  MOZ_ASSERT(mWorkerStore);
  nsRefPtr<WorkerDataStore> workerStore = mWorkerStore;
  return workerStore.forget();
}

already_AddRefed<Promise>
WorkerDataStoreCursor::Next(JSContext* aCx, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DataStoreCursorNextRunnable> runnable =
    new DataStoreCursorNextRunnable(workerPrivate,
                                    mBackingCursor,
                                    promise,
                                    aRv);
  runnable->Dispatch(aCx);

  return promise.forget();
}

void
WorkerDataStoreCursor::Close(JSContext* aCx, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<DataStoreCursorCloseRunnable> runnable =
    new DataStoreCursorCloseRunnable(workerPrivate, mBackingCursor, aRv);
  runnable->Dispatch(aCx);
}

void
WorkerDataStoreCursor::SetDataStoreCursorImpl(DataStoreCursorImpl& aCursor)
{
  NS_NOTREACHED("We don't use this for the WorkerDataStoreCursor!");
}

void
WorkerDataStoreCursor::SetBackingDataStoreCursor(
  const nsMainThreadPtrHandle<DataStoreCursor>& aBackingCursor)
{
  mBackingCursor = aBackingCursor;
}

END_WORKERS_NAMESPACE
