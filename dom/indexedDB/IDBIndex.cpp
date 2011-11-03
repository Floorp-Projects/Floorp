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


#include "IDBIndex.h"

#include "nsIIDBKeyRange.h"
#include "nsIJSContextStack.h"

#include "nsDOMClassInfoID.h"
#include "nsEventDispatcher.h"
#include "nsThreadUtils.h"
#include "mozilla/storage.h"

#include "AsyncConnectionHelper.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "DatabaseInfo.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class GetKeyHelper : public AsyncConnectionHelper
{
public:
  GetKeyHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               IDBIndex* aIndex,
               const Key& aKey)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex), mKey(aKey)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

protected:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  Key mKey;
};

class GetHelper : public GetKeyHelper
{
public:
  GetHelper(IDBTransaction* aTransaction,
            IDBRequest* aRequest,
            IDBIndex* aIndex,
            const Key& aKey)
  : GetKeyHelper(aTransaction, aRequest, aIndex, aKey)
  { }

  ~GetHelper()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneBuffer);
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

protected:
  JSAutoStructuredCloneBuffer mCloneBuffer;
};

class GetAllKeysHelper : public GetKeyHelper
{
public:
  GetAllKeysHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   IDBIndex* aIndex,
                   const Key& aKey,
                   const PRUint32 aLimit)
  : GetKeyHelper(aTransaction, aRequest, aIndex, aKey), mLimit(aLimit)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

protected:
  const PRUint32 mLimit;
  nsTArray<Key> mKeys;
};

class GetAllHelper : public GetKeyHelper
{
public:
  GetAllHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               IDBIndex* aIndex,
               const Key& aKey,
               const PRUint32 aLimit)
  : GetKeyHelper(aTransaction, aRequest, aIndex, aKey), mLimit(aLimit)
  { }

  ~GetAllHelper()
  {
    for (PRUint32 index = 0; index < mCloneBuffers.Length(); index++) {
      IDBObjectStore::ClearStructuredCloneBuffer(mCloneBuffers[index]);
    }
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    GetKeyHelper::ReleaseMainThreadObjects();
  }

protected:
  const PRUint32 mLimit;
  nsTArray<JSAutoStructuredCloneBuffer> mCloneBuffers;
};

class OpenKeyCursorHelper : public AsyncConnectionHelper
{
public:
  OpenKeyCursorHelper(IDBTransaction* aTransaction,
                      IDBRequest* aRequest,
                      IDBIndex* aIndex,
                      IDBKeyRange* aKeyRange,
                      PRUint16 aDirection)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mKeyRange(aKeyRange), mDirection(aDirection)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    mKeyRange = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  nsRefPtr<IDBKeyRange> mKeyRange;
  const PRUint16 mDirection;

  // Out-params.
  Key mKey;
  Key mObjectKey;
  nsCString mContinueQuery;
  nsCString mContinueToQuery;
  Key mRangeKey;
};

class OpenCursorHelper : public AsyncConnectionHelper
{
public:
  OpenCursorHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   IDBIndex* aIndex,
                   IDBKeyRange* aKeyRange,
                   PRUint16 aDirection)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mKeyRange(aKeyRange), mDirection(aDirection)
  { }

  ~OpenCursorHelper()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneBuffer);
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    mKeyRange = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  nsRefPtr<IDBKeyRange> mKeyRange;
  const PRUint16 mDirection;

  // Out-params.
  Key mKey;
  Key mObjectKey;
  JSAutoStructuredCloneBuffer mCloneBuffer;
  nsCString mContinueQuery;
  nsCString mContinueToQuery;
  Key mRangeKey;
};

inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBIndex* aIndex)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  IDBTransaction* transaction = aIndex->ObjectStore()->Transaction();
  IDBDatabase* database = transaction->Database();
  return IDBRequest::Create(aIndex, database->ScriptContext(),
                            database->Owner(), transaction);
}

} // anonymous namespace

