/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
                     BackgroundCursorChild* aBackgroundActor,
                     const Key& aKey)
  : mBackgroundActor(aBackgroundActor)
  , mRequest(aBackgroundActor->GetRequest())
  , mSourceObjectStore(aBackgroundActor->GetObjectStore())
  , mSourceIndex(aBackgroundActor->GetIndex())
  , mTransaction(mRequest->GetTransaction())
  , mScriptOwner(mTransaction->Database()->GetScriptOwner())
  , mCachedKey(JS::UndefinedValue())
  , mCachedPrimaryKey(JS::UndefinedValue())
  , mCachedValue(JS::UndefinedValue())
  , mKey(aKey)
  , mType(aType)
  , mDirection(aBackgroundActor->GetDirection())
  , mHaveCachedKey(false)
  , mHaveCachedPrimaryKey(false)
  , mHaveCachedValue(false)
  , mRooted(false)
  , mContinueCalled(false)
  , mHaveValue(true)
{
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT_IF(aType == Type_ObjectStore || aType == Type_ObjectStoreKey,
                mSourceObjectStore);
  MOZ_ASSERT_IF(aType == Type_Index || aType == Type_IndexKey, mSourceIndex);
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(!aKey.IsUnset());
  MOZ_ASSERT(mScriptOwner);

  if (mScriptOwner) {
    mozilla::HoldJSObjects(this);
    mRooted = true;
  }
}

#ifdef ENABLE_INTL_API
bool
IDBCursor::IsLocaleAware() const
{
  return mSourceIndex && !mSourceIndex->Locale().IsEmpty();
}
#endif

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
IDBCursor::Create(BackgroundCursorChild* aBackgroundActor,
                  const Key& aKey,
                  StructuredCloneReadInfo&& aCloneInfo)
{
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor->GetObjectStore());
  MOZ_ASSERT(!aBackgroundActor->GetIndex());
  MOZ_ASSERT(!aKey.IsUnset());

  nsRefPtr<IDBCursor> cursor =
    new IDBCursor(Type_ObjectStore, aBackgroundActor, aKey);

  cursor->mCloneInfo = Move(aCloneInfo);

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(BackgroundCursorChild* aBackgroundActor,
                  const Key& aKey)
{
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor->GetObjectStore());
  MOZ_ASSERT(!aBackgroundActor->GetIndex());
  MOZ_ASSERT(!aKey.IsUnset());

  nsRefPtr<IDBCursor> cursor =
    new IDBCursor(Type_ObjectStoreKey, aBackgroundActor, aKey);

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(BackgroundCursorChild* aBackgroundActor,
                  const Key& aKey,
                  const Key& aSortKey,
                  const Key& aPrimaryKey,
                  StructuredCloneReadInfo&& aCloneInfo)
{
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor->GetIndex());
  MOZ_ASSERT(!aBackgroundActor->GetObjectStore());
  MOZ_ASSERT(!aKey.IsUnset());
  MOZ_ASSERT(!aPrimaryKey.IsUnset());

  nsRefPtr<IDBCursor> cursor =
    new IDBCursor(Type_Index, aBackgroundActor, aKey);

  cursor->mSortKey = Move(aSortKey);
  cursor->mPrimaryKey = Move(aPrimaryKey);
  cursor->mCloneInfo = Move(aCloneInfo);

  return cursor.forget();
}

// static
already_AddRefed<IDBCursor>
IDBCursor::Create(BackgroundCursorChild* aBackgroundActor,
                  const Key& aKey,
                  const Key& aSortKey,
                  const Key& aPrimaryKey)
{
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor->GetIndex());
  MOZ_ASSERT(!aBackgroundActor->GetObjectStore());
  MOZ_ASSERT(!aKey.IsUnset());
  MOZ_ASSERT(!aPrimaryKey.IsUnset());

  nsRefPtr<IDBCursor> cursor =
    new IDBCursor(Type_IndexKey, aBackgroundActor, aKey);

  cursor->mSortKey = Move(aSortKey);
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

