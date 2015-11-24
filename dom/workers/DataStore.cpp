/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataStore.h"
#include "DataStoreCursor.h"

#include "mozilla/dom/DataStore.h"
#include "mozilla/dom/DataStoreCursor.h"
#include "mozilla/dom/DataStoreChangeEvent.h"
#include "mozilla/dom/DataStoreBinding.h"
#include "mozilla/dom/DataStoreImplBinding.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/ErrorResult.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

BEGIN_WORKERS_NAMESPACE

NS_IMPL_ADDREF_INHERITED(WorkerDataStore, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(WorkerDataStore, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN(WorkerDataStore)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

WorkerDataStore::WorkerDataStore(WorkerGlobalScope* aScope)
  : DOMEventTargetHelper(static_cast<DOMEventTargetHelper*>(aScope))
{}

already_AddRefed<WorkerDataStore>
WorkerDataStore::Constructor(GlobalObject& aGlobal, ErrorResult& aRv)
{
  // We don't allow Gecko to create WorkerDataStore through JS codes like
  // window.DataStore() on the worker, so disable this for now.
  NS_NOTREACHED("Cannot use the chrome constructor on the worker!");
  return nullptr;
}

JSObject*
WorkerDataStore::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DataStoreBinding_workers::Wrap(aCx, this, aGivenProto);
}

// A WorkerMainThreadRunnable which holds a reference to WorkerDataStore.
class DataStoreRunnable : public WorkerMainThreadRunnable
{
protected:
  nsMainThreadPtrHandle<DataStore> mBackingStore;

public:
  DataStoreRunnable(WorkerPrivate* aWorkerPrivate,
                    const nsMainThreadPtrHandle<DataStore>& aBackingStore)
    : WorkerMainThreadRunnable(aWorkerPrivate)
    , mBackingStore(aBackingStore)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }
};

// A DataStoreRunnable to run:
//   - DataStore::GetName(...)
//   - DataStore::GetOwner(...)
//   - DataStore::GetRevisionId(...)
// on the main thread.
class DataStoreGetStringRunnable final : public DataStoreRunnable
{
  typedef void
  (DataStore::*FuncType)(nsAString&, ErrorResult&);

  FuncType mFunc;
  nsAString& mString;

public:
  DataStoreGetStringRunnable(WorkerPrivate* aWorkerPrivate,
                             const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                             FuncType aFunc,
                             nsAString& aString)
    : DataStoreRunnable(aWorkerPrivate, aBackingStore)
    , mFunc(aFunc)
    , mString(aString)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    ErrorResult rv;
    nsString string;
    (mBackingStore.get()->*mFunc)(string, rv);
    mString.Assign(string);

    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
    }

    return true;
  }
};

// A DataStoreRunnable to run DataStore::GetReadOnly(...) on the main
// thread.
class DataStoreGetReadOnlyRunnable final : public DataStoreRunnable
{
public:
  bool mReadOnly;

  DataStoreGetReadOnlyRunnable(WorkerPrivate* aWorkerPrivate,
                               const nsMainThreadPtrHandle<DataStore>& aBackingStore)
    : DataStoreRunnable(aWorkerPrivate, aBackingStore)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    ErrorResult rv;
    mReadOnly = mBackingStore->GetReadOnly(rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
    }

    return true;
  }
};

class DataStoreProxyRunnable : public DataStoreRunnable
{
public:
  DataStoreProxyRunnable(WorkerPrivate* aWorkerPrivate,
                         const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                         Promise* aWorkerPromise)
    : DataStoreRunnable(aWorkerPrivate, aBackingStore)
    , mFailed(false)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    mPromiseWorkerProxy =
      PromiseWorkerProxy::Create(aWorkerPrivate, aWorkerPromise);
  }

  void Dispatch(ErrorResult& aRv)
  {
    if (mPromiseWorkerProxy) {
      DataStoreRunnable::Dispatch(aRv);
    }

    // If the creation of mProxyWorkerProxy failed, the worker is terminating.
    // In this case we don't want to dispatch the runnable and we should stop
    // the promise chain here.
  }

  bool Failed() const
  {
    return mFailed;
  }

