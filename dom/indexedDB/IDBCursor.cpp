/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBCursor.h"

#include "IDBDatabase.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "IndexedDatabaseInlines.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "nsString.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

IDBCursor::IDBCursor(Type aType,
                     IDBObjectStore* aSourceObjectStore,
                     IDBIndex* aSourceIndex,
                     IDBTransaction* aTransaction,
                     BackgroundCursorChild* aBackgroundActor,
                     Direction aDirection,
                     const Key& aKey)
  : mSourceObjectStore(aSourceObjectStore)
  , mSourceIndex(aSourceIndex)
  , mTransaction(aTransaction)
  , mBackgroundActor(aBackgroundActor)
  , mScriptOwner(aTransaction->Database()->GetScriptOwner())
  , mCachedKey(JSVAL_VOID)
  , mCachedPrimaryKey(JSVAL_VOID)
  , mCachedValue(JSVAL_VOID)
  , mKey(aKey)
  , mType(aType)
  , mDirection(aDirection)
  , mHaveCachedKey(false)
  , mHaveCachedPrimaryKey(false)
  , mHaveCachedValue(false)
  , mRooted(false)
  , mContinueCalled(false)
  , mHaveValue(true)
{
  MOZ_ASSERT_IF(aType == Type_ObjectStore || aType == Type_ObjectStoreKey,
                aSourceObjectStore);
  MOZ_ASSERT_IF(aType == Type_Index || aType == Type_IndexKey, aSourceIndex);
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!aKey.IsUnset());
  MOZ_ASSERT(mScriptOwner);

  SetIsDOMBinding();

  if (mScriptOwner) {
    mozilla::HoldJSObjects(this);
    mRooted = true;
  }
}

IDBCursor::~IDBCursor()
{
  AssertIsOnOwningThread();

  DropJSObjects();

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBObjectStore* aObjectStore,
                  BackgroundCursorChild* aBackgroundActor,
                  Direction aDirection,
                  const Key& aKey,
                  StructuredCloneReadInfo&& aCloneInfo)
{
  MOZ_ASSERT(aObjectStore);
  aObjectStore->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!aKey.IsUnset());

  nsRefPtr<IDBCursor> cursor =
    new IDBCursor(Type_ObjectStore,
                  aObjectStore,
                  nullptr,
                  aObjectStore->Transaction(),
                  aBackgroundActor,
                  aDirection,
                  aKey);

  cursor->mCloneInfo = Move(aCloneInfo);

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBObjectStore* aObjectStore,
                  BackgroundCursorChild* aBackgroundActor,
                  Direction aDirection,
                  const Key& aKey)
{
  MOZ_ASSERT(aObjectStore);
  aObjectStore->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!aKey.IsUnset());

  nsRefPtr<IDBCursor> cursor =
    new IDBCursor(Type_ObjectStoreKey,
                  aObjectStore,
                  nullptr,
                  aObjectStore->Transaction(),
                  aBackgroundActor,
                  aDirection,
                  aKey);

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBIndex* aIndex,
                  BackgroundCursorChild* aBackgroundActor,
                  Direction aDirection,
                  const Key& aKey,
                  const Key& aPrimaryKey,
                  StructuredCloneReadInfo&& aCloneInfo)
{
  MOZ_ASSERT(aIndex);
  aIndex->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!aKey.IsUnset());
  MOZ_ASSERT(!aPrimaryKey.IsUnset());

  nsRefPtr<IDBCursor> cursor =
    new IDBCursor(Type_Index,
                  nullptr,
                  aIndex,
                  aIndex->ObjectStore()->Transaction(),
                  aBackgroundActor,
                  aDirection,
                  aKey);

  cursor->mPrimaryKey = Move(aPrimaryKey);
  cursor->mCloneInfo = Move(aCloneInfo);

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(IDBIndex* aIndex,
                  BackgroundCursorChild* aBackgroundActor,
                  Direction aDirection,
                  const Key& aKey,
                  const Key& aPrimaryKey)
{
  MOZ_ASSERT(aIndex);
  aIndex->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!aKey.IsUnset());
  MOZ_ASSERT(!aPrimaryKey.IsUnset());

  nsRefPtr<IDBCursor> cursor =
    new IDBCursor(Type_IndexKey,
                  nullptr,
                  aIndex,
                  aIndex->ObjectStore()->Transaction(),
                  aBackgroundActor,
                  aDirection,
                  aKey);

  cursor->mPrimaryKey = Move(aPrimaryKey);

  return cursor.forget();
}