// static
already_AddRefed<IDBIndex>
IDBIndex::Create(IDBObjectStore* aObjectStore,
                 const IndexInfo* aIndexInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aObjectStore, "Null pointer!");
  NS_ASSERTION(aIndexInfo, "Null pointer!");

  IDBDatabase* database = aObjectStore->Transaction()->Database();

  nsRefPtr<IDBIndex> index = new IDBIndex();

  index->mScriptContext = database->ScriptContext();
  index->mOwner = database->Owner();

  index->mObjectStore = aObjectStore;
  index->mId = aIndexInfo->id;
  index->mName = aIndexInfo->name;
  index->mKeyPath = aIndexInfo->keyPath;
  index->mUnique = aIndexInfo->unique;
  index->mAutoIncrement = aIndexInfo->autoIncrement;

  return index.forget();
}

IDBIndex::IDBIndex()
: mId(LL_MININT),
  mUnique(false),
  mAutoIncrement(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBIndex::~IDBIndex()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBIndex)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBIndex)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mObjectStore)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptContext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBIndex)
  // Don't unlink mObjectStore!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mScriptContext)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBIndex)
  NS_INTERFACE_MAP_ENTRY(nsIIDBIndex)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBIndex)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBIndex)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBIndex)

DOMCI_DATA(IDBIndex, IDBIndex)

NS_IMETHODIMP
IDBIndex::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetStoreName(nsAString& aStoreName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return mObjectStore->GetName(aStoreName);
}

