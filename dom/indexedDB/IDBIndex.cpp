/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "IDBIndex.h"

#include "nsIIDBKeyRange.h"
#include "nsIJSContextStack.h"

#include "nsDOMClassInfoID.h"
#include "nsEventDispatcher.h"
#include "nsThreadUtils.h"
#include "mozilla/storage.h"
#include "xpcpublic.h"

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
               IDBKeyRange* aKeyRange)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mKeyRange(aKeyRange)
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

protected:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  Key mKey;
};

class GetHelper : public GetKeyHelper
{
public:
  GetHelper(IDBTransaction* aTransaction,
            IDBRequest* aRequest,
            IDBIndex* aIndex,
            IDBKeyRange* aKeyRange)
  : GetKeyHelper(aTransaction, aRequest, aIndex, aKeyRange)
  { }

  ~GetHelper()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
    GetKeyHelper::ReleaseMainThreadObjects();
  }

protected:
  StructuredCloneReadInfo mCloneReadInfo;
};

class GetAllKeysHelper : public GetKeyHelper
{
public:
  GetAllKeysHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   IDBIndex* aIndex,
                   IDBKeyRange* aKeyRange,
                   const PRUint32 aLimit)
  : GetKeyHelper(aTransaction, aRequest, aIndex, aKeyRange), mLimit(aLimit)
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
               IDBKeyRange* aKeyRange,
               const PRUint32 aLimit)
  : GetKeyHelper(aTransaction, aRequest, aIndex, aKeyRange), mLimit(aLimit)
  { }

  ~GetAllHelper()
  {
    for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
      IDBObjectStore::ClearStructuredCloneBuffer(
        mCloneReadInfos[index].mCloneBuffer);
    }
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
      IDBObjectStore::ClearStructuredCloneBuffer(
        mCloneReadInfos[index].mCloneBuffer);
    }
    GetKeyHelper::ReleaseMainThreadObjects();
  }

protected:
  const PRUint32 mLimit;
  nsTArray<StructuredCloneReadInfo> mCloneReadInfos;
};

class OpenKeyCursorHelper : public AsyncConnectionHelper
{
public:
  OpenKeyCursorHelper(IDBTransaction* aTransaction,
                      IDBRequest* aRequest,
                      IDBIndex* aIndex,
                      IDBKeyRange* aKeyRange,
                      IDBCursor::Direction aDirection)
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
  const IDBCursor::Direction mDirection;

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
                   IDBCursor::Direction aDirection)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mKeyRange(aKeyRange), mDirection(aDirection)
  { }

  ~OpenCursorHelper()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
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
  const IDBCursor::Direction mDirection;

  // Out-params.
  Key mKey;
  Key mObjectKey;
  StructuredCloneReadInfo mCloneReadInfo;
  nsCString mContinueQuery;
  nsCString mContinueToQuery;
  Key mRangeKey;
};

class CountHelper : public AsyncConnectionHelper
{
public:
  CountHelper(IDBTransaction* aTransaction,
              IDBRequest* aRequest,
              IDBIndex* aIndex,
              IDBKeyRange* aKeyRange)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mKeyRange(aKeyRange), mCount(0)
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
  nsRefPtr<IDBIndex> mIndex;
  nsRefPtr<IDBKeyRange> mKeyRange;
  PRUint64 mCount;
};

inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBIndex* aIndex)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  IDBTransaction* transaction = aIndex->ObjectStore()->Transaction();
  IDBDatabase* database = transaction->Database();
  return IDBRequest::Create(aIndex, database, transaction);
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

  nsRefPtr<IDBIndex> index = new IDBIndex();

  index->mObjectStore = aObjectStore;
  index->mId = aIndexInfo->id;
  index->mName = aIndexInfo->name;
  index->mKeyPath = aIndexInfo->keyPath;
  index->mKeyPathArray = aIndexInfo->keyPathArray;
  index->mUnique = aIndexInfo->unique;
  index->mMultiEntry = aIndexInfo->multiEntry;

  return index.forget();
}

