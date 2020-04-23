/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbcursor_h__
#define mozilla_dom_idbcursor_h__

#include "IDBCursorType.h"
#include "IndexedDatabase.h"
#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IDBTransaction.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "mozilla/InitializedOnce.h"
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

class IDBObjectStoreCursor;
class IDBObjectStoreKeyCursor;
class IDBIndexCursor;
class IDBIndexKeyCursor;

namespace indexedDB {
class BackgroundCursorChildBase;
template <IDBCursorType CursorType>
class BackgroundCursorChild;
}  // namespace indexedDB

class IDBCursor : public nsISupports, public nsWrapperCache {
 public:
  using Key = indexedDB::Key;
  using StructuredCloneReadInfoChild = indexedDB::StructuredCloneReadInfoChild;

  using Direction = IDBCursorDirection;
  using Type = IDBCursorType;

 protected:
  InitializedOnceNotNull<indexedDB::BackgroundCursorChildBase* const>
      mBackgroundActor;

  // TODO: mRequest could be made const if Bug 1575173 is resolved. It is
  // initialized in the constructor and never modified/cleared.
  RefPtr<IDBRequest> mRequest;

  // Sub-classes' mSource will hold this alive.
  const CheckedUnsafePtr<IDBTransaction> mTransaction;

 protected:
  // These are cycle-collected!
  JS::Heap<JS::Value> mCachedKey;
  JS::Heap<JS::Value> mCachedPrimaryKey;
  JS::Heap<JS::Value> mCachedValue;

  const Direction mDirection;

  bool mHaveCachedKey : 1;
  bool mHaveCachedPrimaryKey : 1;
  bool mHaveCachedValue : 1;
  bool mRooted : 1;
  bool mContinueCalled : 1;
  bool mHaveValue : 1;

 public:
  [[nodiscard]] static RefPtr<IDBObjectStoreCursor> Create(
      indexedDB::BackgroundCursorChild<Type::ObjectStore>* aBackgroundActor,
      Key aKey, StructuredCloneReadInfoChild&& aCloneInfo);

  [[nodiscard]] static RefPtr<IDBObjectStoreKeyCursor> Create(
      indexedDB::BackgroundCursorChild<Type::ObjectStoreKey>* aBackgroundActor,
      Key aKey);

  [[nodiscard]] static RefPtr<IDBIndexCursor> Create(
      indexedDB::BackgroundCursorChild<Type::Index>* aBackgroundActor, Key aKey,
      Key aSortKey, Key aPrimaryKey, StructuredCloneReadInfoChild&& aCloneInfo);

  [[nodiscard]] static RefPtr<IDBIndexKeyCursor> Create(
      indexedDB::BackgroundCursorChild<Type::IndexKey>* aBackgroundActor,
      Key aKey, Key aSortKey, Key aPrimaryKey);

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  nsIGlobalObject* GetParentObject() const;

  // XXX: The virtual methods that are used by the DOM binding could be removed
  // on the base class, if we provided a non-polymorphic wrapper instead, which
  // uses a custom dispatch to the actual implementation type. Don't know if
  // this is worth it.

  virtual void GetSource(OwningIDBObjectStoreOrIDBIndex& aSource) const = 0;

  IDBCursorDirection GetDirection() const;

  virtual void GetKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                      ErrorResult& aRv) = 0;

  virtual void GetPrimaryKey(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aResult,
                             ErrorResult& aRv) = 0;

  // XXX: We could move this to a sub-class, since this is only present on
  // IDBCursorWithValue.
  virtual void GetValue(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                        ErrorResult& aRv) = 0;

  virtual void Continue(JSContext* aCx, JS::Handle<JS::Value> aKey,
                        ErrorResult& aRv) = 0;

  virtual void ContinuePrimaryKey(JSContext* aCx, JS::Handle<JS::Value> aKey,
                                  JS::Handle<JS::Value> aPrimaryKey,
                                  ErrorResult& aRv) = 0;

  virtual void Advance(uint32_t aCount, ErrorResult& aRv) = 0;

  [[nodiscard]] virtual RefPtr<IDBRequest> Update(JSContext* aCx,
                                                  JS::Handle<JS::Value> aValue,
                                                  ErrorResult& aRv) = 0;

  [[nodiscard]] virtual RefPtr<IDBRequest> Delete(JSContext* aCx,
                                                  ErrorResult& aRv) = 0;

  void ClearBackgroundActor() {
    AssertIsOnOwningThread();

    mBackgroundActor.destroy();
  }

  virtual void InvalidateCachedResponses() = 0;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBCursor)

 protected:
  IDBCursor(indexedDB::BackgroundCursorChildBase* aBackgroundActor);

  // TODO: Check if we can remove virtual by changing cycle collection.
  virtual ~IDBCursor() = default;

  void ResetBase();
};

