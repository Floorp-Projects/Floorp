/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbindex_h__
#define mozilla_dom_indexeddb_idbindex_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIDBIndex.h"

#include "nsCycleCollectionParticipant.h"

class nsIScriptContext;
class nsPIDOMWindow;

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class IDBObjectStore;
struct IndexInfo;

class IDBIndex MOZ_FINAL : public nsIIDBIndex
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIIDBINDEX

  NS_DECL_CYCLE_COLLECTION_CLASS(IDBIndex)

  static already_AddRefed<IDBIndex>
  Create(IDBObjectStore* aObjectStore,
         const IndexInfo* aIndexInfo);

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

  const nsString& KeyPath() const
  {
    return mKeyPath;
  }

  bool UsesKeyPathArray() const
  {
    return !mKeyPathArray.IsEmpty();
  }
  
  const nsTArray<nsString>& KeyPathArray() const
  {
    return mKeyPathArray;
  }

private:
  IDBIndex();
  ~IDBIndex();

  nsRefPtr<IDBObjectStore> mObjectStore;

  PRInt64 mId;
  nsString mName;
  nsString mKeyPath;
  nsTArray<nsString> mKeyPathArray;
  bool mUnique;
  bool mMultiEntry;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbindex_h__
