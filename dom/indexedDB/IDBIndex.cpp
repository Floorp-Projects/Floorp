/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IDBIndex.h"

#include "nsIIDBKeyRange.h"
#include "nsIJSContextStack.h"

#include "nsContentUtils.h"
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

#include "ipc/IndexedDBChild.h"
#include "ipc/IndexedDBParent.h"

#include "IndexedDatabaseInlines.h"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::dom::indexedDB::ipc;

namespace {

class IndexHelper : public AsyncConnectionHelper
{
public:
  IndexHelper(IDBTransaction* aTransaction,
              IDBRequest* aRequest,
              IDBIndex* aIndex)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mActor(nsnull)
  {
    NS_ASSERTION(aTransaction, "Null transaction!");
    NS_ASSERTION(aRequest, "Null request!");
    NS_ASSERTION(aIndex, "Null index!");
  }

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult Dispatch(nsIEventTarget* aDatabaseThread) MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) = 0;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue) = 0;

protected:
  nsRefPtr<IDBIndex> mIndex;

private:
  IndexedDBIndexRequestChild* mActor;
};

class GetKeyHelper : public IndexHelper
{
public:
  GetKeyHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               IDBIndex* aIndex,
               IDBKeyRange* aKeyRange)
  : IndexHelper(aTransaction, aRequest, aIndex), mKeyRange(aKeyRange)
  { }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

protected:
  // In-params.
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

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

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

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal) MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

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

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

protected:
  const PRUint32 mLimit;
  nsTArray<StructuredCloneReadInfo> mCloneReadInfos;
};

class OpenKeyCursorHelper : public IndexHelper
{
public:
  OpenKeyCursorHelper(IDBTransaction* aTransaction,
                      IDBRequest* aRequest,
                      IDBIndex* aIndex,
                      IDBKeyRange* aKeyRange,
                      IDBCursor::Direction aDirection)
  : IndexHelper(aTransaction, aRequest, aIndex), mKeyRange(aKeyRange),
    mDirection(aDirection)
  { }

  ~OpenKeyCursorHelper()
  {
    NS_ASSERTION(true, "bas");
  }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

protected:
  virtual nsresult EnsureCursor();

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
  const IDBCursor::Direction mDirection;

  // Out-params.
  Key mKey;
  Key mObjectKey;
  nsCString mContinueQuery;
  nsCString mContinueToQuery;
  Key mRangeKey;

  // Only used in the parent process.
  nsRefPtr<IDBCursor> mCursor;
};

class OpenCursorHelper : public OpenKeyCursorHelper
{
public:
  OpenCursorHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   IDBIndex* aIndex,
                   IDBKeyRange* aKeyRange,
                   IDBCursor::Direction aDirection)
  : OpenKeyCursorHelper(aTransaction, aRequest, aIndex, aKeyRange, aDirection)
  { }

  ~OpenCursorHelper()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
  }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

private:
  virtual nsresult EnsureCursor();

  StructuredCloneReadInfo mCloneReadInfo;

  // Only used in the parent process.
  SerializedStructuredCloneReadInfo mSerializedCloneReadInfo;
};

class CountHelper : public IndexHelper
{
public:
  CountHelper(IDBTransaction* aTransaction,
              IDBRequest* aRequest,
              IDBIndex* aIndex,
              IDBKeyRange* aKeyRange)
  : IndexHelper(aTransaction, aRequest, aIndex), mKeyRange(aKeyRange), mCount(0)
  { }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

private:
  nsRefPtr<IDBKeyRange> mKeyRange;
  PRUint64 mCount;
};

inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBIndex* aIndex, JSContext* aCx)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  IDBTransaction* transaction = aIndex->ObjectStore()->Transaction();
  IDBDatabase* database = transaction->Database();
  return IDBRequest::Create(aIndex, database, transaction, aCx);
}

} // anonymous namespace

