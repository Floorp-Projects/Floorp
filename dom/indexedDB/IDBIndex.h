/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbindex_h__
#define mozilla_dom_indexeddb_idbindex_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/indexedDB/IDBObjectStore.h"
#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/KeyPath.h"

class nsIScriptContext;
class nsPIDOMWindow;

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class IDBCursor;
class IDBKeyRange;
class IDBObjectStore;
class IDBRequest;
class IndexedDBIndexChild;
class IndexedDBIndexParent;
class Key;

struct IndexInfo;

class IDBIndex MOZ_FINAL : public nsISupports,
                           public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBIndex)

  static already_AddRefed<IDBIndex>
  Create(IDBObjectStore* aObjectStore,
         const IndexInfo* aIndexInfo,
         bool aCreating);

  IDBObjectStore* ObjectStore()
  {
    return mObjectStore;
  }

  const int64_t Id() const
  {
    return mId;
  }

  const nsString& Name() const
  {
    return mName;
  }

  bool IsUnique() const
  {
    return mUnique;
  }

  bool IsMultiEntry() const
  {
    return mMultiEntry;
  }

  const KeyPath& GetKeyPath() const
  {
    return mKeyPath;
  }

  void
  SetActor(IndexedDBIndexChild* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  SetActor(IndexedDBIndexParent* aActorParent)
  {
    NS_ASSERTION(!aActorParent || !mActorParent,
                 "Shouldn't have more than one!");
    mActorParent = aActorParent;
  }

  IndexedDBIndexChild*
  GetActorChild() const
  {
    return mActorChild;
  }

  IndexedDBIndexParent*
  GetActorParent() const
  {
    return mActorParent;
  }

  already_AddRefed<IDBRequest>
  GetInternal(IDBKeyRange* aKeyRange,
              ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetKeyInternal(IDBKeyRange* aKeyRange,
                 ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAllInternal(IDBKeyRange* aKeyRange,
                 uint32_t aLimit,
                 ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAllKeysInternal(IDBKeyRange* aKeyRange,
                     uint32_t aLimit,
                     ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  CountInternal(IDBKeyRange* aKeyRange,
                ErrorResult& aRv);

  nsresult OpenCursorFromChildProcess(
                            IDBRequest* aRequest,
                            size_t aDirection,
                            const Key& aKey,
                            const Key& aObjectKey,
                            IDBCursor** _retval);

  already_AddRefed<IDBRequest>
  OpenKeyCursorInternal(IDBKeyRange* aKeyRange,
                        size_t aDirection,
                        ErrorResult& aRv);

  nsresult OpenCursorInternal(IDBKeyRange* aKeyRange,
                              size_t aDirection,
                              IDBRequest** _retval);

  nsresult OpenCursorFromChildProcess(
                            IDBRequest* aRequest,
                            size_t aDirection,
                            const Key& aKey,
                            const Key& aObjectKey,
                            const SerializedStructuredCloneReadInfo& aCloneInfo,
                            nsTArray<StructuredCloneFile>& aBlobs,
                            IDBCursor** _retval);

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  IDBObjectStore*
  GetParentObject() const
  {
    return mObjectStore;
  }

  void
  GetName(nsString& aName) const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    aName.Assign(mName);
  }

  IDBObjectStore*
  ObjectStore() const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    return mObjectStore;
  }

  JS::Value
  GetKeyPath(JSContext* aCx, ErrorResult& aRv);

  bool
  MultiEntry() const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    return mMultiEntry;
  }

  bool
  Unique() const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    return mUnique;
  }

  already_AddRefed<IDBRequest>
  OpenCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
             IDBCursorDirection aDirection, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  OpenKeyCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
                IDBCursorDirection aDirection, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Get(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetKey(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Count(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
         ErrorResult& aRv);

  void
  GetStoreName(nsString& aStoreName) const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    mObjectStore->GetName(aStoreName);
  }

  already_AddRefed<IDBRequest>
  GetAll(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
         const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAllKeys(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
             const Optional<uint32_t>& aLimit, ErrorResult& aRv);

private:
  IDBIndex();
  ~IDBIndex();

  nsRefPtr<IDBObjectStore> mObjectStore;

  int64_t mId;
  nsString mName;
  KeyPath mKeyPath;
  JS::Heap<JS::Value> mCachedKeyPath;

  IndexedDBIndexChild* mActorChild;
  IndexedDBIndexParent* mActorParent;

  bool mUnique;
  bool mMultiEntry;
  bool mRooted;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbindex_h__
