/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IDBCursor.h"

#include "mozilla/storage.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsEventDispatcher.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "ProfilerHelpers.h"
#include "TransactionThreadPool.h"

#include "ipc/IndexedDBChild.h"
#include "ipc/IndexedDBParent.h"

#include "IndexedDatabaseInlines.h"
#include "mozilla/dom/BindingDeclarations.h"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::dom::Optional;
using mozilla::ErrorResult;

static_assert(sizeof(size_t) >= sizeof(IDBCursor::Direction),
              "Relying on conversion between size_t and "
              "IDBCursor::Direction");

namespace {

class CursorHelper : public AsyncConnectionHelper
{
public:
  CursorHelper(IDBCursor* aCursor)
  : AsyncConnectionHelper(aCursor->Transaction(), aCursor->Request()),
    mCursor(aCursor), mActor(nullptr)
  {
    NS_ASSERTION(aCursor, "Null cursor!");
  }

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult Dispatch(nsIEventTarget* aDatabaseThread) MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(CursorRequestParams& aParams) = 0;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue) = 0;

protected:
  nsRefPtr<IDBCursor> mCursor;

private:
  IndexedDBCursorRequestChild* mActor;
};

} // anonymous namespace

BEGIN_INDEXEDDB_NAMESPACE

class ContinueHelper : public CursorHelper
{
public:
  ContinueHelper(IDBCursor* aCursor,
                 int32_t aCount)
  : CursorHelper(aCursor), mCount(aCount)
  {
    NS_ASSERTION(aCount > 0, "Must have a count!");
  }

  ~ContinueHelper()
  {
    IDBObjectStore::ClearCloneReadInfo(mCloneReadInfo);
  }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  virtual nsresult
  PackArgumentsForParentProcess(CursorRequestParams& aParams) MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

protected:
  virtual nsresult
  BindArgumentsToStatement(mozIStorageStatement* aStatement) = 0;

  virtual nsresult
  GatherResultsFromStatement(mozIStorageStatement* aStatement) = 0;

  void UpdateCursorState()
  {
    mCursor->mCachedKey = JSVAL_VOID;
    mCursor->mCachedPrimaryKey = JSVAL_VOID;
    mCursor->mCachedValue = JSVAL_VOID;
    mCursor->mHaveCachedKey = false;
    mCursor->mHaveCachedPrimaryKey = false;
    mCursor->mHaveCachedValue = false;
    mCursor->mContinueCalled = false;

    if (mKey.IsUnset()) {
      mCursor->mHaveValue = false;
    }
    else {
      NS_ASSERTION(mCursor->mType == IDBCursor::OBJECTSTORE ||
                   !mObjectKey.IsUnset(), "Bad key!");

      // Set new values.
      mCursor->mKey = mKey;
      mCursor->mObjectKey = mObjectKey;
      mCursor->mContinueToKey.Unset();

      mCursor->mCloneReadInfo.Swap(mCloneReadInfo);
      mCloneReadInfo.mCloneBuffer.clear();
    }
  }

  int32_t mCount;
  Key mKey;
  Key mObjectKey;
  StructuredCloneReadInfo mCloneReadInfo;
};

class ContinueObjectStoreHelper : public ContinueHelper
{
public:
  ContinueObjectStoreHelper(IDBCursor* aCursor,
                            uint32_t aCount)
  : ContinueHelper(aCursor, aCount)
  { }

private:
  nsresult BindArgumentsToStatement(mozIStorageStatement* aStatement);
  nsresult GatherResultsFromStatement(mozIStorageStatement* aStatement);
};

class ContinueIndexHelper : public ContinueHelper
{
public:
  ContinueIndexHelper(IDBCursor* aCursor,
                      uint32_t aCount)
  : ContinueHelper(aCursor, aCount)
  { }

private:
  nsresult BindArgumentsToStatement(mozIStorageStatement* aStatement);
  nsresult GatherResultsFromStatement(mozIStorageStatement* aStatement);
};

