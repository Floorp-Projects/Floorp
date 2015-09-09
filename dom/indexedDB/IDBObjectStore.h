/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbobjectstore_h__
#define mozilla_dom_indexeddb_idbobjectstore_h__

#include "js/RootingAPI.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IDBIndexBinding.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

struct JSClass;
class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

class DOMStringList;
template <typename> class Sequence;

namespace indexedDB {

class IDBCursor;
class IDBRequest;
class IDBTransaction;
class IndexUpdateInfo;
class Key;
class KeyPath;
class ObjectStoreSpec;
struct StructuredCloneReadInfo;

class IDBObjectStore final
  : public nsISupports
  , public nsWrapperCache
{
  // For AddOrPut() and DeleteInternal().
  friend class IDBCursor; 

  static const JSClass sDummyPropJSClass;

  nsRefPtr<IDBTransaction> mTransaction;
  JS::Heap<JS::Value> mCachedKeyPath;

  // This normally points to the ObjectStoreSpec owned by the parent IDBDatabase
  // object. However, if this objectStore is part of a versionchange transaction
  // and it gets deleted then the spec is copied into mDeletedSpec and mSpec is
  // set to point at mDeletedSpec.
  const ObjectStoreSpec* mSpec;
  nsAutoPtr<ObjectStoreSpec> mDeletedSpec;

  nsTArray<nsRefPtr<IDBIndex>> mIndexes;

  const int64_t mId;
  bool mRooted;

public:
  struct StructuredCloneWriteInfo;

  static already_AddRefed<IDBObjectStore>
  Create(IDBTransaction* aTransaction, const ObjectStoreSpec& aSpec);

  static nsresult
  AppendIndexUpdateInfo(int64_t aIndexID,
                        const KeyPath& aKeyPath,
                        bool aUnique,
                        bool aMultiEntry,
                        const nsCString& aLocale,
                        JSContext* aCx,
                        JS::Handle<JS::Value> aObject,
                        nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  static void
  ClearCloneReadInfo(StructuredCloneReadInfo& aReadInfo);

  static bool
  DeserializeValue(JSContext* aCx,
                   StructuredCloneReadInfo& aCloneReadInfo,
                   JS::MutableHandle<JS::Value> aValue);

  static bool
  DeserializeIndexValue(JSContext* aCx,
                        StructuredCloneReadInfo& aCloneReadInfo,
                        JS::MutableHandle<JS::Value> aValue);

#if !defined(MOZ_B2G)
  static bool
  DeserializeUpgradeValue(JSContext* aCx,
                          StructuredCloneReadInfo& aCloneReadInfo,
                          JS::MutableHandle<JS::Value> aValue);
#endif

  static const JSClass*
  DummyPropClass()
  {
    return &sDummyPropJSClass;
  }

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  int64_t
  Id() const
  {
    AssertIsOnOwningThread();

    return mId;
  }

  const nsString&
  Name() const;

  bool
  AutoIncrement() const;

  const KeyPath&
  GetKeyPath() const;

  bool
  HasValidKeyPath() const;

  nsPIDOMWindow*
  GetParentObject() const;

  void
  GetName(nsString& aName) const
  {
    AssertIsOnOwningThread();

    aName = Name();
  }

  void
  GetKeyPath(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
             ErrorResult& aRv);

  already_AddRefed<DOMStringList>
  IndexNames();

  IDBTransaction*
  Transaction() const
  {
    AssertIsOnOwningThread();

    return mTransaction;
  }

  already_AddRefed<IDBRequest>
  Add(JSContext* aCx,
      JS::Handle<JS::Value> aValue,
      JS::Handle<JS::Value> aKey,
      ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return AddOrPut(aCx, aValue, aKey, false, /* aFromCursor */ false, aRv);
  }

  already_AddRefed<IDBRequest>
  Put(JSContext* aCx,
      JS::Handle<JS::Value> aValue,
      JS::Handle<JS::Value> aKey,
      ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return AddOrPut(aCx, aValue, aKey, true, /* aFromCursor */ false, aRv);
  }

  already_AddRefed<IDBRequest>
  Delete(JSContext* aCx,
         JS::Handle<JS::Value> aKey,
         ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return DeleteInternal(aCx, aKey, /* aFromCursor */ false, aRv);
  }

  already_AddRefed<IDBRequest>
  Get(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Clear(ErrorResult& aRv);

  already_AddRefed<IDBIndex>
  CreateIndex(const nsAString& aName,
              const nsAString& aKeyPath,
              const IDBIndexParameters& aOptionalParameters,
              ErrorResult& aRv);

  already_AddRefed<IDBIndex>
  CreateIndex(const nsAString& aName,
              const Sequence<nsString>& aKeyPath,
              const IDBIndexParameters& aOptionalParameters,
              ErrorResult& aRv);

  already_AddRefed<IDBIndex>
  Index(const nsAString& aName, ErrorResult &aRv);

  void
  DeleteIndex(const nsAString& aIndexName, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Count(JSContext* aCx,
        JS::Handle<JS::Value> aKey,
        ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAll(JSContext* aCx,
         JS::Handle<JS::Value> aKey,
         const Optional<uint32_t>& aLimit,
         ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return GetAllInternal(/* aKeysOnly */ false, aCx, aKey, aLimit, aRv);
  }

  already_AddRefed<IDBRequest>
  GetAllKeys(JSContext* aCx,
             JS::Handle<JS::Value> aKey,
             const Optional<uint32_t>& aLimit,
             ErrorResult& aRv)
  {
    AssertIsOnOwningThread();

    return GetAllInternal(/* aKeysOnly */ true, aCx, aKey, aLimit, aRv);
  }

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

  void
  RefreshSpec(bool aMayDelete);

  const ObjectStoreSpec&
  Spec() const;

  void
  NoteDeletion();

  bool
  IsDeleted() const
  {
    AssertIsOnOwningThread();

    return !!mDeletedSpec;
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBObjectStore)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  IDBObjectStore(IDBTransaction* aTransaction, const ObjectStoreSpec* aSpec);

  ~IDBObjectStore();

  nsresult
  GetAddInfo(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aKeyVal,
             StructuredCloneWriteInfo& aCloneWriteInfo,
             Key& aKey,
             nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  already_AddRefed<IDBRequest>
  AddOrPut(JSContext* aCx,
           JS::Handle<JS::Value> aValue,
           JS::Handle<JS::Value> aKey,
           bool aOverwrite,
           bool aFromCursor,
           ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  DeleteInternal(JSContext* aCx,
                 JS::Handle<JS::Value> aKey,
                 bool aFromCursor,
                 ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAllInternal(bool aKeysOnly,
                 JSContext* aCx,
                 JS::Handle<JS::Value> aKey,
                 const Optional<uint32_t>& aLimit,
                 ErrorResult& aRv);

  already_AddRefed<IDBIndex>
  CreateIndexInternal(const nsAString& aName,
                      const KeyPath& aKeyPath,
                      const IDBIndexParameters& aOptionalParameters,
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

#endif // mozilla_dom_indexeddb_idbobjectstore_h__