NS_IMETHODIMP
IDBIndex::GetKeyPath(nsAString& aKeyPath)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aKeyPath.Assign(mKeyPath);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetUnique(bool* aUnique)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aUnique = mUnique;
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetObjectStore(nsIIDBObjectStore** aObjectStore)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIIDBObjectStore> objectStore(mObjectStore);
  objectStore.forget(aObjectStore);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::Get(const jsval& aKey,
              JSContext* aCx,
              nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  Key key;
  nsresult rv = key.SetFromJSVal(aCx, aKey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetHelper> helper =
    new GetHelper(transaction, request, this, key);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetKey(const jsval& aKey,
                 JSContext* aCx,
                 nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  Key key;
  nsresult rv = key.SetFromJSVal(aCx, aKey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetKeyHelper> helper =
    new GetKeyHelper(transaction, request, this, key);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetAll(const jsval& aKey,
                 PRUint32 aLimit,
                 JSContext* aCx,
                 PRUint8 aOptionalArgCount,
                 nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  Key key;
  if (aOptionalArgCount && NS_FAILED(key.SetFromJSVal(aCx, aKey))) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  if (aOptionalArgCount < 2) {
    aLimit = PR_UINT32_MAX;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(transaction, request, this, key, aLimit);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetAllKeys(const jsval& aKey,
                     PRUint32 aLimit,
                     JSContext* aCx,
                     PRUint8 aOptionalArgCount,
                     nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsresult rv;

  Key key;
  if (aOptionalArgCount && NS_FAILED(key.SetFromJSVal(aCx, aKey))) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  if (aOptionalArgCount < 2) {
    aLimit = PR_UINT32_MAX;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllKeysHelper> helper =
    new GetAllKeysHelper(transaction, request, this, key, aLimit);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::OpenCursor(const jsval& aKey,
                     PRUint16 aDirection,
                     JSContext* aCx,
                     PRUint8 aOptionalArgCount,
                     nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsresult rv;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aOptionalArgCount >= 2) {
      if (aDirection != nsIIDBCursor::NEXT &&
          aDirection != nsIIDBCursor::NEXT_NO_DUPLICATE &&
          aDirection != nsIIDBCursor::PREV &&
          aDirection != nsIIDBCursor::PREV_NO_DUPLICATE) {
        return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
      }
    }
    else {
      aDirection = nsIIDBCursor::NEXT;
    }
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(transaction, request, this, keyRange, aDirection);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::OpenKeyCursor(const jsval& aKey,
                        PRUint16 aDirection,
                        JSContext* aCx,
                        PRUint8 aOptionalArgCount,
                        nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsresult rv;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aOptionalArgCount >= 2) {
      if (aDirection != nsIIDBCursor::NEXT &&
          aDirection != nsIIDBCursor::NEXT_NO_DUPLICATE &&
          aDirection != nsIIDBCursor::PREV &&
          aDirection != nsIIDBCursor::PREV_NO_DUPLICATE) {
        return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
      }
    }
    else {
      aDirection = nsIIDBCursor::NEXT;
    }
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenKeyCursorHelper> helper =
    new OpenKeyCursorHelper(transaction, request, this, keyRange, aDirection);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

/*
NS_IMETHODIMP
IDBIndex::Count(const jsval& aKey,
                JSContext* aCx,
                PRUint8 aOptionalArgCount,
                nsIIDBRequest** _retval)
{
  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}
*/

nsresult
GetKeyHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->IndexGetStatement(mIndex->IsUnique(),
                                    mIndex->IsAutoIncrement());
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(value, "value");

  rv = mKey.BindToStatement(stmt, value);
  NS_ENSURE_SUCCESS(rv, rv);

  mKey.Unset();

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (hasResult) {
    rv = mKey.SetFromStatement(stmt, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
GetKeyHelper::GetSuccessResult(JSContext* aCx,
                               jsval* aVal)
{
  NS_ASSERTION(!mKey.IsUnset(), "Badness!");
  return mKey.ToJSVal(aCx, aVal);
}

nsresult
GetHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->IndexGetObjectStatement(mIndex->IsUnique(),
                                          mIndex->IsAutoIncrement());
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(value, "value");

  rv = mKey.BindToStatement(stmt, value);
  NS_ENSURE_SUCCESS(rv, rv);

  mKey.Unset();

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (hasResult) {
    rv = IDBObjectStore::GetStructuredCloneDataFromStatement(stmt, 0,
                                                             mCloneBuffer);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
GetHelper::GetSuccessResult(JSContext* aCx,
                            jsval* aVal)
{
  bool result = IDBObjectStore::DeserializeValue(aCx, mCloneBuffer, aVal);

  mCloneBuffer.clear();

  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult
GetAllKeysHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mKeys.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCString keyColumn;
  nsCString tableName;

  if (mIndex->IsAutoIncrement()) {
    keyColumn.AssignLiteral("ai_object_data_id");
    if (mIndex->IsUnique()) {
      tableName.AssignLiteral("ai_unique_index_data");
    }
    else {
      tableName.AssignLiteral("ai_index_data");
    }
  }
  else {
    keyColumn.AssignLiteral("object_data_key");
    if (mIndex->IsUnique()) {
      tableName.AssignLiteral("unique_index_data");
    }
    else {
      tableName.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(value, "value");

  nsCString keyClause;
  if (!mKey.IsUnset()) {
    keyClause = NS_LITERAL_CSTRING(" AND ") + value +
                NS_LITERAL_CSTRING(" = :") + value;
  }

  nsCString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT ") + keyColumn +
                    NS_LITERAL_CSTRING(" FROM ") + tableName +
                    NS_LITERAL_CSTRING(" WHERE ") + indexId  +
                    NS_LITERAL_CSTRING(" = :") + indexId + keyClause +
                    limitClause;
  
  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!mKey.IsUnset()) {
    rv = mKey.BindToStatement(stmt, value);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mKeys.Capacity() == mKeys.Length()) {
      mKeys.SetCapacity(mKeys.Capacity() * 2);
    }

    Key* key = mKeys.AppendElement();
    NS_ASSERTION(key, "This shouldn't fail!");

    rv = key->SetFromStatement(stmt, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
GetAllKeysHelper::GetSuccessResult(JSContext* aCx,
                                   jsval* aVal)
{
  NS_ASSERTION(mKeys.Length() <= mLimit, "Too many results!");

  nsTArray<Key> keys;
  if (!mKeys.SwapElements(keys)) {
    NS_ERROR("Failed to swap elements!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  JSAutoRequest ar(aCx);

  JSObject* array = JS_NewArrayObject(aCx, 0, NULL);
  if (!array) {
    NS_WARNING("Failed to make array!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (!keys.IsEmpty()) {
    if (!JS_SetArrayLength(aCx, array, jsuint(keys.Length()))) {
      NS_WARNING("Failed to set array length!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    for (uint32 index = 0, count = keys.Length(); index < count; index++) {
      const Key& key = keys[index];
      NS_ASSERTION(!key.IsUnset(), "Bad key!");

      jsval value;
      nsresult rv = key.ToJSVal(aCx, &value);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to get jsval for key!");
        return rv;
      }

      if (!JS_SetElement(aCx, array, index, &value)) {
        NS_WARNING("Failed to set array element!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }
  }

  *aVal = OBJECT_TO_JSVAL(array);
  return NS_OK;
}

nsresult
GetAllHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mCloneBuffers.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCString dataTableName;
  nsCString objectDataId;
  nsCString indexTableName;

  if (mIndex->IsAutoIncrement()) {
    dataTableName.AssignLiteral("ai_object_data");
    objectDataId.AssignLiteral("ai_object_data_id");
    if (mIndex->IsUnique()) {
      indexTableName.AssignLiteral("ai_unique_index_data");
    }
    else {
      indexTableName.AssignLiteral("ai_index_data");
    }
  }
  else {
    dataTableName.AssignLiteral("object_data");
    objectDataId.AssignLiteral("object_data_id");
    if (mIndex->IsUnique()) {
      indexTableName.AssignLiteral("unique_index_data");
    }
    else {
      indexTableName.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(value, "value");

  nsCString keyClause;
  if (!mKey.IsUnset()) {
    keyClause = NS_LITERAL_CSTRING(" AND ") + value +
                NS_LITERAL_CSTRING(" = :") + value;
  }

  nsCString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT data FROM ") + dataTableName +
                    NS_LITERAL_CSTRING(" INNER JOIN ") + indexTableName  +
                    NS_LITERAL_CSTRING(" ON ") + dataTableName +
                    NS_LITERAL_CSTRING(".id = ") + indexTableName +
                    NS_LITERAL_CSTRING(".") + objectDataId +
                    NS_LITERAL_CSTRING(" WHERE ") + indexId  +
                    NS_LITERAL_CSTRING(" = :") + indexId + keyClause +
                    limitClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!mKey.IsUnset()) {
    rv = mKey.BindToStatement(stmt, value);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mCloneBuffers.Capacity() == mCloneBuffers.Length()) {
      if (!mCloneBuffers.SetCapacity(mCloneBuffers.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    JSAutoStructuredCloneBuffer* buffer = mCloneBuffers.AppendElement();
    NS_ASSERTION(buffer, "This shouldn't fail!");

    rv = IDBObjectStore::GetStructuredCloneDataFromStatement(stmt, 0, *buffer);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
GetAllHelper::GetSuccessResult(JSContext* aCx,
                               jsval* aVal)
{
  NS_ASSERTION(mCloneBuffers.Length() <= mLimit, "Too many results!");

  nsresult rv = ConvertCloneBuffersToArray(aCx, mCloneBuffers, aVal);

  for (PRUint32 index = 0; index < mCloneBuffers.Length(); index++) {
    mCloneBuffers[index].clear();
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
OpenKeyCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCString table;
  nsCString keyColumn;

  if (mIndex->IsAutoIncrement()) {
    keyColumn.AssignLiteral("ai_object_data_id");
    if (mIndex->IsUnique()) {
      table.AssignLiteral("ai_unique_index_data");
    }
    else {
      table.AssignLiteral("ai_index_data");
    }
  }
  else {
    keyColumn.AssignLiteral("object_data_key");
    if (mIndex->IsUnique()) {
      table.AssignLiteral("unique_index_data");
    }
    else {
      table.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(lowerKeyName, "lower_key");
  NS_NAMED_LITERAL_CSTRING(upperKeyName, "upper_key");

  NS_NAMED_LITERAL_CSTRING(value, "value");

  nsCAutoString keyRangeClause;
  if (mKeyRange) {
    if (!mKeyRange->Lower().IsUnset()) {
      AppendConditionClause(value, lowerKeyName, false,
                            !mKeyRange->IsLowerOpen(), keyRangeClause);
    }
    if (!mKeyRange->Upper().IsUnset()) {
      AppendConditionClause(value, upperKeyName, true,
                            !mKeyRange->IsUpperOpen(), keyRangeClause);
    }
  }

  nsCAutoString directionClause = NS_LITERAL_CSTRING(" ORDER BY ") + value;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::NEXT_NO_DUPLICATE:
      directionClause.AppendLiteral(" ASC");
      break;

    case nsIIDBCursor::PREV:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      directionClause.AppendLiteral(" DESC");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }
  directionClause += NS_LITERAL_CSTRING(", ") + keyColumn +
                     NS_LITERAL_CSTRING(" ASC");

  nsCString firstQuery = NS_LITERAL_CSTRING("SELECT value, ") + keyColumn +
                         NS_LITERAL_CSTRING(" FROM ") + table +
                         NS_LITERAL_CSTRING(" WHERE index_id = :") + id +
                         keyRangeClause + directionClause +
                         NS_LITERAL_CSTRING(" LIMIT 1");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(firstQuery);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(id, mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    if (!mKeyRange->Lower().IsUnset()) {
      rv = mKeyRange->Lower().BindToStatement(stmt, lowerKeyName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (!mKeyRange->Upper().IsUnset()) {
      rv = mKeyRange->Upper().BindToStatement(stmt, upperKeyName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!hasResult) {
    mKey.Unset();
    return NS_OK;
  }

  rv = mKey.SetFromStatement(stmt, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mObjectKey.SetFromStatement(stmt, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now we need to make the query to get the next match.
  nsCAutoString queryStart = NS_LITERAL_CSTRING("SELECT value, ") + keyColumn +
                             NS_LITERAL_CSTRING(" FROM ") + table +
                             NS_LITERAL_CSTRING(" WHERE index_id = :") + id;

  NS_NAMED_LITERAL_CSTRING(currentKey, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");
  NS_NAMED_LITERAL_CSTRING(objectKey, "object_key");

  switch (mDirection) {
    case nsIIDBCursor::NEXT:
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !mKeyRange->IsUpperOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Upper();
      }
      mContinueQuery = queryStart + NS_LITERAL_CSTRING(" AND ( ( value = :") +
                       currentKey + NS_LITERAL_CSTRING(" AND ") + keyColumn +
                       NS_LITERAL_CSTRING(" > :") + objectKey +
                       NS_LITERAL_CSTRING(" ) OR ( value > :") + currentKey +
                       NS_LITERAL_CSTRING(" ) )") + directionClause +
                       NS_LITERAL_CSTRING(" LIMIT 1");
      mContinueToQuery = queryStart + NS_LITERAL_CSTRING(" AND value >= :") +
                         currentKey + NS_LITERAL_CSTRING(" LIMIT 1");
      break;

    case nsIIDBCursor::NEXT_NO_DUPLICATE:
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !mKeyRange->IsUpperOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Upper();
      }
      mContinueQuery = queryStart + NS_LITERAL_CSTRING(" AND value > :") +
                       currentKey + directionClause +
                       NS_LITERAL_CSTRING(" LIMIT 1");
      mContinueToQuery = queryStart + NS_LITERAL_CSTRING(" AND value >= :") +
                         currentKey + directionClause +
                         NS_LITERAL_CSTRING(" LIMIT 1");
      break;

    case nsIIDBCursor::PREV:
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !mKeyRange->IsLowerOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Lower();
      }
      mContinueQuery = queryStart + NS_LITERAL_CSTRING(" AND ( ( value = :") +
                       currentKey + NS_LITERAL_CSTRING(" AND ") + keyColumn +
                       NS_LITERAL_CSTRING(" < :") + objectKey +
                       NS_LITERAL_CSTRING(" ) OR ( value < :") + currentKey +
                       NS_LITERAL_CSTRING(" ) )") + directionClause +
                       NS_LITERAL_CSTRING(" LIMIT 1");
      mContinueToQuery = queryStart + NS_LITERAL_CSTRING(" AND value <= :") +
                         currentKey + NS_LITERAL_CSTRING(" LIMIT 1");
      break;

    case nsIIDBCursor::PREV_NO_DUPLICATE:
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !mKeyRange->IsLowerOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Lower();
      }
      mContinueQuery = queryStart + NS_LITERAL_CSTRING(" AND value < :") +
                       currentKey + directionClause +
                       NS_LITERAL_CSTRING(" LIMIT 1");
      mContinueToQuery = queryStart + NS_LITERAL_CSTRING(" AND value <= :") +
                         currentKey + directionClause +
                         NS_LITERAL_CSTRING(" LIMIT 1");
      break;

    default:
      NS_NOTREACHED("Unknown direction type!");
  }

  return NS_OK;
}

nsresult
OpenKeyCursorHelper::GetSuccessResult(JSContext* aCx,
                                      jsval* aVal)
{
  if (mKey.IsUnset()) {
    *aVal = JSVAL_VOID;
    return NS_OK;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mIndex, mDirection, mRangeKey,
                      mContinueQuery, mContinueToQuery, mKey, mObjectKey);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return WrapNative(aCx, cursor, aVal);
}

nsresult
OpenCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCString indexTable;
  nsCString objectTable;
  nsCString objectDataIdColumn;
  nsCString keyValueColumn;

  if (mIndex->IsAutoIncrement()) {
    objectTable.AssignLiteral("ai_object_data");
    objectDataIdColumn.AssignLiteral("ai_object_data_id");
    keyValueColumn.AssignLiteral("id");
    if (mIndex->IsUnique()) {
      indexTable.AssignLiteral("ai_unique_index_data");
    }
    else {
      indexTable.AssignLiteral("ai_index_data");
    }
  }
  else {
    objectTable.AssignLiteral("object_data");
    objectDataIdColumn.AssignLiteral("object_data_id");
    keyValueColumn.AssignLiteral("key_value");
    if (mIndex->IsUnique()) {
      indexTable.AssignLiteral("unique_index_data");
    }
    else {
      indexTable.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(lowerKeyName, "lower_key");
  NS_NAMED_LITERAL_CSTRING(upperKeyName, "upper_key");

  nsCString value = indexTable + NS_LITERAL_CSTRING(".value");
  nsCString data = objectTable + NS_LITERAL_CSTRING(".data");
  nsCString keyValue = objectTable + NS_LITERAL_CSTRING(".") + keyValueColumn;

  nsCAutoString keyRangeClause;
  if (mKeyRange) {
    if (!mKeyRange->Lower().IsUnset()) {
      AppendConditionClause(value, lowerKeyName, false,
                            !mKeyRange->IsLowerOpen(), keyRangeClause);
    }
    if (!mKeyRange->Upper().IsUnset()) {
      AppendConditionClause(value, upperKeyName, true,
                            !mKeyRange->IsUpperOpen(), keyRangeClause);
    }
  }

  nsCAutoString directionClause = NS_LITERAL_CSTRING(" ORDER BY ") + value;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::NEXT_NO_DUPLICATE:
      directionClause.AppendLiteral(" ASC");
      break;

    case nsIIDBCursor::PREV:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      directionClause.AppendLiteral(" DESC");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }
  directionClause += NS_LITERAL_CSTRING(", ") + keyValue +
                     NS_LITERAL_CSTRING(" ASC");

  nsCString firstQuery = NS_LITERAL_CSTRING("SELECT ") + value +
                         NS_LITERAL_CSTRING(", ") + keyValue +
                         NS_LITERAL_CSTRING(", ") + data +
                         NS_LITERAL_CSTRING(" FROM ") + objectTable +
                         NS_LITERAL_CSTRING(" INNER JOIN ") + indexTable +
                         NS_LITERAL_CSTRING(" ON ") + indexTable +
                         NS_LITERAL_CSTRING(".") + objectDataIdColumn +
                         NS_LITERAL_CSTRING(" = ") + objectTable +
                         NS_LITERAL_CSTRING(".id WHERE ") + indexTable +
                         NS_LITERAL_CSTRING(".index_id = :id") +
                         keyRangeClause + directionClause +
                         NS_LITERAL_CSTRING(" LIMIT 1");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(firstQuery);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(id, mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    if (!mKeyRange->Lower().IsUnset()) {
      rv = mKeyRange->Lower().BindToStatement(stmt, lowerKeyName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (!mKeyRange->Upper().IsUnset()) {
      rv = mKeyRange->Upper().BindToStatement(stmt, upperKeyName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!hasResult) {
    mKey.Unset();
    return NS_OK;
  }

  rv = mKey.SetFromStatement(stmt, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mObjectKey.SetFromStatement(stmt, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = IDBObjectStore::GetStructuredCloneDataFromStatement(stmt, 2,
                                                           mCloneBuffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now we need to make the query to get the next match.
  nsCAutoString queryStart = NS_LITERAL_CSTRING("SELECT ") + value +
                             NS_LITERAL_CSTRING(", ") + keyValue +
                             NS_LITERAL_CSTRING(", ") + data +
                             NS_LITERAL_CSTRING(" FROM ") + objectTable +
                             NS_LITERAL_CSTRING(" INNER JOIN ") + indexTable +
                             NS_LITERAL_CSTRING(" ON ") + indexTable +
                             NS_LITERAL_CSTRING(".") + objectDataIdColumn +
                             NS_LITERAL_CSTRING(" = ") + objectTable +
                             NS_LITERAL_CSTRING(".id WHERE ") + indexTable +
                             NS_LITERAL_CSTRING(".index_id = :id");

  NS_NAMED_LITERAL_CSTRING(currentKey, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");
  NS_NAMED_LITERAL_CSTRING(objectKey, "object_key");

  switch (mDirection) {
    case nsIIDBCursor::NEXT:
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !mKeyRange->IsUpperOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Upper();
      }
      mContinueQuery = queryStart + NS_LITERAL_CSTRING(" AND ( ( ") +
                       value + NS_LITERAL_CSTRING(" = :") + currentKey +
                       NS_LITERAL_CSTRING(" AND ") + keyValue +
                       NS_LITERAL_CSTRING(" > :") + objectKey +
                       NS_LITERAL_CSTRING(" ) OR ( ") + value +
                       NS_LITERAL_CSTRING(" > :") + currentKey +
                       NS_LITERAL_CSTRING(" ) )") + directionClause +
                       NS_LITERAL_CSTRING(" LIMIT 1");
      mContinueToQuery = queryStart + NS_LITERAL_CSTRING(" AND ") + value +
                         NS_LITERAL_CSTRING(" >= :") +
                         currentKey + directionClause +
                         NS_LITERAL_CSTRING(" LIMIT 1");
      break;

    case nsIIDBCursor::NEXT_NO_DUPLICATE:
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !mKeyRange->IsUpperOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Upper();
      }
      mContinueQuery = queryStart + NS_LITERAL_CSTRING(" AND ") + value +
                       NS_LITERAL_CSTRING(" > :") + currentKey +
                       directionClause + NS_LITERAL_CSTRING(" LIMIT 1");
      mContinueToQuery = queryStart + NS_LITERAL_CSTRING(" AND ") + value +
                         NS_LITERAL_CSTRING(" >= :") + currentKey +
                         directionClause + NS_LITERAL_CSTRING(" LIMIT 1");
      break;

    case nsIIDBCursor::PREV:
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !mKeyRange->IsLowerOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Lower();
      }
      mContinueQuery = queryStart + NS_LITERAL_CSTRING(" AND ( ( ") +
                       value + NS_LITERAL_CSTRING(" = :") + currentKey +
                       NS_LITERAL_CSTRING(" AND ") + keyValue +
                       NS_LITERAL_CSTRING(" < :") + objectKey +
                       NS_LITERAL_CSTRING(" ) OR ( ") + value +
                       NS_LITERAL_CSTRING(" < :") + currentKey +
                       NS_LITERAL_CSTRING(" ) )") + directionClause +
                       NS_LITERAL_CSTRING(" LIMIT 1");
      mContinueToQuery = queryStart + NS_LITERAL_CSTRING(" AND ") + value +
                         NS_LITERAL_CSTRING(" <= :") +
                         currentKey + directionClause +
                         NS_LITERAL_CSTRING(" LIMIT 1");
      break;

    case nsIIDBCursor::PREV_NO_DUPLICATE:
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !mKeyRange->IsLowerOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Lower();
      }
      mContinueQuery = queryStart + NS_LITERAL_CSTRING(" AND ") + value +
                       NS_LITERAL_CSTRING(" < :") + currentKey +
                       directionClause + NS_LITERAL_CSTRING(" LIMIT 1");
      mContinueToQuery = queryStart + NS_LITERAL_CSTRING(" AND ") + value +
                         NS_LITERAL_CSTRING(" <= :") + currentKey +
                         directionClause + NS_LITERAL_CSTRING(" LIMIT 1");
      break;

    default:
      NS_NOTREACHED("Unknown direction type!");
  }

  return NS_OK;
}

nsresult
OpenCursorHelper::GetSuccessResult(JSContext* aCx,
                                   jsval* aVal)
{
  if (mKey.IsUnset()) {
    *aVal = JSVAL_VOID;
    return NS_OK;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mIndex, mDirection, mRangeKey,
                      mContinueQuery, mContinueToQuery, mKey, mObjectKey,
                      mCloneBuffer);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(!mCloneBuffer.data(), "Should have swapped!");

  return WrapNative(aCx, cursor, aVal);
}