class ContinueIndexObjectHelper : public ContinueIndexHelper
{
public:
  ContinueIndexObjectHelper(IDBCursor* aCursor,
                            uint32_t aCount)
  : ContinueIndexHelper(aCursor, aCount)
  { }

private:
  nsresult GatherResultsFromStatement(mozIStorageStatement* aStatement);
};

END_INDEXEDDB_NAMESPACE

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBRequest* aRequest,
                  IDBTransaction* aTransaction,
                  IDBObjectStore* aObjectStore,
                  Direction aDirection,
                  const Key& aRangeKey,
                  const nsACString& aContinueQuery,
                  const nsACString& aContinueToQuery,
                  const Key& aKey,
                  StructuredCloneReadInfo& aCloneReadInfo)
{
  NS_ASSERTION(aObjectStore, "Null pointer!");
  NS_ASSERTION(!aKey.IsUnset(), "Bad key!");

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::CreateCommon(aRequest, aTransaction, aObjectStore, aDirection,
                            aRangeKey, aContinueQuery, aContinueToQuery);
  NS_ASSERTION(cursor, "This shouldn't fail!");

  cursor->mObjectStore = aObjectStore;
  cursor->mType = OBJECTSTORE;
  cursor->mKey = aKey;
  cursor->mCloneReadInfo.Swap(aCloneReadInfo);

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBRequest* aRequest,
                  IDBTransaction* aTransaction,
                  IDBIndex* aIndex,
                  Direction aDirection,
                  const Key& aRangeKey,
                  const nsACString& aContinueQuery,
                  const nsACString& aContinueToQuery,
                  const Key& aKey,
                  const Key& aObjectKey)
{
  NS_ASSERTION(aIndex, "Null pointer!");
  NS_ASSERTION(!aKey.IsUnset(), "Bad key!");
  NS_ASSERTION(!aObjectKey.IsUnset(), "Bad key!");

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::CreateCommon(aRequest, aTransaction, aIndex->ObjectStore(),
                            aDirection, aRangeKey, aContinueQuery,
                            aContinueToQuery);
  NS_ASSERTION(cursor, "This shouldn't fail!");

  cursor->mIndex = aIndex;
  cursor->mType = INDEXKEY;
  cursor->mKey = aKey,
  cursor->mObjectKey = aObjectKey;

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBRequest* aRequest,
                  IDBTransaction* aTransaction,
                  IDBIndex* aIndex,
                  Direction aDirection,
                  const Key& aRangeKey,
                  const nsACString& aContinueQuery,
                  const nsACString& aContinueToQuery,
                  const Key& aKey,
                  const Key& aObjectKey,
                  StructuredCloneReadInfo& aCloneReadInfo)
{
  NS_ASSERTION(aIndex, "Null pointer!");
  NS_ASSERTION(!aKey.IsUnset(), "Bad key!");

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::CreateCommon(aRequest, aTransaction, aIndex->ObjectStore(),
                            aDirection, aRangeKey, aContinueQuery,
                            aContinueToQuery);
  NS_ASSERTION(cursor, "This shouldn't fail!");

  cursor->mObjectStore = aIndex->ObjectStore();
  cursor->mIndex = aIndex;
  cursor->mType = INDEXOBJECT;
  cursor->mKey = aKey;
  cursor->mObjectKey = aObjectKey;
  cursor->mCloneReadInfo.Swap(aCloneReadInfo);

  return cursor.forget();
}

// static
IDBCursor::Direction
IDBCursor::ConvertDirection(mozilla::dom::IDBCursorDirection aDirection)
{
  switch (aDirection) {
    case mozilla::dom::IDBCursorDirection::Next:
      return NEXT;

    case mozilla::dom::IDBCursorDirection::Nextunique:
      return NEXT_UNIQUE;

    case mozilla::dom::IDBCursorDirection::Prev:
      return PREV;

    case mozilla::dom::IDBCursorDirection::Prevunique:
      return PREV_UNIQUE;

    default:
      MOZ_CRASH("Unknown direction!");
  }
}

