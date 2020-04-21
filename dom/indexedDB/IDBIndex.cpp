/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBIndex.h"

#include "IDBCursorType.h"
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

using namespace mozilla::dom::indexedDB;

namespace {

RefPtr<IDBRequest> GenerateRequest(JSContext* aCx, IDBIndex* aIndex) {
  MOZ_ASSERT(aIndex);
  aIndex->AssertIsOnOwningThread();

  auto transaction = aIndex->ObjectStore()->AcquireTransaction();
  auto* const database = transaction->Database();

  return IDBRequest::Create(aCx, aIndex, database, std::move(transaction));
}

}  // namespace

IDBIndex::IDBIndex(IDBObjectStore* aObjectStore, const IndexMetadata* aMetadata)
    : mObjectStore(aObjectStore),
      mCachedKeyPath(JS::UndefinedValue()),
      mMetadata(aMetadata),
      mId(aMetadata->id()),
      mRooted(false) {
  MOZ_ASSERT(aObjectStore);
  aObjectStore->AssertIsOnOwningThread();
  MOZ_ASSERT(aMetadata);
}

IDBIndex::~IDBIndex() {
  AssertIsOnOwningThread();

  if (mRooted) {
    mCachedKeyPath.setUndefined();
    mozilla::DropJSObjects(this);
  }
}

RefPtr<IDBIndex> IDBIndex::Create(IDBObjectStore* aObjectStore,
                                  const IndexMetadata& aMetadata) {
  MOZ_ASSERT(aObjectStore);
  aObjectStore->AssertIsOnOwningThread();

  return new IDBIndex(aObjectStore, &aMetadata);
}

#ifdef DEBUG

void IDBIndex::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mObjectStore);
  mObjectStore->AssertIsOnOwningThread();
}

#endif  // DEBUG

RefPtr<IDBRequest> IDBIndex::OpenCursor(JSContext* aCx,
                                        JS::Handle<JS::Value> aRange,
                                        IDBCursorDirection aDirection,
                                        ErrorResult& aRv) {
  AssertIsOnOwningThread();

  return OpenCursorInternal(/* aKeysOnly */ false, aCx, aRange, aDirection,
                            aRv);
}

RefPtr<IDBRequest> IDBIndex::OpenKeyCursor(JSContext* aCx,
                                           JS::Handle<JS::Value> aRange,
                                           IDBCursorDirection aDirection,
                                           ErrorResult& aRv) {
  AssertIsOnOwningThread();

  return OpenCursorInternal(/* aKeysOnly */ true, aCx, aRange, aDirection, aRv);
}

RefPtr<IDBRequest> IDBIndex::Get(JSContext* aCx, JS::Handle<JS::Value> aKey,
                                 ErrorResult& aRv) {
  AssertIsOnOwningThread();

  return GetInternal(/* aKeyOnly */ false, aCx, aKey, aRv);
}

RefPtr<IDBRequest> IDBIndex::GetKey(JSContext* aCx, JS::Handle<JS::Value> aKey,
                                    ErrorResult& aRv) {
  AssertIsOnOwningThread();

  return GetInternal(/* aKeyOnly */ true, aCx, aKey, aRv);
}

RefPtr<IDBRequest> IDBIndex::GetAll(JSContext* aCx, JS::Handle<JS::Value> aKey,
                                    const Optional<uint32_t>& aLimit,
                                    ErrorResult& aRv) {
  AssertIsOnOwningThread();

  return GetAllInternal(/* aKeysOnly */ false, aCx, aKey, aLimit, aRv);
}

RefPtr<IDBRequest> IDBIndex::GetAllKeys(JSContext* aCx,
                                        JS::Handle<JS::Value> aKey,
                                        const Optional<uint32_t>& aLimit,
                                        ErrorResult& aRv) {
  AssertIsOnOwningThread();

  return GetAllInternal(/* aKeysOnly */ true, aCx, aKey, aLimit, aRv);
}

void IDBIndex::RefreshMetadata(bool aMayDelete) {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mDeletedMetadata, mMetadata == mDeletedMetadata.get());

  const auto& indexes = mObjectStore->Spec().indexes();
  const auto foundIt = std::find_if(
      indexes.cbegin(), indexes.cend(),
      [id = Id()](const auto& metadata) { return metadata.id() == id; });
  const bool found = foundIt != indexes.cend();

  MOZ_ASSERT_IF(!aMayDelete && !mDeletedMetadata, found);

  if (found) {
    mMetadata = &*foundIt;
    MOZ_ASSERT(mMetadata != mDeletedMetadata.get());
    mDeletedMetadata = nullptr;
  } else {
    NoteDeletion();
  }
}

