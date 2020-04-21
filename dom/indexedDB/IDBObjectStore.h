/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbobjectstore_h__
#define mozilla_dom_idbobjectstore_h__

#include "IDBCursor.h"
#include "js/RootingAPI.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IDBIndexBinding.h"
#include "mozilla/UniquePtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

struct JSClass;
class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class DOMStringList;
class IDBRequest;
class IDBTransaction;
class StringOrStringSequence;
template <typename>
class Sequence;

namespace indexedDB {
class Key;
class KeyPath;
class IndexUpdateInfo;
class ObjectStoreSpec;
struct StructuredCloneReadInfoChild;
}  // namespace indexedDB

class IDBObjectStore final : public nsISupports, public nsWrapperCache {
  typedef indexedDB::IndexUpdateInfo IndexUpdateInfo;
  typedef indexedDB::Key Key;
  typedef indexedDB::KeyPath KeyPath;
  typedef indexedDB::ObjectStoreSpec ObjectStoreSpec;
  typedef indexedDB::StructuredCloneReadInfoChild StructuredCloneReadInfoChild;

  // For AddOrPut() and DeleteInternal().
  // TODO Consider removing this, and making the functions public?
  template <IDBCursor::Type>
  friend class IDBTypedCursor;

  static const JSClass sDummyPropJSClass;

  // TODO: This could be made const if Bug 1575173 is resolved. It is
  // initialized in the constructor and never modified/cleared.
  SafeRefPtr<IDBTransaction> mTransaction;
  JS::Heap<JS::Value> mCachedKeyPath;

  // This normally points to the ObjectStoreSpec owned by the parent IDBDatabase
  // object. However, if this objectStore is part of a versionchange transaction
  // and it gets deleted then the spec is copied into mDeletedSpec and mSpec is
  // set to point at mDeletedSpec.
  ObjectStoreSpec* mSpec;
  UniquePtr<ObjectStoreSpec> mDeletedSpec;

  nsTArray<RefPtr<IDBIndex>> mIndexes;
  nsTArray<RefPtr<IDBIndex>> mDeletedIndexes;

  const int64_t mId;
  bool mRooted;

 public:
  struct StructuredCloneWriteInfo;
  struct StructuredCloneInfo;

  class MOZ_STACK_CLASS ValueWrapper final {
    JS::Rooted<JS::Value> mValue;
    bool mCloned;

   public:
    ValueWrapper(JSContext* aCx, JS::Handle<JS::Value> aValue)
        : mValue(aCx, aValue), mCloned(false) {
      MOZ_COUNT_CTOR(IDBObjectStore::ValueWrapper);
    }

    MOZ_COUNTED_DTOR_NESTED(ValueWrapper, IDBObjectStore::ValueWrapper)

    const JS::Rooted<JS::Value>& Value() const { return mValue; }

    bool Clone(JSContext* aCx);
  };

  static MOZ_MUST_USE RefPtr<IDBObjectStore> Create(
      SafeRefPtr<IDBTransaction> aTransaction, ObjectStoreSpec& aSpec);

  static void AppendIndexUpdateInfo(int64_t aIndexID, const KeyPath& aKeyPath,
                                    bool aMultiEntry, const nsCString& aLocale,
                                    JSContext* aCx, JS::Handle<JS::Value> aVal,
                                    nsTArray<IndexUpdateInfo>* aUpdateInfoArray,
                                    ErrorResult* aRv);

  static void ClearCloneReadInfo(
      indexedDB::StructuredCloneReadInfoChild& aReadInfo);

  static bool DeserializeValue(JSContext* aCx,
                               StructuredCloneReadInfoChild&& aCloneReadInfo,
                               JS::MutableHandle<JS::Value> aValue);

  static const JSClass* DummyPropClass() { return &sDummyPropJSClass; }

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  int64_t Id() const {
    AssertIsOnOwningThread();

    return mId;
  }

  const nsString& Name() const;

  bool AutoIncrement() const;

  const KeyPath& GetKeyPath() const;

  bool HasValidKeyPath() const;

  nsIGlobalObject* GetParentObject() const;

