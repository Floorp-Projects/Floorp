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
  ErrorResult& mRv;

public:
  DataStoreGetStringRunnable(WorkerPrivate* aWorkerPrivate,
                             const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                             FuncType aFunc,
                             nsAString& aString,
                             ErrorResult& aRv)
    : DataStoreRunnable(aWorkerPrivate, aBackingStore)
    , mFunc(aFunc)
    , mString(aString)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    nsString string;
    (mBackingStore.get()->*mFunc)(string, mRv);
    mString.Assign(string);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::GetReadOnly(...) on the main
// thread.
class DataStoreGetReadOnlyRunnable final : public DataStoreRunnable
{
  ErrorResult& mRv;

public:
  bool mReadOnly;

public:
  DataStoreGetReadOnlyRunnable(WorkerPrivate* aWorkerPrivate,
                               const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                               ErrorResult& aRv)
    : DataStoreRunnable(aWorkerPrivate, aBackingStore)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    mReadOnly = mBackingStore->GetReadOnly(mRv);
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
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    mPromiseWorkerProxy =
      PromiseWorkerProxy::Create(aWorkerPrivate, aWorkerPromise);
  }

  bool Dispatch(JSContext* aCx)
  {
    if (mPromiseWorkerProxy) {
      return DataStoreRunnable::Dispatch(aCx);
    }

    // If the creation of mProxyWorkerProxy failed, the worker is terminating.
    // In this case we don't want to dispatch the runnable and we should stop
    // the promise chain here.
    return true;
  }

protected:
  nsRefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
};

// A DataStoreRunnable to run DataStore::Get(...) on the main thread.
class DataStoreGetRunnable final : public DataStoreProxyRunnable
{
  Sequence<OwningStringOrUnsignedLong> mId;
  ErrorResult& mRv;

public:
  DataStoreGetRunnable(WorkerPrivate* aWorkerPrivate,
                       const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                       Promise* aWorkerPromise,
                       const Sequence<OwningStringOrUnsignedLong>& aId,
                       ErrorResult& aRv)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    if (!mId.AppendElements(aId, fallible)) {
      mRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    nsRefPtr<Promise> promise = mBackingStore->Get(mId, mRv);
    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::Put(...) on the main thread.
class DataStorePutRunnable final : public DataStoreProxyRunnable
{
  JSAutoStructuredCloneBuffer mObjBuffer;
  const StringOrUnsignedLong& mId;
  const nsString mRevisionId;
  ErrorResult& mRv;

public:
  DataStorePutRunnable(WorkerPrivate* aWorkerPrivate,
                       const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                       Promise* aWorkerPromise,
                       JSContext* aCx,
                       JS::Handle<JS::Value> aObj,
                       const StringOrUnsignedLong& aId,
                       const nsAString& aRevisionId,
                       ErrorResult& aRv)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , mId(aId)
    , mRevisionId(aRevisionId)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    // This needs to be structured cloned while it's still on the worker thread.
    if (!mObjBuffer.write(aCx, aObj)) {
      JS_ClearPendingException(aCx);
      mRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    }
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    // Initialise an AutoJSAPI with the target window.
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mBackingStore->GetParentObject()))) {
      mRv.Throw(NS_ERROR_UNEXPECTED);
      return true;
    }
    JSContext* cx = jsapi.cx();

    JS::Rooted<JS::Value> value(cx);
    if (!mObjBuffer.read(cx, &value)) {
      JS_ClearPendingException(cx);
      mRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
      return true;
    }

    nsRefPtr<Promise> promise = mBackingStore->Put(cx,
                                                   value,
                                                   mId,
                                                   mRevisionId,
                                                   mRv);
    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::Add(...) on the main thread.
