/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBRequest.h"

#include "BackgroundChildImpl.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IndexedDatabaseManager.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Move.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/IDBOpenDBRequestBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIScriptContext.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "ReportInternalError.h"
#include "WorkerFeature.h"
#include "WorkerPrivate.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla {
namespace dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::workers;
using namespace mozilla::ipc;

namespace {

NS_DEFINE_IID(kIDBRequestIID, PRIVATE_IDBREQUEST_IID);

} // namespace

IDBRequest::IDBRequest(IDBDatabase* aDatabase)
  : IDBWrapperCache(aDatabase)
#ifdef DEBUG
  , mOwningThread(nullptr)
#endif
  , mLoggingSerialNumber(0)
  , mLineNo(0)
  , mColumn(0)
  , mHaveResultOrErrorCode(false)
{
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();

  InitMembers();
}

IDBRequest::IDBRequest(nsPIDOMWindowInner* aOwner)
  : IDBWrapperCache(aOwner)
#ifdef DEBUG
  , mOwningThread(nullptr)
#endif
  , mLoggingSerialNumber(0)
  , mLineNo(0)
  , mColumn(0)
  , mHaveResultOrErrorCode(false)
{
  InitMembers();
}

IDBRequest::~IDBRequest()
{
  AssertIsOnOwningThread();
}

#ifdef DEBUG

void
IDBRequest::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mOwningThread);
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
}

#endif // DEBUG

void
IDBRequest::InitMembers()
{
#ifdef DEBUG
  mOwningThread = PR_GetCurrentThread();
#endif
  AssertIsOnOwningThread();

  mResultVal.setUndefined();
  mLoggingSerialNumber = NextSerialNumber();
  mErrorCode = NS_OK;
  mLineNo = 0;
  mColumn = 0;
  mHaveResultOrErrorCode = false;
}

// static
already_AddRefed<IDBRequest>
IDBRequest::Create(JSContext* aCx,
                   IDBDatabase* aDatabase,
                   IDBTransaction* aTransaction)
{
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();

  RefPtr<IDBRequest> request = new IDBRequest(aDatabase);
  CaptureCaller(aCx, request->mFilename, &request->mLineNo, &request->mColumn);

  request->mTransaction = aTransaction;
  request->SetScriptOwner(aDatabase->GetScriptOwner());

  return request.forget();
}

// static
already_AddRefed<IDBRequest>
IDBRequest::Create(JSContext* aCx,
                   IDBObjectStore* aSourceAsObjectStore,
                   IDBDatabase* aDatabase,
                   IDBTransaction* aTransaction)
{
  MOZ_ASSERT(aSourceAsObjectStore);
  aSourceAsObjectStore->AssertIsOnOwningThread();

  RefPtr<IDBRequest> request = Create(aCx, aDatabase, aTransaction);

  request->mSourceAsObjectStore = aSourceAsObjectStore;

  return request.forget();
}

// static
already_AddRefed<IDBRequest>
IDBRequest::Create(JSContext* aCx,
                   IDBIndex* aSourceAsIndex,
                   IDBDatabase* aDatabase,
                   IDBTransaction* aTransaction)
{
  MOZ_ASSERT(aSourceAsIndex);
  aSourceAsIndex->AssertIsOnOwningThread();

  RefPtr<IDBRequest> request = Create(aCx, aDatabase, aTransaction);

  request->mSourceAsIndex = aSourceAsIndex;

  return request.forget();
}

// static
uint64_t
IDBRequest::NextSerialNumber()
{
  BackgroundChildImpl::ThreadLocal* threadLocal =
    BackgroundChildImpl::GetThreadLocalForCurrentThread();
  MOZ_ASSERT(threadLocal);

  ThreadLocal* idbThreadLocal = threadLocal->mIndexedDBThreadLocal;
  MOZ_ASSERT(idbThreadLocal);

  return idbThreadLocal->NextRequestSN();
}

void
IDBRequest::SetLoggingSerialNumber(uint64_t aLoggingSerialNumber)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLoggingSerialNumber > mLoggingSerialNumber);

  mLoggingSerialNumber = aLoggingSerialNumber;
}

void
IDBRequest::CaptureCaller(JSContext* aCx, nsAString& aFilename,
                          uint32_t* aLineNo, uint32_t* aColumn)
{
  MOZ_ASSERT(aFilename.IsEmpty());
  MOZ_ASSERT(aLineNo);
  MOZ_ASSERT(aColumn);

  nsJSUtils::GetCallingLocation(aCx, aFilename, aLineNo, aColumn);
}

void
IDBRequest::GetSource(
             Nullable<OwningIDBObjectStoreOrIDBIndexOrIDBCursor>& aSource) const
{
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

void
IDBRequest::Reset()
{
  AssertIsOnOwningThread();

  mResultVal.setUndefined();
  mHaveResultOrErrorCode = false;
  mError = nullptr;
}

void
IDBRequest::DispatchNonTransactionError(nsresult aErrorCode)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aErrorCode));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aErrorCode) == NS_ERROR_MODULE_DOM_INDEXEDDB);

  SetError(aErrorCode);

  // Make an error event and fire it at the target.
  nsCOMPtr<nsIDOMEvent> event =
    CreateGenericEvent(this,
                       nsDependentString(kErrorEventType),
                       eDoesBubble,
                       eCancelable);
  MOZ_ASSERT(event);

  bool ignored;
  if (NS_FAILED(DispatchEvent(event, &ignored))) {
    NS_WARNING("Failed to dispatch event!");
  }
}

