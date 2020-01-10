/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbcursor_h__
#define mozilla_dom_idbcursor_h__

#include "IndexedDatabase.h"
#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IDBTransaction.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class IDBIndex;
class IDBObjectStore;
class IDBRequest;
class OwningIDBObjectStoreOrIDBIndex;

namespace indexedDB {
class BackgroundCursorChild;
}

// TODO: Consider defining different subclasses for the different cursor types,
// possibly using the CRTP, which would remove the need for various case
// distinctions.
class IDBCursor final : public nsISupports, public nsWrapperCache {
 public:
  using Key = indexedDB::Key;
  using StructuredCloneReadInfo = indexedDB::StructuredCloneReadInfo;

  enum struct Direction {
    Next = 0,
    NextUnique,
    Prev,
    PrevUnique,

    // Only needed for IPC serialization helper, should never be used in code.
    Invalid
  };

  enum struct Type {
    ObjectStore,
    ObjectStoreKey,
    Index,
    IndexKey,
  };

 private:
  indexedDB::BackgroundCursorChild* mBackgroundActor;

  // TODO: mRequest, mSourceObjectStore and mSourceIndex could be made const if
  // Bug 1575173 is resolved. They are initialized in the constructor and never
  // modified/cleared.
  RefPtr<IDBRequest> mRequest;
  RefPtr<IDBObjectStore> mSourceObjectStore;
  RefPtr<IDBIndex> mSourceIndex;

  // mSourceObjectStore or mSourceIndex will hold this alive.
  const CheckedUnsafePtr<IDBTransaction> mTransaction;

  // These are cycle-collected!
  JS::Heap<JS::Value> mCachedKey;
  JS::Heap<JS::Value> mCachedPrimaryKey;
  JS::Heap<JS::Value> mCachedValue;

  Key mKey;
  Key mSortKey;     ///< AKA locale aware key/position elsewhere
  Key mPrimaryKey;  ///< AKA object store key/position elsewhere
  StructuredCloneReadInfo mCloneInfo;

  const Type mType;
  const Direction mDirection;

  bool mHaveCachedKey : 1;
  bool mHaveCachedPrimaryKey : 1;
  bool mHaveCachedValue : 1;
  bool mRooted : 1;
  bool mContinueCalled : 1;
  bool mHaveValue : 1;

 public:
  static MOZ_MUST_USE RefPtr<IDBCursor> Create(
      indexedDB::BackgroundCursorChild* aBackgroundActor, Key aKey,
      StructuredCloneReadInfo&& aCloneInfo);

  static MOZ_MUST_USE RefPtr<IDBCursor> Create(
      indexedDB::BackgroundCursorChild* aBackgroundActor, Key aKey);

  static MOZ_MUST_USE RefPtr<IDBCursor> Create(
      indexedDB::BackgroundCursorChild* aBackgroundActor, Key aKey,
      Key aSortKey, Key aPrimaryKey, StructuredCloneReadInfo&& aCloneInfo);

  static MOZ_MUST_USE RefPtr<IDBCursor> Create(
      indexedDB::BackgroundCursorChild* aBackgroundActor, Key aKey,
      Key aSortKey, Key aPrimaryKey);

  static Direction ConvertDirection(IDBCursorDirection aDirection);

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  nsIGlobalObject* GetParentObject() const;

  void GetSource(OwningIDBObjectStoreOrIDBIndex& aSource) const;

  IDBCursorDirection GetDirection() const;

  Type GetType() const;

  void GetKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
              ErrorResult& aRv);

  void GetPrimaryKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                     ErrorResult& aRv);

  void GetValue(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                ErrorResult& aRv);

  void Continue(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv);

  void ContinuePrimaryKey(JSContext* aCx, JS::Handle<JS::Value> aKey,
                          JS::Handle<JS::Value> aPrimaryKey, ErrorResult& aRv);

  void Advance(uint32_t aCount, ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> Update(JSContext* aCx,
                                         JS::Handle<JS::Value> aValue,
                                         ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> Delete(JSContext* aCx, ErrorResult& aRv);

  void Reset();

  void Reset(Key&& aKey, StructuredCloneReadInfo&& aValue);

  void Reset(Key&& aKey);

  void Reset(Key&& aKey, Key&& aSortKey, Key&& aPrimaryKey,
             StructuredCloneReadInfo&& aValue);

  void Reset(Key&& aKey, Key&& aSortKey, Key&& aPrimaryKey);

  void ClearBackgroundActor() {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  void InvalidateCachedResponses();

  // Checks if this is a locale aware cursor (ie. the index's sortKey is unset)
  bool IsLocaleAware() const;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBCursor)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  IDBCursor(Type aType, indexedDB::BackgroundCursorChild* aBackgroundActor,
            Key aKey);

  ~IDBCursor();

  void DropJSObjects();

  bool IsSourceDeleted() const;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbcursor_h__