// static
auto
IDBCursor::ConvertDirection(IDBCursorDirection aDirection) -> Direction
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

#ifdef DEBUG

void
IDBCursor::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnOwningThread();
}

#endif // DEBUG

void
IDBCursor::DropJSObjects()
{
  AssertIsOnOwningThread();

  Reset();

  if (!mRooted) {
    return;
  }

  mScriptOwner = nullptr;
  mRooted = false;

  mozilla::DropJSObjects(this);
}

void
IDBCursor::Reset()
{
  AssertIsOnOwningThread();

  mCachedKey.setUndefined();
  mCachedPrimaryKey.setUndefined();
  mCachedValue.setUndefined();
  IDBObjectStore::ClearCloneReadInfo(mCloneInfo);

  mHaveCachedKey = false;
  mHaveCachedPrimaryKey = false;
  mHaveCachedValue = false;
  mHaveValue = false;
  mContinueCalled = false;
}

nsPIDOMWindow*
IDBCursor::GetParentObject() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  return mTransaction->GetParentObject();
}

IDBCursorDirection
IDBCursor::GetDirection() const
{
  AssertIsOnOwningThread();

  switch (mDirection) {
    case NEXT:
      return IDBCursorDirection::Next;

    case NEXT_UNIQUE:
      return IDBCursorDirection::Nextunique;

    case PREV:
      return IDBCursorDirection::Prev;

    case PREV_UNIQUE:
      return IDBCursorDirection::Prevunique;

    default:
      MOZ_CRASH("Bad direction!");
  }
}

void
IDBCursor::GetSource(OwningIDBObjectStoreOrIDBIndex& aSource) const
{
  AssertIsOnOwningThread();

  switch (mType) {
    case Type_ObjectStore:
    case Type_ObjectStoreKey:
      MOZ_ASSERT(mSourceObjectStore);
      aSource.SetAsIDBObjectStore() = mSourceObjectStore;
      return;

    case Type_Index:
    case Type_IndexKey:
      MOZ_ASSERT(mSourceIndex);
      aSource.SetAsIDBIndex() = mSourceIndex;
      return;

    default:
      MOZ_ASSERT_UNREACHABLE("Bad type!");
  }
}

void
IDBCursor::GetKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                  ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(!mKey.IsUnset() || !mHaveValue);

  if (!mHaveValue) {
    aResult.setUndefined();
    return;
  }

  if (!mHaveCachedKey) {
    if (!mRooted) {
      mozilla::HoldJSObjects(this);
      mRooted = true;
    }

    aRv = mKey.ToJSVal(aCx, mCachedKey);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    mHaveCachedKey = true;
  }

  JS::ExposeValueToActiveJS(mCachedKey);
  aResult.set(mCachedKey);
}

void
IDBCursor::GetPrimaryKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                         ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (!mHaveValue) {
    aResult.setUndefined();
    return;
  }

  if (!mHaveCachedPrimaryKey) {
    if (!mRooted) {
      mozilla::HoldJSObjects(this);
      mRooted = true;
    }

    const Key& key =
      (mType == Type_ObjectStore || mType == Type_ObjectStoreKey) ?
      mKey :
      mPrimaryKey;

    MOZ_ASSERT(!key.IsUnset());

    aRv = key.ToJSVal(aCx, mCachedPrimaryKey);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    mHaveCachedPrimaryKey = true;
  }

  JS::ExposeValueToActiveJS(mCachedPrimaryKey);
  aResult.set(mCachedPrimaryKey);
}