template <IDBCursor::Type CursorType>
class IDBTypedCursor : public IDBCursor {
 public:
  template <typename... DataArgs>
  explicit IDBTypedCursor(
      indexedDB::BackgroundCursorChild<CursorType>* aBackgroundActor,
      DataArgs&&... aDataArgs);

  static constexpr Type GetType() { return CursorType; }

  // Checks if this is a locale aware cursor (ie. the index's sortKey is unset)
  bool IsLocaleAware() const;

  void GetSource(OwningIDBObjectStoreOrIDBIndex& aSource) const final;

  void GetKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
              ErrorResult& aRv) final;

  void GetPrimaryKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                     ErrorResult& aRv) final;

  void GetValue(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                ErrorResult& aRv) final;

  void Continue(JSContext* aCx, JS::Handle<JS::Value> aKey,
                ErrorResult& aRv) final;

  void ContinuePrimaryKey(JSContext* aCx, JS::Handle<JS::Value> aKey,
                          JS::Handle<JS::Value> aPrimaryKey,
                          ErrorResult& aRv) final;

  void Advance(uint32_t aCount, ErrorResult& aRv) final;

  [[nodiscard]] RefPtr<IDBRequest> Update(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv) final;

  [[nodiscard]] RefPtr<IDBRequest> Delete(JSContext* aCx,
                                          ErrorResult& aRv) final;

  // nsWrapperCache
  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  void InvalidateCachedResponses() final;

  void Reset();

  void Reset(CursorData<CursorType>&& aCursorData);

 private:
  static constexpr bool IsObjectStoreCursor =
      CursorTypeTraits<CursorType>::IsObjectStoreCursor;
  static constexpr bool IsKeyOnlyCursor =
      CursorTypeTraits<CursorType>::IsKeyOnlyCursor;

  CursorSourceType<CursorType>& GetSourceRef() const {
    MOZ_ASSERT(mSource);
    return *mSource;
  }

  IDBObjectStore& GetSourceObjectStoreRef() const {
    if constexpr (IsObjectStoreCursor) {
      return GetSourceRef();
    } else {
      MOZ_ASSERT(!GetSourceRef().IsDeleted());

      auto res = GetSourceRef().ObjectStore();
      MOZ_ASSERT(res);
      return *res;
    }
  }

  indexedDB::BackgroundCursorChild<CursorType>& GetTypedBackgroundActorRef()
      const {
    // We can safely downcast to BackgroundCursorChild<CursorType>*, since we
    // initialized that in the constructor from that type. We just want to avoid
    // having a second typed field.
    return *static_cast<indexedDB::BackgroundCursorChild<CursorType>*>(
        *mBackgroundActor);
  }

  bool IsSourceDeleted() const;

 protected:
  virtual ~IDBTypedCursor() override;

  void DropJSObjects();

  CursorData<CursorType> mData;

  // TODO: mSource could be made const if Bug 1575173 is resolved. It is
  // initialized in the constructor and never modified/cleared.
  RefPtr<CursorSourceType<CursorType>> mSource;
};

// The subclasses defined by this macro are only needed to be able to use the
// cycle collector macros, which do not support templates. If spelled out, the
// cycle collection could be implemented directly on IDBTypedCursor, and these
// classes were not needed.
#define CONCRETE_IDBCURSOR_SUBCLASS(_subclassName, _cursorType)        \
  class _subclassName final : public IDBTypedCursor<_cursorType> {     \
   public:                                                             \
    NS_DECL_ISUPPORTS_INHERITED                                        \
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(_subclassName, IDBCursor) \
                                                                       \
    using IDBTypedCursor<_cursorType>::IDBTypedCursor;                 \
                                                                       \
   private:                                                            \
    ~_subclassName() final = default;                                  \
  };

CONCRETE_IDBCURSOR_SUBCLASS(IDBObjectStoreCursor, IDBCursor::Type::ObjectStore)
CONCRETE_IDBCURSOR_SUBCLASS(IDBObjectStoreKeyCursor,
                            IDBCursor::Type::ObjectStoreKey)
CONCRETE_IDBCURSOR_SUBCLASS(IDBIndexCursor, IDBCursor::Type::Index)
CONCRETE_IDBCURSOR_SUBCLASS(IDBIndexKeyCursor, IDBCursor::Type::IndexKey)

template <IDBCursor::Type CursorType>
using IDBCursorImpl = typename CursorTypeTraits<CursorType>::Type;

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbcursor_h__