// static
already_AddRefed<IDBCursor>
IDBCursor::CreateCommon(IDBRequest* aRequest,
                        IDBTransaction* aTransaction,
                        IDBObjectStore* aObjectStore,
                        Direction aDirection,
                        const Key& aRangeKey,
                        const nsACString& aContinueQuery,
                        const nsACString& aContinueToQuery)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRequest, "Null pointer!");
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aObjectStore, "Null pointer!");
  NS_ASSERTION(!aContinueQuery.IsEmpty() ||
               !IndexedDatabaseManager::IsMainProcess(),
               "Empty query!");
  NS_ASSERTION(!aContinueToQuery.IsEmpty() ||
               !IndexedDatabaseManager::IsMainProcess(),
               "Empty query!");

  nsRefPtr<IDBCursor> cursor = new IDBCursor();

  IDBDatabase* database = aTransaction->Database();
  cursor->mScriptOwner = database->GetScriptOwner();

  if (cursor->mScriptOwner) {
    NS_HOLD_JS_OBJECTS(cursor, IDBCursor);
    cursor->mRooted = true;
  }

  cursor->mRequest = aRequest;
  cursor->mTransaction = aTransaction;
  cursor->mObjectStore = aObjectStore;
  cursor->mDirection = aDirection;
  cursor->mContinueQuery = aContinueQuery;
  cursor->mContinueToQuery = aContinueToQuery;
  cursor->mRangeKey = aRangeKey;

  return cursor.forget();
}

IDBCursor::IDBCursor()
: mScriptOwner(nullptr),
  mType(OBJECTSTORE),
  mDirection(IDBCursor::NEXT),
  mCachedKey(JSVAL_VOID),
  mCachedPrimaryKey(JSVAL_VOID),
  mCachedValue(JSVAL_VOID),
  mActorChild(nullptr),
  mActorParent(nullptr),
  mHaveCachedKey(false),
  mHaveCachedPrimaryKey(false),
  mHaveCachedValue(false),
  mRooted(false),
  mContinueCalled(false),
  mHaveValue(true)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SetIsDOMBinding();
}

IDBCursor::~IDBCursor()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ASSERTION(!mActorParent, "Actor parent owns us, how can we be dying?!");
  if (mActorChild) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }

  DropJSObjects();
  IDBObjectStore::ClearCloneReadInfo(mCloneReadInfo);
}

void
IDBCursor::DropJSObjects()
{
  if (!mRooted) {
    return;
  }
  mScriptOwner = nullptr;
  mCachedKey = JSVAL_VOID;
  mCachedPrimaryKey = JSVAL_VOID;
  mCachedValue = JSVAL_VOID;
  mHaveCachedKey = false;
  mHaveCachedPrimaryKey = false;
  mHaveCachedValue = false;
  mRooted = false;
  mHaveValue = false;
  NS_DROP_JS_OBJECTS(this, IDBCursor);
}

void
IDBCursor::ContinueInternal(const Key& aKey, int32_t aCount, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCount > 0, "Must have a count!");

  if (!mTransaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (!mHaveValue || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  mContinueToKey = aKey;

#ifdef DEBUG
  {
    nsAutoString readyState;
    if (NS_FAILED(mRequest->GetReadyState(readyState))) {
      NS_ERROR("This should never fail!");
    }
    NS_ASSERTION(readyState.EqualsLiteral("done"), "Should be DONE!");
  }
#endif

  mRequest->Reset();

  nsRefPtr<ContinueHelper> helper;
  switch (mType) {
    case OBJECTSTORE:
      helper = new ContinueObjectStoreHelper(this, aCount);
      break;

    case INDEXKEY:
      helper = new ContinueIndexHelper(this, aCount);
      break;

    case INDEXOBJECT:
      helper = new ContinueIndexObjectHelper(this, aCount);
      break;

    default:
      NS_NOTREACHED("Unknown cursor type!");
  }

  nsresult rv = helper->DispatchToTransactionPool();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }

  mContinueCalled = true;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObjectStore)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndex)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBCursor)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_ASSERTION(tmp->mHaveCachedKey || JSVAL_IS_VOID(tmp->mCachedKey),
               "Should have a cached key");
  NS_ASSERTION(tmp->mHaveCachedPrimaryKey ||
               JSVAL_IS_VOID(tmp->mCachedPrimaryKey),
               "Should have a cached primary key");
  NS_ASSERTION(tmp->mHaveCachedValue || JSVAL_IS_VOID(tmp->mCachedValue),
               "Should have a cached value");
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScriptOwner)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedPrimaryKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedValue)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBCursor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  // Don't unlink mObjectStore, mIndex, or mTransaction!
  tmp->DropJSObjects();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRequest)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBCursor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBCursor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBCursor)