protected:
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  bool mFailed;
};

// A DataStoreRunnable to run DataStore::Get(...) on the main thread.
class DataStoreGetRunnable final : public DataStoreProxyRunnable
{
  Sequence<OwningStringOrUnsignedLong> mId;

public:
  DataStoreGetRunnable(WorkerPrivate* aWorkerPrivate,
                       const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                       Promise* aWorkerPromise)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  Sequence<OwningStringOrUnsignedLong>& Id()
  {
    return mId;
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    ErrorResult rv;
    RefPtr<Promise> promise = mBackingStore->Get(mId, rv);
    promise->AppendNativeHandler(mPromiseWorkerProxy);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mFailed = true;
    }

    return true;
  }
};

// A DataStoreRunnable to run DataStore::Put(...) on the main thread.
class DataStorePutRunnable final : public DataStoreProxyRunnable
                                 , public StructuredCloneHolder
{
  const StringOrUnsignedLong& mId;
  const nsString mRevisionId;
  nsresult mError;

public:
  DataStorePutRunnable(WorkerPrivate* aWorkerPrivate,
                       const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                       Promise* aWorkerPromise,
                       const StringOrUnsignedLong& aId,
                       const nsAString& aRevisionId)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , StructuredCloneHolder(CloningNotSupported, TransferringNotSupported,
                            SameProcessDifferentThread)
    , mId(aId)
    , mRevisionId(aRevisionId)
    , mError(NS_OK)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  nsresult ErrorCode() const
  {
    return mError;
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    // Initialise an AutoJSAPI with the target window.
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mBackingStore->GetParentObject()))) {
      mError = NS_ERROR_UNEXPECTED;
      return true;
    }
    JSContext* cx = jsapi.cx();

    ErrorResult rv;
    JS::Rooted<JS::Value> value(cx);
    Read(mBackingStore->GetParentObject(), cx, &value, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mError = NS_ERROR_DOM_DATA_CLONE_ERR;
      return true;
    }

    RefPtr<Promise> promise = mBackingStore->Put(cx, value, mId,
                                                 mRevisionId, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mError = NS_ERROR_FAILURE;
      return true;
    }

    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::Add(...) on the main thread.
class DataStoreAddRunnable final : public DataStoreProxyRunnable
                                 , public StructuredCloneHolder
{
  const Optional<StringOrUnsignedLong>& mId;
  const nsString mRevisionId;
  nsresult mResult;

public:
  DataStoreAddRunnable(WorkerPrivate* aWorkerPrivate,
                       const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                       Promise* aWorkerPromise,
                       const Optional<StringOrUnsignedLong>& aId,
                       const nsAString& aRevisionId)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , StructuredCloneHolder(CloningNotSupported, TransferringNotSupported,
                            SameProcessDifferentThread)
    , mId(aId)
    , mRevisionId(aRevisionId)
    , mResult(NS_OK)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  nsresult ErrorCode() const
  {
    return mResult;
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    // Initialise an AutoJSAPI with the target window.
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mBackingStore->GetParentObject()))) {
      mResult = NS_ERROR_UNEXPECTED;
      return true;
    }
    JSContext* cx = jsapi.cx();

    ErrorResult rv;
    JS::Rooted<JS::Value> value(cx);
    Read(mBackingStore->GetParentObject(), cx, &value, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mResult = NS_ERROR_DOM_DATA_CLONE_ERR;
      return true;
    }

    RefPtr<Promise> promise = mBackingStore->Add(cx, value, mId,
                                                 mRevisionId, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mResult = NS_ERROR_FAILURE;
      return true;
    }

    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::Remove(...) on the main
