/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBRequest.h"

#include <utility>

#include "BackgroundChildImpl.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IndexedDatabaseManager.h"
#include "ReportInternalError.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/IDBOpenDBRequestBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsJSUtils.h"
#include "nsString.h"
#include "ThreadLocal.h"

namespace mozilla::dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::ipc;

IDBRequest::IDBRequest(IDBDatabase* aDatabase)
    : DOMEventTargetHelper(aDatabase),
      mLoggingSerialNumber(0),
      mLineNo(0),
      mColumn(0),
      mHaveResultOrErrorCode(false) {
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();

  InitMembers();
}

IDBRequest::IDBRequest(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal),
      mLoggingSerialNumber(0),
      mLineNo(0),
      mColumn(0),
      mHaveResultOrErrorCode(false) {
  InitMembers();
}

IDBRequest::~IDBRequest() {
  AssertIsOnOwningThread();
  mozilla::DropJSObjects(this);
}

void IDBRequest::InitMembers() {
  AssertIsOnOwningThread();

  mResultVal.setUndefined();
  mLoggingSerialNumber = NextSerialNumber();
  mErrorCode = NS_OK;
  mLineNo = 0;
  mColumn = 0;
  mHaveResultOrErrorCode = false;
}

// static
MovingNotNull<RefPtr<IDBRequest>> IDBRequest::Create(
    JSContext* aCx, IDBDatabase* aDatabase,
    SafeRefPtr<IDBTransaction> aTransaction) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();

  RefPtr<IDBRequest> request = new IDBRequest(aDatabase);
  CaptureCaller(aCx, request->mFilename, &request->mLineNo, &request->mColumn);

  request->mTransaction = std::move(aTransaction);

  return WrapMovingNotNullUnchecked(std::move(request));
}

// static
MovingNotNull<RefPtr<IDBRequest>> IDBRequest::Create(
    JSContext* aCx, IDBObjectStore* aSourceAsObjectStore,
    IDBDatabase* aDatabase, SafeRefPtr<IDBTransaction> aTransaction) {
  MOZ_ASSERT(aSourceAsObjectStore);
  aSourceAsObjectStore->AssertIsOnOwningThread();

  auto request =
      Create(aCx, aDatabase, std::move(aTransaction)).unwrapBasePtr();

  request->mSourceAsObjectStore = aSourceAsObjectStore;

  return WrapMovingNotNullUnchecked(std::move(request));
}

// static
MovingNotNull<RefPtr<IDBRequest>> IDBRequest::Create(
    JSContext* aCx, IDBIndex* aSourceAsIndex, IDBDatabase* aDatabase,
    SafeRefPtr<IDBTransaction> aTransaction) {
  MOZ_ASSERT(aSourceAsIndex);
  aSourceAsIndex->AssertIsOnOwningThread();

  auto request =
      Create(aCx, aDatabase, std::move(aTransaction)).unwrapBasePtr();

  request->mSourceAsIndex = aSourceAsIndex;

  return WrapMovingNotNullUnchecked(std::move(request));
}

// static
uint64_t IDBRequest::NextSerialNumber() {
  BackgroundChildImpl::ThreadLocal* threadLocal =
      BackgroundChildImpl::GetThreadLocalForCurrentThread();
  MOZ_ASSERT(threadLocal);

  const auto& idbThreadLocal = threadLocal->mIndexedDBThreadLocal;
  MOZ_ASSERT(idbThreadLocal);

  return idbThreadLocal->NextRequestSN();
}

void IDBRequest::SetLoggingSerialNumber(uint64_t aLoggingSerialNumber) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLoggingSerialNumber > mLoggingSerialNumber);

  mLoggingSerialNumber = aLoggingSerialNumber;
}

void IDBRequest::CaptureCaller(JSContext* aCx, nsAString& aFilename,
                               uint32_t* aLineNo, uint32_t* aColumn) {
  MOZ_ASSERT(aFilename.IsEmpty());
  MOZ_ASSERT(aLineNo);
  MOZ_ASSERT(aColumn);

  nsJSUtils::GetCallingLocation(aCx, aFilename, aLineNo, aColumn);
}

