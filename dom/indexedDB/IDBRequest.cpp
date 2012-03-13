/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com>
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

USING_INDEXEDDB_NAMESPACE

IDBRequest::IDBRequest()
: mResultVal(JSVAL_VOID),
  mErrorCode(0),
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
  mErrorCode = 0;
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
    mErrorCode = NS_ERROR_GET_CODE(rv);
    return NS_OK;
  }

  // Otherwise we need to get the result from the helper.
  JSContext* cx;
  if (GetScriptOwner()) {
    nsIThreadJSContextStack* cxStack = nsContentUtils::ThreadJSContextStack();
    NS_ASSERTION(cxStack, "Failed to get thread context stack!");

    if (NS_FAILED(cxStack->GetSafeJSContext(&cx))) {
      NS_WARNING("Failed to get safe JSContext!");
      rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      mErrorCode = NS_ERROR_GET_CODE(rv);
      return rv;
    }
  }
  else {
    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    NS_ENSURE_STATE(sc);
    cx = sc->GetNativeContext();
    NS_ASSERTION(cx, "Failed to get a context!");
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
    mErrorCode = 0;
  }
  else {
    mErrorCode = NS_ERROR_GET_CODE(rv);
    mResultVal = JSVAL_VOID;
  }

  return rv;
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
IDBRequest::GetErrorCode(PRUint16* aErrorCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveResultOrErrorCode) {
    // XXX Need a real error code here.
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  *aErrorCode = mErrorCode;
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
  if (JSVAL_IS_GCTHING(tmp->mResultVal)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mResultVal);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing, "mResultVal")
  }
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
