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
#include "mozilla/dom/indexedDB/KeyPath.h"

class nsIDOMBlob;
class nsIScriptContext;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {
class ContentParent;
class PBlobChild;
class PBlobParent;
}
}

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class FileManager;
class IDBCursor;
class IDBKeyRange;
class IDBRequest;
class IndexedDBObjectStoreChild;
class IndexedDBObjectStoreParent;
class Key;

struct IndexInfo;
struct IndexUpdateInfo;
struct ObjectStoreInfo;

struct FileHandleData;
struct BlobOrFileData;

class IDBObjectStore MOZ_FINAL : public nsIIDBObjectStore
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIIDBOBJECTSTORE

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBObjectStore)

  static already_AddRefed<IDBObjectStore>
  Create(IDBTransaction* aTransaction,
         ObjectStoreInfo* aInfo,
         nsIAtom* aDatabaseId,
         bool aCreating);

  static nsresult
  AppendIndexUpdateInfo(int64_t aIndexID,
                        const KeyPath& aKeyPath,
                        bool aUnique,
                        bool aMultiEntry,
                        JSContext* aCx,
                        JS::Handle<JS::Value> aObject,
                        nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  static nsresult
  UpdateIndexes(IDBTransaction* aTransaction,
                int64_t aObjectStoreId,
                const Key& aObjectStoreKey,
                bool aOverwrite,
                int64_t aObjectDataId,
                const nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  static nsresult
  GetStructuredCloneReadInfoFromStatement(mozIStorageStatement* aStatement,
                                          uint32_t aDataIndex,
                                          uint32_t aFileIdsIndex,
                                          IDBDatabase* aDatabase,
                                          StructuredCloneReadInfo& aInfo);

  static void
  ClearCloneReadInfo(StructuredCloneReadInfo& aReadInfo);

  static void
  ClearCloneWriteInfo(StructuredCloneWriteInfo& aWriteInfo);

  static bool
  DeserializeValue(JSContext* aCx,
                   StructuredCloneReadInfo& aCloneReadInfo,
                   jsval* aValue);

  static bool
  SerializeValue(JSContext* aCx,
                 StructuredCloneWriteInfo& aCloneWriteInfo,
                 JS::Handle<JS::Value> aValue);

  template <class DeserializationTraits>
  static JSObject*
  StructuredCloneReadCallback(JSContext* aCx,
                              JSStructuredCloneReader* aReader,
                              uint32_t aTag,
                              uint32_t aData,
                              void* aClosure);
  static JSBool
  StructuredCloneWriteCallback(JSContext* aCx,
                               JSStructuredCloneWriter* aWriter,
                               JS::Handle<JSObject*> aObj,
                               void* aClosure);

  static nsresult
  ConvertFileIdsToArray(const nsAString& aFileIds,
                        nsTArray<int64_t>& aResult);

  // Called only in the main process.
  static nsresult
  ConvertBlobsToActors(ContentParent* aContentParent,
                       FileManager* aFileManager,
                       const nsTArray<StructuredCloneFile>& aFiles,
                       InfallibleTArray<PBlobParent*>& aActors);

  // Called only in the child process.
  static void
  ConvertActorsToBlobs(const InfallibleTArray<PBlobChild*>& aActors,
                       nsTArray<StructuredCloneFile>& aFiles);

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

  int64_t Id() const
  {
    NS_ASSERTION(mId != INT64_MIN, "Don't ask for this yet!");
    return mId;
  }

  const KeyPath& GetKeyPath() const
  {
    return mKeyPath;
  }

  const bool HasValidKeyPath() const
  {
    return mKeyPath.IsValid();
  }

  IDBTransaction* Transaction()
  {
    return mTransaction;
  }

  ObjectStoreInfo* Info()
  {
    return mInfo;
  }

  void
  SetActor(IndexedDBObjectStoreChild* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  SetActor(IndexedDBObjectStoreParent* aActorParent)
  {
    NS_ASSERTION(!aActorParent || !mActorParent,
                 "Shouldn't have more than one!");
    mActorParent = aActorParent;
  }

  IndexedDBObjectStoreChild*
  GetActorChild() const
  {
    return mActorChild;
  }

  IndexedDBObjectStoreParent*
  GetActorParent() const
  {
    return mActorParent;
  }

  nsresult
  CreateIndexInternal(const IndexInfo& aInfo,
                      IDBIndex** _retval);

  nsresult
  IndexInternal(const nsAString& aName,
                IDBIndex** _retval);

  nsresult AddOrPutInternal(
                      const SerializedStructuredCloneWriteInfo& aCloneWriteInfo,
                      const Key& aKey,
                      const InfallibleTArray<IndexUpdateInfo>& aUpdateInfoArray,
                      const nsTArray<nsCOMPtr<nsIDOMBlob> >& aBlobs,
                      bool aOverwrite,
                      IDBRequest** _retval);

  nsresult GetInternal(IDBKeyRange* aKeyRange,
                       JSContext* aCx,
                       IDBRequest** _retval);

  nsresult GetAllInternal(IDBKeyRange* aKeyRange,
                          uint32_t aLimit,
                          JSContext* aCx,
                          IDBRequest** _retval);

  nsresult DeleteInternal(IDBKeyRange* aKeyRange,
                          JSContext* aCx,
                          IDBRequest** _retval);

  nsresult ClearInternal(JSContext* aCx,
                         IDBRequest** _retval);

  nsresult CountInternal(IDBKeyRange* aKeyRange,
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
                            const SerializedStructuredCloneReadInfo& aCloneInfo,
                            nsTArray<StructuredCloneFile>& aBlobs,
                            IDBCursor** _retval);

  void
  SetInfo(ObjectStoreInfo* aInfo);

  static JSClass sDummyPropJSClass;

protected:
  IDBObjectStore();
  ~IDBObjectStore();

  nsresult GetAddInfo(JSContext* aCx,
                      JS::Handle<JS::Value> aValue,
                      JS::Handle<JS::Value> aKeyVal,
                      StructuredCloneWriteInfo& aCloneWriteInfo,
                      Key& aKey,
                      nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  nsresult AddOrPut(const jsval& aValue,
                    const jsval& aKey,
                    JSContext* aCx,
                    uint8_t aOptionalArgCount,
                    bool aOverwrite,
                    IDBRequest** _retval);

  static void
  ClearStructuredCloneBuffer(JSAutoStructuredCloneBuffer& aBuffer);

  static bool
  ReadFileHandle(JSStructuredCloneReader* aReader,
                 FileHandleData* aRetval);

  static bool
  ReadBlobOrFile(JSStructuredCloneReader* aReader,
                 uint32_t aTag,
                 BlobOrFileData* aRetval);
private:
  nsRefPtr<IDBTransaction> mTransaction;

  int64_t mId;
  nsString mName;
  KeyPath mKeyPath;
  JS::Value mCachedKeyPath;
  bool mRooted;
  bool mAutoIncrement;
  nsCOMPtr<nsIAtom> mDatabaseId;
  nsRefPtr<ObjectStoreInfo> mInfo;

  nsTArray<nsRefPtr<IDBIndex> > mCreatedIndexes;

  IndexedDBObjectStoreChild* mActorChild;
  IndexedDBObjectStoreParent* mActorParent;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbobjectstore_h__