JSObject*
IDBCursor::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return mType != INDEXKEY
          ? IDBCursorWithValueBinding::Wrap(aCx, aScope, this)
          : IDBCursorBinding::Wrap(aCx, aScope, this);
}

mozilla::dom::IDBCursorDirection
IDBCursor::GetDirection() const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  switch (mDirection) {
    case NEXT:
      return mozilla::dom::IDBCursorDirection::Next;

    case NEXT_UNIQUE:
      return mozilla::dom::IDBCursorDirection::Nextunique;

    case PREV:
      return mozilla::dom::IDBCursorDirection::Prev;

    case PREV_UNIQUE:
      return mozilla::dom::IDBCursorDirection::Prevunique;

    case DIRECTION_INVALID:
    default:
      MOZ_CRASH("Unknown direction!");
      return mozilla::dom::IDBCursorDirection::Next;
  }
}


already_AddRefed<nsISupports>
IDBCursor::Source() const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsISupports> source;
  if (mType == OBJECTSTORE) {
    source = do_QueryInterface(mObjectStore);
  }
  else {
    source = do_QueryInterface(mIndex);
  }

  return source.forget();
}

JS::Value
IDBCursor::GetKey(JSContext* aCx, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ASSERTION(!mKey.IsUnset() || !mHaveValue, "Bad key!");

  if (!mHaveValue) {
    return JSVAL_VOID;
  }

  if (!mHaveCachedKey) {
    if (!mRooted) {
      NS_HOLD_JS_OBJECTS(this, IDBCursor);
      mRooted = true;
    }

    aRv = mKey.ToJSVal(aCx, mCachedKey);
    ENSURE_SUCCESS(aRv, JSVAL_VOID);

    mHaveCachedKey = true;
  }

  return mCachedKey;
}

JS::Value
IDBCursor::GetPrimaryKey(JSContext* aCx, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveValue) {
    return JSVAL_VOID;
  }

  if (!mHaveCachedPrimaryKey) {
    if (!mRooted) {
      NS_HOLD_JS_OBJECTS(this, IDBCursor);
      mRooted = true;
    }

    JSAutoRequest ar(aCx);

    NS_ASSERTION(mType == OBJECTSTORE ? !mKey.IsUnset() :
                                        !mObjectKey.IsUnset(), "Bad key!");

    const Key& key = mType == OBJECTSTORE ? mKey : mObjectKey;

    aRv = key.ToJSVal(aCx, mCachedPrimaryKey);
    ENSURE_SUCCESS(aRv, JSVAL_VOID);

    mHaveCachedPrimaryKey = true;
  }

  return mCachedPrimaryKey;
}

JS::Value
IDBCursor::GetValue(JSContext* aCx, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mType != INDEXKEY, "GetValue shouldn't exist on index keys");

  if (!mHaveValue) {
    return JSVAL_VOID;
  }

  if (!mHaveCachedValue) {
    if (!mRooted) {
      NS_HOLD_JS_OBJECTS(this, IDBCursor);
      mRooted = true;
    }

    JS::Rooted<JS::Value> val(aCx);
    if (!IDBObjectStore::DeserializeValue(aCx, mCloneReadInfo, &val)) {
      aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
      return JSVAL_VOID;
    }

    mCloneReadInfo.mCloneBuffer.clear();

    mCachedValue = val;
    mHaveCachedValue = true;
  }

  return mCachedValue;
}

