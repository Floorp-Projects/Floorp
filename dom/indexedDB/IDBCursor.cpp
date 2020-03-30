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

using namespace indexedDB;

IDBCursor::IDBCursor(BackgroundCursorChildBase* const aBackgroundActor)
    : mBackgroundActor(aBackgroundActor),
      mRequest(aBackgroundActor->GetRequest()),
      mTransaction(mRequest->GetTransaction()),
      mCachedKey(JS::UndefinedValue()),
      mCachedPrimaryKey(JS::UndefinedValue()),
      mCachedValue(JS::UndefinedValue()),
      mDirection(aBackgroundActor->GetDirection()),
      mHaveCachedKey(false),
      mHaveCachedPrimaryKey(false),
      mHaveCachedValue(false),
      mRooted(false),
      mContinueCalled(false),
      mHaveValue(true) {
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mTransaction);

  mTransaction->RegisterCursor(this);
}

template <IDBCursor::Type CursorType>
IDBTypedCursor<CursorType>::~IDBTypedCursor() {
  AssertIsOnOwningThread();

  mTransaction->UnregisterCursor(this);

  DropJSObjects();

  if (mBackgroundActor) {
    (*mBackgroundActor)->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
RefPtr<IDBObjectStoreCursor> IDBCursor::Create(
    BackgroundCursorChild<Type::ObjectStore>* const aBackgroundActor, Key aKey,
    StructuredCloneReadInfoChild&& aCloneInfo) {
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(!aKey.IsUnset());

  return MakeRefPtr<IDBObjectStoreCursor>(aBackgroundActor, std::move(aKey),
                                          std::move(aCloneInfo));
}

// static
RefPtr<IDBObjectStoreKeyCursor> IDBCursor::Create(
    BackgroundCursorChild<Type::ObjectStoreKey>* const aBackgroundActor,
    Key aKey) {
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(!aKey.IsUnset());

  return MakeRefPtr<IDBObjectStoreKeyCursor>(aBackgroundActor, std::move(aKey));
}

// static
RefPtr<IDBIndexCursor> IDBCursor::Create(
    BackgroundCursorChild<Type::Index>* const aBackgroundActor, Key aKey,
    Key aSortKey, Key aPrimaryKey, StructuredCloneReadInfoChild&& aCloneInfo) {
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(!aKey.IsUnset());
  MOZ_ASSERT(!aPrimaryKey.IsUnset());

  return MakeRefPtr<IDBIndexCursor>(aBackgroundActor, std::move(aKey),
                                    std::move(aSortKey), std::move(aPrimaryKey),
                                    std::move(aCloneInfo));
}

// static
RefPtr<IDBIndexKeyCursor> IDBCursor::Create(
    BackgroundCursorChild<Type::IndexKey>* const aBackgroundActor, Key aKey,
    Key aSortKey, Key aPrimaryKey) {
  MOZ_ASSERT(aBackgroundActor);
  aBackgroundActor->AssertIsOnOwningThread();
  MOZ_ASSERT(!aKey.IsUnset());
  MOZ_ASSERT(!aPrimaryKey.IsUnset());

  return MakeRefPtr<IDBIndexKeyCursor>(aBackgroundActor, std::move(aKey),
                                       std::move(aSortKey),
                                       std::move(aPrimaryKey));
}

#ifdef DEBUG

void IDBCursor::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnOwningThread();
}

#endif  // DEBUG

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::DropJSObjects() {
  AssertIsOnOwningThread();

  Reset();

  if (!mRooted) {
    return;
  }

  mRooted = false;

  mozilla::DropJSObjects(this);
}

template <IDBCursor::Type CursorType>
bool IDBTypedCursor<CursorType>::IsSourceDeleted() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mTransaction->IsActive());

  const auto* const sourceObjectStore = [this]() -> const IDBObjectStore* {
    if constexpr (IsObjectStoreCursor) {
      return mSource;
    } else {
      if (GetSourceRef().IsDeleted()) {
        return nullptr;
      }

      const auto* const res = GetSourceRef().ObjectStore();
      MOZ_ASSERT(res);
      return res;
    }
  }();

  return !sourceObjectStore || sourceObjectStore->IsDeleted();
}

