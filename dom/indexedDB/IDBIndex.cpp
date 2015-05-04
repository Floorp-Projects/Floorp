/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBIndex.h"

#include "FileInfo.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "IndexedDatabase.h"
#include "IndexedDatabaseInlines.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

namespace {

already_AddRefed<IDBRequest>
GenerateRequest(IDBIndex* aIndex)
{
  MOZ_ASSERT(aIndex);
  aIndex->AssertIsOnOwningThread();

  IDBTransaction* transaction = aIndex->ObjectStore()->Transaction();

  nsRefPtr<IDBRequest> request =
    IDBRequest::Create(aIndex, transaction->Database(), transaction);
  MOZ_ASSERT(request);

  return request.forget();
}

} // anonymous namespace

IDBIndex::IDBIndex(IDBObjectStore* aObjectStore, const IndexMetadata* aMetadata)
  : mObjectStore(aObjectStore)
  , mCachedKeyPath(JS::UndefinedValue())
  , mMetadata(aMetadata)
  , mId(aMetadata->id())
  , mRooted(false)
{
  MOZ_ASSERT(aObjectStore);
  aObjectStore->AssertIsOnOwningThread();
  MOZ_ASSERT(aMetadata);
}

IDBIndex::~IDBIndex()
{
  AssertIsOnOwningThread();

  if (mRooted) {
    mCachedKeyPath.setUndefined();
    mozilla::DropJSObjects(this);
  }
}

already_AddRefed<IDBIndex>
IDBIndex::Create(IDBObjectStore* aObjectStore,
                 const IndexMetadata& aMetadata)
{
  MOZ_ASSERT(aObjectStore);
  aObjectStore->AssertIsOnOwningThread();

  nsRefPtr<IDBIndex> index = new IDBIndex(aObjectStore, &aMetadata);

  return index.forget();
}

#ifdef DEBUG

void
IDBIndex::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mObjectStore);
  mObjectStore->AssertIsOnOwningThread();
}

#endif // DEBUG

void
IDBIndex::RefreshMetadata(bool aMayDelete)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mDeletedMetadata, mMetadata == mDeletedMetadata);

  const nsTArray<IndexMetadata>& indexes = mObjectStore->Spec().indexes();

  bool found = false;

  for (uint32_t count = indexes.Length(), index = 0;
       index < count;
       index++) {
    const IndexMetadata& metadata = indexes[index];

    if (metadata.id() == Id()) {
      mMetadata = &metadata;

      found = true;
      break;
    }
  }

  MOZ_ASSERT_IF(!aMayDelete && !mDeletedMetadata, found);

  if (found) {
    MOZ_ASSERT(mMetadata != mDeletedMetadata);
    mDeletedMetadata = nullptr;
  } else {
    NoteDeletion();
  }
}

void
IDBIndex::NoteDeletion()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);
  MOZ_ASSERT(Id() == mMetadata->id());

  if (mDeletedMetadata) {
    MOZ_ASSERT(mMetadata == mDeletedMetadata);
    return;
  }

  mDeletedMetadata = new IndexMetadata(*mMetadata);

  mMetadata = mDeletedMetadata;
}

const nsString&
IDBIndex::Name() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->name();
}

bool
IDBIndex::Unique() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->unique();
}

bool
IDBIndex::MultiEntry() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->multiEntry();
}

const KeyPath&
IDBIndex::GetKeyPath() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->keyPath();
}

nsPIDOMWindow*
IDBIndex::GetParentObject() const
{
  AssertIsOnOwningThread();

  return mObjectStore->GetParentObject();
}

void
IDBIndex::GetKeyPath(JSContext* aCx,
                     JS::MutableHandle<JS::Value> aResult,
                     ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (!mCachedKeyPath.isUndefined()) {
    MOZ_ASSERT(mRooted);
    JS::ExposeValueToActiveJS(mCachedKeyPath);
    aResult.set(mCachedKeyPath);
    return;
  }

  MOZ_ASSERT(!mRooted);

  aRv = GetKeyPath().ToJSVal(aCx, mCachedKeyPath);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (mCachedKeyPath.isGCThing()) {
    mozilla::HoldJSObjects(this);
    mRooted = true;
  }

  JS::ExposeValueToActiveJS(mCachedKeyPath);
  aResult.set(mCachedKeyPath);
}

