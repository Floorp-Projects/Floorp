/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbcursor_h__
#define mozilla_dom_indexeddb_idbcursor_h__

#include "IndexedDatabase.h"
#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

class OwningIDBObjectStoreOrIDBIndex;

namespace indexedDB {

class BackgroundCursorChild;
class IDBIndex;
class IDBObjectStore;
class IDBRequest;
class IDBTransaction;

class IDBCursor MOZ_FINAL
  : public nsISupports
  , public nsWrapperCache
{
public:
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

  nsRefPtr<IDBObjectStore> mSourceObjectStore;
  nsRefPtr<IDBIndex> mSourceIndex;
  nsRefPtr<IDBTransaction> mTransaction;

  BackgroundCursorChild* mBackgroundActor;

  JS::Heap<JSObject*> mScriptOwner;

  // These are cycle-collected!
  JS::Heap<JS::Value> mCachedKey;
  JS::Heap<JS::Value> mCachedPrimaryKey;
  JS::Heap<JS::Value> mCachedValue;

  Key mKey;
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
  Create(IDBObjectStore* aObjectStore,
         BackgroundCursorChild* aBackgroundActor,
         Direction aDirection,
         const Key& aKey,
         StructuredCloneReadInfo&& aCloneInfo);

  static already_AddRefed<IDBCursor>
  Create(IDBObjectStore* aObjectStore,
         BackgroundCursorChild* aBackgroundActor,
         Direction aDirection,
         const Key& aKey);

  static already_AddRefed<IDBCursor>
  Create(IDBIndex* aIndex,
         BackgroundCursorChild* aBackgroundActor,
         Direction aDirection,
         const Key& aKey,
         const Key& aPrimaryKey,
         StructuredCloneReadInfo&& aCloneInfo);

  static already_AddRefed<IDBCursor>
  Create(IDBIndex* aIndex,
         BackgroundCursorChild* aBackgroundActor,
         Direction aDirection,
         const Key& aKey,
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

  nsPIDOMWindow*
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
  Reset(Key&& aKey, Key&& aPrimaryKey, StructuredCloneReadInfo&& aValue);

  void
  Reset(Key&& aKey, Key&& aPrimaryKey);

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
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

private:
  IDBCursor(Type aType,
            IDBObjectStore* aSourceObjectStore,
            IDBIndex* aSourceIndex,
            IDBTransaction* aTransaction,
            BackgroundCursorChild* aBackgroundActor,
            Direction aDirection,
            const Key& aKey);

  ~IDBCursor();

  void
  DropJSObjects();
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbcursor_h__