void IDBCursor::ResetBase() {
  AssertIsOnOwningThread();

  mCachedKey.setUndefined();
  mCachedPrimaryKey.setUndefined();
  mCachedValue.setUndefined();

  mHaveCachedKey = false;
  mHaveCachedPrimaryKey = false;
  mHaveCachedValue = false;
  mHaveValue = false;
  mContinueCalled = false;
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::Reset() {
  AssertIsOnOwningThread();

  if constexpr (!IsKeyOnlyCursor) {
    IDBObjectStore::ClearCloneReadInfo(mData.mCloneInfo);
  }

  ResetBase();
}

nsIGlobalObject* IDBCursor::GetParentObject() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  return mTransaction->GetParentObject();
}

IDBCursorDirection IDBCursor::GetDirection() const {
  AssertIsOnOwningThread();

  switch (mDirection) {
    case Direction::Next:
      return IDBCursorDirection::Next;

    case Direction::Nextunique:
      return IDBCursorDirection::Nextunique;

    case Direction::Prev:
      return IDBCursorDirection::Prev;

    case Direction::Prevunique:
      return IDBCursorDirection::Prevunique;

    default:
      MOZ_CRASH("Bad direction!");
  }
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::GetSource(
    OwningIDBObjectStoreOrIDBIndex& aSource) const {
  AssertIsOnOwningThread();

  if constexpr (IsObjectStoreCursor) {
    aSource.SetAsIDBObjectStore() = mSource;
  } else {
    aSource.SetAsIDBIndex() = mSource;
  }
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::GetKey(JSContext* const aCx,
                                        JS::MutableHandle<JS::Value> aResult,
                                        ErrorResult& aRv) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mData.mKey.IsUnset() || !mHaveValue);

  if (!mHaveValue) {
    aResult.setUndefined();
    return;
  }

  if (!mHaveCachedKey) {
    if (!mRooted) {
      mozilla::HoldJSObjects(this);
      mRooted = true;
    }

    aRv = mData.mKey.ToJSVal(aCx, mCachedKey);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    mHaveCachedKey = true;
  }

  aResult.set(mCachedKey);
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::GetPrimaryKey(
    JSContext* const aCx, JS::MutableHandle<JS::Value> aResult,
    ErrorResult& aRv) {
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

    const Key& key = mData.GetObjectStoreKey();

    MOZ_ASSERT(!key.IsUnset());

    aRv = key.ToJSVal(aCx, mCachedPrimaryKey);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    mHaveCachedPrimaryKey = true;
  }

  aResult.set(mCachedPrimaryKey);
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::GetValue(JSContext* const aCx,
                                          JS::MutableHandle<JS::Value> aResult,
                                          ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if constexpr (!IsKeyOnlyCursor) {
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
      if (NS_WARN_IF(!IDBObjectStore::DeserializeValue(
              aCx, std::move(mData.mCloneInfo), &val))) {
        aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
        return;
      }

      // XXX This seems redundant, sine mData.mCloneInfo is moved above.
      IDBObjectStore::ClearCloneReadInfo(mData.mCloneInfo);

      mCachedValue = val;
      mHaveCachedValue = true;
    }

    aResult.set(mCachedValue);
  } else {
    MOZ_CRASH("This shouldn't be callable on a key-only cursor.");
  }
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::Continue(JSContext* const aCx,
                                          JS::Handle<JS::Value> aKey,
                                          ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!mTransaction->IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (IsSourceDeleted() || !mHaveValue || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  Key key;
  auto result = key.SetFromJSVal(aCx, aKey, aRv);
  if (!result.Is(Ok, aRv)) {
    if (result.Is(Invalid, aRv)) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    }
    return;
  }

  if constexpr (!IsObjectStoreCursor) {
    if (IsLocaleAware() && !key.IsUnset()) {
      Key tmp;

      result = key.ToLocaleAwareKey(tmp, GetSourceRef().Locale(), aRv);
      if (!result.Is(Ok, aRv)) {
        if (result.Is(Invalid, aRv)) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
        }
        return;
      }
      key = tmp;
    }
  }

  const Key& sortKey = mData.GetSortKey(IsLocaleAware());

  if (!key.IsUnset()) {
    switch (mDirection) {
      case Direction::Next:
      case Direction::Nextunique:
        if (key <= sortKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      case Direction::Prev:
      case Direction::Prevunique:
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

  if constexpr (IsObjectStoreCursor) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s)."
        "cursor(%s).continue(%s)",
        "IDBCursor.continue()", mTransaction->LoggingSerialNumber(),
        requestSerialNumber, IDB_LOG_STRINGIFY(mTransaction->Database()),
        IDB_LOG_STRINGIFY(mTransaction), IDB_LOG_STRINGIFY(mSource),
        IDB_LOG_STRINGIFY(mDirection), IDB_LOG_STRINGIFY(key));
  } else {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s)."
        "index(%s).cursor(%s).continue(%s)",
        "IDBCursor.continue()", mTransaction->LoggingSerialNumber(),
        requestSerialNumber, IDB_LOG_STRINGIFY(mTransaction->Database()),
        IDB_LOG_STRINGIFY(mTransaction),
        IDB_LOG_STRINGIFY(GetSourceRef().ObjectStore()),
        IDB_LOG_STRINGIFY(mSource), IDB_LOG_STRINGIFY(mDirection),
        IDB_LOG_STRINGIFY(key));
  }

  GetTypedBackgroundActorRef().SendContinueInternal(ContinueParams(key), mData);

  mContinueCalled = true;
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::ContinuePrimaryKey(
    JSContext* const aCx, JS::Handle<JS::Value> aKey,
    JS::Handle<JS::Value> aPrimaryKey, ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!mTransaction->IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (IsSourceDeleted()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  if (IsObjectStoreCursor ||
      (mDirection != Direction::Next && mDirection != Direction::Prev)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  if constexpr (!IsObjectStoreCursor) {
    if (!mHaveValue || mContinueCalled) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
      return;
    }

    Key key;
    auto result = key.SetFromJSVal(aCx, aKey, aRv);
    if (!result.Is(Ok, aRv)) {
      if (result.Is(Invalid, aRv)) {
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      }
      return;
    }

    if (IsLocaleAware() && !key.IsUnset()) {
      Key tmp;
      result = key.ToLocaleAwareKey(tmp, GetSourceRef().Locale(), aRv);
      if (!result.Is(Ok, aRv)) {
        if (result.Is(Invalid, aRv)) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
        }
        return;
      }
      key = tmp;
    }

    if (key.IsUnset()) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return;
    }

    Key primaryKey;
    result = primaryKey.SetFromJSVal(aCx, aPrimaryKey, aRv);
    if (!result.Is(Ok, aRv)) {
      if (result.Is(Invalid, aRv)) {
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      }
      return;
    }

    if (primaryKey.IsUnset()) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return;
    }

    const Key& sortKey = mData.GetSortKey(IsLocaleAware());

    switch (mDirection) {
      case Direction::Next:
        if (key < sortKey ||
            (key == sortKey && primaryKey <= mData.mObjectStoreKey)) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      case Direction::Prev:
        if (key > sortKey ||
            (key == sortKey && primaryKey >= mData.mObjectStoreKey)) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return;
        }
        break;

      default:
        MOZ_CRASH("Unknown direction type!");
    }

    const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();
    mRequest->SetLoggingSerialNumber(requestSerialNumber);

    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s)."
        "index(%s).cursor(%s).continuePrimaryKey(%s, %s)",
        "IDBCursor.continuePrimaryKey()", mTransaction->LoggingSerialNumber(),
        requestSerialNumber, IDB_LOG_STRINGIFY(mTransaction->Database()),
        IDB_LOG_STRINGIFY(mTransaction),
        IDB_LOG_STRINGIFY(&GetSourceObjectStoreRef()),
        IDB_LOG_STRINGIFY(mSource), IDB_LOG_STRINGIFY(mDirection),
        IDB_LOG_STRINGIFY(key), IDB_LOG_STRINGIFY(primaryKey));

    GetTypedBackgroundActorRef().SendContinueInternal(
        ContinuePrimaryKeyParams(key, primaryKey), mData);

    mContinueCalled = true;
  }
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::Advance(const uint32_t aCount,
                                         ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!aCount) {
    aRv.ThrowTypeError("0 (Zero) is not a valid advance count.");
    return;
  }

  if (!mTransaction->IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (IsSourceDeleted() || !mHaveValue || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();
  mRequest->SetLoggingSerialNumber(requestSerialNumber);

  if constexpr (IsObjectStoreCursor) {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s)."
        "cursor(%s).advance(%ld)",
        "IDBCursor.advance()", mTransaction->LoggingSerialNumber(),
        requestSerialNumber, IDB_LOG_STRINGIFY(mTransaction->Database()),
        IDB_LOG_STRINGIFY(mTransaction), IDB_LOG_STRINGIFY(mSource),
        IDB_LOG_STRINGIFY(mDirection), aCount);
  } else {
    IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
        "database(%s).transaction(%s).objectStore(%s)."
        "index(%s).cursor(%s).advance(%ld)",
        "IDBCursor.advance()", mTransaction->LoggingSerialNumber(),
        requestSerialNumber, IDB_LOG_STRINGIFY(mTransaction->Database()),
        IDB_LOG_STRINGIFY(mTransaction),
        IDB_LOG_STRINGIFY(GetSourceRef().ObjectStore()),
        IDB_LOG_STRINGIFY(mSource), IDB_LOG_STRINGIFY(mDirection), aCount);
  }

  GetTypedBackgroundActorRef().SendContinueInternal(AdvanceParams(aCount),
                                                    mData);

  mContinueCalled = true;
}