void
IDBCursor::Continue(JSContext* aCx,
                    const Optional<JS::Handle<JS::Value> >& aKey,
                    ErrorResult &aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  Key key;
  if (aKey.WasPassed()) {
    aRv = key.SetFromJSVal(aCx, aKey.Value());
    ENSURE_SUCCESS_VOID(aRv);
  }

  if (!key.IsUnset()) {
    switch (mDirection) {
      case IDBCursor::NEXT:
      case IDBCursor::NEXT_UNIQUE:
        if (key <= mKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      case IDBCursor::PREV:
      case IDBCursor::PREV_UNIQUE:
        if (key >= mKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      default:
        NS_NOTREACHED("Unknown direction type!");
    }
  }

  ContinueInternal(key, 1, aRv);
  if (aRv.Failed()) {
    return;
  }

#ifdef IDB_PROFILER_USE_MARKS
  if (mType == OBJECTSTORE) {
    IDB_PROFILER_MARK("IndexedDB Request %llu: "
                      "database(%s).transaction(%s).objectStore(%s).cursor(%s)."
                      "continue(%s)",
                      "IDBRequest[%llu] MT IDBCursor.continue()",
                      Request()->GetSerialNumber(),
                      IDB_PROFILER_STRING(Transaction()->Database()),
                      IDB_PROFILER_STRING(Transaction()),
                      IDB_PROFILER_STRING(mObjectStore),
                      IDB_PROFILER_STRING(mDirection),
                      key.IsUnset() ? "" : IDB_PROFILER_STRING(key));
  }
  else {
    IDB_PROFILER_MARK("IndexedDB Request %llu: "
                      "database(%s).transaction(%s).objectStore(%s).index(%s)."
                      "cursor(%s).continue(%s)",
                      "IDBRequest[%llu] MT IDBCursor.continue()",
                      Request()->GetSerialNumber(),
                      IDB_PROFILER_STRING(Transaction()->Database()),
                      IDB_PROFILER_STRING(Transaction()),
                      IDB_PROFILER_STRING(mObjectStore),
                      IDB_PROFILER_STRING(mIndex),
                      IDB_PROFILER_STRING(mDirection),
                      key.IsUnset() ? "" : IDB_PROFILER_STRING(key));
  }
#endif
}

already_AddRefed<IDBRequest>
IDBCursor::Update(JSContext* aCx, JS::Handle<JS::Value> aValue,
                  ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return nullptr;
  }

  if (!mHaveValue || mType == INDEXKEY) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  NS_ASSERTION(mObjectStore, "This cannot be null!");
  NS_ASSERTION(!mKey.IsUnset() , "Bad key!");
  NS_ASSERTION(mType != INDEXOBJECT || !mObjectKey.IsUnset(), "Bad key!");

  JSAutoRequest ar(aCx);

  Key& objectKey = (mType == OBJECTSTORE) ? mKey : mObjectKey;

  nsRefPtr<IDBRequest> request;
  if (mObjectStore->HasValidKeyPath()) {
    // Make sure the object given has the correct keyPath value set on it.
    const KeyPath& keyPath = mObjectStore->GetKeyPath();
    Key key;

    aRv = keyPath.ExtractKey(aCx, aValue, key);
    if (aRv.Failed()) {
      return nullptr;
    }

    if (key != objectKey) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return nullptr;
    }

    JS::Rooted<JS::Value> value(aCx, aValue);
    Optional<JS::Handle<JS::Value> > keyValue(aCx);
    request = mObjectStore->Put(aCx, value, keyValue, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  else {
    JS::Rooted<JS::Value> keyVal(aCx);
    aRv = objectKey.ToJSVal(aCx, &keyVal);
    ENSURE_SUCCESS(aRv, nullptr);

    JS::Rooted<JS::Value> value(aCx, aValue);
    Optional<JS::Handle<JS::Value> > keyValue(aCx, keyVal);
    request = mObjectStore->Put(aCx, value, keyValue, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

#ifdef IDB_PROFILER_USE_MARKS
  {
    uint64_t requestSerial =
      static_cast<IDBRequest*>(request.get())->GetSerialNumber();
    if (mType == OBJECTSTORE) {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "cursor(%s).update(%s)",
                        "IDBRequest[%llu] MT IDBCursor.update()",
                        requestSerial,
                        IDB_PROFILER_STRING(mTransaction->Database()),
                        IDB_PROFILER_STRING(mTransaction),
                        IDB_PROFILER_STRING(mObjectStore),
                        IDB_PROFILER_STRING(mDirection),
                        mObjectStore->HasValidKeyPath() ? "" :
                          IDB_PROFILER_STRING(objectKey));
    }
    else {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "index(%s).cursor(%s).update(%s)",
                        "IDBRequest[%llu] MT IDBCursor.update()",
                        requestSerial,
                        IDB_PROFILER_STRING(mTransaction->Database()),
                        IDB_PROFILER_STRING(mTransaction),
                        IDB_PROFILER_STRING(mObjectStore),
                        IDB_PROFILER_STRING(mIndex),
                        IDB_PROFILER_STRING(mDirection),
                        mObjectStore->HasValidKeyPath() ? "" :
                          IDB_PROFILER_STRING(objectKey));
    }
  }
#endif

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBCursor::Delete(JSContext* aCx, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return nullptr;
  }

  if (!mHaveValue || mType == INDEXKEY) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  NS_ASSERTION(mObjectStore, "This cannot be null!");
  NS_ASSERTION(!mKey.IsUnset() , "Bad key!");

  Key& objectKey = (mType == OBJECTSTORE) ? mKey : mObjectKey;

  JS::Rooted<JS::Value> key(aCx);
  aRv = objectKey.ToJSVal(aCx, &key);
  ENSURE_SUCCESS(aRv, nullptr);

  nsRefPtr<IDBRequest> request = mObjectStore->Delete(aCx, key, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

#ifdef IDB_PROFILER_USE_MARKS
  {
    uint64_t requestSerial = request->GetSerialNumber();
    if (mType == OBJECTSTORE) {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "cursor(%s).delete(%s)",
                        "IDBRequest[%llu] MT IDBCursor.delete()",
                        requestSerial,
                        IDB_PROFILER_STRING(mTransaction->Database()),
                        IDB_PROFILER_STRING(mTransaction),
                        IDB_PROFILER_STRING(mObjectStore),
                        IDB_PROFILER_STRING(mDirection),
                        mObjectStore->HasValidKeyPath() ? "" :
                          IDB_PROFILER_STRING(objectKey));
    }
    else {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "index(%s).cursor(%s).delete(%s)",
                        "IDBRequest[%llu] MT IDBCursor.delete()",
                        requestSerial,
                        IDB_PROFILER_STRING(mTransaction->Database()),
                        IDB_PROFILER_STRING(mTransaction),
                        IDB_PROFILER_STRING(mObjectStore),
                        IDB_PROFILER_STRING(mIndex),
                        IDB_PROFILER_STRING(mDirection),
                        mObjectStore->HasValidKeyPath() ? "" :
                          IDB_PROFILER_STRING(objectKey));
    }
  }
#endif

  return request.forget();
}

void
IDBCursor::Advance(uint32_t aCount, ErrorResult &aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aCount < 1) {
    aRv.ThrowTypeError(MSG_INVALID_ADVANCE_COUNT);
    return;
  }

  Key key;
  ContinueInternal(key, int32_t(aCount), aRv);
  ENSURE_SUCCESS_VOID(aRv);

#ifdef IDB_PROFILER_USE_MARKS
  {
    if (mType == OBJECTSTORE) {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "cursor(%s).advance(%ld)",
                        "IDBRequest[%llu] MT IDBCursor.advance()",
                        Request()->GetSerialNumber(),
                        IDB_PROFILER_STRING(Transaction()->Database()),
                        IDB_PROFILER_STRING(Transaction()),
                        IDB_PROFILER_STRING(mObjectStore),
                        IDB_PROFILER_STRING(mDirection), aCount);
    }
    else {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "index(%s).cursor(%s).advance(%ld)",
                        "IDBRequest[%llu] MT IDBCursor.advance()",
                        Request()->GetSerialNumber(),
                        IDB_PROFILER_STRING(Transaction()->Database()),
                        IDB_PROFILER_STRING(Transaction()),
                        IDB_PROFILER_STRING(mObjectStore),
                        IDB_PROFILER_STRING(mIndex),
                        IDB_PROFILER_STRING(mDirection), aCount);
    }
  }
