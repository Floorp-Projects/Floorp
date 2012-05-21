/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbobjectstore_h__
#define mozilla_dom_indexeddb_idbobjectstore_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIDBObjectStore.h"
#include "nsIIDBTransaction.h"

#include "nsCycleCollectionParticipant.h"

#include "mozilla/dom/indexedDB/IDBTransaction.h"

class nsIScriptContext;
class nsPIDOMWindow;

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class Key;

struct ObjectStoreInfo;
struct IndexInfo;
struct IndexUpdateInfo;
struct StructuredCloneReadInfo;
struct StructuredCloneWriteInfo;

class IDBObjectStore MOZ_FINAL : public nsIIDBObjectStore
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIIDBOBJECTSTORE

  NS_DECL_CYCLE_COLLECTION_CLASS(IDBObjectStore)

  static already_AddRefed<IDBObjectStore>
  Create(IDBTransaction* aTransaction,
         ObjectStoreInfo* aInfo,
         nsIAtom* aDatabaseId);

  static bool
  IsValidKeyPath(JSContext* aCx, const nsAString& aKeyPath);

  static nsresult
  AppendIndexUpdateInfo(PRInt64 aIndexID,
                        const nsAString& aKeyPath,
                        const nsTArray<nsString>& aKeyPathArray,
                        bool aUnique,
                        bool aMultiEntry,
                        JSContext* aCx,
                        jsval aObject,
                        nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  static nsresult
  UpdateIndexes(IDBTransaction* aTransaction,
                PRInt64 aObjectStoreId,
                const Key& aObjectStoreKey,
                bool aOverwrite,
                PRInt64 aObjectDataId,
                const nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  static nsresult
  GetStructuredCloneReadInfoFromStatement(mozIStorageStatement* aStatement,
                                          PRUint32 aDataIndex,
                                          PRUint32 aFileIdsIndex,
                                          FileManager* aFileManager,
                                          StructuredCloneReadInfo& aInfo);

  static void
  ClearStructuredCloneBuffer(JSAutoStructuredCloneBuffer& aBuffer);

  static bool
  DeserializeValue(JSContext* aCx,
                   StructuredCloneReadInfo& aCloneReadInfo,
                   jsval* aValue);

  static bool
  SerializeValue(JSContext* aCx,
                 StructuredCloneWriteInfo& aCloneWriteInfo,
                 jsval aValue);

  static JSObject*
  StructuredCloneReadCallback(JSContext* aCx,
                              JSStructuredCloneReader* aReader,
                              uint32_t aTag,
                              uint32_t aData,
                              void* aClosure);
  static JSBool
  StructuredCloneWriteCallback(JSContext* aCx,
                               JSStructuredCloneWriter* aWriter,
                               JSObject* aObj,
                               void* aClosure);

  static nsresult
  ConvertFileIdsToArray(const nsAString& aFileIds,
                        nsTArray<PRInt64>& aResult);

  const nsString& Name() const
  {
    return mName;
  }

  bool IsAutoIncrement() const
  {
    return mAutoIncrement;
  }

  bool IsWriteAllowed() const
  {
    return mTransaction->IsWriteAllowed();
  }

  PRInt64 Id() const
  {
    NS_ASSERTION(mId != LL_MININT, "Don't ask for this yet!");
    return mId;
  }

  const nsString& KeyPath() const
  {
    return mKeyPath;
  }

  const bool HasKeyPath() const
  {
    return !mKeyPath.IsVoid() || !mKeyPathArray.IsEmpty();
  }

  bool UsesKeyPathArray() const
  {
    return !mKeyPathArray.IsEmpty();
  }
  
  const nsTArray<nsString>& KeyPathArray() const
  {
    return mKeyPathArray;
  }

  IDBTransaction* Transaction()
  {
    return mTransaction;
  }

  ObjectStoreInfo* Info()
  {
    return mInfo;
  }

protected:
  IDBObjectStore();
  ~IDBObjectStore();

  nsresult GetAddInfo(JSContext* aCx,
                      jsval aValue,
                      jsval aKeyVal,
                      StructuredCloneWriteInfo& aCloneWriteInfo,
                      Key& aKey,
                      nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  nsresult AddOrPut(const jsval& aValue,
                    const jsval& aKey,
                    JSContext* aCx,
                    PRUint8 aOptionalArgCount,
                    nsIIDBRequest** _retval,
                    bool aOverwrite);

private:
  nsRefPtr<IDBTransaction> mTransaction;

  PRInt64 mId;
  nsString mName;
  nsString mKeyPath;
  nsTArray<nsString> mKeyPathArray;
  bool mAutoIncrement;
  nsCOMPtr<nsIAtom> mDatabaseId;
  nsRefPtr<ObjectStoreInfo> mInfo;
  PRUint32 mStructuredCloneVersion;

  nsTArray<nsRefPtr<IDBIndex> > mCreatedIndexes;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbobjectstore_h__
