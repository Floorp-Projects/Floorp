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

#include "nsIScriptContext.h"
#include "nsIVariant.h"

#include "nsComponentManagerUtils.h"
#include "nsDOMClassInfo.h"
#include "nsDOMJSUtils.h"
#include "nsEventDispatcher.h"
#include "nsPIDOMWindow.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBTransaction.h"

USING_INDEXEDDB_NAMESPACE

IDBRequest::IDBRequest()
: mResultVal(JSVAL_VOID),
  mErrorCode(0),
  mResultValRooted(false),
  mHaveResultOrErrorCode(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBRequest::~IDBRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mResultValRooted) {
    // Calling a virtual from the destructor is bad... But we know that we won't
    // call a subclass' implementation because mResultValRooted will be set to
    // false.
    UnrootResultVal();
  }

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

// static
already_AddRefed<IDBRequest>
IDBRequest::Create(nsISupports* aSource,
                   nsIScriptContext* aScriptContext,
                   nsPIDOMWindow* aOwner,
                   IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!aScriptContext || !aOwner) {
    NS_ERROR("Null context and owner!");
    return nsnull;
  }

  nsRefPtr<IDBRequest> request(new IDBRequest());

  request->mSource = aSource;
  request->mTransaction = aTransaction;
  request->mScriptContext = aScriptContext;
  request->mOwner = aOwner;

  return request.forget();
}

void
IDBRequest::Reset()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mHelper = nsnull;
  mResultVal = JSVAL_VOID;
  mHaveResultOrErrorCode = false;
  mErrorCode = 0;
  if (mResultValRooted) {
    UnrootResultVal();
  }
}

void
IDBRequest::SetDone(AsyncConnectionHelper* aHelper)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mHelper, "Already called!");

  mErrorCode = NS_ERROR_GET_CODE(aHelper->GetResultCode());
  if (mErrorCode) {
    mHaveResultOrErrorCode = true;
  }
  else {
    mHelper = aHelper;
  }
}

void
IDBRequest::RootResultVal()
{
  NS_ASSERTION(!mResultValRooted, "This should be false!");
  NS_HOLD_JS_OBJECTS(this, IDBRequest);
  mResultValRooted = true;
}

void
IDBRequest::UnrootResultVal()
{
  NS_ASSERTION(mResultValRooted, "This should be true!");
  NS_DROP_JS_OBJECTS(this, IDBRequest);
  mResultValRooted = false;
}

NS_IMETHODIMP
IDBRequest::GetReadyState(PRUint16* aReadyState)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mHaveResultOrErrorCode || mHelper) {
    *aReadyState = nsIIDBRequest::DONE;
  }
  else {
    *aReadyState = nsIIDBRequest::LOADING;
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
IDBRequest::GetResult(JSContext* aCx,
                      jsval* aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = NS_OK;

  if (!mHaveResultOrErrorCode) {
    if (!mHelper) {
      // XXX Need a real error code here.
      return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
    }

    NS_ASSERTION(!mResultValRooted, "Huh?!");
    NS_ASSERTION(JSVAL_IS_VOID(mResultVal), "Should be undefined!");

    if (NS_SUCCEEDED(mHelper->GetResultCode())) {
      // It's common practice for result values to be rooted before being set.
      // Root now, even though we may unroot below, to make mResultVal safe from
      // GC.
      RootResultVal();

      rv = mHelper->GetSuccessResult(aCx, &mResultVal);
      if (NS_FAILED(rv)) {
        mResultVal = JSVAL_VOID;
      }

      // There's no point in rooting non-GCThings. Unroot if possible.
      if (!JSVAL_IS_GCTHING(mResultVal)) {
        UnrootResultVal();
      }
    }

    mHaveResultOrErrorCode = true;
    mHelper = nsnull;
  }

  *aResult = mResultVal;
  return rv;
}

NS_IMETHODIMP
IDBRequest::GetErrorCode(PRUint16* aErrorCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveResultOrErrorCode && !mHelper) {
    // XXX Need a real error code here.
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  *aErrorCode = mErrorCode;
  return NS_OK;
}

NS_IMETHODIMP
IDBRequest::SetOnsuccess(nsIDOMEventListener* aSuccessListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return RemoveAddEventListener(NS_LITERAL_STRING(SUCCESS_EVT_STR),
                                mOnSuccessListener, aSuccessListener);
}

NS_IMETHODIMP
IDBRequest::GetOnsuccess(nsIDOMEventListener** aSuccessListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return GetInnerEventListener(mOnSuccessListener, aSuccessListener);
}

NS_IMETHODIMP
IDBRequest::SetOnerror(nsIDOMEventListener* aErrorListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return RemoveAddEventListener(NS_LITERAL_STRING(ERROR_EVT_STR),
                                mOnErrorListener, aErrorListener);
}

NS_IMETHODIMP
IDBRequest::GetOnerror(nsIDOMEventListener** aErrorListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return GetInnerEventListener(mOnErrorListener, aErrorListener);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBRequest,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnSuccessListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSource)

  // mHelper is a threadsafe runnable and can't use a cycle-collecting refcnt.
  // We traverse manually here.
  if (tmp->mHelper) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mHelper->mDatabase,
                                                         nsPIDOMEventTarget)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mHelper->mTransaction,
                                                         nsPIDOMEventTarget)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mHelper->mRequest,
                                                         nsPIDOMEventTarget)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBRequest,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnSuccessListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSource)

  // Unlinking mHelper will unlink all the objects that we really care about.
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBRequest)
  if (JSVAL_IS_GCTHING(tmp->mResultVal)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mResultVal);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing)
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_BEGIN(IDBRequest)
  if (tmp->mResultValRooted) {
    tmp->mResultVal = JSVAL_VOID;
    tmp->UnrootResultVal();
  }