IDBIndex::IDBIndex()
: mId(LL_MININT),
  mUnique(false)
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBIndex)
  // Don't unlink mObjectStore!
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
IDBIndex::GetKeyPath(JSContext* aCx,
                     jsval* aVal)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (UsesKeyPathArray()) {
    JSObject* array = JS_NewArrayObject(aCx, mKeyPathArray.Length(), nsnull);
    if (!array) {
      NS_WARNING("Failed to make array!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    for (PRUint32 i = 0; i < mKeyPathArray.Length(); ++i) {
      jsval val;
      nsString tmp(mKeyPathArray[i]);
      if (!xpc::StringToJsval(aCx, tmp, &val)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      if (!JS_SetElement(aCx, array, i, &val)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    *aVal = OBJECT_TO_JSVAL(array);
  }
  else {
    nsString tmp(mKeyPath);
    if (!xpc::StringToJsval(aCx, tmp, aVal)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }
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
IDBIndex::GetMultiEntry(bool* aMultiEntry)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aMultiEntry = mMultiEntry;
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

  nsRefPtr<IDBKeyRange> keyRange;
  nsresult rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!keyRange) {
    // Must specify a key or keyRange for get().
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetHelper> helper =
    new GetHelper(transaction, request, this, keyRange);
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

  nsRefPtr<IDBKeyRange> keyRange;
  nsresult rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!keyRange) {
    // Must specify a key or keyRange for get().
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetKeyHelper> helper =
    new GetKeyHelper(transaction, request, this, keyRange);

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

  nsresult rv;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aOptionalArgCount < 2 || aLimit == 0) {
    aLimit = PR_UINT32_MAX;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(transaction, request, this, keyRange, aLimit);

  rv = helper->DispatchToTransactionPool();
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

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aOptionalArgCount < 2 || aLimit == 0) {
    aLimit = PR_UINT32_MAX;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllKeysHelper> helper =
    new GetAllKeysHelper(transaction, request, this, keyRange, aLimit);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::OpenCursor(const jsval& aKey,
                     const nsAString& aDirection,
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

  IDBCursor::Direction direction = IDBCursor::NEXT;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aOptionalArgCount >= 2) {
      rv = IDBCursor::ParseDirection(aDirection, &direction);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(transaction, request, this, keyRange, direction);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::OpenKeyCursor(const jsval& aKey,
                        const nsAString& aDirection,
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

  IDBCursor::Direction direction = IDBCursor::NEXT;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aOptionalArgCount >= 2) {
      rv = IDBCursor::ParseDirection(aDirection, &direction);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenKeyCursorHelper> helper =
    new OpenKeyCursorHelper(transaction, request, this, keyRange, direction);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

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

  nsresult rv;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<CountHelper> helper =
    new CountHelper(transaction, request, this, keyRange);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
GetKeyHelper::DoDatabaseWork(mozIStorageConnection* /* aConnection */)
{
  NS_ASSERTION(mKeyRange, "Must have a key range here!");

  nsCString indexTable;
  if (mIndex->IsUnique()) {
    indexTable.AssignLiteral("unique_index_data");
  }
  else {
    indexTable.AssignLiteral("index_data");
  }

  nsCString keyRangeClause;
  mKeyRange->GetBindingClause(NS_LITERAL_CSTRING("value"), keyRangeClause);

  NS_ASSERTION(!keyRangeClause.IsEmpty(), "Huh?!");

  nsCString query = NS_LITERAL_CSTRING("SELECT object_data_key FROM ") +
                    indexTable +
                    NS_LITERAL_CSTRING(" WHERE index_id = :index_id") +
                    keyRangeClause +
                    NS_LITERAL_CSTRING(" LIMIT 1");

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = mKeyRange->BindToStatement(stmt);
  NS_ENSURE_SUCCESS(rv, rv);

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
  return mKey.ToJSVal(aCx, aVal);
}

nsresult
GetHelper::DoDatabaseWork(mozIStorageConnection* /* aConnection */)
{
  NS_ASSERTION(mKeyRange, "Must have a key range here!");

  nsCString indexTable;
  if (mIndex->IsUnique()) {
    indexTable.AssignLiteral("unique_index_data");
  }
  else {
    indexTable.AssignLiteral("index_data");
  }

  nsCString keyRangeClause;
  mKeyRange->GetBindingClause(NS_LITERAL_CSTRING("value"), keyRangeClause);

  NS_ASSERTION(!keyRangeClause.IsEmpty(), "Huh?!");

  nsCString query = NS_LITERAL_CSTRING("SELECT data, file_ids FROM object_data "
                                       "INNER JOIN ") + indexTable +
                    NS_LITERAL_CSTRING(" AS index_table ON object_data.id = ") +
                    NS_LITERAL_CSTRING("index_table.object_data_id WHERE "
                                       "index_id = :index_id") +
                    keyRangeClause +
                    NS_LITERAL_CSTRING(" LIMIT 1");

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = mKeyRange->BindToStatement(stmt);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (hasResult) {
    rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(stmt, 0, 1,
      mDatabase->Manager(), mCloneReadInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
GetHelper::GetSuccessResult(JSContext* aCx,
                            jsval* aVal)
{
  bool result = IDBObjectStore::DeserializeValue(aCx, mCloneReadInfo, aVal);

  mCloneReadInfo.mCloneBuffer.clear();

  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult
GetAllKeysHelper::DoDatabaseWork(mozIStorageConnection* /* aConnection */)
{
  nsCString tableName;
  if (mIndex->IsUnique()) {
    tableName.AssignLiteral("unique_index_data");
  }
  else {
    tableName.AssignLiteral("index_data");
  }

  nsCString keyRangeClause;
  if (mKeyRange) {
    mKeyRange->GetBindingClause(NS_LITERAL_CSTRING("value"), keyRangeClause);
  }

  nsCString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT object_data_key FROM ") +
                    tableName +
                    NS_LITERAL_CSTRING(" WHERE index_id = :index_id") +
                    keyRangeClause + limitClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    rv = mKeyRange->BindToStatement(stmt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mKeys.SetCapacity(50);

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
    if (!JS_SetArrayLength(aCx, array, uint32_t(keys.Length()))) {
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
GetAllHelper::DoDatabaseWork(mozIStorageConnection* /* aConnection */)
{
  nsCString indexTable;
  if (mIndex->IsUnique()) {
    indexTable.AssignLiteral("unique_index_data");
  }
  else {
    indexTable.AssignLiteral("index_data");
  }

  nsCString keyRangeClause;
  if (mKeyRange) {
    mKeyRange->GetBindingClause(NS_LITERAL_CSTRING("value"), keyRangeClause);
  }

  nsCString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT data, file_ids FROM object_data "
                                       "INNER JOIN ") + indexTable +
                    NS_LITERAL_CSTRING(" AS index_table ON object_data.id = "
                                       "index_table.object_data_id "
                                       "WHERE index_id = :index_id") +
                    keyRangeClause + limitClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    rv = mKeyRange->BindToStatement(stmt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mCloneReadInfos.SetCapacity(50);

  bool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mCloneReadInfos.Capacity() == mCloneReadInfos.Length()) {
      mCloneReadInfos.SetCapacity(mCloneReadInfos.Capacity() * 2);
    }

    StructuredCloneReadInfo* readInfo = mCloneReadInfos.AppendElement();
    NS_ASSERTION(readInfo, "This shouldn't fail!");

    rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(stmt, 0, 1,
      mDatabase->Manager(), *readInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
GetAllHelper::GetSuccessResult(JSContext* aCx,
                               jsval* aVal)
{
  NS_ASSERTION(mCloneReadInfos.Length() <= mLimit, "Too many results!");

  nsresult rv = ConvertCloneReadInfosToArray(aCx, mCloneReadInfos, aVal);

  for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
    mCloneReadInfos[index].mCloneBuffer.clear();
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
OpenKeyCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCString table;
  if (mIndex->IsUnique()) {
    table.AssignLiteral("unique_index_data");
  }
  else {
    table.AssignLiteral("index_data");
  }

  NS_NAMED_LITERAL_CSTRING(value, "value");

  nsCString keyRangeClause;
  if (mKeyRange) {
    mKeyRange->GetBindingClause(value, keyRangeClause);
  }

  nsCAutoString directionClause(" ORDER BY value ");
  switch (mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause += NS_LITERAL_CSTRING("ASC, object_data_key ASC");
      break;

    case IDBCursor::PREV:
      directionClause += NS_LITERAL_CSTRING("DESC, object_data_key DESC");
      break;

    case IDBCursor::PREV_UNIQUE:
      directionClause += NS_LITERAL_CSTRING("DESC, object_data_key ASC");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }
  nsCString firstQuery = NS_LITERAL_CSTRING("SELECT value, object_data_key "
                                            "FROM ") + table +
                         NS_LITERAL_CSTRING(" WHERE index_id = :index_id") +
                         keyRangeClause + directionClause +
                         NS_LITERAL_CSTRING(" LIMIT 1");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(firstQuery);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    rv = mKeyRange->BindToStatement(stmt);
    NS_ENSURE_SUCCESS(rv, rv);
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
  nsCAutoString queryStart = NS_LITERAL_CSTRING("SELECT value, object_data_key"
                                                " FROM ") + table +
                             NS_LITERAL_CSTRING(" WHERE index_id = :id");

  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mDirection) {
    case IDBCursor::NEXT:
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !mKeyRange->IsUpperOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Upper();
      }
      mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value >= :current_key AND "
                           "( value > :current_key OR "
                           "  object_data_key > :object_key )") +
        directionClause +
        NS_LITERAL_CSTRING(" LIMIT ");
      mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value >= :current_key ") +
        directionClause +
        NS_LITERAL_CSTRING(" LIMIT ");
      break;

    case IDBCursor::NEXT_UNIQUE:
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !mKeyRange->IsUpperOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Upper();
      }
      mContinueQuery =
        queryStart + NS_LITERAL_CSTRING(" AND value > :current_key") +
        directionClause +
        NS_LITERAL_CSTRING(" LIMIT ");
      mContinueToQuery =
        queryStart + NS_LITERAL_CSTRING(" AND value >= :current_key") +
        directionClause +
        NS_LITERAL_CSTRING(" LIMIT ");
      break;

    case IDBCursor::PREV:
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !mKeyRange->IsLowerOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Lower();
      }

      mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value <= :current_key AND "
                           "( value < :current_key OR "
                           "  object_data_key < :object_key )") +
        directionClause +
        NS_LITERAL_CSTRING(" LIMIT ");
      mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value <= :current_key ") +
        directionClause +
        NS_LITERAL_CSTRING(" LIMIT ");
      break;

    case IDBCursor::PREV_UNIQUE:
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !mKeyRange->IsLowerOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Lower();
      }
      mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value < :current_key") +
        directionClause +
        NS_LITERAL_CSTRING(" LIMIT ");
      mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value <= :current_key") +
        directionClause +
        NS_LITERAL_CSTRING(" LIMIT ");
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
  if (mIndex->IsUnique()) {
    indexTable.AssignLiteral("unique_index_data");
  }
  else {
    indexTable.AssignLiteral("index_data");
  }

  NS_NAMED_LITERAL_CSTRING(value, "index_table.value");

  nsCString keyRangeClause;
  if (mKeyRange) {
    mKeyRange->GetBindingClause(value, keyRangeClause);
  }

  nsCAutoString directionClause(" ORDER BY index_table.value ");
  switch (mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause +=
        NS_LITERAL_CSTRING("ASC, index_table.object_data_key ASC");
      break;

    case IDBCursor::PREV:
      directionClause +=
        NS_LITERAL_CSTRING("DESC, index_table.object_data_key DESC");
      break;

    case IDBCursor::PREV_UNIQUE:
      directionClause +=
        NS_LITERAL_CSTRING("DESC, index_table.object_data_key ASC");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }

  nsCString firstQuery =
    NS_LITERAL_CSTRING("SELECT index_table.value, "
                       "index_table.object_data_key, object_data.data, "
                       "object_data.file_ids FROM ") +
    indexTable +
    NS_LITERAL_CSTRING(" AS index_table INNER JOIN object_data ON "
                       "index_table.object_data_id = object_data.id "
                       "WHERE index_table.index_id = :id") +
    keyRangeClause + directionClause +
    NS_LITERAL_CSTRING(" LIMIT 1");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(firstQuery);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    rv = mKeyRange->BindToStatement(stmt);
    NS_ENSURE_SUCCESS(rv, rv);
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

  rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(stmt, 2, 3,
    mDatabase->Manager(), mCloneReadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now we need to make the query to get the next match.
  nsCAutoString queryStart =
    NS_LITERAL_CSTRING("SELECT index_table.value, "
                       "index_table.object_data_key, object_data.data, "
                       "object_data.file_ids FROM ") +
    indexTable +
    NS_LITERAL_CSTRING(" AS index_table INNER JOIN object_data ON "
                       "index_table.object_data_id = object_data.id "
                       "WHERE index_table.index_id = :id");

  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  NS_NAMED_LITERAL_CSTRING(limit, " LIMIT ");

  switch (mDirection) {
    case IDBCursor::NEXT:
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !mKeyRange->IsUpperOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Upper();
      }
      mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value >= :current_key AND "
                           "( index_table.value > :current_key OR "
                           "  index_table.object_data_key > :object_key ) ") +
        directionClause + limit;
      mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value >= :current_key") +
        directionClause + limit;
      break;

    case IDBCursor::NEXT_UNIQUE:
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !mKeyRange->IsUpperOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Upper();
      }
      mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value > :current_key") +
        directionClause + limit;
      mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value >= :current_key") +
        directionClause + limit;
      break;

    case IDBCursor::PREV:
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !mKeyRange->IsLowerOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Lower();
      }
      mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value <= :current_key AND "
                           "( index_table.value < :current_key OR "
                           "  index_table.object_data_key < :object_key ) ") +
        directionClause + limit;
      mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value <= :current_key") +
        directionClause + limit;
      break;

    case IDBCursor::PREV_UNIQUE:
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !mKeyRange->IsLowerOpen(),
                              queryStart);
        mRangeKey = mKeyRange->Lower();
      }
      mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value < :current_key") +
        directionClause + limit;
      mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value <= :current_key") +
        directionClause + limit;
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
                      mCloneReadInfo);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(!mCloneReadInfo.mCloneBuffer.data(), "Should have swapped!");

  return WrapNative(aCx, cursor, aVal);
}

nsresult
CountHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCString table;
  if (mIndex->IsUnique()) {
    table.AssignLiteral("unique_index_data");
  }
  else {
    table.AssignLiteral("index_data");
  }

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

  nsCString query = NS_LITERAL_CSTRING("SELECT count(*) FROM ") + table +
                    NS_LITERAL_CSTRING(" WHERE index_id = :id") +
                    keyRangeClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mIndex->Id());
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
  NS_ENSURE_TRUE(hasResult, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mCount = stmt->AsInt64(0);
  return NS_OK;
}

nsresult
CountHelper::GetSuccessResult(JSContext* aCx,
                              jsval* aVal)
{
  return JS_NewNumberValue(aCx, static_cast<double>(mCount), aVal);
}