void
IDBCursor::GetValue(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                    ErrorResult& aRv)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mType == Type_ObjectStore || mType == Type_Index);

  if (!mHaveValue) {
    aResult.setUndefined();
    return;
  }

  if (!mHaveCachedValue) {
    if (!mRooted) {
      mozilla::HoldJSObjects(this);
      mRooted = true;
    }

    JS::Rooted<JS::Value> val(aCx);
    if (NS_WARN_IF(!IDBObjectStore::DeserializeValue(aCx, mCloneInfo, &val))) {
      aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
      return;
    }

    IDBObjectStore::ClearCloneReadInfo(mCloneInfo);

    mCachedValue = val;
    mHaveCachedValue = true;
  }

  JS::ExposeValueToActiveJS(mCachedValue);
  aResult.set(mCachedValue);
}

void
IDBCursor::Continue(JSContext* aCx,
                    JS::Handle<JS::Value> aKey,
                    ErrorResult &aRv)
{
  AssertIsOnOwningThread();

  if (!mTransaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (!mHaveValue || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  Key key;
  aRv = key.SetFromJSVal(aCx, aKey);
  if (aRv.Failed()) {
    return;
  }

  if (!key.IsUnset()) {
    switch (mDirection) {
      case NEXT:
      case NEXT_UNIQUE:
        if (key <= mKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      case PREV:
      case PREV_UNIQUE:
        if (key >= mKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      default:
        MOZ_CRASH("Unknown direction type!");
    }
  }

  mBackgroundActor->SendContinueInternal(ContinueParams(key));

  mContinueCalled = true;

#ifdef IDB_PROFILER_USE_MARKS
  if (mType == Type_ObjectStore || mType == Type_ObjectStoreKey) {
    IDB_PROFILER_MARK("IndexedDB Request %llu: "
                      "database(%s).transaction(%s).objectStore(%s).cursor(%s)."
                      "continue(%s)",
                      "IDBRequest[%llu] MT IDBCursor.continue()",
                      Request()->GetSerialNumber(),
                      IDB_PROFILER_STRING(Transaction()->Database()),
                      IDB_PROFILER_STRING(Transaction()),
                      IDB_PROFILER_STRING(mSourceObjectStore),
                      IDB_PROFILER_STRING(mDirection),
                      key.IsUnset() ? "" : IDB_PROFILER_STRING(key));
  } else {
    IDB_PROFILER_MARK("IndexedDB Request %llu: "
                      "database(%s).transaction(%s).objectStore(%s).index(%s)."
                      "cursor(%s).continue(%s)",
                      "IDBRequest[%llu] MT IDBCursor.continue()",
                      Request()->GetSerialNumber(),
                      IDB_PROFILER_STRING(Transaction()->Database()),
                      IDB_PROFILER_STRING(Transaction()),
                      IDB_PROFILER_STRING(mSourceIndex->ObjectStore()),
                      IDB_PROFILER_STRING(mSourceIndex),
                      IDB_PROFILER_STRING(mDirection),
                      key.IsUnset() ? "" : IDB_PROFILER_STRING(key));
  }
#endif
}

void
IDBCursor::Advance(uint32_t aCount, ErrorResult &aRv)
{
  AssertIsOnOwningThread();

  if (!mTransaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (!mHaveValue || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  if (!aCount) {
    aRv.ThrowTypeError(MSG_INVALID_ADVANCE_COUNT);
    return;
  }

  mBackgroundActor->SendContinueInternal(AdvanceParams(aCount));

  mContinueCalled = true;

#ifdef IDB_PROFILER_USE_MARKS
  {
    if (mType == Type_ObjectStore || mType == Type_ObjectStoreKey) {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "cursor(%s).advance(%ld)",
                        "IDBRequest[%llu] MT IDBCursor.advance()",
                        Request()->GetSerialNumber(),
                        IDB_PROFILER_STRING(Transaction()->Database()),
                        IDB_PROFILER_STRING(Transaction()),
                        IDB_PROFILER_STRING(mSourceObjectStore),
                        IDB_PROFILER_STRING(mDirection), aCount);
    } else {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "index(%s).cursor(%s).advance(%ld)",
                        "IDBRequest[%llu] MT IDBCursor.advance()",
                        Request()->GetSerialNumber(),
                        IDB_PROFILER_STRING(Transaction()->Database()),
                        IDB_PROFILER_STRING(Transaction()),
                        IDB_PROFILER_STRING(mSourceIndex->ObjectStore()),
                        IDB_PROFILER_STRING(mSourceIndex),
                        IDB_PROFILER_STRING(mDirection), aCount);
    }
  }
#endif
}

already_AddRefed<IDBRequest>
IDBCursor::Update(JSContext* aCx, JS::Handle<JS::Value> aValue,
                  ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (!mTransaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  if (!mHaveValue || mType == Type_ObjectStoreKey || mType == Type_IndexKey) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return nullptr;
  }

  MOZ_ASSERT(mType == Type_ObjectStore || mType == Type_Index);
  MOZ_ASSERT(!mKey.IsUnset());
  MOZ_ASSERT_IF(mType == Type_Index, !mPrimaryKey.IsUnset());

  IDBObjectStore* objectStore;
  if (mType == Type_ObjectStore) {
    objectStore = mSourceObjectStore;
  } else {
    objectStore = mSourceIndex->ObjectStore();
  }

  MOZ_ASSERT(objectStore);

  const Key& primaryKey = (mType == Type_ObjectStore) ? mKey : mPrimaryKey;

  nsRefPtr<IDBRequest> request;

  if (objectStore->HasValidKeyPath()) {
    // Make sure the object given has the correct keyPath value set on it.
    const KeyPath& keyPath = objectStore->GetKeyPath();
    Key key;

    aRv = keyPath.ExtractKey(aCx, aValue, key);
    if (aRv.Failed()) {
      return nullptr;
    }

    if (key != primaryKey) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return nullptr;
    }

    request = objectStore->Put(aCx, aValue, JS::UndefinedHandleValue, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  else {
    JS::Rooted<JS::Value> keyVal(aCx);
    aRv = primaryKey.ToJSVal(aCx, &keyVal);
    if (aRv.Failed()) {
      return nullptr;
    }

    request = objectStore->Put(aCx, aValue, keyVal, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  request->SetSource(this);

#ifdef IDB_PROFILER_USE_MARKS
  {
    uint64_t requestSerial = request->GetSerialNumber();
    if (mType == Type_ObjectStore) {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "cursor(%s).update(%s)",
                        "IDBRequest[%llu] MT IDBCursor.update()",
                        requestSerial,
                        IDB_PROFILER_STRING(mTransaction->Database()),
                        IDB_PROFILER_STRING(mTransaction),
                        IDB_PROFILER_STRING(objectStore),
                        IDB_PROFILER_STRING(mDirection),
                        mObjectStore->HasValidKeyPath() ? "" :
                          IDB_PROFILER_STRING(primaryKey));
    } else {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "index(%s).cursor(%s).update(%s)",
                        "IDBRequest[%llu] MT IDBCursor.update()",
                        requestSerial,
                        IDB_PROFILER_STRING(mTransaction->Database()),
                        IDB_PROFILER_STRING(mTransaction),
                        IDB_PROFILER_STRING(objectStore),
                        IDB_PROFILER_STRING(mSourceIndex),
                        IDB_PROFILER_STRING(mDirection),
                        mObjectStore->HasValidKeyPath() ? "" :
                          IDB_PROFILER_STRING(primaryKey));
    }
  }
#endif

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBCursor::Delete(JSContext* aCx, ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (!mTransaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  if (!mHaveValue || mType == Type_ObjectStoreKey || mType == Type_IndexKey) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return nullptr;
  }

  MOZ_ASSERT(mType == Type_ObjectStore || mType == Type_Index);
  MOZ_ASSERT(!mKey.IsUnset());

  IDBObjectStore* objectStore;
  if (mType == Type_ObjectStore) {
    objectStore = mSourceObjectStore;
  } else {
    objectStore = mSourceIndex->ObjectStore();
  }

  MOZ_ASSERT(objectStore);

  const Key& primaryKey = (mType == Type_ObjectStore) ? mKey : mPrimaryKey;

  JS::Rooted<JS::Value> key(aCx);
  aRv = primaryKey.ToJSVal(aCx, &key);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<IDBRequest> request = objectStore->Delete(aCx, key, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  request->SetSource(this);

#ifdef IDB_PROFILER_USE_MARKS
  {
    uint64_t requestSerial = request->GetSerialNumber();
    if (mType == Type_ObjectStore) {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "cursor(%s).delete(%s)",
                        "IDBRequest[%llu] MT IDBCursor.delete()",
                        requestSerial,
                        IDB_PROFILER_STRING(mTransaction->Database()),
                        IDB_PROFILER_STRING(mTransaction),
                        IDB_PROFILER_STRING(objectStore),
                        IDB_PROFILER_STRING(mDirection),
                        mObjectStore->HasValidKeyPath() ? "" :
                          IDB_PROFILER_STRING(primaryKey));
    } else {
      IDB_PROFILER_MARK("IndexedDB Request %llu: "
                        "database(%s).transaction(%s).objectStore(%s)."
                        "index(%s).cursor(%s).delete(%s)",
                        "IDBRequest[%llu] MT IDBCursor.delete()",
                        requestSerial,
                        IDB_PROFILER_STRING(mTransaction->Database()),
                        IDB_PROFILER_STRING(mTransaction),
                        IDB_PROFILER_STRING(objectStore),
                        IDB_PROFILER_STRING(mSourceIndex),
                        IDB_PROFILER_STRING(mDirection),
                        mObjectStore->HasValidKeyPath() ? "" :
                          IDB_PROFILER_STRING(primaryKey));
    }
  }
#endif

  return request.forget();
}

void
IDBCursor::Reset(Key&& aKey, StructuredCloneReadInfo&& aValue)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mType == Type_ObjectStore);

  Reset();

  mKey = Move(aKey);
  mCloneInfo = Move(aValue);

  mHaveValue = !mKey.IsUnset();
}

void
IDBCursor::Reset(Key&& aKey)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mType == Type_ObjectStoreKey);

  Reset();

  mKey = Move(aKey);

  mHaveValue = !mKey.IsUnset();
}

void
IDBCursor::Reset(Key&& aKey,
                 Key&& aPrimaryKey,
                 StructuredCloneReadInfo&& aValue)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mType == Type_Index);

  Reset();

  mKey = Move(aKey);
  mPrimaryKey = Move(aPrimaryKey);
  mCloneInfo = Move(aValue);

  mHaveValue = !mKey.IsUnset();
}