class DataStoreAddRunnable final : public DataStoreProxyRunnable
{
  JSAutoStructuredCloneBuffer mObjBuffer;
  const Optional<StringOrUnsignedLong>& mId;
  const nsString mRevisionId;
  ErrorResult& mRv;

public:
  DataStoreAddRunnable(WorkerPrivate* aWorkerPrivate,
                       const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                       Promise* aWorkerPromise,
                       JSContext* aCx,
                       JS::Handle<JS::Value> aObj,
                       const Optional<StringOrUnsignedLong>& aId,
                       const nsAString& aRevisionId,
                       ErrorResult& aRv)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , mId(aId)
    , mRevisionId(aRevisionId)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    // This needs to be structured cloned while it's still on the worker thread.
    if (!mObjBuffer.write(aCx, aObj)) {
      JS_ClearPendingException(aCx);
      mRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    }
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    // Initialise an AutoJSAPI with the target window.
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mBackingStore->GetParentObject()))) {
      mRv.Throw(NS_ERROR_UNEXPECTED);
      return true;
    }
    JSContext* cx = jsapi.cx();

    JS::Rooted<JS::Value> value(cx);
    if (!mObjBuffer.read(cx, &value)) {
      JS_ClearPendingException(cx);
      mRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
      return true;
    }

    nsRefPtr<Promise> promise = mBackingStore->Add(cx,
                                                   value,
                                                   mId,
                                                   mRevisionId,
                                                   mRv);
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
  ErrorResult& mRv;

public:
  DataStoreRemoveRunnable(WorkerPrivate* aWorkerPrivate,
                          const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                          Promise* aWorkerPromise,
                          const StringOrUnsignedLong& aId,
                          const nsAString& aRevisionId,
                          ErrorResult& aRv)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , mId(aId)
    , mRevisionId(aRevisionId)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    nsRefPtr<Promise> promise = mBackingStore->Remove(mId, mRevisionId, mRv);
    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::Clear(...) on the main thread.
class DataStoreClearRunnable final : public DataStoreProxyRunnable
{
  const nsString mRevisionId;
  ErrorResult& mRv;

public:
  DataStoreClearRunnable(WorkerPrivate* aWorkerPrivate,
                         const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                         Promise* aWorkerPromise,
                         const nsAString& aRevisionId,
                         ErrorResult& aRv)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , mRevisionId(aRevisionId)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    nsRefPtr<Promise> promise = mBackingStore->Clear(mRevisionId, mRv);
    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

// A DataStoreRunnable to run DataStore::Sync(...) on the main thread.
class DataStoreSyncStoreRunnable final : public DataStoreRunnable
{
  WorkerDataStoreCursor* mWorkerCursor;
  const nsString mRevisionId;
  ErrorResult& mRv;

public:
  DataStoreSyncStoreRunnable(WorkerPrivate* aWorkerPrivate,
                             const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                             WorkerDataStoreCursor* aWorkerCursor,
                             const nsAString& aRevisionId,
                             ErrorResult& aRv)
    : DataStoreRunnable(aWorkerPrivate, aBackingStore)
    , mWorkerCursor(aWorkerCursor)
    , mRevisionId(aRevisionId)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    // Point WorkerDataStoreCursor to DataStoreCursor.
    nsRefPtr<DataStoreCursor> cursor = mBackingStore->Sync(mRevisionId, mRv);
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