bool
IDBCursor::IsSourceDeleted() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mTransaction->IsOpen());

  IDBObjectStore* sourceObjectStore;
  if (mType == Type_Index || mType == Type_IndexKey) {
    MOZ_ASSERT(mSourceIndex);

    if (mSourceIndex->IsDeleted()) {
      return true;
    }

    sourceObjectStore = mSourceIndex->ObjectStore();
    MOZ_ASSERT(sourceObjectStore);
  } else {
    MOZ_ASSERT(mSourceObjectStore);
    sourceObjectStore = mSourceObjectStore;
  }

  return sourceObjectStore->IsDeleted();
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

  if (IsSourceDeleted() || !mHaveValue || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  Key key;
  aRv = key.SetFromJSVal(aCx, aKey);
  if (aRv.Failed()) {
    return;
  }

#ifdef ENABLE_INTL_API
  if (IsLocaleAware() && !key.IsUnset()) {
    Key tmp;
    aRv = key.ToLocaleBasedKey(tmp, mSourceIndex->Locale());
    if (aRv.Failed()) {
      return;
    }
    key = tmp;
  }

  const Key& sortKey = IsLocaleAware() ? mSortKey : mKey;
#else
  const Key& sortKey = mKey;
#endif

  if (!key.IsUnset()) {
    switch (mDirection) {
      case NEXT:
      case NEXT_UNIQUE:
        if (key <= sortKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      case PREV:
      case PREV_UNIQUE:
        if (key >= sortKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      default:
        MOZ_CRASH("Unknown direction type!");
    }
  }

  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();
  mRequest->SetLoggingSerialNumber(requestSerialNumber);

  if (mType == Type_ObjectStore || mType == Type_ObjectStoreKey) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s)."
                   "cursor(%s).continue(%s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBCursor.continue()",
                 IDB_LOG_ID_STRING(),
                 mTransaction->LoggingSerialNumber(),
                 requestSerialNumber,
                 IDB_LOG_STRINGIFY(mTransaction->Database()),
                 IDB_LOG_STRINGIFY(mTransaction),
                 IDB_LOG_STRINGIFY(mSourceObjectStore),
                 IDB_LOG_STRINGIFY(mDirection),
                 IDB_LOG_STRINGIFY(key));
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s)."
                   "index(%s).cursor(%s).continue(%s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBCursor.continue()",
                 IDB_LOG_ID_STRING(),
                 mTransaction->LoggingSerialNumber(),
                 requestSerialNumber,
                 IDB_LOG_STRINGIFY(mTransaction->Database()),
                 IDB_LOG_STRINGIFY(mTransaction),
                 IDB_LOG_STRINGIFY(mSourceIndex->ObjectStore()),
                 IDB_LOG_STRINGIFY(mSourceIndex),
                 IDB_LOG_STRINGIFY(mDirection),
                 IDB_LOG_STRINGIFY(key));
  }

  mBackgroundActor->SendContinueInternal(ContinueParams(key), mKey);

  mContinueCalled = true;
}

