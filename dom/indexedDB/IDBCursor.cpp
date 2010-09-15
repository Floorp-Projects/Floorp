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

#include "IDBCursor.h"

#include "nsIIDBDatabaseException.h"
#include "nsIVariant.h"

#include "jscntxt.h"
#include "mozilla/storage.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSON.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBEvents.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "Savepoint.h"
#include "TransactionThreadPool.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class UpdateHelper : public AsyncConnectionHelper
{
public:
  UpdateHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               PRInt64 aObjectStoreID,
               const nsAString& aValue,
               const Key& aKey,
               bool aAutoIncrement,
               nsTArray<IndexUpdateInfo>& aIndexUpdateInfo)
  : AsyncConnectionHelper(aTransaction, aRequest), mOSID(aObjectStoreID),
    mValue(aValue), mKey(aKey), mAutoIncrement(aAutoIncrement)
  {
    mIndexUpdateInfo.SwapElements(aIndexUpdateInfo);
  }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  const PRInt64 mOSID;
  const nsString mValue;
  const Key mKey;
  const bool mAutoIncrement;
  nsTArray<IndexUpdateInfo> mIndexUpdateInfo;
};

class RemoveHelper : public AsyncConnectionHelper
{
public:
  RemoveHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               PRInt64 aObjectStoreID,
               const Key& aKey,
               bool aAutoIncrement)
  : AsyncConnectionHelper(aTransaction, aRequest), mOSID(aObjectStoreID),
    mKey(aKey), mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  const PRInt64 mOSID;
  const nsString mValue;
  const Key mKey;
  const bool mAutoIncrement;
};

inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBCursor* aCursor)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  IDBDatabase* database = aCursor->Transaction()->Database();
  return IDBRequest::Create(static_cast<nsPIDOMEventTarget*>(aCursor),
                            database->ScriptContext(), database->Owner());
}

} // anonymous namespace

BEGIN_INDEXEDDB_NAMESPACE

class ContinueRunnable : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  ContinueRunnable(IDBCursor* aCursor,
                   const Key& aKey)
  : mCursor(aCursor), mKey(aKey)
  { }

private:
  nsRefPtr<IDBCursor> mCursor;
  const Key mKey;
};

END_INDEXEDDB_NAMESPACE

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBRequest* aRequest,
                  IDBTransaction* aTransaction,
                  IDBObjectStore* aObjectStore,
                  PRUint16 aDirection,
                  nsTArray<KeyValuePair>& aData)
{
  NS_ASSERTION(aObjectStore, "Null pointer!");

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::CreateCommon(aRequest, aTransaction, aDirection);

  cursor->mObjectStore = aObjectStore;

  if (!cursor->mData.SwapElements(aData)) {
    NS_ERROR("Out of memory?!");
    return nsnull;
  }
  NS_ASSERTION(!cursor->mData.IsEmpty(), "Should never have an empty set!");

  cursor->mDataIndex = cursor->mData.Length() - 1;
  cursor->mType = OBJECTSTORE;

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBRequest* aRequest,
                  IDBTransaction* aTransaction,
                  IDBIndex* aIndex,
                  PRUint16 aDirection,
                  nsTArray<KeyKeyPair>& aData)
{
  NS_ASSERTION(aIndex, "Null pointer!");

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::CreateCommon(aRequest, aTransaction, aDirection);

  cursor->mObjectStore = aIndex->ObjectStore();
  cursor->mIndex = aIndex;

  if (!cursor->mKeyData.SwapElements(aData)) {
    NS_ERROR("Out of memory?!");
    return nsnull;
  }
  NS_ASSERTION(!cursor->mKeyData.IsEmpty(), "Should never have an empty set!");

  cursor->mDataIndex = cursor->mKeyData.Length() - 1;
  cursor->mType = INDEX;

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBRequest* aRequest,
                  IDBTransaction* aTransaction,
                  IDBIndex* aIndex,
                  PRUint16 aDirection,
                  nsTArray<KeyValuePair>& aData)
{
  NS_ASSERTION(aIndex, "Null pointer!");

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::CreateCommon(aRequest, aTransaction, aDirection);

  cursor->mObjectStore = aIndex->ObjectStore();
  cursor->mIndex = aIndex;

  if (!cursor->mData.SwapElements(aData)) {
    NS_ERROR("Out of memory?!");
    return nsnull;
  }
  NS_ASSERTION(!cursor->mData.IsEmpty(), "Should never have an empty set!");

  cursor->mDataIndex = cursor->mData.Length() - 1;
  cursor->mType = INDEXOBJECT;

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::CreateCommon(IDBRequest* aRequest,
                        IDBTransaction* aTransaction,
                        PRUint16 aDirection)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRequest, "Null pointer!");
  NS_ASSERTION(aTransaction, "Null pointer!");

  nsRefPtr<IDBCursor> cursor(new IDBCursor());

  cursor->mScriptContext = aTransaction->Database()->ScriptContext();
  cursor->mOwner = aTransaction->Database()->Owner();

  cursor->mRequest = aRequest;
  cursor->mTransaction = aTransaction;
  cursor->mDirection = aDirection;

  return cursor.forget();
}