template <IDBCursor::Type CursorType>
RefPtr<IDBRequest> IDBTypedCursor<CursorType>::Update(
    JSContext* const aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!mTransaction->IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return nullptr;
  }

  if (mTransaction->GetMode() == IDBTransaction::Mode::Cleanup ||
      IsSourceDeleted() || !mHaveValue || IsKeyOnlyCursor || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  if constexpr (!IsKeyOnlyCursor) {
    MOZ_ASSERT(!mData.mKey.IsUnset());
    if constexpr (!IsObjectStoreCursor) {
      MOZ_ASSERT(!mData.mObjectStoreKey.IsUnset());
    }

    mTransaction->InvalidateCursorCaches();

    IDBObjectStore::ValueWrapper valueWrapper(aCx, aValue);

    const Key& primaryKey = mData.GetObjectStoreKey();

    RefPtr<IDBRequest> request;

    IDBObjectStore& objectStore = GetSourceObjectStoreRef();
    if (objectStore.HasValidKeyPath()) {
      if (!valueWrapper.Clone(aCx)) {
        aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
        return nullptr;
      }

      // Make sure the object given has the correct keyPath value set on it.
      const KeyPath& keyPath = objectStore.GetKeyPath();
      Key key;

      aRv = keyPath.ExtractKey(aCx, valueWrapper.Value(), key);
      if (aRv.Failed()) {
        return nullptr;
      }

      if (key != primaryKey) {
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
        return nullptr;
      }

      request = objectStore.AddOrPut(aCx, valueWrapper,
                                     /* aKey */ JS::UndefinedHandleValue,
                                     /* aOverwrite */ true,
                                     /* aFromCursor */ true, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
    } else {
      JS::Rooted<JS::Value> keyVal(aCx);
      aRv = primaryKey.ToJSVal(aCx, &keyVal);
      if (aRv.Failed()) {
        return nullptr;
      }

      request = objectStore.AddOrPut(aCx, valueWrapper, keyVal,
                                     /* aOverwrite */ true,
                                     /* aFromCursor */ true, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
    }

    request->SetSource(this);

    if (IsObjectStoreCursor) {
      IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
          "database(%s).transaction(%s).objectStore(%s)."
          "cursor(%s).update(%s)",
          "IDBCursor.update()", mTransaction->LoggingSerialNumber(),
          request->LoggingSerialNumber(),
          IDB_LOG_STRINGIFY(mTransaction->Database()),
          IDB_LOG_STRINGIFY(mTransaction), IDB_LOG_STRINGIFY(&objectStore),
          IDB_LOG_STRINGIFY(mDirection),
          IDB_LOG_STRINGIFY(&objectStore, primaryKey));
    } else {
      IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
          "database(%s).transaction(%s).objectStore(%s)."
          "index(%s).cursor(%s).update(%s)",
          "IDBCursor.update()", mTransaction->LoggingSerialNumber(),
          request->LoggingSerialNumber(),
          IDB_LOG_STRINGIFY(mTransaction->Database()),
          IDB_LOG_STRINGIFY(mTransaction), IDB_LOG_STRINGIFY(&objectStore),
          IDB_LOG_STRINGIFY(mSource), IDB_LOG_STRINGIFY(mDirection),
          IDB_LOG_STRINGIFY(&objectStore, primaryKey));
    }

    return request;
  } else {
    // XXX: Just to work around a bug in gcc, which otherwise claims 'control
    // reaches end of non-void function', which is not true.
    return nullptr;
  }
}

