/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBRequest.h"

#include "nsIScriptContext.h"

#include "nsComponentManagerUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMJSUtils.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsEventDispatcher.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"
#include "nsWrapperCacheInlines.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBTransaction.h"

namespace {

#ifdef MOZ_ENABLE_PROFILER_SPS
uint64_t gNextSerialNumber = 1;
#endif

} // anonymous namespace

USING_INDEXEDDB_NAMESPACE

IDBRequest::IDBRequest()
: mResultVal(JSVAL_VOID),
  mActorParent(nullptr),
#ifdef MOZ_ENABLE_PROFILER_SPS
  mSerialNumber(gNextSerialNumber++),
#endif
  mErrorCode(NS_OK),
  mLineNo(0),
  mHaveResultOrErrorCode(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBRequest::~IDBRequest()
{
  mResultVal = JSVAL_VOID;
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

// static
already_AddRefed<IDBRequest>
IDBRequest::Create(nsISupports* aSource,
                   IDBWrapperCache* aOwnerCache,
                   IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  nsRefPtr<IDBRequest> request(new IDBRequest());

  request->mSource = aSource;
  request->mTransaction = aTransaction;
  request->BindToOwner(aOwnerCache);
  request->SetScriptOwner(aOwnerCache->GetScriptOwner());
  request->CaptureCaller();

  return request.forget();
}

void
IDBRequest::Reset()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mResultVal = JSVAL_VOID;
  mHaveResultOrErrorCode = false;
  mError = nullptr;
}

nsresult
IDBRequest::NotifyHelperCompleted(HelperBase* aHelper)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mHaveResultOrErrorCode, "Already called!");
  NS_ASSERTION(JSVAL_IS_VOID(mResultVal), "Should be undefined!");

  mHaveResultOrErrorCode = true;

  nsresult rv = aHelper->GetResultCode();

  // If the request failed then set the error code and return.
  if (NS_FAILED(rv)) {
    SetError(rv);
    return NS_OK;
  }

  // See if our window is still valid. If not then we're going to pretend that
  // we never completed.
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return NS_OK;
  }

  // Otherwise we need to get the result from the helper.
  AutoPushJSContext cx(GetJSContext());
  if (!cx) {
    NS_WARNING("Failed to get safe JSContext!");
    rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    SetError(rv);
    return rv;
  }

  JS::Rooted<JSObject*> global(cx, GetParentObject());
  NS_ASSERTION(global, "This should never be null!");

  JSAutoCompartment ac(cx, global);
  AssertIsRooted();

  JS::Rooted<JS::Value> value(cx);
  rv = aHelper->GetSuccessResult(cx, &value);
  if (NS_FAILED(rv)) {
    NS_WARNING("GetSuccessResult failed!");
  }

  if (NS_SUCCEEDED(rv)) {
    mError = nullptr;
    mResultVal = value;
  }
  else {
    SetError(rv);
    mResultVal = JSVAL_VOID;
  }

  return rv;
}

void
IDBRequest::NotifyHelperSentResultsToChildProcess(nsresult aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mHaveResultOrErrorCode, "Already called!");
  NS_ASSERTION(JSVAL_IS_VOID(mResultVal), "Should be undefined!");

  // See if our window is still valid. If not then we're going to pretend that
  // we never completed.
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  mHaveResultOrErrorCode = true;

  if (NS_FAILED(aRv)) {
    SetError(aRv);
  }
}

void
IDBRequest::SetError(nsresult aRv)
{
  NS_ASSERTION(NS_FAILED(aRv), "Er, what?");
  NS_ASSERTION(!mError, "Already have an error?");

  mHaveResultOrErrorCode = true;
  mError = new mozilla::dom::DOMError(GetOwner(), aRv);
  mErrorCode = aRv;

  mResultVal = JSVAL_VOID;
}

#ifdef DEBUG
nsresult
IDBRequest::GetErrorCode() const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mHaveResultOrErrorCode, "Don't call me yet!");
  return mErrorCode;
}
#endif

JSContext*
IDBRequest::GetJSContext()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  JSContext* cx;

  if (GetScriptOwner()) {
    return nsContentUtils::GetSafeJSContext();
  }

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, nullptr);
  NS_ENSURE_TRUE(sc, nullptr);

  cx = sc->GetNativeContext();
  NS_ASSERTION(cx, "Failed to get a context!");

  return cx;
}