void IDBRequest::GetSource(
    Nullable<OwningIDBObjectStoreOrIDBIndexOrIDBCursor>& aSource) const {
  AssertIsOnOwningThread();

  MOZ_ASSERT_IF(mSourceAsObjectStore, !mSourceAsIndex);
  MOZ_ASSERT_IF(mSourceAsIndex, !mSourceAsObjectStore);
  MOZ_ASSERT_IF(mSourceAsCursor, mSourceAsObjectStore || mSourceAsIndex);

  // Always check cursor first since cursor requests hold both the cursor and
  // the objectStore or index the cursor came from.
  if (mSourceAsCursor) {
    aSource.SetValue().SetAsIDBCursor() = mSourceAsCursor;
  } else if (mSourceAsObjectStore) {
    aSource.SetValue().SetAsIDBObjectStore() = mSourceAsObjectStore;
  } else if (mSourceAsIndex) {
    aSource.SetValue().SetAsIDBIndex() = mSourceAsIndex;
  } else {
    aSource.SetNull();
  }
}

void IDBRequest::Reset() {
  AssertIsOnOwningThread();

  mResultVal.setUndefined();

  mHaveResultOrErrorCode = false;
  mError = nullptr;
}

void IDBRequest::SetError(nsresult aRv) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aRv));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aRv) == NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT(!mError);

  mHaveResultOrErrorCode = true;
  mError = DOMException::Create(aRv);
  mErrorCode = aRv;

  mResultVal.setUndefined();
}

#ifdef DEBUG

nsresult IDBRequest::GetErrorCode() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mHaveResultOrErrorCode);

  return mErrorCode;
}

DOMException* IDBRequest::GetErrorAfterResult() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mHaveResultOrErrorCode);

  return mError;
}

#endif  // DEBUG

void IDBRequest::GetCallerLocation(nsAString& aFilename, uint32_t* aLineNo,
                                   uint32_t* aColumn) const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLineNo);
  MOZ_ASSERT(aColumn);

  aFilename = mFilename;
  *aLineNo = mLineNo;
  *aColumn = mColumn;
}

IDBRequestReadyState IDBRequest::ReadyState() const {
  AssertIsOnOwningThread();

  return IsPending() ? IDBRequestReadyState::Pending
                     : IDBRequestReadyState::Done;
}

void IDBRequest::SetSource(IDBCursor* aSource) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aSource);
  MOZ_ASSERT(mSourceAsObjectStore || mSourceAsIndex);
  MOZ_ASSERT(!mSourceAsCursor);

  mSourceAsCursor = aSource;
}

JSObject* IDBRequest::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return IDBRequest_Binding::Wrap(aCx, this, aGivenProto);
}

void IDBRequest::GetResult(JS::MutableHandle<JS::Value> aResult,
                           ErrorResult& aRv) const {
  AssertIsOnOwningThread();

  if (!mHaveResultOrErrorCode) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  aResult.set(mResultVal);
}

DOMException* IDBRequest::GetError(ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!mHaveResultOrErrorCode) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return mError;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBRequest,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceAsObjectStore)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceAsIndex)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceAsCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBRequest,
                                                DOMEventTargetHelper)
  mozilla::DropJSObjects(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceAsObjectStore)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceAsIndex)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceAsCursor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransaction)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(IDBRequest, DOMEventTargetHelper)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // DOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResultVal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBRequest)
  if (aIID.Equals(NS_GET_IID(mozilla::dom::detail::PrivateIDBRequest))) {
    foundInterface = static_cast<EventTarget*>(this);
  } else
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBRequest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBRequest, DOMEventTargetHelper)

void IDBRequest::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  AssertIsOnOwningThread();

  aVisitor.mCanHandle = true;
  aVisitor.SetParentTarget(mTransaction.unsafeGetRawPtr(), false);
}