#endif
}

void
CursorHelper::ReleaseMainThreadObjects()
{
  mCursor = nullptr;
  AsyncConnectionHelper::ReleaseMainThreadObjects();
}

nsresult
CursorHelper::Dispatch(nsIEventTarget* aDatabaseThread)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB", "CursorHelper::Dispatch");

  if (IndexedDatabaseManager::IsMainProcess()) {
    return AsyncConnectionHelper::Dispatch(aDatabaseThread);
  }

  // If we've been invalidated then there's no point sending anything to the
  // parent process.
  if (mCursor->Transaction()->Database()->IsInvalidated()) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  IndexedDBCursorChild* cursorActor = mCursor->GetActorChild();
  NS_ASSERTION(cursorActor, "Must have an actor here!");

  CursorRequestParams params;
  nsresult rv = PackArgumentsForParentProcess(params);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NoDispatchEventTarget target;
  rv = AsyncConnectionHelper::Dispatch(&target);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mActor = new IndexedDBCursorRequestChild(this, mCursor, params.type());
  cursorActor->SendPIndexedDBRequestConstructor(mActor, params);

  return NS_OK;
}

nsresult
ContinueHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("IndexedDB", "ContinueHelper::DoDatabaseWork");

  // We need to pick a query based on whether or not the cursor's mContinueToKey
  // is set. If it is unset then othing was passed to continue so we'll grab the
  // next item in the database that is greater than (less than, if we're running
  // a PREV cursor) the current key. If it is set then a key was passed to
  // continue so we'll grab the next item in the database that is greater than
  // (less than, if we're running a PREV cursor) or equal to the key that was
  // specified.

  nsAutoCString query;
  if (mCursor->mContinueToKey.IsUnset()) {
    query.Assign(mCursor->mContinueQuery);
  }
  else {
    query.Assign(mCursor->mContinueToQuery);
  }
  NS_ASSERTION(!query.IsEmpty(), "Bad query!");

  query.AppendInt(mCount);

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = BindArgumentsToStatement(stmt);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(mCount > 0, "Not ok!");

  bool hasResult;
  for (int32_t index = 0; index < mCount; index++) {
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (!hasResult) {
      break;
    }
  }

  if (hasResult) {
    rv = GatherResultsFromStatement(stmt);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else {
    mKey.Unset();
  }

  return NS_OK;
}