void
IDBRequest::CaptureCaller()
{
  AutoJSContext cx;

  const char* filename = nullptr;
  uint32_t lineNo = 0;
  if (!nsJSUtils::GetCallingLocation(cx, &filename, &lineNo)) {
    // If our caller is in another process, we won't have a JSContext on the
    // stack, and AutoJSContext will push the SafeJSContext. But that won't have
    // any script on it (certainly not after the push), so GetCallingLocation
    // will fail when it calls JS_DescribeScriptedCaller. That's fine.
    NS_WARNING("Failed to get caller.");
    return;
  }

  mFilename.Assign(NS_ConvertUTF8toUTF16(filename));
  mLineNo = lineNo;
}

void
IDBRequest::FillScriptErrorEvent(nsScriptErrorEvent* aEvent) const
{
  aEvent->lineNr = mLineNo;
  aEvent->fileName = mFilename.get();
}

NS_IMETHODIMP
IDBRequest::GetReadyState(nsAString& aReadyState)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsPending()) {
    aReadyState.AssignLiteral("pending");
  }
  else {
    aReadyState.AssignLiteral("done");
  }

  return NS_OK;
}

NS_IMETHODIMP
IDBRequest::GetSource(nsISupports** aSource)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsISupports> source(mSource);
  source.forget(aSource);
  return NS_OK;
}

NS_IMETHODIMP
IDBRequest::GetTransaction(nsIIDBTransaction** aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIIDBTransaction> transaction(mTransaction);
  transaction.forget(aTransaction);
  return NS_OK;
}

NS_IMETHODIMP
IDBRequest::GetResult(jsval* aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveResultOrErrorCode) {
    // XXX Need a real error code here.
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  *aResult = mResultVal;
  return NS_OK;
}

mozilla::dom::DOMError*
IDBRequest::GetError(mozilla::ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveResultOrErrorCode) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return mError;
}

NS_IMETHODIMP
IDBRequest::GetError(nsISupports** aError)
{
  ErrorResult rv;
  *aError = GetError(rv);
  NS_IF_ADDREF(*aError);
  return rv.ErrorCode();
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBRequest, IDBWrapperCache)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS because
  // nsDOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBRequest, IDBWrapperCache)
  tmp->mResultVal = JSVAL_VOID;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransaction)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(IDBRequest, IDBWrapperCache)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // nsDOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResultVal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBRequest)
NS_INTERFACE_MAP_END_INHERITING(IDBWrapperCache)

NS_IMPL_ADDREF_INHERITED(IDBRequest, IDBWrapperCache)
NS_IMPL_RELEASE_INHERITED(IDBRequest, IDBWrapperCache)

DOMCI_DATA(IDBRequest, IDBRequest)

NS_IMPL_EVENT_HANDLER(IDBRequest, success)
NS_IMPL_EVENT_HANDLER(IDBRequest, error)

nsresult
IDBRequest::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mTransaction;
  return NS_OK;
}

IDBOpenDBRequest::~IDBOpenDBRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

// static
already_AddRefed<IDBOpenDBRequest>
IDBOpenDBRequest::Create(IDBFactory* aFactory,
                         nsPIDOMWindow* aOwner,
                         JS::Handle<JSObject*> aScriptOwner)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aFactory, "Null pointer!");

  nsRefPtr<IDBOpenDBRequest> request = new IDBOpenDBRequest();

  request->BindToOwner(aOwner);
  request->SetScriptOwner(aScriptOwner);
  request->CaptureCaller();
  request->mFactory = aFactory;

  return request.forget();
}

void
IDBOpenDBRequest::SetTransaction(IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ASSERTION(!aTransaction || !mTransaction,
               "Shouldn't have a transaction here!");

  mTransaction = aTransaction;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBOpenDBRequest,
                                                  IDBRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFactory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBOpenDBRequest,
                                                IDBRequest)
  // Don't unlink mFactory!
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBOpenDBRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBOpenDBRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBOpenDBRequest)
NS_INTERFACE_MAP_END_INHERITING(IDBRequest)

NS_IMPL_ADDREF_INHERITED(IDBOpenDBRequest, IDBRequest)
NS_IMPL_RELEASE_INHERITED(IDBOpenDBRequest, IDBRequest)

DOMCI_DATA(IDBOpenDBRequest, IDBOpenDBRequest)

NS_IMPL_EVENT_HANDLER(IDBOpenDBRequest, blocked)
NS_IMPL_EVENT_HANDLER(IDBOpenDBRequest, upgradeneeded)

nsresult
IDBOpenDBRequest::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return IndexedDatabaseManager::FireWindowOnError(GetOwner(), aVisitor);
}