template <IDBCursor::Type CursorType>
RefPtr<IDBRequest> IDBTypedCursor<CursorType>::Delete(JSContext* const aCx,
                                                      ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!mTransaction->IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return nullptr;
  }

  if (IsSourceDeleted() || !mHaveValue || IsKeyOnlyCursor || mContinueCalled) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  if constexpr (!IsKeyOnlyCursor) {
    MOZ_ASSERT(!mData.mKey.IsUnset());

    mTransaction->InvalidateCursorCaches();

    const Key& primaryKey = mData.GetObjectStoreKey();

    JS::Rooted<JS::Value> key(aCx);
    aRv = primaryKey.ToJSVal(aCx, &key);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    auto& objectStore = GetSourceObjectStoreRef();
    RefPtr<IDBRequest> request =
        objectStore.DeleteInternal(aCx, key, /* aFromCursor */ true, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    request->SetSource(this);

    if (IsObjectStoreCursor) {
      IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
          "database(%s).transaction(%s).objectStore(%s)."
          "cursor(%s).delete(%s)",
          "IDBCursor.delete()", mTransaction->LoggingSerialNumber(),
          request->LoggingSerialNumber(),
          IDB_LOG_STRINGIFY(mTransaction->Database()),
          IDB_LOG_STRINGIFY(mTransaction), IDB_LOG_STRINGIFY(&objectStore),
          IDB_LOG_STRINGIFY(mDirection),
          IDB_LOG_STRINGIFY(&objectStore, primaryKey));
    } else {
      IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
          "database(%s).transaction(%s).objectStore(%s)."
          "index(%s).cursor(%s).delete(%s)",
          "IDBCursor.delete()", mTransaction->LoggingSerialNumber(),
          request->LoggingSerialNumber(),
          IDB_LOG_STRINGIFY(mTransaction->Database()),
          IDB_LOG_STRINGIFY(mTransaction), IDB_LOG_STRINGIFY(&objectStore),
          IDB_LOG_STRINGIFY(mSource), IDB_LOG_STRINGIFY(mDirection),
          IDB_LOG_STRINGIFY(&objectStore, primaryKey));
    }

    return request;
  } else {
    // XXX: Just to work around a bug in gcc, which otherwise claims 'control
    // reaches end of non-void function', which is not true.
    return nullptr;
  }
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::Reset(CursorData<CursorType>&& aCursorData) {
  this->AssertIsOnOwningThread();

  Reset();

  mData = std::move(aCursorData);

  mHaveValue = !mData.mKey.IsUnset();
}