nsresult
ContinueHelper::GetSuccessResult(JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aVal)
{
  UpdateCursorState();

  if (mKey.IsUnset()) {
    aVal.setNull();
  }
  else {
    nsresult rv = WrapNative(aCx, mCursor, aVal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
ContinueHelper::ReleaseMainThreadObjects()
{
  IDBObjectStore::ClearCloneReadInfo(mCloneReadInfo);
  CursorHelper::ReleaseMainThreadObjects();
}

nsresult
ContinueHelper::PackArgumentsForParentProcess(CursorRequestParams& aParams)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "ContinueHelper::PackArgumentsForParentProcess");

  ContinueParams params;

  params.key() = mCursor->mContinueToKey;
  params.count() = uint32_t(mCount);

  aParams = params;
  return NS_OK;
}

AsyncConnectionHelper::ChildProcessSendResult
ContinueHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "ContinueHelper::SendResponseToChildProcess");

  IndexedDBRequestParentBase* actor = mRequest->GetActorParent();
  NS_ASSERTION(actor, "How did we get this far without an actor?");

  InfallibleTArray<PBlobParent*> blobsParent;

  if (NS_SUCCEEDED(aResultCode)) {
    IDBDatabase* database = mTransaction->Database();
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

  ResponseValue response;
  if (NS_FAILED(aResultCode)) {
    response = aResultCode;
  }
  else {
    ContinueResponse continueResponse;
    continueResponse.key() = mKey;
    continueResponse.objectKey() = mObjectKey;
    continueResponse.cloneInfo() = mCloneReadInfo;
    continueResponse.blobsParent().SwapElements(blobsParent);
    response = continueResponse;
  }

  if (!actor->SendResponse(response)) {
    return Error;
  }

  UpdateCursorState();

  return Success_Sent;
}