IDBCursor::IDBCursor()
: mDirection(nsIIDBCursor::NEXT),
  mCachedValue(JSVAL_VOID),
  mHaveCachedValue(false),
  mValueRooted(false),
  mContinueCalled(false),
  mDataIndex(0),
  mType(OBJECTSTORE)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBCursor::~IDBCursor()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mValueRooted) {
    NS_DROP_JS_OBJECTS(this, IDBCursor);
  }

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBCursor)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBCursor,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mRequest,
                                                       nsPIDOMEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mTransaction,
                                                       nsPIDOMEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mObjectStore,
                                                       nsPIDOMEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mIndex,
                                                       nsPIDOMEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_BEGIN(IDBCursor)
  if (tmp->mValueRooted) {
    NS_DROP_JS_OBJECTS(tmp, IDBCursor);
    tmp->mCachedValue = JSVAL_VOID;
    tmp->mHaveCachedValue = false;
    tmp->mValueRooted = false;
  }
NS_IMPL_CYCLE_COLLECTION_ROOT_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBCursor)
  if (JSVAL_IS_GCTHING(tmp->mCachedValue)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mCachedValue);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing)
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBCursor,
                                                nsDOMEventTargetHelper)
  // Don't unlink mObjectStore, mIndex, or mTransaction!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBCursor)
  NS_INTERFACE_MAP_ENTRY(nsIIDBCursor)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBCursor)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBCursor, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBCursor, nsDOMEventTargetHelper)

DOMCI_DATA(IDBCursor, IDBCursor)

NS_IMETHODIMP
IDBCursor::GetDirection(PRUint16* aDirection)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aDirection = mDirection;
  return NS_OK;
}

NS_IMETHODIMP
IDBCursor::GetKey(nsIVariant** aKey)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mCachedKey) {
    nsresult rv;
    nsCOMPtr<nsIWritableVariant> variant =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    const Key& key = mType == INDEX ?
                     mKeyData[mDataIndex].key :
                     mData[mDataIndex].key;
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
IDBCursor::GetValue(JSContext* aCx,
                    jsval* aValue)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv;

  if (mType == INDEX) {
    const Key& value = mKeyData[mDataIndex].value;
    NS_ASSERTION(!value.IsUnset() && !value.IsNull(), "Bad key!");

    rv = IDBObjectStore::GetJSValFromKey(value, aCx, aValue);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  if (!mHaveCachedValue) {
    JSAutoRequest ar(aCx);

    nsCOMPtr<nsIJSON> json(new nsJSON());
    rv = json->DecodeToJSVal(mData[mDataIndex].value, aCx, &mCachedValue);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mValueRooted) {
      NS_HOLD_JS_OBJECTS(this, IDBCursor);
      mValueRooted = true;
    }

    mHaveCachedValue = true;
  }

  *aValue = mCachedValue;
  return NS_OK;
}