// static
already_AddRefed<IDBIndex>
IDBIndex::Create(IDBObjectStore* aObjectStore,
                 const IndexInfo* aIndexInfo,
                 bool aCreating)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aObjectStore, "Null pointer!");
  NS_ASSERTION(aIndexInfo, "Null pointer!");

  nsRefPtr<IDBIndex> index = new IDBIndex();

  index->mObjectStore = aObjectStore;
  index->mId = aIndexInfo->id;
  index->mName = aIndexInfo->name;
  index->mKeyPath = aIndexInfo->keyPath;
  index->mUnique = aIndexInfo->unique;
  index->mMultiEntry = aIndexInfo->multiEntry;

  if (!IndexedDatabaseManager::IsMainProcess()) {
    IndexedDBObjectStoreChild* objectStoreActor = aObjectStore->GetActorChild();
    NS_ASSERTION(objectStoreActor, "Must have an actor here!");

    nsAutoPtr<IndexedDBIndexChild> actor(new IndexedDBIndexChild(index));

    IndexConstructorParams params;

    if (aCreating) {
      CreateIndexParams createParams;
      createParams.info() = *aIndexInfo;
      params = createParams;
    }
    else {
      GetIndexParams getParams;
      getParams.name() = aIndexInfo->name;
      params = getParams;
    }

    objectStoreActor->SendPIndexedDBIndexConstructor(actor.forget(), params);
  }

  return index.forget();
}

IDBIndex::IDBIndex()
: mId(LL_MININT),
  mKeyPath(0),
  mCachedKeyPath(JSVAL_VOID),
  mActorChild(nsnull),
  mActorParent(nsnull),
  mUnique(false),
  mMultiEntry(false),
  mRooted(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBIndex::~IDBIndex()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mActorParent, "Actor parent owns us, how can we be dying?!");

  if (mRooted) {
    NS_DROP_JS_OBJECTS(this, IDBIndex);
  }

  if (mActorChild) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