void
IDBRequest::SetError(nsresult aRv)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aRv));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aRv) == NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT(!mError);

  mHaveResultOrErrorCode = true;
  mError = new DOMError(GetOwner(), aRv);
  mErrorCode = aRv;

  mResultVal.setUndefined();
}

#ifdef DEBUG

nsresult
IDBRequest::GetErrorCode() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mHaveResultOrErrorCode);

  return mErrorCode;
}

DOMError*
IDBRequest::GetErrorAfterResult() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mHaveResultOrErrorCode);

  return mError;
}

#endif // DEBUG

void
IDBRequest::GetCallerLocation(nsAString& aFilename, uint32_t* aLineNo,
                              uint32_t* aColumn) const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLineNo);
  MOZ_ASSERT(aColumn);

  aFilename = mFilename;
  *aLineNo = mLineNo;
  *aColumn = mColumn;
}

IDBRequestReadyState
IDBRequest::ReadyState() const
{
  AssertIsOnOwningThread();

  return IsPending() ?
    IDBRequestReadyState::Pending :
    IDBRequestReadyState::Done;
}

void
IDBRequest::SetSource(IDBCursor* aSource)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aSource);
  MOZ_ASSERT(mSourceAsObjectStore || mSourceAsIndex);
  MOZ_ASSERT(!mSourceAsCursor);

  mSourceAsCursor = aSource;
}

JSObject*
IDBRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return IDBRequestBinding::Wrap(aCx, this, aGivenProto);
}

void
IDBRequest::GetResult(JS::MutableHandle<JS::Value> aResult,
                      ErrorResult& aRv) const
{
  AssertIsOnOwningThread();

  if (!mHaveResultOrErrorCode) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  JS::ExposeValueToActiveJS(mResultVal);
  aResult.set(mResultVal);
}

void
IDBRequest::SetResultCallback(ResultCallback* aCallback)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mHaveResultOrErrorCode);
  MOZ_ASSERT(mResultVal.isUndefined());
  MOZ_ASSERT(!mError);

  // See if our window is still valid.
  if (NS_WARN_IF(NS_FAILED(CheckInnerWindowCorrectness()))) {
    SetError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }

  AutoJSAPI autoJS;
  Maybe<JSAutoCompartment> ac;

  if (GetScriptOwner()) {
    // If we have a script owner we want the SafeJSContext and then to enter the
    // script owner's compartment.
    autoJS.Init();
    ac.emplace(autoJS.cx(), GetScriptOwner());
  } else {
    // Otherwise our owner is a window and we use that to initialize.
    MOZ_ASSERT(GetOwner());
    if (!autoJS.Init(GetOwner())) {
      IDB_WARNING("Failed to initialize AutoJSAPI!");
      SetError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return;
    }
  }
  autoJS.TakeOwnershipOfErrorReporting();

  JSContext* cx = autoJS.cx();

  AssertIsRooted();

  JS::Rooted<JS::Value> result(cx);
  nsresult rv = aCallback->GetResult(cx, &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // This can only fail if the structured clone contains a mutable file
    // and the child is not in the main thread and main process.
    // In that case CreateAndWrapMutableFile() returns false which shows up
    // as NS_ERROR_DOM_DATA_CLONE_ERR here.
    MOZ_ASSERT(rv == NS_ERROR_DOM_DATA_CLONE_ERR);

    // We are not setting a result or an error object here since we want to
    // throw an exception when the 'result' property is being touched.
    return;
  }

  mError = nullptr;
  mResultVal = result;

  mHaveResultOrErrorCode = true;
}

DOMError*
IDBRequest::GetError(ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (!mHaveResultOrErrorCode) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return mError;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBRequest, IDBWrapperCache)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS because
  // DOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceAsObjectStore)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceAsIndex)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceAsCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBRequest, IDBWrapperCache)
  tmp->mResultVal.setUndefined();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceAsObjectStore)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceAsIndex)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceAsCursor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransaction)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(IDBRequest, IDBWrapperCache)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // DOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResultVal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBRequest)
  if (aIID.Equals(kIDBRequestIID)) {
    foundInterface = this;
  } else
NS_INTERFACE_MAP_END_INHERITING(IDBWrapperCache)

NS_IMPL_ADDREF_INHERITED(IDBRequest, IDBWrapperCache)
NS_IMPL_RELEASE_INHERITED(IDBRequest, IDBWrapperCache)

nsresult
IDBRequest::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  AssertIsOnOwningThread();

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mTransaction;
  return NS_OK;
}