nsresult
ContinueHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TContinueResponse,
               "Bad response type!");

  const ContinueResponse& response = aResponseValue.get_ContinueResponse();

  mKey = response.key();
  mObjectKey = response.objectKey();

  const SerializedStructuredCloneReadInfo& cloneInfo = response.cloneInfo();

  NS_ASSERTION((!cloneInfo.dataLength && !cloneInfo.data) ||
               (cloneInfo.dataLength && cloneInfo.data),
               "Inconsistent clone info!");

  if (!mCloneReadInfo.SetFromSerialized(cloneInfo)) {
    NS_WARNING("Failed to copy clone buffer!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  IDBObjectStore::ConvertActorsToBlobs(response.blobsChild(),
                                       mCloneReadInfo.mFiles);
  return NS_OK;
}

nsresult
ContinueObjectStoreHelper::BindArgumentsToStatement(
                                               mozIStorageStatement* aStatement)
{
  // Bind object store id.
  nsresult rv = aStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                            mCursor->mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(currentKeyName, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKeyName, "range_key");

  // Bind current key.
  const Key& currentKey = mCursor->mContinueToKey.IsUnset() ?
                          mCursor->mKey :
                          mCursor->mContinueToKey;

  rv = currentKey.BindToStatement(aStatement, currentKeyName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind range key if it is specified.
  const Key& rangeKey = mCursor->mRangeKey;

  if (!rangeKey.IsUnset()) {
    rv = rangeKey.BindToStatement(aStatement, rangeKeyName);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
ContinueObjectStoreHelper::GatherResultsFromStatement(
                                               mozIStorageStatement* aStatement)
{
  // Figure out what kind of key we have next.
  nsresult rv = mKey.SetFromStatement(aStatement, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(aStatement, 1, 2,
    mDatabase, mCloneReadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
ContinueIndexHelper::BindArgumentsToStatement(mozIStorageStatement* aStatement)
{
  // Bind index id.
  nsresult rv = aStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                            mCursor->mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(currentKeyName, "current_key");

  // Bind current key.
  const Key& currentKey = mCursor->mContinueToKey.IsUnset() ?
                          mCursor->mKey :
                          mCursor->mContinueToKey;

  rv = currentKey.BindToStatement(aStatement, currentKeyName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind range key if it is specified.
  if (!mCursor->mRangeKey.IsUnset()) {
    NS_NAMED_LITERAL_CSTRING(rangeKeyName, "range_key");
    rv = mCursor->mRangeKey.BindToStatement(aStatement, rangeKeyName);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Bind object key if duplicates are allowed and we're not continuing to a
  // specific key.
  if ((mCursor->mDirection == IDBCursor::NEXT ||
       mCursor->mDirection == IDBCursor::PREV) &&
       mCursor->mContinueToKey.IsUnset()) {
    NS_ASSERTION(!mCursor->mObjectKey.IsUnset(), "Bad key!");

    NS_NAMED_LITERAL_CSTRING(objectKeyName, "object_key");
    rv = mCursor->mObjectKey.BindToStatement(aStatement, objectKeyName);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
ContinueIndexHelper::GatherResultsFromStatement(
                                               mozIStorageStatement* aStatement)
{
  nsresult rv = mKey.SetFromStatement(aStatement, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mObjectKey.SetFromStatement(aStatement, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
ContinueIndexObjectHelper::GatherResultsFromStatement(
                                               mozIStorageStatement* aStatement)
{
  nsresult rv = mKey.SetFromStatement(aStatement, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mObjectKey.SetFromStatement(aStatement, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(aStatement, 2, 3,
    mDatabase, mCloneReadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