nsresult
IDBIndex::GetInternal(IDBKeyRange* aKeyRange,
                      JSContext* aCx,
                      IDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this, aCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetHelper> helper =
    new GetHelper(transaction, request, this, aKeyRange);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
IDBIndex::GetKeyInternal(IDBKeyRange* aKeyRange,
                         JSContext* aCx,
                         IDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this, aCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetKeyHelper> helper =
    new GetKeyHelper(transaction, request, this, aKeyRange);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
IDBIndex::GetAllInternal(IDBKeyRange* aKeyRange,
                         PRUint32 aLimit,
                         JSContext* aCx,
                         IDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this, aCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(transaction, request, this, aKeyRange, aLimit);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
IDBIndex::GetAllKeysInternal(IDBKeyRange* aKeyRange,
                             PRUint32 aLimit,
                             JSContext* aCx,
                             IDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this, aCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllKeysHelper> helper =
    new GetAllKeysHelper(transaction, request, this, aKeyRange, aLimit);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
IDBIndex::CountInternal(IDBKeyRange* aKeyRange,
                        JSContext* aCx,
                        IDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this, aCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<CountHelper> helper =
    new CountHelper(transaction, request, this, aKeyRange);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
IDBIndex::OpenKeyCursorInternal(IDBKeyRange* aKeyRange,
                                size_t aDirection,
                                JSContext* aCx,
                                IDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  IDBCursor::Direction direction =
    static_cast<IDBCursor::Direction>(aDirection);

  nsRefPtr<IDBRequest> request = GenerateRequest(this, aCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenKeyCursorHelper> helper =
    new OpenKeyCursorHelper(transaction, request, this, aKeyRange, direction);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
IDBIndex::OpenCursorInternal(IDBKeyRange* aKeyRange,
                             size_t aDirection,
                             JSContext* aCx,
                             IDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  IDBCursor::Direction direction =
    static_cast<IDBCursor::Direction>(aDirection);

  nsRefPtr<IDBRequest> request = GenerateRequest(this, aCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(transaction, request, this, aKeyRange, direction);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
IDBIndex::OpenCursorFromChildProcess(IDBRequest* aRequest,
                                     size_t aDirection,
                                     const Key& aKey,
                                     const Key& aObjectKey,
                                     IDBCursor** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBCursor::Direction direction =
    static_cast<IDBCursor::Direction>(aDirection);

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(aRequest, mObjectStore->Transaction(), this, direction,
                      Key(), EmptyCString(), EmptyCString(), aKey, aObjectKey);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  cursor.forget(_retval);
  return NS_OK;
}

nsresult
IDBIndex::OpenCursorFromChildProcess(
                            IDBRequest* aRequest,
                            size_t aDirection,
                            const Key& aKey,
                            const Key& aObjectKey,
                            const SerializedStructuredCloneReadInfo& aCloneInfo,
                            IDBCursor** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION((!aCloneInfo.dataLength && !aCloneInfo.data) ||
               (aCloneInfo.dataLength && aCloneInfo.data),
               "Inconsistent clone info!");

  IDBCursor::Direction direction =
    static_cast<IDBCursor::Direction>(aDirection);

  StructuredCloneReadInfo cloneInfo;

  if (!cloneInfo.SetFromSerialized(aCloneInfo)) {
    NS_WARNING("Failed to copy clone buffer!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(aRequest, mObjectStore->Transaction(), this, direction,
                      Key(), EmptyCString(), EmptyCString(), aKey, aObjectKey,
                      cloneInfo);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(!cloneInfo.mCloneBuffer.data(), "Should have swapped!");

  cursor.forget(_retval);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBIndex)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBIndex)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedKeyPath)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBIndex)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mObjectStore)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBIndex)
  // Don't unlink mObjectStore!

  tmp->mCachedKeyPath = JSVAL_VOID;

  if (tmp->mRooted) {
    NS_DROP_JS_OBJECTS(tmp, IDBIndex);
    tmp->mRooted = false;
  }
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

  if (!JSVAL_IS_VOID(mCachedKeyPath)) {
    *aVal = mCachedKeyPath;
    return NS_OK;
  }

  nsresult rv = GetKeyPath().ToJSVal(aCx, &mCachedKeyPath);
  NS_ENSURE_SUCCESS(rv, rv);

  if (JSVAL_IS_GCTHING(mCachedKeyPath)) {
    NS_HOLD_JS_OBJECTS(this, IDBIndex);
    mRooted = true;
  }

  *aVal = mCachedKeyPath;
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

  nsRefPtr<IDBRequest> request;
  rv = GetInternal(keyRange, aCx, getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

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

  nsRefPtr<IDBRequest> request;
  rv = GetKeyInternal(keyRange, aCx, getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

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

  nsRefPtr<IDBRequest> request;
  rv = GetAllInternal(keyRange, aLimit, aCx, getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

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

  nsRefPtr<IDBRequest> request;
  rv = GetAllKeysInternal(keyRange, aLimit, aCx, getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

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

  nsRefPtr<IDBRequest> request = GenerateRequest(this, aCx);
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

  nsRefPtr<IDBRequest> request;
  rv = OpenKeyCursorInternal(keyRange, direction, aCx,
                             getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

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

  nsRefPtr<IDBRequest> request;
  rv = CountInternal(keyRange, aCx, getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

void
IndexHelper::ReleaseMainThreadObjects()
{
  mIndex = nsnull;
  AsyncConnectionHelper::ReleaseMainThreadObjects();
}

nsresult
IndexHelper::Dispatch(nsIEventTarget* aDatabaseThread)
{
  if (IndexedDatabaseManager::IsMainProcess()) {
    return AsyncConnectionHelper::Dispatch(aDatabaseThread);
  }

  IndexedDBIndexChild* indexActor = mIndex->GetActorChild();
  NS_ASSERTION(indexActor, "Must have an actor here!");

  IndexRequestParams params;
  nsresult rv = PackArgumentsForParentProcess(params);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NoDispatchEventTarget target;
  rv = AsyncConnectionHelper::Dispatch(&target);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mActor = new IndexedDBIndexRequestChild(this, mIndex, params.type());
  indexActor->SendPIndexedDBRequestConstructor(mActor, params);

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

void
GetKeyHelper::ReleaseMainThreadObjects()
{
  mKeyRange = nsnull;
  IndexHelper::ReleaseMainThreadObjects();
}

nsresult
GetKeyHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  NS_ASSERTION(mKeyRange, "This should never be null!");

  GetKeyParams params;

  mKeyRange->ToSerializedKeyRange(params.keyRange());

  aParams = params;
  return NS_OK;
}

HelperBase::ChildProcessSendResult
GetKeyHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  if (!actor) {
    return Success_NotSent;
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    GetKeyResponse getKeyResponse;
    getKeyResponse.key() = mKey;
    response = getKeyResponse;
  }

  if (!actor->Send__delete__(actor, response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
GetKeyHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TGetKeyResponse,
               "Bad response type!");

  mKey = aResponseValue.get_GetKeyResponse().key();
  return NS_OK;
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
      mDatabase, mCloneReadInfo);
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

void
GetHelper::ReleaseMainThreadObjects()
{
  IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
  GetKeyHelper::ReleaseMainThreadObjects();
}

nsresult
GetHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  NS_ASSERTION(mKeyRange, "This should never be null!");

  FIXME_Bug_521898_index::GetParams params;

  mKeyRange->ToSerializedKeyRange(params.keyRange());

  aParams = params;
  return NS_OK;
}

HelperBase::ChildProcessSendResult
GetHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  if (!actor) {
    return Success_NotSent;
  }

  if (!mCloneReadInfo.mFileInfos.IsEmpty()) {
    NS_WARNING("No support for transferring blobs across processes yet!");
    return Error;
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    SerializedStructuredCloneReadInfo readInfo;
    readInfo = mCloneReadInfo;
    GetResponse getResponse = readInfo;
    response = getResponse;
  }

  if (!actor->Send__delete__(actor, response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
GetHelper::UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TGetResponse,
               "Bad response type!");

  const SerializedStructuredCloneReadInfo& cloneInfo =
    aResponseValue.get_GetResponse().cloneInfo();

  NS_ASSERTION((!cloneInfo.dataLength && !cloneInfo.data) ||
               (cloneInfo.dataLength && cloneInfo.data),
               "Inconsistent clone info!");

  if (!mCloneReadInfo.SetFromSerialized(cloneInfo)) {
    NS_WARNING("Failed to copy clone buffer!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

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
GetAllKeysHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  GetAllKeysParams params;

  if (mKeyRange) {
    FIXME_Bug_521898_index::KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    params.optionalKeyRange() = keyRange;
  }
  else {
    params.optionalKeyRange() = mozilla::void_t();
  }

  params.limit() = mLimit;

  aParams = params;
  return NS_OK;
}

HelperBase::ChildProcessSendResult
GetAllKeysHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  if (!actor) {
    return Success_NotSent;
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    GetAllKeysResponse getAllKeysResponse;
    getAllKeysResponse.keys().AppendElements(mKeys);
    response = getAllKeysResponse;
  }

  if (!actor->Send__delete__(actor, response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
GetAllKeysHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TGetAllKeysResponse,
               "Bad response type!");

  mKeys.AppendElements(aResponseValue.get_GetAllKeysResponse().keys());
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
      mDatabase, *readInfo);
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

void
GetAllHelper::ReleaseMainThreadObjects()
{
  for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
    IDBObjectStore::ClearStructuredCloneBuffer(
      mCloneReadInfos[index].mCloneBuffer);
  }
  GetKeyHelper::ReleaseMainThreadObjects();
}

nsresult
GetAllHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  FIXME_Bug_521898_index::GetAllParams params;

  if (mKeyRange) {
    FIXME_Bug_521898_index::KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    params.optionalKeyRange() = keyRange;
  }
  else {
    params.optionalKeyRange() = mozilla::void_t();
  }

  params.limit() = mLimit;

  aParams = params;
  return NS_OK;
}

HelperBase::ChildProcessSendResult
GetAllHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  if (!actor) {
    return Success_NotSent;
  }

  for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
    if (!mCloneReadInfos[index].mFileInfos.IsEmpty()) {
      NS_WARNING("No support for transferring blobs across processes yet!");
      return Error;
    }
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    GetAllResponse getAllResponse;

    InfallibleTArray<SerializedStructuredCloneReadInfo>& infos =
      getAllResponse.cloneInfos();

    infos.SetCapacity(mCloneReadInfos.Length());

    for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
      SerializedStructuredCloneReadInfo* info = infos.AppendElement();
      *info = mCloneReadInfos[index];
    }

    response = getAllResponse;
  }

  if (!actor->Send__delete__(actor, response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
GetAllHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TGetAllResponse,
               "Bad response type!");

  const InfallibleTArray<SerializedStructuredCloneReadInfo>& cloneInfos =
    aResponseValue.get_GetAllResponse().cloneInfos();

  mCloneReadInfos.SetCapacity(cloneInfos.Length());

  for (PRUint32 index = 0; index < cloneInfos.Length(); index++) {
    const SerializedStructuredCloneReadInfo srcInfo = cloneInfos[index];

    StructuredCloneReadInfo* destInfo = mCloneReadInfos.AppendElement();
    if (!destInfo->SetFromSerialized(srcInfo)) {
      NS_WARNING("Failed to copy clone buffer!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

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
OpenKeyCursorHelper::EnsureCursor()
{
  if (mCursor || mKey.IsUnset()) {
    return NS_OK;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mIndex, mDirection, mRangeKey,
                      mContinueQuery, mContinueToQuery, mKey, mObjectKey);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mCursor.swap(cursor);
  return NS_OK;
}

nsresult
OpenKeyCursorHelper::GetSuccessResult(JSContext* aCx,
                                      jsval* aVal)
{
  nsresult rv = EnsureCursor();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCursor) {
    rv = WrapNative(aCx, mCursor, aVal);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else {
    *aVal = JSVAL_VOID;
  }

  return NS_OK;
}

void
OpenKeyCursorHelper::ReleaseMainThreadObjects()
{
  mKeyRange = nsnull;
  mCursor = nsnull;
  IndexHelper::ReleaseMainThreadObjects();
}

nsresult
OpenKeyCursorHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  OpenKeyCursorParams params;

  if (mKeyRange) {
    FIXME_Bug_521898_index::KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    params.optionalKeyRange() = keyRange;
  }
  else {
    params.optionalKeyRange() = mozilla::void_t();
  }

  params.direction() = mDirection;

  aParams = params;
  return NS_OK;
}

HelperBase::ChildProcessSendResult
OpenKeyCursorHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  if (!actor) {
    return Success_NotSent;
  }

  NS_ASSERTION(!mCursor, "Shouldn't have this yet!");

  if (NS_SUCCEEDED(aResultCode)) {
    nsresult rv = EnsureCursor();
    if (NS_FAILED(rv)) {
      NS_WARNING("EnsureCursor failed!");
      aResultCode = rv;
    }
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    OpenCursorResponse openCursorResponse;

    if (!mCursor) {
      openCursorResponse = mozilla::void_t();
    }
    else {
      IndexedDBIndexParent* indexActor = mIndex->GetActorParent();
      NS_ASSERTION(indexActor, "Must have an actor here!");

      IndexedDBRequestParentBase* requestActor = mRequest->GetActorParent();
      NS_ASSERTION(requestActor, "Must have an actor here!");

      IndexCursorConstructorParams params;
      params.requestParent() = requestActor;
      params.direction() = mDirection;
      params.key() = mKey;
      params.objectKey() = mObjectKey;
      params.optionalCloneInfo() = mozilla::void_t();

      IndexedDBCursorParent* cursorActor = new IndexedDBCursorParent(mCursor);

      if (!indexActor->SendPIndexedDBCursorConstructor(cursorActor, params)) {
        return Error;
      }

      openCursorResponse = cursorActor;
    }

    response = openCursorResponse;
  }

  if (!actor->Send__delete__(actor, response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
OpenKeyCursorHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TOpenCursorResponse,
               "Bad response type!");
  NS_ASSERTION(aResponseValue.get_OpenCursorResponse().type() ==
               OpenCursorResponse::Tvoid_t ||
               aResponseValue.get_OpenCursorResponse().type() ==
               OpenCursorResponse::TPIndexedDBCursorChild,
               "Bad response union type!");
  NS_ASSERTION(!mCursor, "Shouldn't have this yet!");

  const OpenCursorResponse& response =
    aResponseValue.get_OpenCursorResponse();

  switch (response.type()) {
    case OpenCursorResponse::Tvoid_t:
      break;

    case OpenCursorResponse::TPIndexedDBCursorChild: {
      IndexedDBCursorChild* actor =
        static_cast<IndexedDBCursorChild*>(
          response.get_PIndexedDBCursorChild());

      mCursor = actor->ForgetStrongCursor();
      NS_ASSERTION(mCursor, "This should never be null!");

    } break;

    default:
      NS_NOTREACHED("Unknown response union type!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
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
    mDatabase, mCloneReadInfo);
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
OpenCursorHelper::EnsureCursor()
{
  if (mCursor || mKey.IsUnset()) {
    return NS_OK;
  }

  mSerializedCloneReadInfo = mCloneReadInfo;

  NS_ASSERTION(mSerializedCloneReadInfo.data &&
               mSerializedCloneReadInfo.dataLength,
               "Shouldn't be possible!");

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mIndex, mDirection, mRangeKey,
                      mContinueQuery, mContinueToQuery, mKey, mObjectKey,
                      mCloneReadInfo);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(!mCloneReadInfo.mCloneBuffer.data(), "Should have swapped!");

  mCursor.swap(cursor);
  return NS_OK;
}

void
OpenCursorHelper::ReleaseMainThreadObjects()
{
  IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);

  // These don't need to be released on the main thread but they're only valid
  // as long as mCursor is set.
  mSerializedCloneReadInfo.data = nsnull;
  mSerializedCloneReadInfo.dataLength = 0;

  OpenKeyCursorHelper::ReleaseMainThreadObjects();
}

nsresult
OpenCursorHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  FIXME_Bug_521898_index::OpenCursorParams params;

  if (mKeyRange) {
    FIXME_Bug_521898_index::KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    params.optionalKeyRange() = keyRange;
  }
  else {
    params.optionalKeyRange() = mozilla::void_t();
  }

  params.direction() = mDirection;

  aParams = params;
  return NS_OK;
}

HelperBase::ChildProcessSendResult
OpenCursorHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  if (!actor) {
    return Success_NotSent;
  }

  if (!mCloneReadInfo.mFileInfos.IsEmpty()) {
    NS_WARNING("No support for transferring blobs across processes yet!");
    return Error;
  }

  NS_ASSERTION(!mCursor, "Shouldn't have this yet!");

  if (NS_SUCCEEDED(aResultCode)) {
    nsresult rv = EnsureCursor();
    if (NS_FAILED(rv)) {
      NS_WARNING("EnsureCursor failed!");
      aResultCode = rv;
    }
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    OpenCursorResponse openCursorResponse;

    if (!mCursor) {
      openCursorResponse = mozilla::void_t();
    }
    else {
      IndexedDBIndexParent* indexActor = mIndex->GetActorParent();
      NS_ASSERTION(indexActor, "Must have an actor here!");

      IndexedDBRequestParentBase* requestActor = mRequest->GetActorParent();
      NS_ASSERTION(requestActor, "Must have an actor here!");

      NS_ASSERTION(mSerializedCloneReadInfo.data &&
                   mSerializedCloneReadInfo.dataLength,
                   "Shouldn't be possible!");

      IndexCursorConstructorParams params;
      params.requestParent() = requestActor;
      params.direction() = mDirection;
      params.key() = mKey;
      params.objectKey() = mObjectKey;
      params.optionalCloneInfo() = mSerializedCloneReadInfo;

      IndexedDBCursorParent* cursorActor = new IndexedDBCursorParent(mCursor);

      if (!indexActor->SendPIndexedDBCursorConstructor(cursorActor, params)) {
        return Error;
      }

      openCursorResponse = cursorActor;
    }

    response = openCursorResponse;
  }

  if (!actor->Send__delete__(actor, response)) {
    return Error;
  }

  return Success_Sent;
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
  if (!JS_NewNumberValue(aCx, static_cast<double>(mCount), aVal)) {
    NS_WARNING("Failed to make number value!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void
CountHelper::ReleaseMainThreadObjects()
{
  mKeyRange = nsnull;
  IndexHelper::ReleaseMainThreadObjects();
}

nsresult
CountHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  FIXME_Bug_521898_index::CountParams params;

  if (mKeyRange) {
    FIXME_Bug_521898_index::KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    params.optionalKeyRange() = keyRange;
  }
  else {
    params.optionalKeyRange() = mozilla::void_t();
  }

  aParams = params;
  return NS_OK;
}

HelperBase::ChildProcessSendResult
CountHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  if (!actor) {
    return Success_NotSent;
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    CountResponse countResponse = mCount;
    response = countResponse;
  }

  if (!actor->Send__delete__(actor, response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
CountHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TCountResponse,
               "Bad response type!");

  mCount = aResponseValue.get_CountResponse().count();
  return NS_OK;
}