void IDBIndex::NoteDeletion() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);
  MOZ_ASSERT(Id() == mMetadata->id());

  if (mDeletedMetadata) {
    MOZ_ASSERT(mMetadata == mDeletedMetadata.get());
    return;
  }

  mDeletedMetadata = MakeUnique<IndexMetadata>(*mMetadata);

  mMetadata = mDeletedMetadata.get();
}

const nsString& IDBIndex::Name() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->name();
}

void IDBIndex::SetName(const nsAString& aName, ErrorResult& aRv) {
  AssertIsOnOwningThread();

  const auto& transaction = mObjectStore->TransactionRef();

  if (transaction.GetMode() != IDBTransaction::Mode::VersionChange ||
      mDeletedMetadata) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (!transaction.IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (aName == mMetadata->name()) {
    return;
  }

  // Cache logging string of this index before renaming.
  const LoggingString loggingOldIndex(this);

  const int64_t indexId = Id();

  nsresult rv =
      transaction.Database()->RenameIndex(mObjectStore->Id(), indexId, aName);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  // Don't do this in the macro because we always need to increment the serial
  // number to keep in sync with the parent.
  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();

  IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
      "database(%s).transaction(%s).objectStore(%s).index(%s)."
      "rename(%s)",
      "IDBIndex.rename()", transaction.LoggingSerialNumber(),
      requestSerialNumber, IDB_LOG_STRINGIFY(transaction.Database()),
      IDB_LOG_STRINGIFY(transaction), IDB_LOG_STRINGIFY(mObjectStore),
      loggingOldIndex.get(), IDB_LOG_STRINGIFY(this));

  mObjectStore->MutableTransactionRef().RenameIndex(mObjectStore, indexId,
                                                    aName);
}

bool IDBIndex::Unique() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->unique();
}

bool IDBIndex::MultiEntry() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->multiEntry();
}

bool IDBIndex::LocaleAware() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->locale().IsEmpty();
}

const indexedDB::KeyPath& IDBIndex::GetKeyPath() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->keyPath();
}

void IDBIndex::GetLocale(nsString& aLocale) const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  if (mMetadata->locale().IsEmpty()) {
    SetDOMStringToNull(aLocale);
  } else {
    CopyASCIItoUTF16(mMetadata->locale(), aLocale);
  }
}

const nsCString& IDBIndex::Locale() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->locale();
}

bool IDBIndex::IsAutoLocale() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  return mMetadata->autoLocale();
}

nsIGlobalObject* IDBIndex::GetParentObject() const {
  AssertIsOnOwningThread();

  return mObjectStore->GetParentObject();
}

void IDBIndex::GetKeyPath(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                          ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!mCachedKeyPath.isUndefined()) {
    MOZ_ASSERT(mRooted);
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

  aResult.set(mCachedKeyPath);
}

RefPtr<IDBRequest> IDBIndex::GetInternal(bool aKeyOnly, JSContext* aCx,
                                         JS::Handle<JS::Value> aKey,
                                         ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (mDeletedMetadata) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  const auto& transaction = mObjectStore->TransactionRef();
  if (!transaction.IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  RefPtr<IDBKeyRange> keyRange;
  IDBKeyRange::FromJSVal(aCx, aKey, &keyRange, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!keyRange) {
    // Must specify a key or keyRange for get() and getKey().
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_KEY_ERR);
    return nullptr;
  }

  const int64_t objectStoreId = mObjectStore->Id();
  const int64_t indexId = Id();

  SerializedKeyRange serializedKeyRange;
  keyRange->ToSerialized(serializedKeyRange);

  RequestParams params;

  if (aKeyOnly) {
    params = IndexGetKeyParams(objectStoreId, indexId, serializedKeyRange);
  } else {
    params = IndexGetParams(objectStoreId, indexId, serializedKeyRange);
  }

  auto request = GenerateRequest(aCx, this);
  MOZ_ASSERT(request);

  if (aKeyOnly) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s).index(%s)."
        "getKey(%s)",
        "IDBIndex.getKey()", transaction.LoggingSerialNumber(),
        request->LoggingSerialNumber(),
        IDB_LOG_STRINGIFY(transaction.Database()),
        IDB_LOG_STRINGIFY(transaction), IDB_LOG_STRINGIFY(mObjectStore),
        IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(keyRange));
  } else {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s).index(%s)."
        "get(%s)",
        "IDBIndex.get()", transaction.LoggingSerialNumber(),
        request->LoggingSerialNumber(),
        IDB_LOG_STRINGIFY(transaction.Database()),
        IDB_LOG_STRINGIFY(transaction), IDB_LOG_STRINGIFY(mObjectStore),
        IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(keyRange));
  }

  auto& mutableTransaction = mObjectStore->MutableTransactionRef();

  // TODO: This is necessary to preserve request ordering only. Proper
  // sequencing of requests should be done in a more sophisticated manner that
  // doesn't require invalidating cursor caches (Bug 1580499).
  mutableTransaction.InvalidateCursorCaches();

  mutableTransaction.StartRequest(request, params);

  return request;
}