class IDBOpenDBRequest::WorkerFeature final
  : public mozilla::dom::workers::WorkerFeature
{
  WorkerPrivate* mWorkerPrivate;
#ifdef DEBUG
  // This is only here so that assertions work in the destructor even if
  // NoteAddFeatureFailed was called.
  WorkerPrivate* mWorkerPrivateDEBUG;
#endif

public:
  explicit
  WorkerFeature(WorkerPrivate* aWorkerPrivate)
    : mWorkerPrivate(aWorkerPrivate)
#ifdef DEBUG
    , mWorkerPrivateDEBUG(aWorkerPrivate)
#endif
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    MOZ_COUNT_CTOR(IDBOpenDBRequest::WorkerFeature);
  }

  ~WorkerFeature()
  {
#ifdef DEBUG
    mWorkerPrivateDEBUG->AssertIsOnWorkerThread();
#endif

    MOZ_COUNT_DTOR(IDBOpenDBRequest::WorkerFeature);

    if (mWorkerPrivate) {
      mWorkerPrivate->RemoveFeature(this);
    }
  }

  void
  NoteAddFeatureFailed()
  {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    mWorkerPrivate = nullptr;
  }

private:
  virtual bool
  Notify(JSContext* aCx, Status aStatus) override;
};

IDBOpenDBRequest::IDBOpenDBRequest(IDBFactory* aFactory,
                                   nsPIDOMWindowInner* aOwner,
                                   bool aFileHandleDisabled)
  : IDBRequest(aOwner)
  , mFactory(aFactory)
  , mFileHandleDisabled(aFileHandleDisabled)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFactory);

  // aOwner may be null.
}

IDBOpenDBRequest::~IDBOpenDBRequest()
{
  AssertIsOnOwningThread();
}

// static
already_AddRefed<IDBOpenDBRequest>
IDBOpenDBRequest::CreateForWindow(JSContext* aCx,
                                  IDBFactory* aFactory,
                                  nsPIDOMWindowInner* aOwner,
                                  JS::Handle<JSObject*> aScriptOwner)
{
  MOZ_ASSERT(aFactory);
  aFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(aScriptOwner);

  bool fileHandleDisabled = !IndexedDatabaseManager::IsFileHandleEnabled();

  RefPtr<IDBOpenDBRequest> request =
    new IDBOpenDBRequest(aFactory, aOwner, fileHandleDisabled);
  CaptureCaller(aCx, request->mFilename, &request->mLineNo, &request->mColumn);

  request->SetScriptOwner(aScriptOwner);

  return request.forget();
}

// static
already_AddRefed<IDBOpenDBRequest>
IDBOpenDBRequest::CreateForJS(JSContext* aCx,
                              IDBFactory* aFactory,
                              JS::Handle<JSObject*> aScriptOwner)
{
  MOZ_ASSERT(aFactory);
  aFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aScriptOwner);

  bool fileHandleDisabled = !IndexedDatabaseManager::IsFileHandleEnabled();

  RefPtr<IDBOpenDBRequest> request =
    new IDBOpenDBRequest(aFactory, nullptr, fileHandleDisabled);
  CaptureCaller(aCx, request->mFilename, &request->mLineNo, &request->mColumn);

  request->SetScriptOwner(aScriptOwner);

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    workerPrivate->AssertIsOnWorkerThread();

    nsAutoPtr<WorkerFeature> feature(new WorkerFeature(workerPrivate));
    if (NS_WARN_IF(!workerPrivate->AddFeature(feature))) {
      feature->NoteAddFeatureFailed();
      return nullptr;
    }

    request->mWorkerFeature = Move(feature);
  }

  return request.forget();
}

void
IDBOpenDBRequest::SetTransaction(IDBTransaction* aTransaction)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(!aTransaction || !mTransaction);

  mTransaction = aTransaction;
}

void
IDBOpenDBRequest::NoteComplete()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerFeature);

  // If we have a WorkerFeature installed on the worker then nulling this out
  // will uninstall it from the worker.
  mWorkerFeature = nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBOpenDBRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBOpenDBRequest,
                                                  IDBRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFactory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBOpenDBRequest,
                                                IDBRequest)
  // Don't unlink mFactory!
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBOpenDBRequest)
NS_INTERFACE_MAP_END_INHERITING(IDBRequest)

NS_IMPL_ADDREF_INHERITED(IDBOpenDBRequest, IDBRequest)
NS_IMPL_RELEASE_INHERITED(IDBOpenDBRequest, IDBRequest)

nsresult
IDBOpenDBRequest::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  nsresult rv =
    IndexedDatabaseManager::CommonPostHandleEvent(aVisitor, mFactory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

JSObject*
IDBOpenDBRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnOwningThread();

  return IDBOpenDBRequestBinding::Wrap(aCx, this, aGivenProto);
}

bool
IDBOpenDBRequest::
WorkerFeature::Notify(JSContext* aCx, Status aStatus)
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aStatus > Running);

  // There's nothing we can really do here at the moment...
  NS_WARNING("Worker closing but IndexedDB is waiting to open a database!");

  return true;
}

} // namespace dom
} // namespace mozilla
