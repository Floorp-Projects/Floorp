/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbindex_h__
#define mozilla_dom_indexeddb_idbindex_h__

#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"
#include "nsWrapperCache.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

template <typename> class Sequence;

namespace indexedDB {

class IDBObjectStore;
class IDBRequest;
class IndexMetadata;
class Key;
class KeyPath;

class IDBIndex final
  : public nsISupports
  , public nsWrapperCache
{
  nsRefPtr<IDBObjectStore> mObjectStore;

  JS::Heap<JS::Value> mCachedKeyPath;

  // This normally points to the IndexMetadata owned by the parent IDBDatabase
  // object. However, if this index is part of a versionchange transaction and
  // it gets deleted then the metadata is copied into mDeletedMetadata and
  // mMetadata is set to point at mDeletedMetadata.
  const IndexMetadata* mMetadata;
  nsAutoPtr<IndexMetadata> mDeletedMetadata;

  const int64_t mId;
  bool mRooted;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBIndex)

  static already_AddRefed<IDBIndex>
  Create(IDBObjectStore* aObjectStore, const IndexMetadata& aMetadata);

  int64_t
  Id() const
  {
    AssertIsOnOwningThread();

    return mId;
  }

  const nsString&
  Name() const;

  bool
  Unique() const;

  bool
  MultiEntry() const;

  bool
  LocaleAware() const;

  const KeyPath&
  GetKeyPath() const;

  void
  GetLocale(nsString& aLocale) const;

  const nsCString&
  Locale() const;

  bool
  IsAutoLocale() const;

  IDBObjectStore*
  ObjectStore() const
  {
    AssertIsOnOwningThread();
    return mObjectStore;
  }

  nsPIDOMWindow*
  GetParentObject() const;

  void
  GetName(nsString& aName) const
  {
    aName = Name();
  }

  void
  GetKeyPath(JSContext* aCx,
             JS::MutableHandle<JS::Value> aResult,
             ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  OpenCursor(JSContext* aCx,
             JS::Handle<JS::Value> aRange,
             IDBCursorDirection aDirection,
             ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return OpenCursorInternal(/* aKeysOnly */ false, aCx, aRange, aDirection,
                              aRv);
  }

  already_AddRefed<IDBRequest>
  OpenKeyCursor(JSContext* aCx,
                JS::Handle<JS::Value> aRange,
                IDBCursorDirection aDirection,
                ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return OpenCursorInternal(/* aKeysOnly */ true, aCx, aRange, aDirection,
                              aRv);
  }

  already_AddRefed<IDBRequest>
  Get(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return GetInternal(/* aKeyOnly */ false, aCx, aKey, aRv);
  }

  already_AddRefed<IDBRequest>
  GetKey(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return GetInternal(/* aKeyOnly */ true, aCx, aKey, aRv);
  }

  already_AddRefed<IDBRequest>
  Count(JSContext* aCx, JS::Handle<JS::Value> aKey,
         ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAll(JSContext* aCx, JS::Handle<JS::Value> aKey,
         const Optional<uint32_t>& aLimit, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return GetAllInternal(/* aKeysOnly */ false, aCx, aKey, aLimit, aRv);
  }

  already_AddRefed<IDBRequest>
  GetAllKeys(JSContext* aCx, JS::Handle<JS::Value> aKey,
             const Optional<uint32_t>& aLimit, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return GetAllInternal(/* aKeysOnly */ true, aCx, aKey, aLimit, aRv);
  }

  void
  RefreshMetadata(bool aMayDelete);

  void
  NoteDeletion();

  bool
  IsDeleted() const
  {
    AssertIsOnOwningThread();

    return !!mDeletedMetadata;
  }

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  IDBIndex(IDBObjectStore* aObjectStore, const IndexMetadata* aMetadata);

  ~IDBIndex();

  already_AddRefed<IDBRequest>
  GetInternal(bool aKeyOnly,
              JSContext* aCx,
              JS::Handle<JS::Value> aKey,
              ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAllInternal(bool aKeysOnly,
                 JSContext* aCx,
                 JS::Handle<JS::Value> aKey,
                 const Optional<uint32_t>& aLimit,
                 ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  OpenCursorInternal(bool aKeysOnly,
                     JSContext* aCx,
                     JS::Handle<JS::Value> aRange,
                     IDBCursorDirection aDirection,
                     ErrorResult& aRv);
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbindex_h__
