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

#include "IDBCursorRequest.h"

#include "nsIIDBDatabaseException.h"
#include "nsIVariant.h"

#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSON.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBObjectStoreRequest.h"
#include "IDBTransactionRequest.h"

USING_INDEXEDDB_NAMESPACE

BEGIN_INDEXEDDB_NAMESPACE

class ContinueRunnable : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  ContinueRunnable(IDBCursorRequest* aCursor,
                   const Key& aKey)
  : mCursor(aCursor), mKey(aKey)
  { }

private:
  nsRefPtr<IDBCursorRequest> mCursor;
  const Key mKey;
};

END_INDEXEDDB_NAMESPACE

// static
already_AddRefed<IDBCursorRequest>
IDBCursorRequest::Create(IDBRequest* aRequest,
                         IDBTransactionRequest* aTransaction,
                         IDBObjectStoreRequest* aObjectStore,
                         PRUint16 aDirection,
                         nsTArray<KeyValuePair>& aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRequest, "Null pointer!");
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aObjectStore, "Null pointer!");

  nsRefPtr<IDBCursorRequest> cursor(new IDBCursorRequest());
  cursor->mRequest = aRequest;
  cursor->mTransaction = aTransaction;
  cursor->mObjectStore = aObjectStore;
  cursor->mDirection = aDirection;

  if (!cursor->mData.SwapElements(aData)) {
    NS_ERROR("Out of memory?!");
    return nsnull;
  }
  NS_ASSERTION(!cursor->mData.IsEmpty(), "Should ever have an empty set!");

  cursor->mDataIndex = cursor->mData.Length() - 1;

  return cursor.forget();
}

IDBCursorRequest::IDBCursorRequest()
: mDirection(nsIIDBCursor::NEXT),
  mCachedValue(JSVAL_VOID),
  mHaveCachedValue(false),
  mJSRuntime(nsnull),
  mDataIndex(0)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBCursorRequest::~IDBCursorRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mJSRuntime) {
    JS_RemoveRootRT(mJSRuntime, &mCachedValue);
  }
}

NS_IMPL_ADDREF(IDBCursorRequest)
NS_IMPL_RELEASE(IDBCursorRequest)

NS_INTERFACE_MAP_BEGIN(IDBCursorRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, IDBRequest::Generator)
  NS_INTERFACE_MAP_ENTRY(nsIIDBCursorRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBCursor)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBCursorRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBCursorRequest, IDBCursorRequest)

NS_IMETHODIMP
IDBCursorRequest::GetDirection(PRUint16* aDirection)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aDirection = mDirection;
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::GetKey(nsIVariant** aKey)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mCachedKey) {
    nsresult rv;
    nsCOMPtr<nsIWritableVariant> variant =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    const Key& key = mData[mDataIndex].key;
    NS_ASSERTION(!key.IsUnset() && !key.IsNull(), "Bad key!");

    if (key.IsString()) {
      rv = variant->SetAsAString(key.StringValue());
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (key.IsInt()) {
      rv = variant->SetAsInt64(key.IntValue());
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      NS_NOTREACHED("Huh?!");
    }

    rv = variant->SetWritable(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIWritableVariant* result;
    variant.forget(&result);

    mCachedKey = dont_AddRef(static_cast<nsIVariant*>(result));
  }

  nsCOMPtr<nsIVariant> result(mCachedKey);
  result.forget(aKey);
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::GetValue(nsIVariant** /* aValue */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_WARNING("Using a slow path for GetValue! Fix this now!");

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

  nsAXPCNativeCallContext* cc;
  nsresult rv = xpc->GetCurrentNativeCallContext(&cc);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(cc, NS_ERROR_UNEXPECTED);

  jsval* retval;
  rv = cc->GetRetValPtr(&retval);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mHaveCachedValue) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, rv);

    JSAutoRequest ar(cx);

    if (!mJSRuntime) {
      JSRuntime* rt = JS_GetRuntime(cx);

      JSBool ok = JS_AddNamedRootRT(rt, &mCachedValue,
                                    "IDBCursorRequest::mCachedValue");
      NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

      mJSRuntime = rt;
    }

    nsCOMPtr<nsIJSON> json(new nsJSON());
    rv = json->DecodeToJSVal(mData[mDataIndex].value, cx, &mCachedValue);
    NS_ENSURE_SUCCESS(rv, rv);

    mHaveCachedValue = true;
  }

  *retval = mCachedValue;
  cc->SetReturnValueWasSet(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::Continue(nsIVariant* aKey,
                           PRUint8 aOptionalArgCount,
                           PRBool* _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  Key key;
  nsresult rv = IDBObjectStoreRequest::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (key.IsNull()) {
    if (aOptionalArgCount) {
      return NS_ERROR_INVALID_ARG;
    }
    else {
      key = Key::UNSETKEY;
    }
  }

  nsRefPtr<ContinueRunnable> runnable(new ContinueRunnable(this, key));
  rv = NS_DispatchToCurrentThread(runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  mTransaction->OnNewRequest();

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::Update(nsIVariant* aValue,
                         nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mObjectStore->IsWriteAllowed()) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IDBCursorRequest::Remove(nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mObjectStore->IsWriteAllowed()) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ContinueRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Remove cached stuff from last time.
  mCursor->mCachedKey = nsnull;
  mCursor->mCachedValue = JSVAL_VOID;
  mCursor->mHaveCachedValue = false;

  mCursor->mData.RemoveElementAt(mCursor->mDataIndex);
  if (mCursor->mDataIndex) {
    mCursor->mDataIndex--;
  }

  nsCOMPtr<nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!variant) {
    NS_ERROR("Couldn't create variant!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (mCursor->mData.IsEmpty()) {
    rv = variant->SetAsEmpty();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    if (!mKey.IsUnset()) {
      NS_ASSERTION(!mKey.IsNull(), "Huh?!");

      NS_WARNING("Using a slow O(n) search for continue(key), do something "
                 "smarter!");

      // Skip ahead to our next key match.
      PRInt32 index = PRInt32(mCursor->mDataIndex);
      while (index >= 0) {
        const Key& key = mCursor->mData[index].key;
        if (mKey == key) {
          break;
        }
        if (key < mKey) {
          index--;
          continue;
        }
        index = -1;
        break;
      }

      if (index >= 0) {
        mCursor->mDataIndex = PRUint32(index);
        mCursor->mData.RemoveElementsAt(index + 1,
                                        mCursor->mData.Length() - index - 1);
      }
    }

    rv = variant->SetAsISupports(static_cast<IDBRequest::Generator*>(mCursor));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = variant->SetWritable(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEvent> event =
    IDBSuccessEvent::Create(mCursor->mRequest, variant, mCursor->mTransaction);
  if (!event) {
    NS_ERROR("Failed to create event!");
    return NS_ERROR_FAILURE;
  }

  PRBool dummy;
  mCursor->mRequest->DispatchEvent(event, &dummy);

  mCursor->mTransaction->OnRequestFinished();

  return NS_OK;
}