RefPtr<IDBRequest> IDBIndex::GetAllInternal(bool aKeysOnly, JSContext* aCx,
                                            JS::Handle<JS::Value> aKey,
                                            const Optional<uint32_t>& aLimit,
                                            ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (mDeletedMetadata) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  const auto& transaction = mObjectStore->TransactionRef();
  if (!transaction.IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  RefPtr<IDBKeyRange> keyRange;
  IDBKeyRange::FromJSVal(aCx, aKey, &keyRange, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  const int64_t objectStoreId = mObjectStore->Id();
  const int64_t indexId = Id();

  Maybe<SerializedKeyRange> optionalKeyRange;
  if (keyRange) {
    SerializedKeyRange serializedKeyRange;
    keyRange->ToSerialized(serializedKeyRange);
    optionalKeyRange.emplace(serializedKeyRange);
  }

  const uint32_t limit = aLimit.WasPassed() ? aLimit.Value() : 0;

  const auto& params =
      aKeysOnly ? RequestParams{IndexGetAllKeysParams(objectStoreId, indexId,
                                                      optionalKeyRange, limit)}
                : RequestParams{IndexGetAllParams(objectStoreId, indexId,
                                                  optionalKeyRange, limit)};

  auto request = GenerateRequest(aCx, this);
  MOZ_ASSERT(request);

  if (aKeysOnly) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s).index(%s)."
        "getAllKeys(%s, %s)",
        "IDBIndex.getAllKeys()", transaction.LoggingSerialNumber(),
        request->LoggingSerialNumber(),
        IDB_LOG_STRINGIFY(transaction.Database()),
        IDB_LOG_STRINGIFY(transaction), IDB_LOG_STRINGIFY(mObjectStore),
        IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(keyRange),
        IDB_LOG_STRINGIFY(aLimit));
  } else {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s).index(%s)."
        "getAll(%s, %s)",
        "IDBIndex.getAll()", transaction.LoggingSerialNumber(),
        request->LoggingSerialNumber(),
        IDB_LOG_STRINGIFY(transaction.Database()),
        IDB_LOG_STRINGIFY(transaction), IDB_LOG_STRINGIFY(mObjectStore),
        IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(keyRange),
        IDB_LOG_STRINGIFY(aLimit));
  }

  auto& mutableTransaction = mObjectStore->MutableTransactionRef();

  // TODO: This is necessary to preserve request ordering only. Proper
  // sequencing of requests should be done in a more sophisticated manner that
  // doesn't require invalidating cursor caches (Bug 1580499).
  mutableTransaction.InvalidateCursorCaches();

  mutableTransaction.StartRequest(request, params);

  return request;
}

