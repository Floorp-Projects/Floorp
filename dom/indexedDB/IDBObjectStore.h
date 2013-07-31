/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbobjectstore_h__
#define mozilla_dom_indexeddb_idbobjectstore_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IDBIndexBinding.h"
#include "mozilla/dom/IDBObjectStoreBinding.h"
#include "nsCycleCollectionParticipant.h"

#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/IDBTransaction.h"
#include "mozilla/dom/indexedDB/KeyPath.h"

class nsIDOMBlob;
class nsIScriptContext;
class nsPIDOMWindow;
class nsIIDBIndex;

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

class IDBObjectStore MOZ_FINAL : public nsISupports,
                                 public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
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
                   JS::MutableHandle<JS::Value> aValue);

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

  already_AddRefed<nsIIDBIndex>
  CreateIndexInternal(const IndexInfo& aInfo,
                      ErrorResult& aRv);

  nsresult AddOrPutInternal(
                      const SerializedStructuredCloneWriteInfo& aCloneWriteInfo,
                      const Key& aKey,
                      const InfallibleTArray<IndexUpdateInfo>& aUpdateInfoArray,
                      const nsTArray<nsCOMPtr<nsIDOMBlob> >& aBlobs,
                      bool aOverwrite,
                      IDBRequest** _retval);

  already_AddRefed<IDBRequest>
  GetInternal(IDBKeyRange* aKeyRange,
              ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAllInternal(IDBKeyRange* aKeyRange,
                 uint32_t aLimit,
                 ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  DeleteInternal(IDBKeyRange* aKeyRange,
                 ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  CountInternal(IDBKeyRange* aKeyRange,
                ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  OpenCursorInternal(IDBKeyRange* aKeyRange,
                     size_t aDirection,
                     ErrorResult& aRv);

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

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  IDBTransaction*
  GetParentObject() const
  {
    return mTransaction;
  }

  void
  GetName(nsString& aName) const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    aName.Assign(mName);
  }

  JS::Value
  GetKeyPath(JSContext* aCx, ErrorResult& aRv);

  already_AddRefed<nsIDOMDOMStringList>
  GetIndexNames(ErrorResult& aRv);

  IDBTransaction*
  Transaction() const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    return mTransaction;
  }

  bool
  AutoIncrement() const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    return mAutoIncrement;
  }

  already_AddRefed<IDBRequest>
  Put(JSContext* aCx, JS::Handle<JS::Value> aValue,
      const Optional<JS::Handle<JS::Value> >& aKey, ErrorResult& aRv)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    return AddOrPut(aCx, aValue, aKey, true, aRv);
  }

  already_AddRefed<IDBRequest>
  Add(JSContext* aCx, JS::Handle<JS::Value> aValue,
      const Optional<JS::Handle<JS::Value> >& aKey, ErrorResult& aRv)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    return AddOrPut(aCx, aValue, aKey, false, aRv);
  }

  already_AddRefed<IDBRequest>
  Delete(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Get(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Clear(ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  OpenCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
             IDBCursorDirection aDirection, ErrorResult& aRv);

  already_AddRefed<nsIIDBIndex>
  CreateIndex(JSContext* aCx, const nsAString& aName, const nsAString& aKeyPath,
              const IDBIndexParameters& aOptionalParameters, ErrorResult& aRv);

  already_AddRefed<nsIIDBIndex>
  CreateIndex(JSContext* aCx, const nsAString& aName,
              const Sequence<nsString >& aKeyPath,
              const IDBIndexParameters& aOptionalParameters, ErrorResult& aRv);

  already_AddRefed<nsIIDBIndex>
  Index(const nsAString& aName, ErrorResult &aRv);

  void
  DeleteIndex(const nsAString& aIndexName, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Count(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
        ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  GetAll(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
         const Optional<uint32_t >& aLimit, ErrorResult& aRv);

protected:
  IDBObjectStore();
  ~IDBObjectStore();

  nsresult GetAddInfo(JSContext* aCx,
                      JS::Handle<JS::Value> aValue,
                      JS::Handle<JS::Value> aKeyVal,
                      StructuredCloneWriteInfo& aCloneWriteInfo,
                      Key& aKey,
                      nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  already_AddRefed<IDBRequest>
  AddOrPut(JSContext* aCx, JS::Handle<JS::Value> aValue,
           const Optional<JS::Handle<JS::Value> >& aKey, bool aOverwrite,
           ErrorResult& aRv);

  already_AddRefed<nsIIDBIndex>
  CreateIndex(JSContext* aCx, const nsAString& aName, KeyPath& aKeyPath,
              const IDBIndexParameters& aOptionalParameters, ErrorResult& aRv);

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
  JS::Heap<JS::Value> mCachedKeyPath;
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
