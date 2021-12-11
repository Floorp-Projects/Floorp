/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbindex_h__
#define mozilla_dom_idbindex_h__

#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/UniquePtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class IDBObjectStore;
class IDBRequest;
template <typename>
class Sequence;

namespace indexedDB {
class IndexMetadata;
class KeyPath;
}  // namespace indexedDB

class IDBIndex final : public nsISupports, public nsWrapperCache {
  // TODO: This could be made const if Bug 1575173 is resolved. It is
  // initialized in the constructor and never modified/cleared.
  RefPtr<IDBObjectStore> mObjectStore;

  JS::Heap<JS::Value> mCachedKeyPath;

  // This normally points to the IndexMetadata owned by the parent IDBDatabase
  // object. However, if this index is part of a versionchange transaction and
  // it gets deleted then the metadata is copied into mDeletedMetadata and
  // mMetadata is set to point at mDeletedMetadata.
  const indexedDB::IndexMetadata* mMetadata;
  UniquePtr<indexedDB::IndexMetadata> mDeletedMetadata;

  const int64_t mId;
  bool mRooted;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBIndex)

  [[nodiscard]] static RefPtr<IDBIndex> Create(
      IDBObjectStore* aObjectStore, const indexedDB::IndexMetadata& aMetadata);

  int64_t Id() const {
    AssertIsOnOwningThread();

    return mId;
  }

  const nsString& Name() const;

  bool Unique() const;

  bool MultiEntry() const;

  bool LocaleAware() const;

  const indexedDB::KeyPath& GetKeyPath() const;

  void GetLocale(nsString& aLocale) const;

  const nsCString& Locale() const;

  bool IsAutoLocale() const;

  IDBObjectStore* ObjectStore() const {
    AssertIsOnOwningThread();
    return mObjectStore;
  }

  nsIGlobalObject* GetParentObject() const;

  void GetName(nsString& aName) const { aName = Name(); }

  void SetName(const nsAString& aName, ErrorResult& aRv);

  void GetKeyPath(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                  ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> OpenCursor(JSContext* aCx,
                                              JS::Handle<JS::Value> aRange,
                                              IDBCursorDirection aDirection,
                                              ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> OpenKeyCursor(JSContext* aCx,
                                                 JS::Handle<JS::Value> aRange,
                                                 IDBCursorDirection aDirection,
                                                 ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> Get(JSContext* aCx,
                                       JS::Handle<JS::Value> aKey,
                                       ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> GetKey(JSContext* aCx,
                                          JS::Handle<JS::Value> aKey,
                                          ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> Count(JSContext* aCx,
                                         JS::Handle<JS::Value> aKey,
                                         ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> GetAll(JSContext* aCx,
                                          JS::Handle<JS::Value> aKey,
                                          const Optional<uint32_t>& aLimit,
                                          ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> GetAllKeys(JSContext* aCx,
                                              JS::Handle<JS::Value> aKey,
                                              const Optional<uint32_t>& aLimit,
                                              ErrorResult& aRv);

  void RefreshMetadata(bool aMayDelete);

  void NoteDeletion();

  bool IsDeleted() const {
    AssertIsOnOwningThread();

    return !!mDeletedMetadata;
  }

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  IDBIndex(IDBObjectStore* aObjectStore,
           const indexedDB::IndexMetadata* aMetadata);

  ~IDBIndex();

  [[nodiscard]] RefPtr<IDBRequest> GetInternal(bool aKeyOnly, JSContext* aCx,
                                               JS::Handle<JS::Value> aKey,
                                               ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> GetAllInternal(
      bool aKeysOnly, JSContext* aCx, JS::Handle<JS::Value> aKey,
      const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBRequest> OpenCursorInternal(
      bool aKeysOnly, JSContext* aCx, JS::Handle<JS::Value> aRange,
      IDBCursorDirection aDirection, ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbindex_h__