  nsRefPtr<DataStoreGetStringRunnable> runnable =
    new DataStoreGetStringRunnable(workerPrivate,
                                   mBackingStore,
                                   &DataStore::GetName,
                                   aName,
                                   aRv);
  runnable->Dispatch(aCx);
}

void
WorkerDataStore::GetOwner(JSContext* aCx, nsAString& aOwner, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<DataStoreGetStringRunnable> runnable =
    new DataStoreGetStringRunnable(workerPrivate,
                                   mBackingStore,
                                   &DataStore::GetOwner,
                                   aOwner,
                                   aRv);
  runnable->Dispatch(aCx);
}

bool
WorkerDataStore::GetReadOnly(JSContext* aCx, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<DataStoreGetReadOnlyRunnable> runnable =
    new DataStoreGetReadOnlyRunnable(workerPrivate, mBackingStore, aRv);
  runnable->Dispatch(aCx);

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

  nsRefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DataStoreGetRunnable> runnable =
    new DataStoreGetRunnable(workerPrivate,
                             mBackingStore,
                             promise,
                             aId,
                             aRv);
  runnable->Dispatch(aCx);

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

  nsRefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DataStorePutRunnable> runnable =
    new DataStorePutRunnable(workerPrivate,
                             mBackingStore,
                             promise,
                             aCx,
                             aObj,
                             aId,
                             aRevisionId,
                             aRv);
  runnable->Dispatch(aCx);

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

  nsRefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DataStoreAddRunnable> runnable =
    new DataStoreAddRunnable(workerPrivate,
                             mBackingStore,
                             promise,
                             aCx,
                             aObj,
                             aId,
                             aRevisionId,
                             aRv);
  runnable->Dispatch(aCx);

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

  nsRefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DataStoreRemoveRunnable> runnable =
    new DataStoreRemoveRunnable(workerPrivate,
                                mBackingStore,
                                promise,
                                aId,
                                aRevisionId,
                                aRv);
  runnable->Dispatch(aCx);

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

  nsRefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DataStoreClearRunnable> runnable =
    new DataStoreClearRunnable(workerPrivate,
                               mBackingStore,
                               promise,
                               aRevisionId,
                               aRv);
  runnable->Dispatch(aCx);

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

  nsRefPtr<DataStoreGetStringRunnable> runnable =
    new DataStoreGetStringRunnable(workerPrivate,
                                   mBackingStore,
                                   &DataStore::GetRevisionId,
                                   aRevisionId,
                                   aRv);
  runnable->Dispatch(aCx);
}

// A DataStoreRunnable to run DataStore::GetLength(...) on the main thread.
class DataStoreGetLengthRunnable final : public DataStoreProxyRunnable
{
  ErrorResult& mRv;

public:
  DataStoreGetLengthRunnable(WorkerPrivate* aWorkerPrivate,
                             const nsMainThreadPtrHandle<DataStore>& aBackingStore,
                             Promise* aWorkerPromise,
                             ErrorResult& aRv)
    : DataStoreProxyRunnable(aWorkerPrivate, aBackingStore, aWorkerPromise)
    , mRv(aRv)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    nsRefPtr<Promise> promise = mBackingStore->GetLength(mRv);
    promise->AppendNativeHandler(mPromiseWorkerProxy);
    return true;
  }
};

already_AddRefed<Promise>
WorkerDataStore::GetLength(JSContext* aCx, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DataStoreGetLengthRunnable> runnable =
    new DataStoreGetLengthRunnable(workerPrivate,
                                   mBackingStore,
                                   promise,
                                   aRv);
  runnable->Dispatch(aCx);

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
  nsRefPtr<WorkerDataStoreCursor> workerCursor =
    new WorkerDataStoreCursor(this);

  // DataStoreSyncStoreRunnable will point the WorkerDataStoreCursor to the
  // DataStoreCursor created on the main thread.
  nsRefPtr<DataStoreSyncStoreRunnable> runnable =
    new DataStoreSyncStoreRunnable(workerPrivate,
                                   mBackingStore,
                                   workerCursor,
                                   aRevisionId,
                                   aRv);
  runnable->Dispatch(aCx);

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

    nsRefPtr<WorkerDataStore> workerStore =
      mDataStoreChangeEventProxy->GetWorkerStore();

    DataStoreChangeEventInit eventInit;
    eventInit.mBubbles = false;
    eventInit.mCancelable = false;
    eventInit.mRevisionId = mRevisionId;
    eventInit.mId = mId;
    eventInit.mOperation = mOperation;
    eventInit.mOwner = mOwner;

    nsRefPtr<DataStoreChangeEvent> event =
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
  nsRefPtr<DataStoreChangeEventProxy> mDataStoreChangeEventProxy;

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

  nsRefPtr<DataStoreChangeEvent> event =
    static_cast<DataStoreChangeEvent*>(aEvent);

  nsRefPtr<DispatchDataStoreChangeEventRunnable> runnable =
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
