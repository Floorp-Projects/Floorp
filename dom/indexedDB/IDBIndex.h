/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbindex_h__
#define mozilla_dom_indexeddb_idbindex_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/KeyPath.h"

#include "nsIIDBIndex.h"

#include "nsCycleCollectionParticipant.h"

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

class IDBIndex MOZ_FINAL : public nsIIDBIndex
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIIDBINDEX

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBIndex)

  static already_AddRefed<IDBIndex>
  Create(IDBObjectStore* aObjectStore,
         const IndexInfo* aIndexInfo,
         bool aCreating);

  IDBObjectStore* ObjectStore()
  {
    return mObjectStore;
  }

  const PRInt64 Id() const
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

  nsresult GetInternal(IDBKeyRange* aKeyRange,
                       JSContext* aCx,
                       IDBRequest** _retval);

  nsresult GetKeyInternal(IDBKeyRange* aKeyRange,
                          JSContext* aCx,
                          IDBRequest** _retval);

  nsresult GetAllInternal(IDBKeyRange* aKeyRange,
                          PRUint32 aLimit,
                          JSContext* aCx,
                          IDBRequest** _retval);

  nsresult GetAllKeysInternal(IDBKeyRange* aKeyRange,
                              PRUint32 aLimit,
                              JSContext* aCx,
                              IDBRequest** _retval);

  nsresult CountInternal(IDBKeyRange* aKeyRange,
                         JSContext* aCx,
                         IDBRequest** _retval);

  nsresult OpenCursorFromChildProcess(
                            IDBRequest* aRequest,
                            size_t aDirection,
                            const Key& aKey,
                            const Key& aObjectKey,
                            IDBCursor** _retval);

  nsresult OpenKeyCursorInternal(IDBKeyRange* aKeyRange,
                                 size_t aDirection,
                                 JSContext* aCx,
                                 IDBRequest** _retval);

  nsresult OpenCursorInternal(IDBKeyRange* aKeyRange,
                              size_t aDirection,
                              JSContext* aCx,
                              IDBRequest** _retval);

  nsresult OpenCursorFromChildProcess(
                            IDBRequest* aRequest,
                            size_t aDirection,
                            const Key& aKey,
                            const Key& aObjectKey,
                            const SerializedStructuredCloneReadInfo& aCloneInfo,
                            nsTArray<StructuredCloneFile>& aBlobs,
                            IDBCursor** _retval);

private:
  IDBIndex();
  ~IDBIndex();

  nsRefPtr<IDBObjectStore> mObjectStore;

  PRInt64 mId;
  nsString mName;
  KeyPath mKeyPath;
  JS::Value mCachedKeyPath;

  IndexedDBIndexChild* mActorChild;
  IndexedDBIndexParent* mActorParent;

  bool mUnique;
  bool mMultiEntry;
  bool mRooted;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbindex_h__