NS_IMETHODIMP
IDBCursor::Continue(const jsval &aKey,
                    JSContext* aCx,
                    PRUint8 aOptionalArgCount,
                    PRBool* _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mContinueCalled) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromJSVal(aKey, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (key.IsNull()) {
    if (aOptionalArgCount) {
      return NS_ERROR_INVALID_ARG;
    }
    else {
      key = Key::UNSETKEY;
    }
  }

  TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
  NS_ENSURE_TRUE(pool, NS_ERROR_FAILURE);

  nsRefPtr<ContinueRunnable> runnable(new ContinueRunnable(this, key));

  rv = pool->Dispatch(mTransaction, runnable, false, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  mTransaction->OnNewRequest();

  mContinueCalled = true;

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
IDBCursor::Update(const jsval &aValue,
                  JSContext* aCx,
                  nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mType != OBJECTSTORE) {
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mObjectStore->IsWriteAllowed()) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  const Key& key = mData[mDataIndex].key;
  NS_ASSERTION(!key.IsUnset() && !key.IsNull(), "Bad key!");

  JSAutoRequest ar(aCx);

  js::AutoValueRooter clone(aCx);
  nsresult rv = nsContentUtils::CreateStructuredClone(aCx, aValue,
                                                      clone.jsval_addr());
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mObjectStore->KeyPath().IsEmpty()) {
    // Make sure the object given has the correct keyPath value set on it or
    // we will add it.
    const nsString& keyPath = mObjectStore->KeyPath();
    const jschar* keyPathChars = reinterpret_cast<const jschar*>(keyPath.get());
    const size_t keyPathLen = keyPath.Length();

    js::AutoValueRooter prop(aCx);
    JSBool ok = JS_GetUCProperty(aCx, JSVAL_TO_OBJECT(clone.jsval_value()),
                                 keyPathChars, keyPathLen, prop.jsval_addr());
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

    if (JSVAL_IS_VOID(prop.jsval_value())) {
      rv = IDBObjectStore::GetJSValFromKey(key, aCx, prop.jsval_addr());
      NS_ENSURE_SUCCESS(rv, rv);

      ok = JS_DefineUCProperty(aCx, JSVAL_TO_OBJECT(clone.jsval_value()),
                               keyPathChars, keyPathLen, prop.jsval_value(), nsnull,
                               nsnull, JSPROP_ENUMERATE);
      NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
    }
    else {
      Key newKey;
      rv = IDBObjectStore::GetKeyFromJSVal(prop.jsval_value(), newKey);
      NS_ENSURE_SUCCESS(rv, rv);

      if (newKey.IsUnset() || newKey.IsNull() || newKey != key) {
        return NS_ERROR_INVALID_ARG;
      }
    }
  }

  nsTArray<IndexUpdateInfo> indexUpdateInfo;
  rv = IDBObjectStore::GetIndexUpdateInfo(mObjectStore->GetObjectStoreInfo(),
                                          aCx, clone.jsval_value(), indexUpdateInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIJSON> json(new nsJSON());

  nsString jsonValue;
  rv = json->EncodeFromJSVal(clone.jsval_addr(), aCx, jsonValue);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<UpdateHelper> helper =
    new UpdateHelper(mTransaction, request, mObjectStore->Id(), jsonValue, key,
                     mObjectStore->IsAutoIncrement(), indexUpdateInfo);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBCursor::Remove(nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mType != OBJECTSTORE) {
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mObjectStore->IsWriteAllowed()) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  const Key& key = mData[mDataIndex].key;
  NS_ASSERTION(!key.IsUnset() && !key.IsNull(), "Bad key!");

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<RemoveHelper> helper =
    new RemoveHelper(mTransaction, request, mObjectStore->Id(), key,
                     mObjectStore->IsAutoIncrement());
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

PRUint16
UpdateHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsresult rv;
  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Badness!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->AddStatement(false, true, mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  Savepoint savepoint(mTransaction);

  NS_NAMED_LITERAL_CSTRING(keyValue, "key_value");

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(keyValue, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(keyValue, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("data"), mValue);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::CONSTRAINT_ERR;
  }

  // Update our indexes if needed.
  if (!mIndexUpdateInfo.IsEmpty()) {
    PRInt64 objectDataId = mAutoIncrement ? mKey.IntValue() : LL_MININT;
    rv = IDBObjectStore::UpdateIndexes(mTransaction, mOSID, mKey,
                                       mAutoIncrement, true,
                                       objectDataId, mIndexUpdateInfo);
    if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
      return nsIIDBDatabaseException::CONSTRAINT_ERR;
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  rv = savepoint.Release();
  return NS_SUCCEEDED(rv) ? OK : nsIIDBDatabaseException::UNKNOWN_ERR;
}

PRUint16
UpdateHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Badness!");

  if (mKey.IsString()) {
    aResult->SetAsAString(mKey.StringValue());
  }
  else if (mKey.IsInt()) {
    aResult->SetAsInt64(mKey.IntValue());
  }
  else {
    NS_NOTREACHED("Bad key!");
  }
  return OK;
}

PRUint16
RemoveHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->RemoveStatement(mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Must have a key here!");

  NS_NAMED_LITERAL_CSTRING(key_value, "key_value");

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(key_value, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(key_value, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  // Search for it!
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
RemoveHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Badness!");

  if (mKey.IsString()) {
    aResult->SetAsAString(mKey.StringValue());
  }
  else if (mKey.IsInt()) {
    aResult->SetAsInt64(mKey.IntValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  return OK;
}

NS_IMETHODIMP
ContinueRunnable::Run()
{
  nsresult rv;

  if (!NS_IsMainThread()) {
    rv = NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // mCursor must be null after this method finishes. Swap out now.
  nsRefPtr<IDBCursor> cursor;
  cursor.swap(mCursor);

  // Remove cached stuff from last time.
  cursor->mCachedKey = nsnull;
  cursor->mCachedValue = JSVAL_VOID;
  cursor->mHaveCachedValue = false;
  cursor->mContinueCalled = false;

  if (cursor->mType == IDBCursor::INDEX) {
    cursor->mKeyData.RemoveElementAt(cursor->mDataIndex);
  }
  else {
    cursor->mData.RemoveElementAt(cursor->mDataIndex);
  }
  if (cursor->mDataIndex) {
    cursor->mDataIndex--;
  }

  nsCOMPtr<nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!variant) {
    NS_ERROR("Couldn't create variant!");
    return NS_ERROR_FAILURE;
  }

  PRBool empty = cursor->mType == IDBCursor::INDEX ?
                 cursor->mKeyData.IsEmpty() :
                 cursor->mData.IsEmpty();

  if (empty) {
    rv = variant->SetAsEmpty();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    if (!mKey.IsUnset()) {
      NS_ASSERTION(!mKey.IsNull(), "Huh?!");

      NS_WARNING("Using a slow O(n) search for continue(key), do something "
                 "smarter!");

      // Skip ahead to our next key match.
      PRInt32 index = PRInt32(cursor->mDataIndex);

      if (cursor->mType == IDBCursor::INDEX) {
        while (index >= 0) {
          const Key& key = cursor->mKeyData[index].key;
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
          cursor->mDataIndex = PRUint32(index);
          cursor->mKeyData.RemoveElementsAt(index + 1,
                                            cursor->mKeyData.Length() - index
                                            - 1);
        }
      }
      else {
        while (index >= 0) {
          const Key& key = cursor->mData[index].key;
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
          cursor->mDataIndex = PRUint32(index);
          cursor->mData.RemoveElementsAt(index + 1,
                                         cursor->mData.Length() - index - 1);
        }
      }
    }

    rv = variant->SetAsISupports(static_cast<nsPIDOMEventTarget*>(cursor));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = variant->SetWritable(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEvent> event =
    IDBSuccessEvent::Create(cursor->mRequest, variant, cursor->mTransaction);
  if (!event) {
    NS_ERROR("Failed to create event!");
    return NS_ERROR_FAILURE;
  }

  PRBool dummy;
  cursor->mRequest->DispatchEvent(event, &dummy);

  cursor->mTransaction->OnRequestFinished();
  return NS_OK;
}