already_AddRefed<IDBRequest>
IDBIndex::GetInternal(bool aKeyOnly,
                      JSContext* aCx,
                      JS::Handle<JS::Value> aKey,
                      ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!keyRange) {
    // Must specify a key or keyRange for get() and getKey().
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return nullptr;
  }

  const int64_t objectStoreId = mObjectStore->Id();
  const int64_t indexId = Id();

  OptionalKeyRange optionalKeyRange;
  if (keyRange) {
    SerializedKeyRange serializedKeyRange;
    keyRange->ToSerialized(serializedKeyRange);
    optionalKeyRange = serializedKeyRange;
  } else {
    optionalKeyRange = void_t();
  }

  RequestParams params;

  if (aKeyOnly) {
    params = IndexGetKeyParams(objectStoreId, indexId, optionalKeyRange);
  } else {
    params = IndexGetParams(objectStoreId, indexId, optionalKeyRange);
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  MOZ_ASSERT(request);

  if (aKeyOnly) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s).index(%s)."
                   "getKey(%s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBIndex.getKey()",
                 IDB_LOG_ID_STRING(),
                 transaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(transaction->Database()),
                 IDB_LOG_STRINGIFY(transaction),
                 IDB_LOG_STRINGIFY(mObjectStore),
                 IDB_LOG_STRINGIFY(this),
                 IDB_LOG_STRINGIFY(keyRange));
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s).index(%s)."
                   "get(%s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBIndex.get()",
                 IDB_LOG_ID_STRING(),
                 transaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(transaction->Database()),
                 IDB_LOG_STRINGIFY(transaction),
                 IDB_LOG_STRINGIFY(mObjectStore),
                 IDB_LOG_STRINGIFY(this),
                 IDB_LOG_STRINGIFY(keyRange));
  }

  transaction->StartRequest(request, params);

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::GetAllInternal(bool aKeysOnly,
                         JSContext* aCx,
                         JS::Handle<JS::Value> aKey,
                         const Optional<uint32_t>& aLimit,
                         ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  const int64_t objectStoreId = mObjectStore->Id();
  const int64_t indexId = Id();

  OptionalKeyRange optionalKeyRange;
  if (keyRange) {
    SerializedKeyRange serializedKeyRange;
    keyRange->ToSerialized(serializedKeyRange);
    optionalKeyRange = serializedKeyRange;
  } else {
    optionalKeyRange = void_t();
  }

  const uint32_t limit = aLimit.WasPassed() ? aLimit.Value() : 0;

  RequestParams params;
  if (aKeysOnly) {
    params = IndexGetAllKeysParams(objectStoreId, indexId, optionalKeyRange,
                                   limit);
  } else {
    params = IndexGetAllParams(objectStoreId, indexId, optionalKeyRange, limit);
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  MOZ_ASSERT(request);

  if (aKeysOnly) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s).index(%s)."
                   "getAllKeys(%s, %s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBIndex.getAllKeys()",
                 IDB_LOG_ID_STRING(),
                 transaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(transaction->Database()),
                 IDB_LOG_STRINGIFY(transaction),
                 IDB_LOG_STRINGIFY(mObjectStore),
                 IDB_LOG_STRINGIFY(this),
                 IDB_LOG_STRINGIFY(keyRange),
                 IDB_LOG_STRINGIFY(aLimit));
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s).index(%s)."
                   "getAll(%s, %s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBIndex.getAll()",
                 IDB_LOG_ID_STRING(),
                 transaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(transaction->Database()),
                 IDB_LOG_STRINGIFY(transaction),
                 IDB_LOG_STRINGIFY(mObjectStore),
                 IDB_LOG_STRINGIFY(this),
                 IDB_LOG_STRINGIFY(keyRange),
                 IDB_LOG_STRINGIFY(aLimit));
  }

  transaction->StartRequest(request, params);

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::OpenCursorInternal(bool aKeysOnly,
                             JSContext* aCx,
                             JS::Handle<JS::Value> aRange,
                             IDBCursorDirection aDirection,
                             ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aRange, getter_AddRefs(keyRange));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  int64_t objectStoreId = mObjectStore->Id();
  int64_t indexId = Id();

  OptionalKeyRange optionalKeyRange;

  if (keyRange) {
    SerializedKeyRange serializedKeyRange;
    keyRange->ToSerialized(serializedKeyRange);

    optionalKeyRange = Move(serializedKeyRange);
  } else {
    optionalKeyRange = void_t();
  }

  IDBCursor::Direction direction = IDBCursor::ConvertDirection(aDirection);

  OpenCursorParams params;
  if (aKeysOnly) {
    IndexOpenKeyCursorParams openParams;
    openParams.objectStoreId() = objectStoreId;
    openParams.indexId() = indexId;
    openParams.optionalKeyRange() = Move(optionalKeyRange);
    openParams.direction() = direction;

    params = Move(openParams);
  } else {
    IndexOpenCursorParams openParams;
    openParams.objectStoreId() = objectStoreId;
    openParams.indexId() = indexId;
    openParams.optionalKeyRange() = Move(optionalKeyRange);
    openParams.direction() = direction;

    params = Move(openParams);
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  MOZ_ASSERT(request);

  if (aKeysOnly) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s).index(%s)."
                   "openKeyCursor(%s, %s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: IDBIndex.openKeyCursor()",
                 IDB_LOG_ID_STRING(),
                 transaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(transaction->Database()),
                 IDB_LOG_STRINGIFY(transaction),
                 IDB_LOG_STRINGIFY(mObjectStore),
                 IDB_LOG_STRINGIFY(this),
                 IDB_LOG_STRINGIFY(keyRange),
                 IDB_LOG_STRINGIFY(direction));
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                   "database(%s).transaction(%s).objectStore(%s).index(%s)."
                   "openCursor(%s, %s)",
                 "IndexedDB %s: C T[%lld] R[%llu]: "
                   "IDBObjectStore.openKeyCursor()",
                 IDB_LOG_ID_STRING(),
                 transaction->LoggingSerialNumber(),
                 request->LoggingSerialNumber(),
                 IDB_LOG_STRINGIFY(transaction->Database()),
                 IDB_LOG_STRINGIFY(transaction),
                 IDB_LOG_STRINGIFY(mObjectStore),
                 IDB_LOG_STRINGIFY(this),
                 IDB_LOG_STRINGIFY(keyRange),
                 IDB_LOG_STRINGIFY(direction));
  }

  BackgroundCursorChild* actor =
    new BackgroundCursorChild(request, this, direction);

  mObjectStore->Transaction()->OpenCursor(actor, params);

  return request.forget();
}