NS_IMPL_CYCLE_COLLECTION_ROOT_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBRequest)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBRequest, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBRequest, nsDOMEventTargetHelper)

DOMCI_DATA(IDBRequest, IDBRequest)

nsresult
IDBRequest::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aVisitor.mCanHandle = PR_TRUE;
  aVisitor.mParentTarget = mTransaction;
  return NS_OK;
}

IDBVersionChangeRequest::~IDBVersionChangeRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mResultValRooted) {
    UnrootResultVal();
  }
}

// static
already_AddRefed<IDBVersionChangeRequest>
IDBVersionChangeRequest::Create(nsISupports* aSource,
                                nsIScriptContext* aScriptContext,
                                nsPIDOMWindow* aOwner,
                                IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!aScriptContext || !aOwner) {
    NS_ERROR("Null context and owner!");
    return nsnull;
  }

  nsRefPtr<IDBVersionChangeRequest> request(new IDBVersionChangeRequest());

  request->mSource = aSource;
  request->mTransaction = aTransaction;
  request->mScriptContext = aScriptContext;
  request->mOwner = aOwner;

  return request.forget();
}

void
IDBVersionChangeRequest::RootResultVal()
{
  NS_ASSERTION(!mResultValRooted, "This should be false!");
  NS_HOLD_JS_OBJECTS(this, IDBVersionChangeRequest);
  mResultValRooted = true;
}

void
IDBVersionChangeRequest::UnrootResultVal()
{
  NS_ASSERTION(mResultValRooted, "This should be true!");
  NS_DROP_JS_OBJECTS(this, IDBVersionChangeRequest);
  mResultValRooted = false;
}

NS_IMETHODIMP
IDBVersionChangeRequest::SetOnblocked(nsIDOMEventListener* aBlockedListener)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(BLOCKED_EVT_STR),
                                mOnBlockedListener, aBlockedListener);
}

NS_IMETHODIMP
IDBVersionChangeRequest::GetOnblocked(nsIDOMEventListener** aBlockedListener)
{
  return GetInnerEventListener(mOnBlockedListener, aBlockedListener);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBVersionChangeRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBVersionChangeRequest,
                                                  IDBRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnBlockedListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBVersionChangeRequest,
                                                IDBRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnBlockedListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBVersionChangeRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBVersionChangeRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBVersionChangeRequest)
NS_INTERFACE_MAP_END_INHERITING(IDBRequest)

NS_IMPL_ADDREF_INHERITED(IDBVersionChangeRequest, IDBRequest)
NS_IMPL_RELEASE_INHERITED(IDBVersionChangeRequest, IDBRequest)

DOMCI_DATA(IDBVersionChangeRequest, IDBVersionChangeRequest)