RefPtr<IDBRequest> IDBIndex::OpenCursorInternal(bool aKeysOnly, JSContext* aCx,
                                                JS::Handle<JS::Value> aRange,
                                                IDBCursorDirection aDirection,
                                                ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (mDeletedMetadata) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  const auto& transaction = mObjectStore->TransactionRef();
  if (!transaction.IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  RefPtr<IDBKeyRange> keyRange;
  IDBKeyRange::FromJSVal(aCx, aRange, &keyRange, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  const int64_t objectStoreId = mObjectStore->Id();
  const int64_t indexId = Id();

  Maybe<SerializedKeyRange> optionalKeyRange;

  if (keyRange) {
    SerializedKeyRange serializedKeyRange;
    keyRange->ToSerialized(serializedKeyRange);

    optionalKeyRange.emplace(std::move(serializedKeyRange));
  }

  const CommonIndexOpenCursorParams commonIndexParams = {
      {objectStoreId, std::move(optionalKeyRange), aDirection}, indexId};

  const auto params =
      aKeysOnly ? OpenCursorParams{IndexOpenKeyCursorParams{commonIndexParams}}
                : OpenCursorParams{IndexOpenCursorParams{commonIndexParams}};

  auto request = GenerateRequest(aCx, this);
  MOZ_ASSERT(request);

  if (aKeysOnly) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s).index(%s)."
        "openKeyCursor(%s, %s)",
        "IDBIndex.openKeyCursor()", transaction.LoggingSerialNumber(),
        request->LoggingSerialNumber(),
        IDB_LOG_STRINGIFY(transaction.Database()),
        IDB_LOG_STRINGIFY(transaction), IDB_LOG_STRINGIFY(mObjectStore),
        IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(keyRange),
        IDB_LOG_STRINGIFY(aDirection));
  } else {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s).index(%s)."
        "openCursor(%s, %s)",
        "IDBIndex.openCursor()", transaction.LoggingSerialNumber(),
        request->LoggingSerialNumber(),
        IDB_LOG_STRINGIFY(transaction.Database()),
        IDB_LOG_STRINGIFY(transaction), IDB_LOG_STRINGIFY(mObjectStore),
        IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(keyRange),
        IDB_LOG_STRINGIFY(aDirection));
  }

  BackgroundCursorChildBase* const actor =
      aKeysOnly ? static_cast<BackgroundCursorChildBase*>(
                      new BackgroundCursorChild<IDBCursorType::IndexKey>(
                          request, this, aDirection))
                : new BackgroundCursorChild<IDBCursorType::Index>(request, this,
                                                                  aDirection);

  auto& mutableTransaction = mObjectStore->MutableTransactionRef();

  // TODO: This is necessary to preserve request ordering only. Proper
  // sequencing of requests should be done in a more sophisticated manner that
  // doesn't require invalidating cursor caches (Bug 1580499).
  mutableTransaction.InvalidateCursorCaches();

  mutableTransaction.OpenCursor(actor, params);

  return request;
}

RefPtr<IDBRequest> IDBIndex::Count(JSContext* aCx, JS::Handle<JS::Value> aKey,
                                   ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (mDeletedMetadata) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  const auto& transaction = mObjectStore->TransactionRef();
  if (!transaction.IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  RefPtr<IDBKeyRange> keyRange;
  IDBKeyRange::FromJSVal(aCx, aKey, &keyRange, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  IndexCountParams params;
  params.objectStoreId() = mObjectStore->Id();
  params.indexId() = Id();

  if (keyRange) {
    SerializedKeyRange serializedKeyRange;
    keyRange->ToSerialized(serializedKeyRange);
    params.optionalKeyRange().emplace(serializedKeyRange);
  }

  auto request = GenerateRequest(aCx, this);
  MOZ_ASSERT(request);

  IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
      "database(%s).transaction(%s).objectStore(%s).index(%s)."
      "count(%s)",
      "IDBIndex.count()", transaction.LoggingSerialNumber(),
      request->LoggingSerialNumber(), IDB_LOG_STRINGIFY(transaction.Database()),
      IDB_LOG_STRINGIFY(transaction), IDB_LOG_STRINGIFY(mObjectStore),
      IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(keyRange));

  auto& mutableTransaction = mObjectStore->MutableTransactionRef();

  // TODO: This is necessary to preserve request ordering only. Proper
  // sequencing of requests should be done in a more sophisticated manner that
  // doesn't require invalidating cursor caches (Bug 1580499).
  mutableTransaction.InvalidateCursorCaches();

  mutableTransaction.StartRequest(request, params);

  return request;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBIndex)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBIndex)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBIndex)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_MULTI_ZONE_JSHOLDER_CLASS(IDBIndex)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBIndex)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCachedKeyPath)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBIndex)
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

JSObject* IDBIndex::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return IDBIndex_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