already_AddRefed<IDBRequest>
IDBIndex::Count(JSContext* aCx,
                JS::Handle<JS::Value> aKey,
                ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  aRv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  if (aRv.Failed()) {
    return nullptr;
  }

  IndexCountParams params;
  params.objectStoreId() = mObjectStore->Id();
  params.indexId() = Id();

  if (keyRange) {
    SerializedKeyRange serializedKeyRange;
    keyRange->ToSerialized(serializedKeyRange);
    params.optionalKeyRange() = serializedKeyRange;
  } else {
    params.optionalKeyRange() = void_t();
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  MOZ_ASSERT(request);

  IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                 "database(%s).transaction(%s).objectStore(%s).index(%s)."
                 "count(%s)",
               "IndexedDB %s: C T[%lld] R[%llu]: IDBObjectStore.count()",
               IDB_LOG_ID_STRING(),
               transaction->LoggingSerialNumber(),
               request->LoggingSerialNumber(),
               IDB_LOG_STRINGIFY(transaction->Database()),
               IDB_LOG_STRINGIFY(transaction),
               IDB_LOG_STRINGIFY(mObjectStore),
               IDB_LOG_STRINGIFY(this),
               IDB_LOG_STRINGIFY(keyRange));

  transaction->StartRequest(request, params);

  return request.forget();
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBIndex)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBIndex)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBIndex)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

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

  tmp->mCachedKeyPath.setUndefined();

  if (tmp->mRooted) {
    mozilla::DropJSObjects(tmp);
    tmp->mRooted = false;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
IDBIndex::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return IDBIndexBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