void
IDBCursor::Advance(uint32_t aCount, ErrorResult &aRv)
{
  AssertIsOnOwningThread();

  if (!aCount) {
    aRv.ThrowTypeError<MSG_INVALID_ADVANCE_COUNT>();
    return;
  }

  if (!mTransaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }


  if (IsSourceDeleted() || !mHaveValue || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();
  mRequest->SetLoggingSerialNumber(requestSerialNumber);

  if (mType == Type_ObjectStore || mType == Type_ObjectStoreKey) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s)."
                   "cursor(%s).advance(%ld)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBCursor.advance()",
                 IDB_LOG_ID_STRING(),
                 mTransaction->LoggingSerialNumber(),
                 requestSerialNumber,
                 IDB_LOG_STRINGIFY(mTransaction->Database()),
                 IDB_LOG_STRINGIFY(mTransaction),
                 IDB_LOG_STRINGIFY(mSourceObjectStore),
                 IDB_LOG_STRINGIFY(mDirection),
                 aCount);
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s)."
                   "index(%s).cursor(%s).advance(%ld)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBCursor.advance()",
                 IDB_LOG_ID_STRING(),
                 mTransaction->LoggingSerialNumber(),
                 requestSerialNumber,
                 IDB_LOG_STRINGIFY(mTransaction->Database()),
                 IDB_LOG_STRINGIFY(mTransaction),
                 IDB_LOG_STRINGIFY(mSourceIndex->ObjectStore()),
                 IDB_LOG_STRINGIFY(mSourceIndex),
                 IDB_LOG_STRINGIFY(mDirection),
                 aCount);
  }

  mBackgroundActor->SendContinueInternal(AdvanceParams(aCount), mKey);

  mContinueCalled = true;
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

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return nullptr;
  }

  if (IsSourceDeleted() ||
      !mHaveValue ||
      mType == Type_ObjectStoreKey ||
      mType == Type_IndexKey) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  MOZ_ASSERT(mType == Type_ObjectStore || mType == Type_Index);
  MOZ_ASSERT(!mKey.IsUnset());
  MOZ_ASSERT_IF(mType == Type_Index, !mPrimaryKey.IsUnset());

  mBackgroundActor->InvalidateCachedResponses();

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

    request = objectStore->AddOrPut(aCx,
                                    aValue,
                                    /* aKey */ JS::UndefinedHandleValue,
                                    /* aOverwrite */ true,
                                    /* aFromCursor */ true,
                                    aRv);
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

    request = objectStore->AddOrPut(aCx,
                                    aValue,
                                    keyVal,
                                    /* aOverwrite */ true,
                                    /* aFromCursor */ true,
                                    aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  request->SetSource(this);

  if (mType == Type_ObjectStore) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s)."
                   "cursor(%s).update(%s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBCursor.update()",
                 IDB_LOG_ID_STRING(),
                 mTransaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(mTransaction->Database()),
                 IDB_LOG_STRINGIFY(mTransaction),
                 IDB_LOG_STRINGIFY(objectStore),
                 IDB_LOG_STRINGIFY(mDirection),
                 IDB_LOG_STRINGIFY(objectStore, primaryKey));
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s)."
                   "index(%s).cursor(%s).update(%s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBCursor.update()",
                 IDB_LOG_ID_STRING(),
                 mTransaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(mTransaction->Database()),
                 IDB_LOG_STRINGIFY(mTransaction),
                 IDB_LOG_STRINGIFY(objectStore),
                 IDB_LOG_STRINGIFY(mSourceIndex),
                 IDB_LOG_STRINGIFY(mDirection),
                 IDB_LOG_STRINGIFY(objectStore, primaryKey));
  }

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

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return nullptr;
  }

  if (IsSourceDeleted() ||
      !mHaveValue ||
      mType == Type_ObjectStoreKey ||
      mType == Type_IndexKey ||
      mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  MOZ_ASSERT(mType == Type_ObjectStore || mType == Type_Index);
  MOZ_ASSERT(!mKey.IsUnset());

  mBackgroundActor->InvalidateCachedResponses();

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

  nsRefPtr<IDBRequest> request =
    objectStore->DeleteInternal(aCx, key, /* aFromCursor */ true, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  request->SetSource(this);

  if (mType == Type_ObjectStore) {
  IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                 "database(%s).transaction(%s).objectStore(%s)."
                 "cursor(%s).delete(%s)",
               "IndexedDB %s: C T[%lld] R[%llu]: IDBCursor.delete()",
               IDB_LOG_ID_STRING(),
               mTransaction->LoggingSerialNumber(),
               request->LoggingSerialNumber(),
               IDB_LOG_STRINGIFY(mTransaction->Database()),
               IDB_LOG_STRINGIFY(mTransaction),
               IDB_LOG_STRINGIFY(objectStore),
               IDB_LOG_STRINGIFY(mDirection),
               IDB_LOG_STRINGIFY(objectStore, primaryKey));
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s)."
                   "index(%s).cursor(%s).delete(%s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBCursor.delete()",
                 IDB_LOG_ID_STRING(),
                 mTransaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(mTransaction->Database()),
                 IDB_LOG_STRINGIFY(mTransaction),
                 IDB_LOG_STRINGIFY(objectStore),
                 IDB_LOG_STRINGIFY(mSourceIndex),
                 IDB_LOG_STRINGIFY(mDirection),
                 IDB_LOG_STRINGIFY(objectStore, primaryKey));
  }

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
                 Key&& aSortKey,
                 Key&& aPrimaryKey,
                 StructuredCloneReadInfo&& aValue)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mType == Type_Index);

  Reset();

  mKey = Move(aKey);
  mSortKey = Move(aSortKey);
  mPrimaryKey = Move(aPrimaryKey);
  mCloneInfo = Move(aValue);

  mHaveValue = !mKey.IsUnset();
}

void
IDBCursor::Reset(Key&& aKey,
                 Key&& aSortKey,
                 Key&& aPrimaryKey)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mType == Type_IndexKey);

  Reset();

  mKey = Move(aKey);
  mSortKey = Move(aSortKey);
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceObjectStore)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceIndex)
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
  // Don't unlink mRequest, mSourceObjectStore, or mSourceIndex!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->DropJSObjects();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
IDBCursor::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnOwningThread();

  switch (mType) {
    case Type_ObjectStore:
    case Type_Index:
      return IDBCursorWithValueBinding::Wrap(aCx, this, aGivenProto);

    case Type_ObjectStoreKey:
    case Type_IndexKey:
      return IDBCursorBinding::Wrap(aCx, this, aGivenProto);

    default:
      MOZ_CRASH("Bad type!");
  }
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