// thread.
class DataStoreRemoveRunnable final : public DataStoreProxyRunnable
{
  const StringOrUnsignedLong& mId;
  const nsString mRevisionId;

public:
  DataStoreRemoveRunnable(WorkerPrivate* aWorkerPrivate,
                          const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                          Promise* aWorkerPromise,
                          const StringOrUnsignedLong& aId,
                          const nsAString& aRevisionId)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , mId(aId)
    , mRevisionId(aRevisionId)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    ErrorResult rv;
    RefPtr<Promise> promise = mBackingStore->Remove(mId, mRevisionId, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mFailed = true;
      return true;
    }

    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::Clear(...) on the main thread.
class DataStoreClearRunnable final : public DataStoreProxyRunnable
{
  const nsString mRevisionId;

public:
  DataStoreClearRunnable(WorkerPrivate* aWorkerPrivate,
                         const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                         Promise* aWorkerPromise,
                         const nsAString& aRevisionId)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , mRevisionId(aRevisionId)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    ErrorResult rv;
    RefPtr<Promise> promise = mBackingStore->Clear(mRevisionId, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mFailed = true;
      return true;
    }

    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::Sync(...) on the main thread.
class DataStoreSyncStoreRunnable final : public DataStoreRunnable
{
  WorkerDataStoreCursor* mWorkerCursor;
  const nsString mRevisionId;
  bool mFailed;

public:
  DataStoreSyncStoreRunnable(WorkerPrivate* aWorkerPrivate,
                             const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                             WorkerDataStoreCursor* aWorkerCursor,
                             const nsAString& aRevisionId)
    : DataStoreRunnable(aWorkerPrivate, aBackingStore)
    , mWorkerCursor(aWorkerCursor)
    , mRevisionId(aRevisionId)
    , mFailed(false)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool Failed() const
  {
    return mFailed;
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    // Point WorkerDataStoreCursor to DataStoreCursor.
    ErrorResult rv;
    RefPtr<DataStoreCursor> cursor = mBackingStore->Sync(mRevisionId, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mFailed = true;
      return true;
    }

    nsMainThreadPtrHandle<DataStoreCursor> backingCursor(
      new nsMainThreadPtrHolder<DataStoreCursor>(cursor));
    mWorkerCursor->SetBackingDataStoreCursor(backingCursor);

    return true;
  }
};

void
WorkerDataStore::GetName(JSContext* aCx, nsAString& aName, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<DataStoreGetStringRunnable> runnable =
    new DataStoreGetStringRunnable(workerPrivate,
                                   mBackingStore,
                                   &DataStore::GetName,
                                   aName);
  runnable->Dispatch(aRv);
}

void
WorkerDataStore::GetOwner(JSContext* aCx, nsAString& aOwner, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<DataStoreGetStringRunnable> runnable =
    new DataStoreGetStringRunnable(workerPrivate,
                                   mBackingStore,
                                   &DataStore::GetOwner,
                                   aOwner);
  runnable->Dispatch(aRv);
}

bool
WorkerDataStore::GetReadOnly(JSContext* aCx, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<DataStoreGetReadOnlyRunnable> runnable =
    new DataStoreGetReadOnlyRunnable(workerPrivate, mBackingStore);
  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return true; // To be safe, I guess.
  }