void
IDBCursor::Reset(Key&& aKey, Key&& aPrimaryKey)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mType == Type_IndexKey);

  Reset();

  mKey = Move(aKey);
  mPrimaryKey = Move(aPrimaryKey);

  mHaveValue = !mKey.IsUnset();
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBCursor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBCursor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBCursor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBCursor)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceObjectStore)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceIndex)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBCursor)
  MOZ_ASSERT_IF(!tmp->mHaveCachedKey, tmp->mCachedKey.isUndefined());
  MOZ_ASSERT_IF(!tmp->mHaveCachedPrimaryKey,
                tmp->mCachedPrimaryKey.isUndefined());
  MOZ_ASSERT_IF(!tmp->mHaveCachedValue, tmp->mCachedValue.isUndefined());

  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScriptOwner)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedPrimaryKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedValue)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBCursor)
  // Don't unlink mSourceObjectStore or mSourceIndex or mTransaction!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->DropJSObjects();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
IDBCursor::WrapObject(JSContext* aCx)
{
  AssertIsOnOwningThread();

  switch (mType) {
    case Type_ObjectStore:
    case Type_Index:
      return IDBCursorWithValueBinding::Wrap(aCx, this);

    case Type_ObjectStoreKey:
    case Type_IndexKey:
      return IDBCursorBinding::Wrap(aCx, this);

    default:
      MOZ_CRASH("Bad type!");
  }
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
