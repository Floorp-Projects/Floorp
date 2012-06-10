/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBRequest.h"

#include "nsIJSContextStack.h"
#include "nsIScriptContext.h"

#include "nsComponentManagerUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMJSUtils.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsPIDOMWindow.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"
#include "nsWrapperCacheInlines.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBTransaction.h"
#include "DOMError.h"

USING_INDEXEDDB_NAMESPACE

IDBRequest::IDBRequest()
: mResultVal(JSVAL_VOID),
  mActorParent(nsnull),
  mErrorCode(NS_OK),
  mHaveResultOrErrorCode(false),
  mRooted(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBRequest::~IDBRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  UnrootResultVal();
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
  if (!request->SetScriptOwner(aOwnerCache->GetScriptOwner())) {
    return nsnull;
  }

  return request.forget();
}

void
IDBRequest::Reset()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mResultVal = JSVAL_VOID;
  mHaveResultOrErrorCode = false;
  mError = nsnull;
  UnrootResultVal();
}

nsresult
IDBRequest::NotifyHelperCompleted(HelperBase* aHelper)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mHaveResultOrErrorCode, "Already called!");
  NS_ASSERTION(!PreservingWrapper(), "Already rooted?!");
  NS_ASSERTION(JSVAL_IS_VOID(mResultVal), "Should be undefined!");

  // See if our window is still valid. If not then we're going to pretend that
  // we never completed.
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return NS_OK;
  }

  mHaveResultOrErrorCode = true;

  nsresult rv = aHelper->GetResultCode();

  // If the request failed then set the error code and return.
  if (NS_FAILED(rv)) {
    SetError(rv);
    return NS_OK;
  }

  // Otherwise we need to get the result from the helper.
  JSContext* cx = GetJSContext();
  if (!cx) {
    NS_WARNING("Failed to get safe JSContext!");
    rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    SetError(rv);
    return rv;
  }

  JSObject* global = GetParentObject();
  NS_ASSERTION(global, "This should never be null!");

  JSAutoRequest ar(cx);
  JSAutoEnterCompartment ac;
  if (ac.enter(cx, global)) {
    RootResultVal();

    rv = aHelper->GetSuccessResult(cx, &mResultVal);
    if (NS_FAILED(rv)) {
      NS_WARNING("GetSuccessResult failed!");
    }
  }
  else {
    NS_WARNING("Failed to enter correct compartment!");
    rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (NS_SUCCEEDED(rv)) {
    mError = nsnull;
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
  NS_ASSERTION(!PreservingWrapper(), "Already rooted?!");
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
  mError = DOMError::CreateForNSResult(aRv);
  mErrorCode = aRv;

  mResultVal = JSVAL_VOID;
  UnrootResultVal();
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
    nsIThreadJSContextStack* cxStack = nsContentUtils::ThreadJSContextStack();
    NS_ASSERTION(cxStack, "Failed to get thread context stack!");

    cx = cxStack->GetSafeJSContext();
    NS_ENSURE_TRUE(cx, nsnull);

    return cx;
  }

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, nsnull);
  NS_ENSURE_TRUE(sc, nsnull);

  cx = sc->GetNativeContext();
  NS_ASSERTION(cx, "Failed to get a context!");

  return cx;
}

void
IDBRequest::RootResultValInternal()
{
  NS_HOLD_JS_OBJECTS(this, IDBRequest);
}

void
IDBRequest::UnrootResultValInternal()
{
  NS_DROP_JS_OBJECTS(this, IDBRequest);
}

NS_IMETHODIMP
IDBRequest::GetReadyState(nsAString& aReadyState)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mHaveResultOrErrorCode) {
    aReadyState.AssignLiteral("done");
  }
  else {
    aReadyState.AssignLiteral("pending");
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

NS_IMETHODIMP
IDBRequest::GetError(nsIDOMDOMError** aError)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveResultOrErrorCode) {
    // XXX Need a real error code here.
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  NS_IF_ADDREF(*aError = mError);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBRequest, IDBWrapperCache)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS because
  // nsDOMEventTargetHelper does it for us.
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(success)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(error)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mTransaction,
                                                       nsPIDOMEventTarget)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBRequest, IDBWrapperCache)
  tmp->mResultVal = JSVAL_VOID;
  tmp->UnrootResultVal();
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(success)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(error)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTransaction)
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

NS_IMPL_EVENT_HANDLER(IDBRequest, success);
NS_IMPL_EVENT_HANDLER(IDBRequest, error);

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

  UnrootResultVal();
}

// static
already_AddRefed<IDBOpenDBRequest>
IDBOpenDBRequest::Create(nsPIDOMWindow* aOwner,
                         JSObject* aScriptOwner)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  nsRefPtr<IDBOpenDBRequest> request(new IDBOpenDBRequest());

  request->BindToOwner(aOwner);
  if (!request->SetScriptOwner(aScriptOwner)) {
    return nsnull;
  }

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

void
IDBOpenDBRequest::RootResultValInternal()
{
  NS_HOLD_JS_OBJECTS(this, IDBOpenDBRequest);
}

void
IDBOpenDBRequest::UnrootResultValInternal()
{
  NS_DROP_JS_OBJECTS(this, IDBOpenDBRequest);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBOpenDBRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBOpenDBRequest,
                                                  IDBRequest)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(upgradeneeded)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(blocked)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBOpenDBRequest,
                                                IDBRequest)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(upgradeneeded)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(blocked)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBOpenDBRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBOpenDBRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBOpenDBRequest)
NS_INTERFACE_MAP_END_INHERITING(IDBRequest)

NS_IMPL_ADDREF_INHERITED(IDBOpenDBRequest, IDBRequest)
NS_IMPL_RELEASE_INHERITED(IDBOpenDBRequest, IDBRequest)

DOMCI_DATA(IDBOpenDBRequest, IDBOpenDBRequest)

NS_IMPL_EVENT_HANDLER(IDBOpenDBRequest, blocked);
NS_IMPL_EVENT_HANDLER(IDBOpenDBRequest, upgradeneeded);

nsresult
IDBOpenDBRequest::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  NS_ENSURE_TRUE(aVisitor.mDOMEvent, NS_ERROR_UNEXPECTED);

  nsPIDOMWindow* owner = GetOwner();
  if (!owner) {
    return NS_OK;
  }

  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) {
    nsString type;
    nsresult rv = aVisitor.mDOMEvent->GetType(type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (type.EqualsLiteral(ERROR_EVT_STR)) {
      nsRefPtr<nsDOMEvent> duplicateEvent =
        CreateGenericEvent(type, eDoesNotBubble, eNotCancelable);
      NS_ENSURE_STATE(duplicateEvent);

      nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(owner));
      NS_ASSERTION(target, "How can this happen?!");

      bool dummy;
      rv = target->DispatchEvent(duplicateEvent, &dummy);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return NS_OK;
}