template <IDBCursor::Type CursorType>
void IDBTypedCursor<CursorType>::InvalidateCachedResponses() {
  AssertIsOnOwningThread();

  // TODO: Can mBackgroundActor actually be empty at this point?
  if (mBackgroundActor) {
    GetTypedBackgroundActorRef().InvalidateCachedResponses();
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBCursor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBCursor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBCursor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBCursor)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBCursor)
  MOZ_ASSERT_IF(!tmp->mHaveCachedKey, tmp->mCachedKey.isUndefined());
  MOZ_ASSERT_IF(!tmp->mHaveCachedPrimaryKey,
                tmp->mCachedPrimaryKey.isUndefined());
  MOZ_ASSERT_IF(!tmp->mHaveCachedValue, tmp->mCachedValue.isUndefined());

  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCachedKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCachedPrimaryKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCachedValue)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBCursor)
// Unlinking is done in the subclasses.
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// Don't unlink mRequest or mSource in
// NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED!
#define NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_SUBCLASS_METHODS(_subclassName)    \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_subclassName, IDBCursor) \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)                                \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                       \
                                                                              \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_subclassName, IDBCursor)   \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER                         \
    tmp->DropJSObjects();                                                     \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                         \
                                                                              \
  NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(_subclassName)                      \
  NS_INTERFACE_MAP_END_INHERITING(IDBCursor)                                  \
                                                                              \
  NS_IMPL_ADDREF_INHERITED(_subclassName, IDBCursor)                          \
  NS_IMPL_RELEASE_INHERITED(_subclassName, IDBCursor)