  void GetName(nsString& aName) const {
    AssertIsOnOwningThread();

    aName = Name();
  }

  void SetName(const nsAString& aName, ErrorResult& aRv);

  void GetKeyPath(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                  ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<DOMStringList> IndexNames();

  const IDBTransaction& TransactionRef() const {
    AssertIsOnOwningThread();

    return *mTransaction;
  }

  IDBTransaction& MutableTransactionRef() {
    AssertIsOnOwningThread();

    return *mTransaction;
  }

  SafeRefPtr<IDBTransaction> AcquireTransaction() const {
    AssertIsOnOwningThread();

    return mTransaction.clonePtr();
  }

  RefPtr<IDBTransaction> Transaction() const {
    AssertIsOnOwningThread();

    return AsRefPtr(mTransaction.clonePtr());
  }

  MOZ_MUST_USE RefPtr<IDBRequest> Add(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue,
                                      JS::Handle<JS::Value> aKey,
                                      ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> Put(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue,
                                      JS::Handle<JS::Value> aKey,
                                      ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> Delete(JSContext* aCx,
                                         JS::Handle<JS::Value> aKey,
                                         ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> Get(JSContext* aCx,
                                      JS::Handle<JS::Value> aKey,
                                      ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> GetKey(JSContext* aCx,
                                         JS::Handle<JS::Value> aKey,
                                         ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> Clear(JSContext* aCx, ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBIndex> CreateIndex(
      const nsAString& aName, const StringOrStringSequence& aKeyPath,
      const IDBIndexParameters& aOptionalParameters, ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBIndex> Index(const nsAString& aName, ErrorResult& aRv);

  void DeleteIndex(const nsAString& aName, ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> Count(JSContext* aCx,
                                        JS::Handle<JS::Value> aKey,
                                        ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> GetAll(JSContext* aCx,
                                         JS::Handle<JS::Value> aKey,
                                         const Optional<uint32_t>& aLimit,
                                         ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> GetAllKeys(JSContext* aCx,
                                             JS::Handle<JS::Value> aKey,
                                             const Optional<uint32_t>& aLimit,
                                             ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> OpenCursor(JSContext* aCx,
                                             JS::Handle<JS::Value> aRange,
                                             IDBCursorDirection aDirection,
                                             ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> OpenCursor(JSContext* aCx,
                                             IDBCursorDirection aDirection,
                                             ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> OpenKeyCursor(JSContext* aCx,
                                                JS::Handle<JS::Value> aRange,
                                                IDBCursorDirection aDirection,
                                                ErrorResult& aRv);

  void RefreshSpec(bool aMayDelete);

  const ObjectStoreSpec& Spec() const;

  void NoteDeletion();

  bool IsDeleted() const {
    AssertIsOnOwningThread();

    return !!mDeletedSpec;
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBObjectStore)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  IDBObjectStore(SafeRefPtr<IDBTransaction> aTransaction,
                 ObjectStoreSpec* aSpec);

  ~IDBObjectStore();

  void GetAddInfo(JSContext* aCx, ValueWrapper& aValueWrapper,
                  JS::Handle<JS::Value> aKeyVal,
                  StructuredCloneWriteInfo& aCloneWriteInfo, Key& aKey,
                  nsTArray<IndexUpdateInfo>& aUpdateInfoArray,
                  ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> AddOrPut(JSContext* aCx,
                                           ValueWrapper& aValueWrapper,
                                           JS::Handle<JS::Value> aKey,
                                           bool aOverwrite, bool aFromCursor,
                                           ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> DeleteInternal(JSContext* aCx,
                                                 JS::Handle<JS::Value> aKey,
                                                 bool aFromCursor,
                                                 ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> GetInternal(bool aKeyOnly, JSContext* aCx,
                                              JS::Handle<JS::Value> aKey,
                                              ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> GetAllInternal(
      bool aKeysOnly, JSContext* aCx, JS::Handle<JS::Value> aKey,
      const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBRequest> OpenCursorInternal(
      bool aKeysOnly, JSContext* aCx, JS::Handle<JS::Value> aRange,
      IDBCursorDirection aDirection, ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbobjectstore_h__