  return runnable->mReadOnly;
}

already_AddRefed<Promise>
WorkerDataStore::Get(JSContext* aCx,
                     const Sequence<OwningStringOrUnsignedLong>& aId,
                     ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<DataStoreGetRunnable> runnable =
    new DataStoreGetRunnable(workerPrivate,
                             mBackingStore,
                             promise);

  if (!runnable->Id().AppendElements(aId, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (runnable->Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<Promise>
WorkerDataStore::Put(JSContext* aCx,
                     JS::Handle<JS::Value> aObj,
                     const StringOrUnsignedLong& aId,
                     const nsAString& aRevisionId,
                     ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<DataStorePutRunnable> runnable =
    new DataStorePutRunnable(workerPrivate,
                             mBackingStore,
                             promise,
                             aId,
                             aRevisionId);
  runnable->Write(aCx, aObj, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (NS_FAILED(runnable->ErrorCode())) {
    aRv.Throw(runnable->ErrorCode());
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<Promise>
WorkerDataStore::Add(JSContext* aCx,
                     JS::Handle<JS::Value> aObj,
                     const Optional<StringOrUnsignedLong>& aId,
                     const nsAString& aRevisionId,
                     ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<DataStoreAddRunnable> runnable =
    new DataStoreAddRunnable(workerPrivate,
                             mBackingStore,
                             promise,
                             aId,
                             aRevisionId);
  runnable->Write(aCx, aObj, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (NS_FAILED(runnable->ErrorCode())) {
    aRv.Throw(runnable->ErrorCode());
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<Promise>
WorkerDataStore::Remove(JSContext* aCx,
                        const StringOrUnsignedLong& aId,
                        const nsAString& aRevisionId,
                        ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<DataStoreRemoveRunnable> runnable =
    new DataStoreRemoveRunnable(workerPrivate,
                                mBackingStore,
                                promise,
                                aId,
                                aRevisionId);
  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (runnable->Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<Promise>
WorkerDataStore::Clear(JSContext* aCx,
                       const nsAString& aRevisionId,
                       ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<DataStoreClearRunnable> runnable =
    new DataStoreClearRunnable(workerPrivate,
                               mBackingStore,
                               promise,
                               aRevisionId);
  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (runnable->Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return promise.forget();
}

void
WorkerDataStore::GetRevisionId(JSContext* aCx,
                               nsAString& aRevisionId,
                               ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<DataStoreGetStringRunnable> runnable =
    new DataStoreGetStringRunnable(workerPrivate,
                                   mBackingStore,
                                   &DataStore::GetRevisionId,
                                   aRevisionId);
  runnable->Dispatch(aRv);
}

// A DataStoreRunnable to run DataStore::GetLength(...) on the main thread.
class DataStoreGetLengthRunnable final : public DataStoreProxyRunnable
{
public:
  DataStoreGetLengthRunnable(WorkerPrivate* aWorkerPrivate,
                             const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                             Promise* aWorkerPromise)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    ErrorResult rv;
    RefPtr<Promise> promise = mBackingStore->GetLength(rv);
    promise->AppendNativeHandler(mPromiseWorkerProxy);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mFailed = true;
    }

    return true;
  }
};

already_AddRefed<Promise>
WorkerDataStore::GetLength(JSContext* aCx, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<DataStoreGetLengthRunnable> runnable =
    new DataStoreGetLengthRunnable(workerPrivate,
                                   mBackingStore,
                                   promise);
  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (runnable->Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<WorkerDataStoreCursor>
WorkerDataStore::Sync(JSContext* aCx,
                      const nsAString& aRevisionId,
                      ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  // Create a WorkerDataStoreCursor on the worker. Note that we need to pass
  // this WorkerDataStore into the WorkerDataStoreCursor, so that it can keep
  // track of which WorkerDataStore owns the WorkerDataStoreCursor.
  RefPtr<WorkerDataStoreCursor> workerCursor =
    new WorkerDataStoreCursor(this);

  // DataStoreSyncStoreRunnable will point the WorkerDataStoreCursor to the
  // DataStoreCursor created on the main thread.
  RefPtr<DataStoreSyncStoreRunnable> runnable =
    new DataStoreSyncStoreRunnable(workerPrivate,
                                   mBackingStore,
                                   workerCursor,
                                   aRevisionId);
  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (runnable->Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return workerCursor.forget();
}

void
WorkerDataStore::SetBackingDataStore(
  const nsMainThreadPtrHandle<DataStore>& aBackingStore)
{
  mBackingStore = aBackingStore;
}

void
WorkerDataStore::SetDataStoreChangeEventProxy(
  DataStoreChangeEventProxy* aEventProxy)
{
  mEventProxy = aEventProxy;
}

// A WorkerRunnable to dispatch the DataStoreChangeEvent on the worker thread.
class DispatchDataStoreChangeEventRunnable : public WorkerRunnable
{
public:
  DispatchDataStoreChangeEventRunnable(
    DataStoreChangeEventProxy* aDataStoreChangeEventProxy,
    DataStoreChangeEvent* aEvent)
    : WorkerRunnable(aDataStoreChangeEventProxy->GetWorkerPrivate(),
                     WorkerThreadUnchangedBusyCount)
    , mDataStoreChangeEventProxy(aDataStoreChangeEventProxy)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mDataStoreChangeEventProxy);

    aEvent->GetRevisionId(mRevisionId);
    aEvent->GetId(mId);
    aEvent->GetOperation(mOperation);
    aEvent->GetOwner(mOwner);
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aWorkerPrivate == mWorkerPrivate);

    MOZ_ASSERT(mDataStoreChangeEventProxy);

    RefPtr<WorkerDataStore> workerStore =
      mDataStoreChangeEventProxy->GetWorkerStore();

    DataStoreChangeEventInit eventInit;
    eventInit.mBubbles = false;
    eventInit.mCancelable = false;
    eventInit.mRevisionId = mRevisionId;
    eventInit.mId = mId;
    eventInit.mOperation = mOperation;
    eventInit.mOwner = mOwner;

    RefPtr<DataStoreChangeEvent> event =
      DataStoreChangeEvent::Constructor(workerStore,
                                        NS_LITERAL_STRING("change"),
                                        eventInit);

    workerStore->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    return true;
  }

protected:
  ~DispatchDataStoreChangeEventRunnable()
  {}

private:
  RefPtr<DataStoreChangeEventProxy> mDataStoreChangeEventProxy;

  nsString mRevisionId;
  Nullable<OwningStringOrUnsignedLong> mId;
  nsString mOperation;
  nsString mOwner;
};

DataStoreChangeEventProxy::DataStoreChangeEventProxy(
  WorkerPrivate* aWorkerPrivate,
  WorkerDataStore* aWorkerStore)
  : mWorkerPrivate(aWorkerPrivate)
  , mWorkerStore(aWorkerStore)
  , mCleanedUp(false)
  , mCleanUpLock("cleanUpLock")
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerStore);

  // Let the WorkerDataStore keep the DataStoreChangeEventProxy alive to catch
  // the coming events until the WorkerDataStore is released.
  mWorkerStore->SetDataStoreChangeEventProxy(this);

  // We do this to make sure the worker thread won't shut down before the event
  // is dispatched to the WorkerStore on the worker thread.
  if (!mWorkerPrivate->AddFeature(mWorkerPrivate->GetJSContext(), this)) {
    MOZ_ASSERT(false, "cannot add the worker feature!");
    return;
  }
}

WorkerPrivate*
DataStoreChangeEventProxy::GetWorkerPrivate() const
{
  // It's ok to race on |mCleanedUp|, because it will never cause us to fire
  // the assertion when we should not.
  MOZ_ASSERT(!mCleanedUp);

  return mWorkerPrivate;
}

WorkerDataStore*
DataStoreChangeEventProxy::GetWorkerStore() const
{
  return mWorkerStore;
}

// nsIDOMEventListener implementation.

NS_IMPL_ISUPPORTS(DataStoreChangeEventProxy, nsIDOMEventListener)

NS_IMETHODIMP
DataStoreChangeEventProxy::HandleEvent(nsIDOMEvent* aEvent)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mCleanUpLock);
  // If the worker thread's been cancelled we don't need to dispatch the event.
  if (mCleanedUp) {
    return NS_OK;
  }

  RefPtr<DataStoreChangeEvent> event =
    static_cast<DataStoreChangeEvent*>(aEvent);

  RefPtr<DispatchDataStoreChangeEventRunnable> runnable =
    new DispatchDataStoreChangeEventRunnable(this, event);

  {
    AutoSafeJSContext cx;
    JSAutoRequest ar(cx);
    runnable->Dispatch(cx);
  }

  return NS_OK;
}

// WorkerFeature implementation.

bool
DataStoreChangeEventProxy::Notify(JSContext* aCx, Status aStatus)
{
  MutexAutoLock lock(mCleanUpLock);

  // |mWorkerPrivate| might not be safe to use anymore if we have already
  // cleaned up and RemoveFeature(), so we need to check |mCleanedUp| first.
  if (mCleanedUp) {
    return true;
  }

  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->GetJSContext() == aCx);

  // Release the WorkerStore and remove the DataStoreChangeEventProxy from the
  // features of the worker thread since the worker thread has been cancelled.
  if (aStatus >= Canceling) {
    mWorkerStore = nullptr;
    mWorkerPrivate->RemoveFeature(aCx, this);
    mCleanedUp = true;
  }

  return true;
}

END_WORKERS_NAMESPACE
