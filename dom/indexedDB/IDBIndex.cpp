/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IDBIndex.h"

#include <algorithm>
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/storage.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

#include "ipc/IndexedDBChild.h"
#include "ipc/IndexedDBParent.h"

#include "IndexedDatabaseInlines.h"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::dom;
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::ErrorResult;
using mozilla::Move;

namespace {

class IndexHelper : public AsyncConnectionHelper
{
public:
  IndexHelper(IDBTransaction* aTransaction,
              IDBRequest* aRequest,
              IDBIndex* aIndex)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mActor(nullptr)
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
                                    JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

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
    IDBObjectStore::ClearCloneReadInfo(mCloneReadInfo);
  }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

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
                   const uint32_t aLimit)
  : GetKeyHelper(aTransaction, aRequest, aIndex, aKeyRange), mLimit(aLimit)
  { }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

protected:
  const uint32_t mLimit;
  nsTArray<Key> mKeys;
};

class GetAllHelper : public GetKeyHelper
{
public:
  GetAllHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               IDBIndex* aIndex,
               IDBKeyRange* aKeyRange,
               const uint32_t aLimit)
  : GetKeyHelper(aTransaction, aRequest, aIndex, aKeyRange), mLimit(aLimit)
  { }

  ~GetAllHelper()
  {
    for (uint32_t index = 0; index < mCloneReadInfos.Length(); index++) {
      IDBObjectStore::ClearCloneReadInfo(mCloneReadInfos[index]);
    }
  }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

protected:
  const uint32_t mLimit;
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
                                    JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

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
    IDBObjectStore::ClearCloneReadInfo(mCloneReadInfo);
  }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

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
                                    JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

private:
  nsRefPtr<IDBKeyRange> mKeyRange;
  uint64_t mCount;
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
: mId(INT64_MIN),
  mKeyPath(0),
  mCachedKeyPath(JSVAL_VOID),
  mActorChild(nullptr),
  mActorParent(nullptr),
  mUnique(false),
  mMultiEntry(false),
  mRooted(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SetIsDOMBinding();
}

IDBIndex::~IDBIndex()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mActorParent, "Actor parent owns us, how can we be dying?!");

  if (mRooted) {
    mCachedKeyPath = JSVAL_VOID;
    mozilla::DropJSObjects(this);
  }

  if (mActorChild) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

