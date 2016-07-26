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
#include "mozilla/dom/indexedDB/Key.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class IDBIndex;
class IDBObjectStore;
class IDBRequest;
class IDBTransaction;
class OwningIDBObjectStoreOrIDBIndex;

namespace indexedDB {
class BackgroundCursorChild;
}

class IDBCursor final
  : public nsISupports
  , public nsWrapperCache
{
public:
  typedef indexedDB::Key Key;
  typedef indexedDB::StructuredCloneReadInfo StructuredCloneReadInfo;

  enum Direction
  {
    NEXT = 0,
    NEXT_UNIQUE,
    PREV,
    PREV_UNIQUE,

    // Only needed for IPC serialization helper, should never be used in code.
    DIRECTION_INVALID
  };

private:
  enum Type
  {
    Type_ObjectStore,
    Type_ObjectStoreKey,
    Type_Index,
    Type_IndexKey,
  };

  indexedDB::BackgroundCursorChild* mBackgroundActor;

  RefPtr<IDBRequest> mRequest;
  RefPtr<IDBObjectStore> mSourceObjectStore;
  RefPtr<IDBIndex> mSourceIndex;

  // mSourceObjectStore or mSourceIndex will hold this alive.
  IDBTransaction* mTransaction;

  JS::Heap<JSObject*> mScriptOwner;

  // These are cycle-collected!
  JS::Heap<JS::Value> mCachedKey;
  JS::Heap<JS::Value> mCachedPrimaryKey;
  JS::Heap<JS::Value> mCachedValue;

  Key mKey;
  Key mSortKey;
  Key mPrimaryKey;
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
  static already_AddRefed<IDBCursor>
  Create(indexedDB::BackgroundCursorChild* aBackgroundActor,
         const Key& aKey,
         StructuredCloneReadInfo&& aCloneInfo);

  static already_AddRefed<IDBCursor>
  Create(indexedDB::BackgroundCursorChild* aBackgroundActor,
         const Key& aKey);

  static already_AddRefed<IDBCursor>
  Create(indexedDB::BackgroundCursorChild* aBackgroundActor,
         const Key& aKey,
         const Key& aSortKey,
         const Key& aPrimaryKey,
         StructuredCloneReadInfo&& aCloneInfo);

  static already_AddRefed<IDBCursor>
  Create(indexedDB::BackgroundCursorChild* aBackgroundActor,
         const Key& aKey,
         const Key& aSortKey,
         const Key& aPrimaryKey);

  static Direction
  ConvertDirection(IDBCursorDirection aDirection);

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  nsPIDOMWindowInner*
  GetParentObject() const;

  void
  GetSource(OwningIDBObjectStoreOrIDBIndex& aSource) const;

  IDBCursorDirection
  GetDirection() const;

  void
  GetKey(JSContext* aCx,
         JS::MutableHandle<JS::Value> aResult,
         ErrorResult& aRv);

  void
  GetPrimaryKey(JSContext* aCx,
                JS::MutableHandle<JS::Value> aResult,
                ErrorResult& aRv);

  void
  GetValue(JSContext* aCx,
           JS::MutableHandle<JS::Value> aResult,
           ErrorResult& aRv);

  void
  Continue(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv);

  void
  ContinuePrimaryKey(JSContext* aCx,
                     JS::Handle<JS::Value> aKey,
                     JS::Handle<JS::Value> aPrimaryKey,
                     ErrorResult& aRv);

  void
  Advance(uint32_t aCount, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Update(JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Delete(JSContext* aCx, ErrorResult& aRv);

  void
  Reset();

  void
  Reset(Key&& aKey, StructuredCloneReadInfo&& aValue);

  void
  Reset(Key&& aKey);

  void
  Reset(Key&& aKey, Key&& aSortKey, Key&& aPrimaryKey, StructuredCloneReadInfo&& aValue);

  void
  Reset(Key&& aKey, Key&& aSortKey, Key&& aPrimaryKey);

  void
  ClearBackgroundActor()
  {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBCursor)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  IDBCursor(Type aType,
            indexedDB::BackgroundCursorChild* aBackgroundActor,
            const Key& aKey);

  ~IDBCursor();

#ifdef ENABLE_INTL_API
  // Checks if this is a locale aware cursor (ie. the index's sortKey is unset)
  bool
  IsLocaleAware() const;
#endif

  void
  DropJSObjects();

  bool
  IsSourceDeleted() const;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_idbcursor_h__