#define NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_SUBCLASS(_subclassName) \
  NS_IMPL_CYCLE_COLLECTION_CLASS(_subclassName)                    \
  NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_SUBCLASS_METHODS(_subclassName)

#define NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_MULTI_ZONE_JSHOLDER_SUBCLASS( \
    _subclassName)                                                       \
  NS_IMPL_CYCLE_COLLECTION_MULTI_ZONE_JSHOLDER_CLASS(_subclassName)      \
  NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_SUBCLASS_METHODS(_subclassName)

NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_MULTI_ZONE_JSHOLDER_SUBCLASS(
    IDBObjectStoreCursor)
NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_SUBCLASS(IDBObjectStoreKeyCursor)
NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_MULTI_ZONE_JSHOLDER_SUBCLASS(IDBIndexCursor)
NS_IMPL_CYCLE_COLLECTION_IDBCURSOR_SUBCLASS(IDBIndexKeyCursor)

template <IDBCursor::Type CursorType>
JSObject* IDBTypedCursor<CursorType>::WrapObject(
    JSContext* const aCx, JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  return IsKeyOnlyCursor
             ? IDBCursor_Binding::Wrap(aCx, this, aGivenProto)
             : IDBCursorWithValue_Binding::Wrap(aCx, this, aGivenProto);
}

template <IDBCursor::Type CursorType>
template <typename... DataArgs>
IDBTypedCursor<CursorType>::IDBTypedCursor(
    indexedDB::BackgroundCursorChild<CursorType>* const aBackgroundActor,
    DataArgs&&... aDataArgs)
    : IDBCursor{aBackgroundActor},
      mData{std::forward<DataArgs>(aDataArgs)...},
      mSource(aBackgroundActor->GetSource()) {}

template <IDBCursor::Type CursorType>
bool IDBTypedCursor<CursorType>::IsLocaleAware() const {
  if constexpr (IsObjectStoreCursor) {
    return false;
  } else {
    return !GetSourceRef().Locale().IsEmpty();
  }
}

template class IDBTypedCursor<IDBCursorType::ObjectStore>;
template class IDBTypedCursor<IDBCursorType::ObjectStoreKey>;
template class IDBTypedCursor<IDBCursorType::Index>;
template class IDBTypedCursor<IDBCursorType::IndexKey>;

}  // namespace dom
}  // namespace mozilla