already_AddRefed<IDBRequest>
IDBIndex::GetInternal(IDBKeyRange* aKeyRange, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  if (!request) {
    IDB_WARNING("Failed to generate request!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<GetHelper> helper =
    new GetHelper(transaction, request, this, aKeyRange);

  nsresult rv = helper->DispatchToTransactionPool();
  if (NS_FAILED(rv)) {
    IDB_WARNING("Failed to dispatch!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  IDB_PROFILER_MARK("IndexedDB Request %llu: "
                    "database(%s).transaction(%s).objectStore(%s).index(%s)."
                    "get(%s)",
                    "IDBRequest[%llu] MT IDBIndex.get()",
                    request->GetSerialNumber(),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()->
                                        Database()),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()),
                    IDB_PROFILER_STRING(ObjectStore()),
                    IDB_PROFILER_STRING(this), IDB_PROFILER_STRING(aKeyRange));

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::GetKeyInternal(IDBKeyRange* aKeyRange, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  if (!request) {
    IDB_WARNING("Failed to generate request!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<GetKeyHelper> helper =
    new GetKeyHelper(transaction, request, this, aKeyRange);

  nsresult rv = helper->DispatchToTransactionPool();
  if (NS_FAILED(rv)) {
    IDB_WARNING("Failed to dispatch!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  IDB_PROFILER_MARK("IndexedDB Request %llu: "
                    "database(%s).transaction(%s).objectStore(%s).index(%s)."
                    "getKey(%s)",
                    "IDBRequest[%llu] MT IDBIndex.getKey()",
                    request->GetSerialNumber(),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()->
                                        Database()),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()),
                    IDB_PROFILER_STRING(ObjectStore()),
                    IDB_PROFILER_STRING(this), IDB_PROFILER_STRING(aKeyRange));

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::GetAllInternal(IDBKeyRange* aKeyRange, uint32_t aLimit,
                         ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  if (!request) {
    IDB_WARNING("Failed to generate request!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(transaction, request, this, aKeyRange, aLimit);

  nsresult rv = helper->DispatchToTransactionPool();
  if (NS_FAILED(rv)) {
    IDB_WARNING("Failed to dispatch!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  IDB_PROFILER_MARK("IndexedDB Request %llu: "
                    "database(%s).transaction(%s).objectStore(%s).index(%s)."
                    "getAll(%s, %lu)",
                    "IDBRequest[%llu] MT IDBIndex.getAll()",
                    request->GetSerialNumber(),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()->
                                        Database()),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()),
                    IDB_PROFILER_STRING(ObjectStore()),
                    IDB_PROFILER_STRING(this),
                    IDB_PROFILER_STRING(aKeyRange), aLimit);

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::GetAllKeysInternal(IDBKeyRange* aKeyRange, uint32_t aLimit,
                             ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  if (!request) {
    IDB_WARNING("Failed to generate request!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<GetAllKeysHelper> helper =
    new GetAllKeysHelper(transaction, request, this, aKeyRange, aLimit);

  nsresult rv = helper->DispatchToTransactionPool();
  if (NS_FAILED(rv)) {
    IDB_WARNING("Failed to dispatch!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  IDB_PROFILER_MARK("IndexedDB Request %llu: "
                    "database(%s).transaction(%s).objectStore(%s).index(%s)."
                    "getAllKeys(%s, %lu)",
                    "IDBRequest[%llu] MT IDBIndex.getAllKeys()",
                    request->GetSerialNumber(),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()->
                                        Database()),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()),
                    IDB_PROFILER_STRING(ObjectStore()),
                    IDB_PROFILER_STRING(this), IDB_PROFILER_STRING(aKeyRange),
                    aLimit);

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::CountInternal(IDBKeyRange* aKeyRange, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  if (!request) {
    IDB_WARNING("Failed to generate request!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<CountHelper> helper =
    new CountHelper(transaction, request, this, aKeyRange);

  nsresult rv = helper->DispatchToTransactionPool();
  if (NS_FAILED(rv)) {
    IDB_WARNING("Failed to dispatch!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  IDB_PROFILER_MARK("IndexedDB Request %llu: "
                    "database(%s).transaction(%s).objectStore(%s).index(%s)."
                    "count(%s)",
                    "IDBRequest[%llu] MT IDBIndex.count()",
                    request->GetSerialNumber(),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()->
                                        Database()),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()),
                    IDB_PROFILER_STRING(ObjectStore()),
                    IDB_PROFILER_STRING(this), IDB_PROFILER_STRING(aKeyRange));

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::OpenKeyCursorInternal(IDBKeyRange* aKeyRange, size_t aDirection,
                                ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  IDBCursor::Direction direction =
    static_cast<IDBCursor::Direction>(aDirection);

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  if (!request) {
    IDB_WARNING("Failed to generate request!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<OpenKeyCursorHelper> helper =
    new OpenKeyCursorHelper(transaction, request, this, aKeyRange, direction);

  nsresult rv = helper->DispatchToTransactionPool();
  if (NS_FAILED(rv)) {
    IDB_WARNING("Failed to dispatch!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  IDB_PROFILER_MARK("IndexedDB Request %llu: "
                    "database(%s).transaction(%s).objectStore(%s).index(%s)."
                    "openKeyCursor(%s)",
                    "IDBRequest[%llu] MT IDBIndex.openKeyCursor()",
                    request->GetSerialNumber(),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()->
                                        Database()),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()),
                    IDB_PROFILER_STRING(ObjectStore()),
                    IDB_PROFILER_STRING(this), IDB_PROFILER_STRING(aKeyRange),
                    IDB_PROFILER_STRING(direction));

  return request.forget();
}

nsresult
IDBIndex::OpenCursorInternal(IDBKeyRange* aKeyRange,
                             size_t aDirection,
                             IDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  IDBCursor::Direction direction =
    static_cast<IDBCursor::Direction>(aDirection);

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  IDB_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(transaction, request, this, aKeyRange, direction);

  nsresult rv = helper->DispatchToTransactionPool();
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  IDB_PROFILER_MARK("IndexedDB Request %llu: "
                    "database(%s).transaction(%s).objectStore(%s).index(%s)."
                    "openCursor(%s)",
                    "IDBRequest[%llu] MT IDBIndex.openCursor()",
                    request->GetSerialNumber(),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()->
                                        Database()),
                    IDB_PROFILER_STRING(ObjectStore()->Transaction()),
                    IDB_PROFILER_STRING(ObjectStore()),
                    IDB_PROFILER_STRING(this), IDB_PROFILER_STRING(aKeyRange),
                    IDB_PROFILER_STRING(direction));

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
  IDB_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

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
                            nsTArray<StructuredCloneFile>& aBlobs,
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
    IDB_WARNING("Failed to copy clone buffer!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  cloneInfo.mFiles.SwapElements(aBlobs);

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(aRequest, mObjectStore->Transaction(), this, direction,
                      Key(), EmptyCString(), EmptyCString(), aKey, aObjectKey,
                      Move(cloneInfo));
  IDB_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(!cloneInfo.mCloneBuffer.data(), "Should have swapped!");

  cursor.forget(_retval);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBIndex)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBIndex)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedKeyPath)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBIndex)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObjectStore)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBIndex)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER

  // Don't unlink mObjectStore!

  tmp->mCachedKeyPath = JSVAL_VOID;

  if (tmp->mRooted) {
    mozilla::DropJSObjects(tmp);
    tmp->mRooted = false;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBIndex)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBIndex)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBIndex)

JSObject*
IDBIndex::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return IDBIndexBinding::Wrap(aCx, aScope, this);
}

JS::Value
IDBIndex::GetKeyPath(JSContext* aCx, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!JSVAL_IS_VOID(mCachedKeyPath)) {
    return mCachedKeyPath;
  }

  aRv = GetKeyPath().ToJSVal(aCx, mCachedKeyPath);
  ENSURE_SUCCESS(aRv, JSVAL_VOID);

  if (JSVAL_IS_GCTHING(mCachedKeyPath)) {
    mozilla::HoldJSObjects(this);
    mRooted = true;
  }

  return mCachedKeyPath;
}

already_AddRefed<IDBRequest>
IDBIndex::Get(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  ENSURE_SUCCESS(aRv, nullptr);

  if (!keyRange) {
    // Must specify a key or keyRange for getKey().
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return nullptr;
  }

  return GetInternal(keyRange, aRv);
}

already_AddRefed<IDBRequest>
IDBIndex::GetKey(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  ENSURE_SUCCESS(aRv, nullptr);

  if (!keyRange) {
    // Must specify a key or keyRange for get().
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return nullptr;
  }

  return GetKeyInternal(keyRange, aRv);
}

already_AddRefed<IDBRequest>
IDBIndex::GetAll(JSContext* aCx, JS::Handle<JS::Value> aKey,
                 const Optional<uint32_t>& aLimit, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  ENSURE_SUCCESS(aRv, nullptr);

  uint32_t limit = UINT32_MAX;
  if (aLimit.WasPassed() && aLimit.Value() > 0) {
    limit = aLimit.Value();
  }

  return GetAllInternal(keyRange, limit, aRv);
}

already_AddRefed<IDBRequest>
IDBIndex::GetAllKeys(JSContext* aCx,
                     JS::Handle<JS::Value> aKey,
                     const Optional<uint32_t>& aLimit, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  ENSURE_SUCCESS(aRv, nullptr);

  uint32_t limit = UINT32_MAX;
  if (aLimit.WasPassed() && aLimit.Value() > 0) {
    limit = aLimit.Value();
  }

  return GetAllKeysInternal(keyRange, limit, aRv);
}

already_AddRefed<IDBRequest>
IDBIndex::OpenCursor(JSContext* aCx,
                     JS::Handle<JS::Value> aRange,
                     IDBCursorDirection aDirection, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aRange, getter_AddRefs(keyRange));
  ENSURE_SUCCESS(aRv, nullptr);

  IDBCursor::Direction direction = IDBCursor::ConvertDirection(aDirection);

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  if (!request) {
    IDB_WARNING("Failed to generate request!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(transaction, request, this, keyRange, direction);

  nsresult rv = helper->DispatchToTransactionPool();
  if (NS_FAILED(rv)) {
    IDB_WARNING("Failed to dispatch!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::OpenKeyCursor(JSContext* aCx,
                        JS::Handle<JS::Value> aRange,
                        IDBCursorDirection aDirection, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aRange, getter_AddRefs(keyRange));
  ENSURE_SUCCESS(aRv, nullptr);

  IDBCursor::Direction direction = IDBCursor::ConvertDirection(aDirection);

  return OpenKeyCursorInternal(keyRange, direction, aRv);
}

already_AddRefed<IDBRequest>
IDBIndex::Count(JSContext* aCx, JS::Handle<JS::Value> aKey,
                ErrorResult& aRv)
{
  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  ENSURE_SUCCESS(aRv, nullptr);

  return CountInternal(keyRange, aRv);
}

void
IndexHelper::ReleaseMainThreadObjects()
{
  mIndex = nullptr;
  AsyncConnectionHelper::ReleaseMainThreadObjects();
}

nsresult
IndexHelper::Dispatch(nsIEventTarget* aDatabaseThread)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB", "IndexHelper::Dispatch");

  if (IndexedDatabaseManager::IsMainProcess()) {
    return AsyncConnectionHelper::Dispatch(aDatabaseThread);
  }

  // If we've been invalidated then there's no point sending anything to the
  // parent process.
  if (mIndex->ObjectStore()->Transaction()->Database()->IsInvalidated()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  IndexedDBIndexChild* indexActor = mIndex->GetActorChild();
  NS_ASSERTION(indexActor, "Must have an actor here!");

  IndexRequestParams params;
  nsresult rv = PackArgumentsForParentProcess(params);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NoDispatchEventTarget target;
  rv = AsyncConnectionHelper::Dispatch(&target);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mActor = new IndexedDBIndexRequestChild(this, mIndex, params.type());
  indexActor->SendPIndexedDBRequestConstructor(mActor, params);

  return NS_OK;
}

nsresult
GetKeyHelper::DoDatabaseWork(mozIStorageConnection* /* aConnection */)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(mKeyRange, "Must have a key range here!");

  PROFILER_LABEL("IndexedDB", "GetKeyHelper::DoDatabaseWork");

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
  IDB_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = mKeyRange->BindToStatement(stmt);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (hasResult) {
    rv = mKey.SetFromStatement(stmt, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
GetKeyHelper::GetSuccessResult(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aVal)
{
  return mKey.ToJSVal(aCx, aVal);
}

void
GetKeyHelper::ReleaseMainThreadObjects()
{
  mKeyRange = nullptr;
  IndexHelper::ReleaseMainThreadObjects();
}

nsresult
GetKeyHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(mKeyRange, "This should never be null!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetKeyHelper::PackArgumentsForParentProcess");

  GetKeyParams params;

  mKeyRange->ToSerializedKeyRange(params.keyRange());

  aParams = params;
  return NS_OK;
}

AsyncConnectionHelper::ChildProcessSendResult
GetKeyHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetKeyHelper::SendResponseToChildProcess");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  NS_ASSERTION(actor, "How did we get this far without an actor?");

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    GetKeyResponse getKeyResponse;
    getKeyResponse.key() = mKey;
    response = getKeyResponse;
  }

  if (!actor->SendResponse(response)) {
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
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(mKeyRange, "Must have a key range here!");

  PROFILER_LABEL("IndexedDB", "GetHelper::DoDatabaseWork [IDBIndex.cpp]");

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
  IDB_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = mKeyRange->BindToStatement(stmt);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (hasResult) {
    rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(stmt, 0, 1,
      mDatabase, mCloneReadInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
GetHelper::GetSuccessResult(JSContext* aCx,
                            JS::MutableHandle<JS::Value> aVal)
{
  bool result = IDBObjectStore::DeserializeValue(aCx, mCloneReadInfo, aVal);

  mCloneReadInfo.mCloneBuffer.clear();

  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  return NS_OK;
}

void
GetHelper::ReleaseMainThreadObjects()
{
  IDBObjectStore::ClearCloneReadInfo(mCloneReadInfo);
  GetKeyHelper::ReleaseMainThreadObjects();
}

nsresult
GetHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(mKeyRange, "This should never be null!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetHelper::PackArgumentsForParentProcess "
                             "[IDBIndex.cpp]");

  GetParams params;

  mKeyRange->ToSerializedKeyRange(params.keyRange());

  aParams = params;
  return NS_OK;
}

AsyncConnectionHelper::ChildProcessSendResult
GetHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetHelper::SendResponseToChildProcess "
                             "[IDBIndex.cpp]");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  NS_ASSERTION(actor, "How did we get this far without an actor?");

  InfallibleTArray<PBlobParent*> blobsParent;

  if (NS_SUCCEEDED(aResultCode)) {
    IDBDatabase* database = mIndex->ObjectStore()->Transaction()->Database();
    NS_ASSERTION(database, "This should never be null!");

    ContentParent* contentParent = database->GetContentParent();
    NS_ASSERTION(contentParent, "This should never be null!");

    FileManager* fileManager = database->Manager();
    NS_ASSERTION(fileManager, "This should never be null!");

    const nsTArray<StructuredCloneFile>& files = mCloneReadInfo.mFiles;

    aResultCode =
      IDBObjectStore::ConvertBlobsToActors(contentParent, fileManager, files,
                                           blobsParent);
    if (NS_FAILED(aResultCode)) {
      NS_WARNING("ConvertBlobActors failed!");
    }
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    GetResponse getResponse;
    getResponse.cloneInfo() = mCloneReadInfo;
    getResponse.blobsParent().SwapElements(blobsParent);
    response = getResponse;
  }

  if (!actor->SendResponse(response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
GetHelper::UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TGetResponse,
               "Bad response type!");

  const GetResponse& getResponse = aResponseValue.get_GetResponse();
  const SerializedStructuredCloneReadInfo& cloneInfo = getResponse.cloneInfo();

  NS_ASSERTION((!cloneInfo.dataLength && !cloneInfo.data) ||
               (cloneInfo.dataLength && cloneInfo.data),
               "Inconsistent clone info!");

  if (!mCloneReadInfo.SetFromSerialized(cloneInfo)) {
    IDB_WARNING("Failed to copy clone buffer!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  IDBObjectStore::ConvertActorsToBlobs(getResponse.blobsChild(),
                                       mCloneReadInfo.mFiles);
  return NS_OK;
}

nsresult
GetAllKeysHelper::DoDatabaseWork(mozIStorageConnection* /* aConnection */)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());

  PROFILER_LABEL("IndexedDB",
                 "GetAllKeysHelper::DoDatabaseWork [IDBIndex.cpp]");

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
  if (mLimit != UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT object_data_key FROM ") +
                    tableName +
                    NS_LITERAL_CSTRING(" WHERE index_id = :index_id") +
                    keyRangeClause + limitClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  IDB_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    rv = mKeyRange->BindToStatement(stmt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mKeys.SetCapacity(std::min<uint32_t>(50, mLimit));

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
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
GetAllKeysHelper::GetSuccessResult(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aVal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mKeys.Length() <= mLimit);

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetAllKeysHelper::GetSuccessResult "
                             "[IDBIndex.cpp]");

  nsTArray<Key> keys;
  mKeys.SwapElements(keys);

  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0));
  if (!array) {
    IDB_WARNING("Failed to make array!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (!keys.IsEmpty()) {
    if (!JS_SetArrayLength(aCx, array, uint32_t(keys.Length()))) {
      IDB_WARNING("Failed to set array length!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    for (uint32_t index = 0, count = keys.Length(); index < count; index++) {
      const Key& key = keys[index];
      NS_ASSERTION(!key.IsUnset(), "Bad key!");

      JS::Rooted<JS::Value> value(aCx);
      nsresult rv = key.ToJSVal(aCx, &value);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to get jsval for key!");
        return rv;
      }

      if (!JS_SetElement(aCx, array, index, value)) {
        IDB_WARNING("Failed to set array element!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }
  }

  aVal.setObject(*array);
  return NS_OK;
}

nsresult
GetAllKeysHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IndexedDatabaseManager::IsMainProcess());

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetAllKeysHelper::PackArgumentsForParentProcess "
                             "[IDBIndex.cpp]");

  GetAllKeysParams params;

  if (mKeyRange) {
    KeyRange keyRange;
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

AsyncConnectionHelper::ChildProcessSendResult
GetAllKeysHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetAllKeysHelper::SendResponseToChildProcess "
                             "[IDBIndex.cpp]");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  NS_ASSERTION(actor, "How did we get this far without an actor?");

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    GetAllKeysResponse getAllKeysResponse;
    getAllKeysResponse.keys().AppendElements(mKeys);
    response = getAllKeysResponse;
  }

  if (!actor->SendResponse(response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
GetAllKeysHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TGetAllKeysResponse);

  mKeys.AppendElements(aResponseValue.get_GetAllKeysResponse().keys());
  return NS_OK;
}

nsresult
GetAllHelper::DoDatabaseWork(mozIStorageConnection* /* aConnection */)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("IndexedDB", "GetAllHelper::DoDatabaseWork [IDBIndex.cpp]");

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
  if (mLimit != UINT32_MAX) {
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
  IDB_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

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
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
GetAllHelper::GetSuccessResult(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aVal)
{
  NS_ASSERTION(mCloneReadInfos.Length() <= mLimit, "Too many results!");

  nsresult rv = ConvertToArrayAndCleanup(aCx, mCloneReadInfos, aVal);

  NS_ASSERTION(mCloneReadInfos.IsEmpty(),
               "Should have cleared in ConvertToArrayAndCleanup");
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
GetAllHelper::ReleaseMainThreadObjects()
{
  for (uint32_t index = 0; index < mCloneReadInfos.Length(); index++) {
    IDBObjectStore::ClearCloneReadInfo(mCloneReadInfos[index]);
  }
  GetKeyHelper::ReleaseMainThreadObjects();
}

nsresult
GetAllHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetAllHelper::PackArgumentsForParentProcess "
                             "[IDBIndex.cpp]");

  GetAllParams params;

  if (mKeyRange) {
    KeyRange keyRange;
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

AsyncConnectionHelper::ChildProcessSendResult
GetAllHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "GetAllHelper::SendResponseToChildProcess "
                             "[IDBIndex.cpp]");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  NS_ASSERTION(actor, "How did we get this far without an actor?");

  GetAllResponse getAllResponse;

  if (NS_SUCCEEDED(aResultCode) && !mCloneReadInfos.IsEmpty()) {
    IDBDatabase* database = mIndex->ObjectStore()->Transaction()->Database();
    NS_ASSERTION(database, "This should never be null!");

    ContentParent* contentParent = database->GetContentParent();
    NS_ASSERTION(contentParent, "This should never be null!");

    FileManager* fileManager = database->Manager();
    NS_ASSERTION(fileManager, "This should never be null!");

    uint32_t length = mCloneReadInfos.Length();

    InfallibleTArray<SerializedStructuredCloneReadInfo>& infos =
      getAllResponse.cloneInfos();
    infos.SetCapacity(length);

    InfallibleTArray<BlobArray>& blobArrays = getAllResponse.blobs();
    blobArrays.SetCapacity(length);

    for (uint32_t index = 0;
         NS_SUCCEEDED(aResultCode) && index < length;
         index++) {
      const StructuredCloneReadInfo& clone = mCloneReadInfos[index];

      // Append the structured clone data.
      SerializedStructuredCloneReadInfo* info = infos.AppendElement();
      *info = clone;

      const nsTArray<StructuredCloneFile>& files = clone.mFiles;

      // Now take care of the files.
      BlobArray* blobArray = blobArrays.AppendElement();

      InfallibleTArray<PBlobParent*>& blobs = blobArray->blobsParent();

      aResultCode =
        IDBObjectStore::ConvertBlobsToActors(contentParent, fileManager, files,
                                             blobs);
      if (NS_FAILED(aResultCode)) {
        NS_WARNING("ConvertBlobsToActors failed!");
        break;
      }
    }
  }

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    response = getAllResponse;
  }

  if (!actor->SendResponse(response)) {
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

  const GetAllResponse& getAllResponse = aResponseValue.get_GetAllResponse();
  const InfallibleTArray<SerializedStructuredCloneReadInfo>& cloneInfos =
    getAllResponse.cloneInfos();
  const InfallibleTArray<BlobArray>& blobArrays = getAllResponse.blobs();

  mCloneReadInfos.SetCapacity(cloneInfos.Length());

  for (uint32_t index = 0; index < cloneInfos.Length(); index++) {
    const SerializedStructuredCloneReadInfo srcInfo = cloneInfos[index];
    const InfallibleTArray<PBlobChild*>& blobs = blobArrays[index].blobsChild();

    StructuredCloneReadInfo* destInfo = mCloneReadInfos.AppendElement();
    if (!destInfo->SetFromSerialized(srcInfo)) {
      IDB_WARNING("Failed to copy clone buffer!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    IDBObjectStore::ConvertActorsToBlobs(blobs, destInfo->mFiles);
  }

  return NS_OK;
}

nsresult
OpenKeyCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(aConnection, "Passed a null connection!");

  PROFILER_LABEL("IndexedDB", "OpenKeyCursorHelper::DoDatabaseWork");

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

  nsAutoCString directionClause(" ORDER BY value ");
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
  IDB_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    rv = mKeyRange->BindToStatement(stmt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!hasResult) {
    mKey.Unset();
    return NS_OK;
  }

  rv = mKey.SetFromStatement(stmt, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mObjectKey.SetFromStatement(stmt, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now we need to make the query to get the next match.
  nsAutoCString queryStart = NS_LITERAL_CSTRING("SELECT value, object_data_key"
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
  IDB_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mCursor.swap(cursor);
  return NS_OK;
}

nsresult
OpenKeyCursorHelper::GetSuccessResult(JSContext* aCx,
                                      JS::MutableHandle<JS::Value> aVal)
{
  nsresult rv = EnsureCursor();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCursor) {
    rv = WrapNative(aCx, mCursor, aVal);
    IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else {
    aVal.setUndefined();
  }

  return NS_OK;
}

void
OpenKeyCursorHelper::ReleaseMainThreadObjects()
{
  mKeyRange = nullptr;
  mCursor = nullptr;
  IndexHelper::ReleaseMainThreadObjects();
}

nsresult
OpenKeyCursorHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "OpenKeyCursorHelper::"
                             "PackArgumentsForParentProcess [IDBIndex.cpp]");

  OpenKeyCursorParams params;

  if (mKeyRange) {
    KeyRange keyRange;
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

AsyncConnectionHelper::ChildProcessSendResult
OpenKeyCursorHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(!mCursor, "Shouldn't have this yet!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "OpenKeyCursorHelper::SendResponseToChildProcess");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  NS_ASSERTION(actor, "How did we get this far without an actor?");

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

      if (!indexActor->OpenCursor(mCursor, params, openCursorResponse)) {
        return Error;
      }
    }

    response = openCursorResponse;
  }

  if (!actor->SendResponse(response)) {
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
      MOZ_CRASH();
  }

  return NS_OK;
}

nsresult
OpenCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(aConnection, "Passed a null connection!");

  PROFILER_LABEL("IndexedDB",
                 "OpenCursorHelper::DoDatabaseWork [IDBIndex.cpp]");

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

  nsAutoCString directionClause(" ORDER BY index_table.value ");
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
  IDB_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mIndex->Id());
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    rv = mKeyRange->BindToStatement(stmt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

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
  nsAutoCString queryStart =
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
                      Move(mCloneReadInfo));
  IDB_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(!mCloneReadInfo.mCloneBuffer.data(), "Should have swapped!");

  mCursor.swap(cursor);
  return NS_OK;
}

void
OpenCursorHelper::ReleaseMainThreadObjects()
{
  IDBObjectStore::ClearCloneReadInfo(mCloneReadInfo);

  // These don't need to be released on the main thread but they're only valid
  // as long as mCursor is set.
  mSerializedCloneReadInfo.data = nullptr;
  mSerializedCloneReadInfo.dataLength = 0;

  OpenKeyCursorHelper::ReleaseMainThreadObjects();
}

nsresult
OpenCursorHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "OpenCursorHelper::PackArgumentsForParentProcess "
                             "[IDBIndex.cpp]");

  OpenCursorParams params;

  if (mKeyRange) {
    KeyRange keyRange;
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

AsyncConnectionHelper::ChildProcessSendResult
OpenCursorHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(!mCursor, "Shouldn't have this yet!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "OpenCursorHelper::SendResponseToChildProcess "
                             "[IDBIndex.cpp]");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  NS_ASSERTION(actor, "How did we get this far without an actor?");

  InfallibleTArray<PBlobParent*> blobsParent;

  if (NS_SUCCEEDED(aResultCode)) {
    IDBDatabase* database = mIndex->ObjectStore()->Transaction()->Database();
    NS_ASSERTION(database, "This should never be null!");

    ContentParent* contentParent = database->GetContentParent();
    NS_ASSERTION(contentParent, "This should never be null!");

    FileManager* fileManager = database->Manager();
    NS_ASSERTION(fileManager, "This should never be null!");

    const nsTArray<StructuredCloneFile>& files = mCloneReadInfo.mFiles;

    aResultCode =
      IDBObjectStore::ConvertBlobsToActors(contentParent, fileManager, files,
                                           blobsParent);
    if (NS_FAILED(aResultCode)) {
      NS_WARNING("ConvertBlobsToActors failed!");
    }
  }

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
      params.blobsParent().SwapElements(blobsParent);

      if (!indexActor->OpenCursor(mCursor, params, openCursorResponse)) {
        return Error;
      }
    }

    response = openCursorResponse;
  }

  if (!actor->SendResponse(response)) {
    return Error;
  }

  return Success_Sent;
}

nsresult
CountHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("IndexedDB", "CountHelper::DoDatabaseWork [IDBIndex.cpp]");

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

  nsAutoCString keyRangeClause;
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
  IDB_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mIndex->Id());
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

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
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  IDB_ENSURE_TRUE(hasResult, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mCount = stmt->AsInt64(0);
  return NS_OK;
}

nsresult
CountHelper::GetSuccessResult(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aVal)
{
  aVal.setNumber(static_cast<double>(mCount));
  return NS_OK;
}

void
CountHelper::ReleaseMainThreadObjects()
{
  mKeyRange = nullptr;
  IndexHelper::ReleaseMainThreadObjects();
}

nsresult
CountHelper::PackArgumentsForParentProcess(IndexRequestParams& aParams)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "CountHelper::PackArgumentsForParentProcess "
                             "[IDBIndex.cpp]");

  CountParams params;

  if (mKeyRange) {
    KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    params.optionalKeyRange() = keyRange;
  }
  else {
    params.optionalKeyRange() = mozilla::void_t();
  }

  aParams = params;
  return NS_OK;
}

AsyncConnectionHelper::ChildProcessSendResult
CountHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "CountHelper::SendResponseToChildProcess "
                             "[IDBIndex.cpp]");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  NS_ASSERTION(actor, "How did we get this far without an actor?");

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    CountResponse countResponse = mCount;
    response = countResponse;
  }

  if (!actor->SendResponse(response)) {
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