IDBOpenDBRequest::IDBOpenDBRequest(SafeRefPtr<IDBFactory> aFactory,
                                   nsIGlobalObject* aGlobal)
    : IDBRequest(aGlobal),
      mFactory(std::move(aFactory)),
      mIncreasedActiveDatabaseCount(false) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFactory);
  MOZ_ASSERT(aGlobal);
}

IDBOpenDBRequest::~IDBOpenDBRequest() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mIncreasedActiveDatabaseCount);
}

// static
RefPtr<IDBOpenDBRequest> IDBOpenDBRequest::Create(
    JSContext* aCx, SafeRefPtr<IDBFactory> aFactory, nsIGlobalObject* aGlobal) {
  MOZ_ASSERT(aFactory);
  aFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aGlobal);

  RefPtr<IDBOpenDBRequest> request =
      new IDBOpenDBRequest(std::move(aFactory), aGlobal);
  CaptureCaller(aCx, request->mFilename, &request->mLineNo, &request->mColumn);

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    workerPrivate->AssertIsOnWorkerThread();

    request->mWorkerRef =
        StrongWorkerRef::Create(workerPrivate, "IDBOpenDBRequest");
    if (NS_WARN_IF(!request->mWorkerRef)) {
      return nullptr;
    }
  }

  request->IncreaseActiveDatabaseCount();

  return request;
}

void IDBOpenDBRequest::SetTransaction(SafeRefPtr<IDBTransaction> aTransaction) {
  AssertIsOnOwningThread();

  MOZ_ASSERT(!aTransaction || !mTransaction);

  mTransaction = std::move(aTransaction);
}

void IDBOpenDBRequest::DispatchNonTransactionError(nsresult aErrorCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aErrorCode));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aErrorCode) == NS_ERROR_MODULE_DOM_INDEXEDDB);

  // The actor failed to initiate, decrease the number of active IDBOpenRequests
  // here since NoteComplete won't be called.
  MaybeDecreaseActiveDatabaseCount();

  SetError(aErrorCode);

  // Make an error event and fire it at the target.
  auto event = CreateGenericEvent(this, nsDependentString(kErrorEventType),
                                  eDoesBubble, eCancelable);

  IgnoredErrorResult rv;
  DispatchEvent(*event, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to dispatch event!");
  }
}

void IDBOpenDBRequest::NoteComplete() {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerRef);

  // Normally, we decrease the number of active IDBOpenRequests here.
  MaybeDecreaseActiveDatabaseCount();

  // If we have a WorkerRef, then nulling this out will release the worker.
  mWorkerRef = nullptr;
}

void IDBOpenDBRequest::IncreaseActiveDatabaseCount() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mIncreasedActiveDatabaseCount);

  // Increase the number of active IDBOpenRequests.
  // Note: We count here instead of the actor's ctor because the preemption
  // could happen at next JS interrupt but its BackgroundFactoryRequestChild
  // could be created asynchronously from IDBFactory::BackgroundCreateCallback
  // ::ActorCreated() if its PBackgroundChild is not created yet on this thread.
  mFactory->UpdateActiveDatabaseCount(1);
  mIncreasedActiveDatabaseCount = true;
}

void IDBOpenDBRequest::MaybeDecreaseActiveDatabaseCount() {
  AssertIsOnOwningThread();

  if (mIncreasedActiveDatabaseCount) {
    // Decrease the number of active IDBOpenRequests.
    mFactory->UpdateActiveDatabaseCount(-1);
    mIncreasedActiveDatabaseCount = false;
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBOpenDBRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBOpenDBRequest, IDBRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFactory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBOpenDBRequest, IDBRequest)
  // Don't unlink mFactory!
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBOpenDBRequest)
NS_INTERFACE_MAP_END_INHERITING(IDBRequest)

NS_IMPL_ADDREF_INHERITED(IDBOpenDBRequest, IDBRequest)
NS_IMPL_RELEASE_INHERITED(IDBOpenDBRequest, IDBRequest)

JSObject* IDBOpenDBRequest::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  return IDBOpenDBRequest_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
