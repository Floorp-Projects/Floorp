/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBObjectStore.h"

#include "nsIJSContextStack.h"

#include "jsfriendapi.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/storage.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsDOMFile.h"
#include "nsDOMLists.h"
#include "nsEventDispatcher.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "snappy/snappy.h"
#include "test_quota.h"
#include "xpcpublic.h"

#include "AsyncConnectionHelper.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBIndex.h"
#include "IDBKeyRange.h"
#include "IDBTransaction.h"
#include "DatabaseInfo.h"
#include "DictionaryHelpers.h"

#define FILE_COPY_BUFFER_SIZE 32768

USING_INDEXEDDB_NAMESPACE

namespace {

class AddHelper : public AsyncConnectionHelper
{
public:
  AddHelper(IDBTransaction* aTransaction,
            IDBRequest* aRequest,
            IDBObjectStore* aObjectStore,
            StructuredCloneWriteInfo& aCloneWriteInfo,
            const Key& aKey,
            bool aOverwrite,
            nsTArray<IndexUpdateInfo>& aIndexUpdateInfo)
  : AsyncConnectionHelper(aTransaction, aRequest), mObjectStore(aObjectStore),
    mKey(aKey), mOverwrite(aOverwrite)
  {
    mCloneWriteInfo.Swap(aCloneWriteInfo);
    mIndexUpdateInfo.SwapElements(aIndexUpdateInfo);
  }

  ~AddHelper()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneWriteInfo.mCloneBuffer);
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneWriteInfo.mCloneBuffer);
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBObjectStore> mObjectStore;

  // These may change in the autoincrement case.
  StructuredCloneWriteInfo mCloneWriteInfo;
  Key mKey;
  const bool mOverwrite;
  nsTArray<IndexUpdateInfo> mIndexUpdateInfo;
};

class GetHelper : public AsyncConnectionHelper
{
public:
  GetHelper(IDBTransaction* aTransaction,
            IDBRequest* aRequest,
            IDBObjectStore* aObjectStore,
            IDBKeyRange* aKeyRange)
  : AsyncConnectionHelper(aTransaction, aRequest), mObjectStore(aObjectStore),
    mKeyRange(aKeyRange)
  { }

  ~GetHelper()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    mKeyRange = nsnull;
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

protected:
  // In-params.
  nsRefPtr<IDBObjectStore> mObjectStore;
  nsRefPtr<IDBKeyRange> mKeyRange;

private:
  // Out-params.
  StructuredCloneReadInfo mCloneReadInfo;
};

class DeleteHelper : public GetHelper
{
public:
  DeleteHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               IDBObjectStore* aObjectStore,
               IDBKeyRange* aKeyRange)
  : GetHelper(aTransaction, aRequest, aObjectStore, aKeyRange)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);
};

class ClearHelper : public AsyncConnectionHelper
{
public:
  ClearHelper(IDBTransaction* aTransaction,
              IDBRequest* aRequest,
              IDBObjectStore* aObjectStore)
  : AsyncConnectionHelper(aTransaction, aRequest), mObjectStore(aObjectStore)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

protected:
  // In-params.
  nsRefPtr<IDBObjectStore> mObjectStore;
};

class OpenCursorHelper : public AsyncConnectionHelper
{
public:
  OpenCursorHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   IDBObjectStore* aObjectStore,
                   IDBKeyRange* aKeyRange,
                   IDBCursor::Direction aDirection)
  : AsyncConnectionHelper(aTransaction, aRequest), mObjectStore(aObjectStore),
    mKeyRange(aKeyRange), mDirection(aDirection)
  { }

  ~OpenCursorHelper()
  {
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    mKeyRange = nsnull;
    IDBObjectStore::ClearStructuredCloneBuffer(mCloneReadInfo.mCloneBuffer);
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBObjectStore> mObjectStore;
  nsRefPtr<IDBKeyRange> mKeyRange;
  const IDBCursor::Direction mDirection;

  // Out-params.
  Key mKey;
  StructuredCloneReadInfo mCloneReadInfo;
  nsCString mContinueQuery;
  nsCString mContinueToQuery;
  Key mRangeKey;
};

class CreateIndexHelper : public AsyncConnectionHelper
{
public:
  CreateIndexHelper(IDBTransaction* aTransaction, IDBIndex* aIndex);

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);

  nsresult OnSuccess()
  {
    return NS_OK;
  }

  void OnError()
  {
    NS_ASSERTION(mTransaction->IsAborted(), "How else can this fail?!");
  }

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  nsresult InsertDataFromObjectStore(mozIStorageConnection* aConnection);

  static void DestroyTLSEntry(void* aPtr);

  static PRUintn sTLSIndex;

  // In-params.
  nsRefPtr<IDBIndex> mIndex;
};

PRUintn CreateIndexHelper::sTLSIndex = PRUintn(BAD_TLS_INDEX);

class DeleteIndexHelper : public AsyncConnectionHelper
{
public:
  DeleteIndexHelper(IDBTransaction* aTransaction,
                    const nsAString& aName,
                    IDBObjectStore* aObjectStore)
  : AsyncConnectionHelper(aTransaction, nsnull), mName(aName),
    mObjectStore(aObjectStore)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);

  nsresult OnSuccess()
  {
    return NS_OK;
  }

  void OnError()
  {
    NS_ASSERTION(mTransaction->IsAborted(), "How else can this fail?!");
  }

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params
  nsString mName;
  nsRefPtr<IDBObjectStore> mObjectStore;
};

class GetAllHelper : public AsyncConnectionHelper
{
public:
  GetAllHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               IDBObjectStore* aObjectStore,
               IDBKeyRange* aKeyRange,
               const PRUint32 aLimit)
  : AsyncConnectionHelper(aTransaction, aRequest), mObjectStore(aObjectStore),
    mKeyRange(aKeyRange), mLimit(aLimit)
  { }

  ~GetAllHelper()
  {
    for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
      IDBObjectStore::ClearStructuredCloneBuffer(
        mCloneReadInfos[index].mCloneBuffer);
    }
  }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    mKeyRange = nsnull;
    for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
      IDBObjectStore::ClearStructuredCloneBuffer(
        mCloneReadInfos[index].mCloneBuffer);
    }
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

protected:
  // In-params.
  nsRefPtr<IDBObjectStore> mObjectStore;
  nsRefPtr<IDBKeyRange> mKeyRange;
  const PRUint32 mLimit;

private:
  // Out-params.
  nsTArray<StructuredCloneReadInfo> mCloneReadInfos;
};

class CountHelper : public AsyncConnectionHelper
{
public:
  CountHelper(IDBTransaction* aTransaction,
              IDBRequest* aRequest,
              IDBObjectStore* aObjectStore,
              IDBKeyRange* aKeyRange)
  : AsyncConnectionHelper(aTransaction, aRequest), mObjectStore(aObjectStore),
    mKeyRange(aKeyRange), mCount(0)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    mKeyRange = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

protected:
  nsRefPtr<IDBObjectStore> mObjectStore;
  nsRefPtr<IDBKeyRange> mKeyRange;

private:
  PRUint64 mCount;
};

NS_STACK_CLASS
class AutoRemoveIndex
{
public:
  AutoRemoveIndex(ObjectStoreInfo* aObjectStoreInfo,
                  const nsAString& aIndexName)
  : mObjectStoreInfo(aObjectStoreInfo), mIndexName(aIndexName)
  { }

  ~AutoRemoveIndex()
  {
    if (mObjectStoreInfo) {
      for (PRUint32 i = 0; i < mObjectStoreInfo->indexes.Length(); i++) {
        if (mObjectStoreInfo->indexes[i].name == mIndexName) {
          mObjectStoreInfo->indexes.RemoveElementAt(i);
          break;
        }
      }
    }
  }

  void forget()
  {
    mObjectStoreInfo = nsnull;
  }

private:
  ObjectStoreInfo* mObjectStoreInfo;
  nsString mIndexName;
};

inline
bool
IgnoreWhitespace(PRUnichar c)
{
  return false;
}

typedef nsCharSeparatedTokenizerTemplate<IgnoreWhitespace> KeyPathTokenizer;

inline
nsresult
GetJSValFromKeyPath(JSContext* aCx,
                    jsval aVal,
                    const nsAString& aKeyPath,
                    jsval& aKey)
{
  NS_ASSERTION(aCx, "Null pointer!");
  // aVal can be primitive iff the key path is empty.
  NS_ASSERTION(IDBObjectStore::IsValidKeyPath(aCx, aKeyPath),
               "This will explode!");

  KeyPathTokenizer tokenizer(aKeyPath, '.');

  jsval intermediate = aVal;
  while (tokenizer.hasMoreTokens()) {
    const nsDependentSubstring& token = tokenizer.nextToken();

    NS_ASSERTION(!token.IsEmpty(), "Should be a valid keypath");

    const jschar* keyPathChars = token.BeginReading();
    const size_t keyPathLen = token.Length();

    if (JSVAL_IS_PRIMITIVE(intermediate)) {
      intermediate = JSVAL_VOID;
      break;
    }

    JSBool ok = JS_GetUCProperty(aCx, JSVAL_TO_OBJECT(intermediate),
                                 keyPathChars, keyPathLen, &intermediate);
    NS_ENSURE_TRUE(ok, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  
  aKey = intermediate;
  return NS_OK;
}

inline
nsresult
GetKeyFromValue(JSContext* aCx,
                jsval aVal,
                const nsAString& aKeyPath,
                Key& aKey)
{
  jsval key;
  nsresult rv = GetJSValFromKeyPath(aCx, aVal, aKeyPath, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_FAILED(aKey.SetFromJSVal(aCx, key))) {
    aKey.Unset();
  }

  return NS_OK;
}

inline
nsresult
GetKeyFromValue(JSContext* aCx,
                jsval aVal,
                const nsTArray<nsString>& aKeyPathArray,
                Key& aKey)
{
  NS_ASSERTION(!aKeyPathArray.IsEmpty(),
               "Should not use empty keyPath array");
  for (PRUint32 i = 0; i < aKeyPathArray.Length(); ++i) {
    jsval key;
    nsresult rv = GetJSValFromKeyPath(aCx, aVal, aKeyPathArray[i], key);
    NS_ENSURE_SUCCESS(rv, rv);

    if (NS_FAILED(aKey.AppendArrayItem(aCx, i == 0, key))) {
      NS_ASSERTION(aKey.IsUnset(), "Encoding error should unset");
      return NS_OK;
    }
  }

  aKey.FinishArray();

  return NS_OK;
}


inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBObjectStore* aObjectStore)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  IDBDatabase* database = aObjectStore->Transaction()->Database();
  return IDBRequest::Create(aObjectStore, database,
                            aObjectStore->Transaction());
}

JSClass gDummyPropClass = {
  "dummy", 0,
  JS_PropertyStub,  JS_PropertyStub,
  JS_PropertyStub,  JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub,
  JS_ConvertStub
};

} // anonymous namespace

// static
already_AddRefed<IDBObjectStore>
IDBObjectStore::Create(IDBTransaction* aTransaction,
                       ObjectStoreInfo* aStoreInfo,
                       nsIAtom* aDatabaseId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBObjectStore> objectStore = new IDBObjectStore();

  objectStore->mTransaction = aTransaction;
  objectStore->mName = aStoreInfo->name;
  objectStore->mId = aStoreInfo->id;
  objectStore->mKeyPath = aStoreInfo->keyPath;
  objectStore->mKeyPathArray = aStoreInfo->keyPathArray;
  objectStore->mAutoIncrement = !!aStoreInfo->nextAutoIncrementId;
  objectStore->mDatabaseId = aDatabaseId;
  objectStore->mInfo = aStoreInfo;

  return objectStore.forget();
}

// static
bool
IDBObjectStore::IsValidKeyPath(JSContext* aCx,
                               const nsAString& aKeyPath)
{
  NS_ASSERTION(!aKeyPath.IsVoid(), "What?");

  KeyPathTokenizer tokenizer(aKeyPath, '.');

  while (tokenizer.hasMoreTokens()) {
    nsString token(tokenizer.nextToken());

    if (!token.Length()) {
      return false;
    }

    jsval stringVal;
    if (!xpc::StringToJsval(aCx, token, &stringVal)) {
      return false;
    }

    NS_ASSERTION(JSVAL_IS_STRING(stringVal), "This should never happen");
    JSString* str = JSVAL_TO_STRING(stringVal);

    JSBool isIdentifier = JS_FALSE;
    if (!JS_IsIdentifier(aCx, str, &isIdentifier) || !isIdentifier) {
      return false;
    }
  }

  // If the very last character was a '.', the tokenizer won't give us an empty
  // token, but the keyPath is still invalid.
  if (!aKeyPath.IsEmpty() &&
      aKeyPath.CharAt(aKeyPath.Length() - 1) == '.') {
    return false;
  }

  return true;
}

// static
nsresult
IDBObjectStore::AppendIndexUpdateInfo(PRInt64 aIndexID,
                                      const nsAString& aKeyPath,
                                      const nsTArray<nsString>& aKeyPathArray,
                                      bool aUnique,
                                      bool aMultiEntry,
                                      JSContext* aCx,
                                      jsval aVal,
                                      nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
  nsresult rv;
  if (!aKeyPathArray.IsEmpty()) {
    Key arrayKey;
    rv = GetKeyFromValue(aCx, aVal, aKeyPathArray, arrayKey);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!arrayKey.IsUnset()) {
      IndexUpdateInfo* updateInfo = aUpdateInfoArray.AppendElement();
      updateInfo->indexId = aIndexID;
      updateInfo->indexUnique = aUnique;
      updateInfo->value = arrayKey;
    }

    return NS_OK;
  }

  jsval key;
  rv = GetJSValFromKeyPath(aCx, aVal, aKeyPath, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aMultiEntry && !JSVAL_IS_PRIMITIVE(key) &&
      JS_IsArrayObject(aCx, JSVAL_TO_OBJECT(key))) {
    JSObject* array = JSVAL_TO_OBJECT(key);
    uint32_t arrayLength;
    if (!JS_GetArrayLength(aCx, array, &arrayLength)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    for (uint32_t arrayIndex = 0; arrayIndex < arrayLength; arrayIndex++) {
      jsval arrayItem;
      if (!JS_GetElement(aCx, array, arrayIndex, &arrayItem)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      Key value;
      if (NS_FAILED(value.SetFromJSVal(aCx, arrayItem)) ||
          value.IsUnset()) {
        // Not a value we can do anything with, ignore it.
        continue;
      }

      IndexUpdateInfo* updateInfo = aUpdateInfoArray.AppendElement();
      updateInfo->indexId = aIndexID;
      updateInfo->indexUnique = aUnique;
      updateInfo->value = value;
    }
  }
  else {
    Key value;
    if (NS_FAILED(value.SetFromJSVal(aCx, key)) ||
        value.IsUnset()) {
      // Not a value we can do anything with, ignore it.
      return NS_OK;
    }

    IndexUpdateInfo* updateInfo = aUpdateInfoArray.AppendElement();
    updateInfo->indexId = aIndexID;
    updateInfo->indexUnique = aUnique;
    updateInfo->value = value;
  }

  return NS_OK;
}

// static
nsresult
IDBObjectStore::UpdateIndexes(IDBTransaction* aTransaction,
                              PRInt64 aObjectStoreId,
                              const Key& aObjectStoreKey,
                              bool aOverwrite,
                              PRInt64 aObjectDataId,
                              const nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv;

  NS_ASSERTION(aObjectDataId != LL_MININT, "Bad objectData id!");

  NS_NAMED_LITERAL_CSTRING(objectDataId, "object_data_id");

  if (aOverwrite) {
    stmt = aTransaction->GetCachedStatement(
      "DELETE FROM unique_index_data "
      "WHERE object_data_id = :object_data_id; "
      "DELETE FROM index_data "
      "WHERE object_data_id = :object_data_id");

    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(objectDataId, aObjectDataId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRUint32 infoCount = aUpdateInfoArray.Length();
  for (PRUint32 i = 0; i < infoCount; i++) {
    const IndexUpdateInfo& updateInfo = aUpdateInfoArray[i];

    // Insert new values.

    stmt = updateInfo.indexUnique ?
      aTransaction->GetCachedStatement(
        "INSERT INTO unique_index_data "
          "(index_id, object_data_id, object_data_key, value) "
        "VALUES (:index_id, :object_data_id, :object_data_key, :value)") :
      aTransaction->GetCachedStatement(
        "INSERT OR IGNORE INTO index_data ("
          "index_id, object_data_id, object_data_key, value) "
        "VALUES (:index_id, :object_data_id, :object_data_key, :value)");

    NS_ENSURE_TRUE(stmt, NS_ERROR_FAILURE);

    mozStorageStatementScoper scoper4(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                               updateInfo.indexId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt64ByName(objectDataId, aObjectDataId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aObjectStoreKey.BindToStatement(stmt,
                                         NS_LITERAL_CSTRING("object_data_key"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = updateInfo.value.BindToStatement(stmt, NS_LITERAL_CSTRING("value"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    if (rv == NS_ERROR_STORAGE_CONSTRAINT && updateInfo.indexUnique) {
      // If we're inserting multiple entries for the same unique index, then
      // we might have failed to insert due to colliding with another entry for
      // the same index in which case we should ignore it.
      
      for (PRInt32 j = (PRInt32)i - 1;
           j >= 0 && aUpdateInfoArray[j].indexId == updateInfo.indexId;
           --j) {
        if (updateInfo.value == aUpdateInfoArray[j].value) {
          // We found a key with the same value for the same index. So we
          // must have had a collision with a value we just inserted.
          rv = NS_OK;
          break;
        }
      }
    }

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

// static
nsresult
IDBObjectStore::GetStructuredCloneReadInfoFromStatement(
                                           mozIStorageStatement* aStatement,
                                           PRUint32 aDataIndex,
                                           PRUint32 aFileIdsIndex,
                                           FileManager* aFileManager,
                                           StructuredCloneReadInfo& aInfo)
{
#ifdef DEBUG
  {
    PRInt32 type;
    NS_ASSERTION(NS_SUCCEEDED(aStatement->GetTypeOfIndex(aDataIndex, &type)) &&
                 type == mozIStorageStatement::VALUE_TYPE_BLOB,
                 "Bad value type!");
  }
#endif

  const PRUint8* blobData;
  PRUint32 blobDataLength;
  nsresult rv = aStatement->GetSharedBlob(aDataIndex, &blobDataLength,
                                          &blobData);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  const char* compressed = reinterpret_cast<const char*>(blobData);
  size_t compressedLength = size_t(blobDataLength);

  size_t uncompressedLength;
  if (!snappy::GetUncompressedLength(compressed, compressedLength,
                                     &uncompressedLength)) {
    NS_WARNING("Snappy can't determine uncompressed length!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsAutoArrayPtr<char> uncompressed(new char[uncompressedLength]);

  if (!snappy::RawUncompress(compressed, compressedLength,
                             uncompressed.get())) {
    NS_WARNING("Snappy can't determine uncompressed length!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  JSAutoStructuredCloneBuffer& buffer = aInfo.mCloneBuffer;
  if (!buffer.copy(reinterpret_cast<const uint64_t *>(uncompressed.get()),
                   uncompressedLength)) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  bool isNull;
  rv = aStatement->GetIsNull(aFileIdsIndex, &isNull);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (isNull) {
    return NS_OK;
  }

  nsString ids;
  rv = aStatement->GetString(aFileIdsIndex, ids);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsAutoTArray<PRInt64, 10> array;
  rv = ConvertFileIdsToArray(ids, array);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  for (PRUint32 i = 0; i < array.Length(); i++) {
    const PRInt64& id = array.ElementAt(i);

    nsRefPtr<FileInfo> fileInfo = aFileManager->GetFileInfo(id);
    NS_ASSERTION(fileInfo, "Null file info!");

    aInfo.mFileInfos.AppendElement(fileInfo);
  }

  return NS_OK;
}

// static
void
IDBObjectStore::ClearStructuredCloneBuffer(JSAutoStructuredCloneBuffer& aBuffer)
{
  if (aBuffer.data()) {
    aBuffer.clear();
  }
}

// static
bool
IDBObjectStore::DeserializeValue(JSContext* aCx,
                                 StructuredCloneReadInfo& aCloneReadInfo,
                                 jsval* aValue)
{
  NS_ASSERTION(NS_IsMainThread(),
               "Should only be deserializing on the main thread!");
  NS_ASSERTION(aCx, "A JSContext is required!");

  JSAutoStructuredCloneBuffer& buffer = aCloneReadInfo.mCloneBuffer;

  if (!buffer.data()) {
    *aValue = JSVAL_VOID;
    return true;
  }

  JSAutoRequest ar(aCx);

  JSStructuredCloneCallbacks callbacks = {
    IDBObjectStore::StructuredCloneReadCallback,
    nsnull,
    nsnull
  };

  return buffer.read(aCx, aValue, &callbacks, &aCloneReadInfo);
}

// static
bool
IDBObjectStore::SerializeValue(JSContext* aCx,
                               StructuredCloneWriteInfo& aCloneWriteInfo,
                               jsval aValue)
{
  NS_ASSERTION(NS_IsMainThread(),
               "Should only be serializing on the main thread!");
  NS_ASSERTION(aCx, "A JSContext is required!");

  JSAutoRequest ar(aCx);

  JSStructuredCloneCallbacks callbacks = {
    nsnull,
    StructuredCloneWriteCallback,
    nsnull
  };

  JSAutoStructuredCloneBuffer& buffer = aCloneWriteInfo.mCloneBuffer;

  return buffer.write(aCx, aValue, &callbacks, &aCloneWriteInfo);
}

static inline PRUint32
SwapBytes(PRUint32 u)
{
#ifdef IS_BIG_ENDIAN
  return ((u & 0x000000ffU) << 24) |                                          
         ((u & 0x0000ff00U) << 8) |                                           
         ((u & 0x00ff0000U) >> 8) |                                           
         ((u & 0xff000000U) >> 24);
#else
  return u;
#endif
}

static inline double
SwapBytes(PRUint64 u)
{
#ifdef IS_BIG_ENDIAN
  return ((u & 0x00000000000000ffLLU) << 56) |
         ((u & 0x000000000000ff00LLU) << 40) |
         ((u & 0x0000000000ff0000LLU) << 24) |
         ((u & 0x00000000ff000000LLU) << 8) |
         ((u & 0x000000ff00000000LLU) >> 8) |
         ((u & 0x0000ff0000000000LLU) >> 24) |
         ((u & 0x00ff000000000000LLU) >> 40) |
         ((u & 0xff00000000000000LLU) >> 56);
#else
  return double(u);
#endif
}

static inline bool
StructuredCloneReadString(JSStructuredCloneReader* aReader,
                          nsCString& aString)
{
  PRUint32 length;
  if (!JS_ReadBytes(aReader, &length, sizeof(PRUint32))) {
    NS_WARNING("Failed to read length!");
    return false;
  }
  length = SwapBytes(length);

  if (!EnsureStringLength(aString, length)) {
    NS_WARNING("Out of memory?");
    return false;
  }
  char* buffer = aString.BeginWriting();

  if (!JS_ReadBytes(aReader, buffer, length)) {
    NS_WARNING("Failed to read type!");
    return false;
  }

  return true;
}

JSObject*
IDBObjectStore::StructuredCloneReadCallback(JSContext* aCx,
                                            JSStructuredCloneReader* aReader,
                                            uint32_t aTag,
                                            uint32_t aData,
                                            void* aClosure)
{
  if (aTag == SCTAG_DOM_BLOB || aTag == SCTAG_DOM_FILE) {
    StructuredCloneReadInfo* cloneReadInfo =
      reinterpret_cast<StructuredCloneReadInfo*>(aClosure);

    NS_ASSERTION(aData < cloneReadInfo->mFileInfos.Length(),
                 "Bad blob index!");

    nsRefPtr<FileInfo> fileInfo = cloneReadInfo->mFileInfos[aData];
    nsRefPtr<FileManager> fileManager = fileInfo->Manager();
    nsCOMPtr<nsIFile> directory = fileManager->GetDirectory();
    if (!directory) {
      return nsnull;
    }

    nsCOMPtr<nsIFile> nativeFile =
      fileManager->GetFileForId(directory, fileInfo->Id());
    if (!nativeFile) {
      return nsnull;
    }

    PRUint64 size;
    if (!JS_ReadBytes(aReader, &size, sizeof(PRUint64))) {
      NS_WARNING("Failed to read size!");
      return nsnull;
    }
    size = SwapBytes(size);

    nsCString type;
    if (!StructuredCloneReadString(aReader, type)) {
      return nsnull;
    }
    NS_ConvertUTF8toUTF16 convType(type);

    if (aTag == SCTAG_DOM_BLOB) {
      nsCOMPtr<nsIDOMBlob> blob = new nsDOMFileFile(convType, size,
                                                    nativeFile, fileInfo);

      jsval wrappedBlob;
      nsresult rv =
        nsContentUtils::WrapNative(aCx, JS_GetGlobalForScopeChain(aCx), blob,
                                   &NS_GET_IID(nsIDOMBlob), &wrappedBlob);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to wrap native!");
        return nsnull;
      }

      return JSVAL_TO_OBJECT(wrappedBlob);
    }

    nsCString name;
    if (!StructuredCloneReadString(aReader, name)) {
      return nsnull;
    }
    NS_ConvertUTF8toUTF16 convName(name);

    nsCOMPtr<nsIDOMFile> file = new nsDOMFileFile(convName, convType, size,
                                                  nativeFile, fileInfo);

    jsval wrappedFile;
    nsresult rv =
      nsContentUtils::WrapNative(aCx, JS_GetGlobalForScopeChain(aCx), file,
                                 &NS_GET_IID(nsIDOMFile), &wrappedFile);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to wrap native!");
      return nsnull;
    }

    return JSVAL_TO_OBJECT(wrappedFile);
  }

  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(aCx);

  if (runtimeCallbacks) {
    return runtimeCallbacks->read(aCx, aReader, aTag, aData, nsnull);
  }

  return nsnull;
}

JSBool
IDBObjectStore::StructuredCloneWriteCallback(JSContext* aCx,
                                             JSStructuredCloneWriter* aWriter,
                                             JSObject* aObj,
                                             void* aClosure)
{
  StructuredCloneWriteInfo* cloneWriteInfo =
    reinterpret_cast<StructuredCloneWriteInfo*>(aClosure);

  if (JS_GetClass(aObj) == &gDummyPropClass) {
    NS_ASSERTION(cloneWriteInfo->mOffsetToKeyProp == 0,
                 "We should not have been here before!");
    cloneWriteInfo->mOffsetToKeyProp = js_GetSCOffset(aWriter);

    PRUint64 value = 0;
    return JS_WriteBytes(aWriter, &value, sizeof(value));
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(aCx, aObj, getter_AddRefs(wrappedNative));

  if (wrappedNative) {
    nsISupports* supports = wrappedNative->Native();

    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(supports);
    if (blob) {
      nsCOMPtr<nsIDOMFile> file = do_QueryInterface(blob);

      PRUint64 size;
      if (NS_FAILED(blob->GetSize(&size))) {
        return false;
      }
      size = SwapBytes(size);

      nsString type;
      if (NS_FAILED(blob->GetType(type))) {
        return false;
      }
      NS_ConvertUTF16toUTF8 convType(type);
      PRUint32 convTypeLength = SwapBytes(convType.Length());

      if (!JS_WriteUint32Pair(aWriter, file ? SCTAG_DOM_FILE : SCTAG_DOM_BLOB,
                              cloneWriteInfo->mBlobs.Length()) ||
          !JS_WriteBytes(aWriter, &size, sizeof(PRUint64)) ||
          !JS_WriteBytes(aWriter, &convTypeLength, sizeof(PRUint32)) ||
          !JS_WriteBytes(aWriter, convType.get(), convType.Length())) {
        return false;
      }

      if (file) {
        nsString name;
        if (NS_FAILED(file->GetName(name))) {
          return false;
        }
        NS_ConvertUTF16toUTF8 convName(name);
        PRUint32 convNameLength = SwapBytes(convName.Length());

        if (!JS_WriteBytes(aWriter, &convNameLength, sizeof(PRUint32)) ||
            !JS_WriteBytes(aWriter, convName.get(), convName.Length())) {
          return false;
        }
      }

      cloneWriteInfo->mBlobs.AppendElement(blob);

      return true;
    }
  }

  // try using the runtime callbacks
  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(aCx);
  if (runtimeCallbacks) {
    return runtimeCallbacks->write(aCx, aWriter, aObj, nsnull);
  }

  return false;
}

nsresult
IDBObjectStore::ConvertFileIdsToArray(const nsAString& aFileIds,
                                      nsTArray<PRInt64>& aResult)
{
  nsCharSeparatedTokenizerTemplate<IgnoreWhitespace> tokenizer(aFileIds, ' ');

  while (tokenizer.hasMoreTokens()) {
    nsString token(tokenizer.nextToken());

    NS_ASSERTION(!token.IsEmpty(), "Should be a valid id!");

    nsresult rv;
    PRInt32 id = token.ToInteger(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    PRInt64* element = aResult.AppendElement();
    *element = id;
  }

  return NS_OK;
}

IDBObjectStore::IDBObjectStore()
: mId(LL_MININT),
  mAutoIncrement(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBObjectStore::~IDBObjectStore()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

nsresult
IDBObjectStore::GetAddInfo(JSContext* aCx,
                           jsval aValue,
                           jsval aKeyVal,
                           StructuredCloneWriteInfo& aCloneWriteInfo,
                           Key& aKey,
                           nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
  nsresult rv;

  // Return DATA_ERR if a key was passed in and this objectStore uses inline
  // keys.
  if (!JSVAL_IS_VOID(aKeyVal) && HasKeyPath()) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  JSAutoRequest ar(aCx);

  if (!HasKeyPath()) {
    // Out-of-line keys must be passed in.
    rv = aKey.SetFromJSVal(aCx, aKeyVal);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (!mAutoIncrement) {
    // Inline keys live on the object. Make sure that the value passed in is an
    // object.
    if (UsesKeyPathArray()) {
      rv = GetKeyFromValue(aCx, aValue, mKeyPathArray, aKey);
    }
    else {
      rv = GetKeyFromValue(aCx, aValue, mKeyPath, aKey);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Return DATA_ERR if no key was specified this isn't an autoIncrement
  // objectStore.
  if (aKey.IsUnset() && !mAutoIncrement) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  // Figure out indexes and the index values to update here.
  PRUint32 count = mInfo->indexes.Length();
  aUpdateInfoArray.SetCapacity(count); // Pretty good estimate
  for (PRUint32 indexesIndex = 0; indexesIndex < count; indexesIndex++) {
    const IndexInfo& indexInfo = mInfo->indexes[indexesIndex];

    rv = AppendIndexUpdateInfo(indexInfo.id, indexInfo.keyPath,
                               indexInfo.keyPathArray, indexInfo.unique,
                               indexInfo.multiEntry, aCx, aValue,
                               aUpdateInfoArray);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsString targetObjectPropName;
  JSObject* targetObject = nsnull;

  rv = NS_OK;
  if (mAutoIncrement && HasKeyPath()) {
    NS_ASSERTION(aKey.IsUnset(), "Shouldn't have gotten the key yet!");

    if (JSVAL_IS_PRIMITIVE(aValue)) {
      return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
    }

    KeyPathTokenizer tokenizer(mKeyPath, '.');
    NS_ASSERTION(tokenizer.hasMoreTokens(),
                 "Shouldn't have empty keypath and autoincrement");

    JSObject* obj = JSVAL_TO_OBJECT(aValue);
    while (tokenizer.hasMoreTokens()) {
      const nsDependentSubstring& token = tokenizer.nextToken();
  
      NS_ASSERTION(!token.IsEmpty(), "Should be a valid keypath");
  
      const jschar* keyPathChars = token.BeginReading();
      const size_t keyPathLen = token.Length();
  
      JSBool hasProp;
      if (!targetObject) {
        // We're still walking the chain of existing objects

        JSBool ok = JS_HasUCProperty(aCx, obj, keyPathChars, keyPathLen,
                                     &hasProp);
        NS_ENSURE_TRUE(ok, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

        if (hasProp) {
          // Get if the property exists...
          jsval intermediate;
          JSBool ok = JS_GetUCProperty(aCx, obj, keyPathChars, keyPathLen,
                                       &intermediate);
          NS_ENSURE_TRUE(ok, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

          if (tokenizer.hasMoreTokens()) {
            // ...and walk to it if there are more steps...
            if (JSVAL_IS_PRIMITIVE(intermediate)) {
              return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
            }
            obj = JSVAL_TO_OBJECT(intermediate);
          }
          else {
            // ...otherwise use it as key
            aKey.SetFromJSVal(aCx, intermediate);
            if (aKey.IsUnset()) {
              return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
            }
          }
        }
        else {
          // If the property doesn't exist, fall into below path of starting
          // to define properties
          targetObject = obj;
          targetObjectPropName = token;
        }
      }

      if (targetObject) {
        // We have started inserting new objects or are about to just insert
        // the first one.
        if (tokenizer.hasMoreTokens()) {
          // If we're not at the end, we need to add a dummy object to the chain.
          JSObject* dummy = JS_NewObject(aCx, nsnull, nsnull, nsnull);
          if (!dummy) {
            rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
            break;
          }
  
          if (!JS_DefineUCProperty(aCx, obj, token.BeginReading(),
                                   token.Length(),
                                   OBJECT_TO_JSVAL(dummy), nsnull, nsnull,
                                   JSPROP_ENUMERATE)) {
            rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
            break;
          }
  
          obj = dummy;
        }
        else {
          JSObject* dummy = JS_NewObject(aCx, &gDummyPropClass, nsnull, nsnull);
          if (!dummy) {
            rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
            break;
          }
  
          if (!JS_DefineUCProperty(aCx, obj, token.BeginReading(),
                                   token.Length(), OBJECT_TO_JSVAL(dummy),
                                   nsnull, nsnull, JSPROP_ENUMERATE)) {
            rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
            break;
          }
        }
      }
    }
  }

  aCloneWriteInfo.mOffsetToKeyProp = 0;

  // We guard on rv being a success because we need to run the property
  // deletion code below even if we should not be serializing the value
  if (NS_SUCCEEDED(rv) && 
      !IDBObjectStore::SerializeValue(aCx, aCloneWriteInfo, aValue)) {
    rv = NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  if (targetObject) {
    // If this fails, we lose, and the web page sees a magical property
    // appear on the object :-(
    jsval succeeded;
    if (!JS_DeleteUCProperty2(aCx, targetObject,
                              targetObjectPropName.get(),
                              targetObjectPropName.Length(), &succeeded)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
    NS_ASSERTION(JSVAL_IS_BOOLEAN(succeeded), "Wtf?");
    NS_ENSURE_TRUE(JSVAL_TO_BOOLEAN(succeeded),
                   NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  return rv;
}

nsresult
IDBObjectStore::AddOrPut(const jsval& aValue,
                         const jsval& aKey,
                         JSContext* aCx,
                         PRUint8 aOptionalArgCount,
                         nsIIDBRequest** _retval,
                         bool aOverwrite)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  if (!IsWriteAllowed()) {
    return NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR;
  }

  jsval keyval = (aOptionalArgCount >= 1) ? aKey : JSVAL_VOID;

  StructuredCloneWriteInfo cloneWriteInfo;
  Key key;
  nsTArray<IndexUpdateInfo> updateInfo;

  nsresult rv = GetAddInfo(aCx, aValue, keyval, cloneWriteInfo, key,
                           updateInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<AddHelper> helper =
    new AddHelper(mTransaction, request, this, cloneWriteInfo, key, aOverwrite,
                  updateInfo);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBObjectStore)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBObjectStore)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mTransaction,
                                                       nsIDOMEventTarget)

  for (PRUint32 i = 0; i < tmp->mCreatedIndexes.Length(); i++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCreatedIndexes[i]");
    cb.NoteXPCOMChild(static_cast<nsIIDBIndex*>(tmp->mCreatedIndexes[i].get()));
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBObjectStore)
  // Don't unlink mTransaction!

  tmp->mCreatedIndexes.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBObjectStore)
  NS_INTERFACE_MAP_ENTRY(nsIIDBObjectStore)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBObjectStore)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBObjectStore)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBObjectStore)

DOMCI_DATA(IDBObjectStore, IDBObjectStore)

NS_IMETHODIMP
IDBObjectStore::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::GetKeyPath(JSContext* aCx,
                           jsval* aVal)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (UsesKeyPathArray()) {
    JSObject* array = JS_NewArrayObject(aCx, mKeyPathArray.Length(), nsnull);
    if (!array) {
      NS_WARNING("Failed to make array!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    for (PRUint32 i = 0; i < mKeyPathArray.Length(); ++i) {
      jsval val;
      nsString tmp(mKeyPathArray[i]);
      if (!xpc::StringToJsval(aCx, tmp, &val)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      if (!JS_SetElement(aCx, array, i, &val)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    *aVal = OBJECT_TO_JSVAL(array);
  }
  else {
    nsString tmp(mKeyPath);
    if (!xpc::StringToJsval(aCx, tmp, aVal)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::GetTransaction(nsIIDBTransaction** aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIIDBTransaction> transaction(mTransaction);
  transaction.forget(aTransaction);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::GetAutoIncrement(bool* aAutoIncrement)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aAutoIncrement = mAutoIncrement;
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::GetIndexNames(nsIDOMDOMStringList** aIndexNames)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());

  PRUint32 count = mInfo->indexes.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(mInfo->indexes[index].name),
                   NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  list.forget(aIndexNames);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Get(const jsval& aKey,
                    JSContext* aCx,
                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  nsresult rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!keyRange) {
    // Must specify a key or keyRange for get().
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetHelper> helper =
    new GetHelper(mTransaction, request, this, keyRange);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::GetAll(const jsval& aKey,
                       PRUint32 aLimit,
                       JSContext* aCx,
                       PRUint8 aOptionalArgCount,
                       nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsresult rv;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aOptionalArgCount < 2 || aLimit == 0) {
    aLimit = PR_UINT32_MAX;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(mTransaction, request, this, keyRange, aLimit);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Add(const jsval& aValue,
                    const jsval& aKey,
                    JSContext* aCx,
                    PRUint8 aOptionalArgCount,
                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return AddOrPut(aValue, aKey, aCx, aOptionalArgCount, _retval, false);
}

NS_IMETHODIMP
IDBObjectStore::Put(const jsval& aValue,
                    const jsval& aKey,
                    JSContext* aCx,
                    PRUint8 aOptionalArgCount,
                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return AddOrPut(aValue, aKey, aCx, aOptionalArgCount, _retval, true);
}

NS_IMETHODIMP
IDBObjectStore::Delete(const jsval& aKey,
                       JSContext* aCx,
                       nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  if (!IsWriteAllowed()) {
    return NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  nsresult rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!keyRange) {
    // Must specify a key or keyRange for delete().
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<DeleteHelper> helper =
    new DeleteHelper(mTransaction, request, this, keyRange);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Clear(nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  if (!IsWriteAllowed()) {
    return NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<ClearHelper> helper(new ClearHelper(mTransaction, request, this));

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::OpenCursor(const jsval& aKey,
                           const nsAString& aDirection,
                           JSContext* aCx,
                           PRUint8 aOptionalArgCount,
                           nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsresult rv;

  IDBCursor::Direction direction = IDBCursor::NEXT;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aOptionalArgCount >= 2) {
      rv = IDBCursor::ParseDirection(aDirection, &direction);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(mTransaction, request, this, keyRange, direction);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::CreateIndex(const nsAString& aName,
                            const jsval& aKeyPath,
                            const jsval& aOptions,
                            JSContext* aCx,
                            nsIIDBIndex** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  // Get KeyPath
  nsString keyPath;
  nsTArray<nsString> keyPathArray;

  // See if this is a JS array.
  if (!JSVAL_IS_PRIMITIVE(aKeyPath) &&
      JS_IsArrayObject(aCx, JSVAL_TO_OBJECT(aKeyPath))) {

    JSObject* obj = JSVAL_TO_OBJECT(aKeyPath);

    uint32_t length;
    if (!JS_GetArrayLength(aCx, obj, &length)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (!length) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    keyPathArray.SetCapacity(length);

    for (uint32_t index = 0; index < length; index++) {
      jsval val;
      JSString* jsstr;
      nsDependentJSString str;
      if (!JS_GetElement(aCx, obj, index, &val) ||
          !(jsstr = JS_ValueToString(aCx, val)) ||
          !str.init(aCx, jsstr)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      if (!IsValidKeyPath(aCx, str)) {
        return NS_ERROR_DOM_SYNTAX_ERR;
      }

      keyPathArray.AppendElement(str);
    }

    NS_ASSERTION(!keyPathArray.IsEmpty(), "This shouldn't have happened!");
  }
  else {
    JSString* jsstr;
    nsDependentJSString str;
    if (!(jsstr = JS_ValueToString(aCx, aKeyPath)) ||
        !str.init(aCx, jsstr)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (!IsValidKeyPath(aCx, str)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    keyPath = str;
  }

  // Check name and current mode
  IDBTransaction* transaction = AsyncConnectionHelper::GetCurrentTransaction();

  if (!transaction ||
      transaction != mTransaction ||
      mTransaction->GetMode() != IDBTransaction::VERSION_CHANGE) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  bool found = false;
  PRUint32 indexCount = mInfo->indexes.Length();
  for (PRUint32 index = 0; index < indexCount; index++) {
    if (mInfo->indexes[index].name == aName) {
      found = true;
      break;
    }
  }

  if (found) {
    return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
  }

  NS_ASSERTION(mTransaction->IsOpen(), "Impossible!");

  mozilla::dom::IDBIndexParameters params;

  // Get optional arguments.
  if (!JSVAL_IS_VOID(aOptions) && !JSVAL_IS_NULL(aOptions)) {
    nsresult rv = params.Init(aCx, &aOptions);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (params.multiEntry && !keyPathArray.IsEmpty()) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  DatabaseInfo* databaseInfo = mTransaction->DBInfo();

  IndexInfo* indexInfo = mInfo->indexes.AppendElement();
  indexInfo->id = databaseInfo->nextIndexId++;
  indexInfo->name = aName;
  indexInfo->keyPath = keyPath;
  indexInfo->keyPathArray.SwapElements(keyPathArray);
  indexInfo->unique = params.unique;
  indexInfo->multiEntry = params.multiEntry;

  // Don't leave this in the list if we fail below!
  AutoRemoveIndex autoRemove(mInfo, aName);

#ifdef DEBUG
  for (PRUint32 index = 0; index < mCreatedIndexes.Length(); index++) {
    if (mCreatedIndexes[index]->Name() == aName) {
      NS_ERROR("Already created this one!");
    }
  }
#endif

  nsRefPtr<IDBIndex> index(IDBIndex::Create(this, indexInfo));

  if (!mCreatedIndexes.AppendElement(index)) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsRefPtr<CreateIndexHelper> helper =
    new CreateIndexHelper(mTransaction, index);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  autoRemove.forget();

  index.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Index(const nsAString& aName,
                      nsIIDBIndex** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  IndexInfo* indexInfo = nsnull;
  PRUint32 indexCount = mInfo->indexes.Length();
  for (PRUint32 index = 0; index < indexCount; index++) {
    if (mInfo->indexes[index].name == aName) {
      indexInfo = &(mInfo->indexes[index]);
      break;
    }
  }

  if (!indexInfo) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR;
  }

  nsRefPtr<IDBIndex> retval;
  for (PRUint32 i = 0; i < mCreatedIndexes.Length(); i++) {
    nsRefPtr<IDBIndex>& index = mCreatedIndexes[i];
    if (index->Name() == aName) {
      retval = index;
      break;
    }
  }

  if (!retval) {
    retval = IDBIndex::Create(this, indexInfo);
    NS_ENSURE_TRUE(retval, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (!mCreatedIndexes.AppendElement(retval)) {
      NS_WARNING("Out of memory!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  retval.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::DeleteIndex(const nsAString& aName)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = AsyncConnectionHelper::GetCurrentTransaction();

  if (!transaction ||
      transaction != mTransaction ||
      mTransaction->GetMode() != IDBTransaction::VERSION_CHANGE) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  NS_ASSERTION(mTransaction->IsOpen(), "Impossible!");

  PRUint32 index = 0;
  for (; index < mInfo->indexes.Length(); index++) {
    if (mInfo->indexes[index].name == aName) {
      break;
    }
  }

  if (index == mInfo->indexes.Length()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR;
  }

  nsRefPtr<DeleteIndexHelper> helper =
    new DeleteIndexHelper(mTransaction, aName, this);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mInfo->indexes.RemoveElementAt(index);

  for (PRUint32 i = 0; i < mCreatedIndexes.Length(); i++) {
    if (mCreatedIndexes[i]->Name() == aName) {
      mCreatedIndexes.RemoveElementAt(i);
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Count(const jsval& aKey,
                      JSContext* aCx,
                      PRUint8 aOptionalArgCount,
                      nsIIDBRequest** _retval)
{
  if (!mTransaction->IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR;
  }

  nsresult rv;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aOptionalArgCount) {
    rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<CountHelper> helper =
    new CountHelper(mTransaction, request, this, keyRange);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

inline nsresult
CopyData(nsIInputStream* aStream, quota_FILE* aFile)
{
  do {
    char copyBuffer[FILE_COPY_BUFFER_SIZE];

    PRUint32 numRead;
    nsresult rv = aStream->Read(copyBuffer, FILE_COPY_BUFFER_SIZE, &numRead);
    NS_ENSURE_SUCCESS(rv, rv);

    if (numRead <= 0) {
      break;
    }

    size_t numWrite = sqlite3_quota_fwrite(copyBuffer, 1, numRead, aFile);
    NS_ENSURE_TRUE(numWrite == numRead, NS_ERROR_FAILURE);
  } while (true);

  // Flush and sync
  NS_ENSURE_TRUE(sqlite3_quota_fflush(aFile, 1) == 0,
                 NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
AddHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsresult rv;
  bool keyUnset = mKey.IsUnset();
  PRInt64 osid = mObjectStore->Id();
  const nsString& keyPath = mObjectStore->KeyPath();

  // The "|| keyUnset" here is mostly a debugging tool. If a key isn't
  // specified we should never have a collision and so it shouldn't matter
  // if we allow overwrite or not. By not allowing overwrite we raise
  // detectable errors rather than corrupting data
  nsCOMPtr<mozIStorageStatement> stmt = !mOverwrite || keyUnset ?
    mTransaction->GetCachedStatement(
      "INSERT INTO object_data (object_store_id, key_value, data, file_ids) "
      "VALUES (:osid, :key_value, :data, :file_ids)") :
    mTransaction->GetCachedStatement(
      "INSERT OR REPLACE INTO object_data (object_store_id, key_value, data, file_ids) "
      "VALUES (:osid, :key_value, :data, :file_ids)");
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), osid);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(!keyUnset || mObjectStore->IsAutoIncrement(),
               "Should have key unless autoincrement");

  PRInt64 autoIncrementNum = 0;

  if (mObjectStore->IsAutoIncrement()) {
    if (keyUnset) {
      autoIncrementNum = mObjectStore->Info()->nextAutoIncrementId;
      if (autoIncrementNum > (1LL << 53)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
      mKey.SetFromInteger(autoIncrementNum);
    }
    else if (mKey.IsFloat() &&
             mKey.ToFloat() >= mObjectStore->Info()->nextAutoIncrementId) {
      autoIncrementNum = floor(mKey.ToFloat());
    }

    if (keyUnset && !keyPath.IsEmpty()) {
      // Special case where someone put an object into an autoIncrement'ing
      // objectStore with no key in its keyPath set. We needed to figure out
      // which row id we would get above before we could set that properly.

      // This is a duplicate of the js engine's byte munging here
      union {
        double d;
        PRUint64 u;
      } pun;
    
      pun.d = SwapBytes(static_cast<PRUint64>(autoIncrementNum));

      JSAutoStructuredCloneBuffer& buffer = mCloneWriteInfo.mCloneBuffer;
      PRUint64 offsetToKeyProp = mCloneWriteInfo.mOffsetToKeyProp;

      memcpy((char*)buffer.data() + offsetToKeyProp, &pun.u, sizeof(PRUint64));
    }
  }

  mKey.BindToStatement(stmt, NS_LITERAL_CSTRING("key_value"));

  // Compress the bytes before adding into the database.
  const char* uncompressed =
    reinterpret_cast<const char*>(mCloneWriteInfo.mCloneBuffer.data());
  size_t uncompressedLength = mCloneWriteInfo.mCloneBuffer.nbytes();

  size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);
  // This will hold our compressed data until the end of the method. The
  // BindBlobByName function will copy it.
  nsAutoArrayPtr<char> compressed(new char[compressedLength]);

  snappy::RawCompress(uncompressed, uncompressedLength, compressed.get(),
                      &compressedLength);

  const PRUint8* dataBuffer = reinterpret_cast<const PRUint8*>(compressed.get());
  size_t dataBufferLength = compressedLength;

  rv = stmt->BindBlobByName(NS_LITERAL_CSTRING("data"), dataBuffer,
                            dataBufferLength);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  // Handle blobs
  nsRefPtr<FileManager> fileManager = mDatabase->Manager();
  nsCOMPtr<nsIFile> directory = fileManager->GetDirectory();
  NS_ENSURE_TRUE(directory, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsAutoString fileIds;

  for (PRUint32 index = 0; index < mCloneWriteInfo.mBlobs.Length(); index++) {
    nsCOMPtr<nsIDOMBlob>& domBlob = mCloneWriteInfo.mBlobs[index];

    PRInt64 id = -1;

    // Check if it is a blob created from this db or the blob was already
    // stored in this db
    nsRefPtr<FileInfo> fileInfo = domBlob->GetFileInfo(fileManager);
    if (fileInfo) {
      id = fileInfo->Id();
    }

    if (id == -1) {
      fileInfo = fileManager->GetNewFileInfo();
      NS_ENSURE_TRUE(fileInfo, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      id = fileInfo->Id();

      mTransaction->OnNewFileInfo(fileInfo);

      // Copy it
      nsCOMPtr<nsIInputStream> inputStream;
      rv = domBlob->GetInternalStream(getter_AddRefs(inputStream));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      nsCOMPtr<nsIFile> nativeFile = fileManager->GetFileForId(directory, id);
      NS_ENSURE_TRUE(nativeFile, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      nsString nativeFilePath;
      rv = nativeFile->GetPath(nativeFilePath);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      quota_FILE* file =
        sqlite3_quota_fopen(NS_ConvertUTF16toUTF8(nativeFilePath).get(), "wb");
      NS_ENSURE_TRUE(file, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      rv = CopyData(inputStream, file);

      NS_ENSURE_TRUE(sqlite3_quota_fclose(file) == 0,
                     NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      domBlob->AddFileInfo(fileInfo);
    }

    if (index) {
      fileIds.Append(NS_LITERAL_STRING(" "));
    }
    fileIds.AppendInt(id);
  }

  if (fileIds.IsEmpty()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("file_ids"));
  }
  else {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("file_ids"), fileIds);
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->Execute();
  if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
    NS_ASSERTION(!keyUnset, "Generated key had a collision!?");
    return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  PRInt64 objectDataId;
  rv = aConnection->GetLastInsertRowID(&objectDataId);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  // Update our indexes if needed.
  if (mOverwrite || !mIndexUpdateInfo.IsEmpty()) {
    rv = IDBObjectStore::UpdateIndexes(mTransaction, osid, mKey, mOverwrite,
                                       objectDataId, mIndexUpdateInfo);
    if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
      return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  if (autoIncrementNum) {
    mObjectStore->Info()->nextAutoIncrementId = autoIncrementNum + 1;
  }

  return NS_OK;
}

nsresult
AddHelper::GetSuccessResult(JSContext* aCx,
                            jsval* aVal)
{
  NS_ASSERTION(!mKey.IsUnset(), "Badness!");

  mCloneWriteInfo.mCloneBuffer.clear();

  return mKey.ToJSVal(aCx, aVal);
}

nsresult
GetHelper::DoDatabaseWork(mozIStorageConnection* /* aConnection */)
{
  NS_ASSERTION(mKeyRange, "Must have a key range here!");

  nsCString keyRangeClause;
  mKeyRange->GetBindingClause(NS_LITERAL_CSTRING("key_value"), keyRangeClause);

  NS_ASSERTION(!keyRangeClause.IsEmpty(), "Huh?!");

  nsCString query = NS_LITERAL_CSTRING("SELECT data, file_ids FROM object_data "
                                       "WHERE object_store_id = :osid") +
                    keyRangeClause + NS_LITERAL_CSTRING(" LIMIT 1");

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = mKeyRange->BindToStatement(stmt);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (hasResult) {
    rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(stmt, 0, 1,
      mDatabase->Manager(), mCloneReadInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
GetHelper::GetSuccessResult(JSContext* aCx,
                            jsval* aVal)
{
  bool result = IDBObjectStore::DeserializeValue(aCx, mCloneReadInfo, aVal);

  mCloneReadInfo.mCloneBuffer.clear();

  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult
DeleteHelper::DoDatabaseWork(mozIStorageConnection* /*aConnection */)
{
  NS_ASSERTION(mKeyRange, "Must have a key range here!");

  nsCString keyRangeClause;
  mKeyRange->GetBindingClause(NS_LITERAL_CSTRING("key_value"), keyRangeClause);

  NS_ASSERTION(!keyRangeClause.IsEmpty(), "Huh?!");

  nsCString query = NS_LITERAL_CSTRING("DELETE FROM object_data "
                                       "WHERE object_store_id = :osid") +
                    keyRangeClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                                      mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = mKeyRange->BindToStatement(stmt);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
DeleteHelper::GetSuccessResult(JSContext* aCx,
                               jsval* aVal)
{
  *aVal = JSVAL_VOID;
  return NS_OK;
}

nsresult
ClearHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM object_data "
                         "WHERE object_store_id = :osid"));
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                                      mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
OpenCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_NAMED_LITERAL_CSTRING(keyValue, "key_value");

  nsCString keyRangeClause;
  if (mKeyRange) {
    mKeyRange->GetBindingClause(keyValue, keyRangeClause);
  }

  nsCAutoString directionClause;
  switch (mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AssignLiteral(" ORDER BY key_value ASC");
      break;

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE:
      directionClause.AssignLiteral(" ORDER BY key_value DESC");
      break;

    default:
      NS_NOTREACHED("Unknown direction type!");
  }

  nsCString firstQuery = NS_LITERAL_CSTRING("SELECT key_value, data, file_ids "
                                            "FROM object_data "
                                            "WHERE object_store_id = :id") +
                         keyRangeClause + directionClause +
                         NS_LITERAL_CSTRING(" LIMIT 1");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(firstQuery);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                      mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    rv = mKeyRange->BindToStatement(stmt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!hasResult) {
    mKey.Unset();
    return NS_OK;
  }

  rv = mKey.SetFromStatement(stmt, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(stmt, 1, 2,
    mDatabase->Manager(), mCloneReadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now we need to make the query to get the next match.
  keyRangeClause.Truncate();
  nsCAutoString continueToKeyRangeClause;

  NS_NAMED_LITERAL_CSTRING(currentKey, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      AppendConditionClause(keyValue, currentKey, false, false,
                            keyRangeClause);
      AppendConditionClause(keyValue, currentKey, false, true,
                            continueToKeyRangeClause);
      if (mKeyRange && !mKeyRange->Upper().IsUnset()) {
        AppendConditionClause(keyValue, rangeKey, true,
                              !mKeyRange->IsUpperOpen(), keyRangeClause);
        AppendConditionClause(keyValue, rangeKey, true,
                              !mKeyRange->IsUpperOpen(),
                              continueToKeyRangeClause);
        mRangeKey = mKeyRange->Upper();
      }
      break;

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE:
      AppendConditionClause(keyValue, currentKey, true, false, keyRangeClause);
      AppendConditionClause(keyValue, currentKey, true, true,
                           continueToKeyRangeClause);
      if (mKeyRange && !mKeyRange->Lower().IsUnset()) {
        AppendConditionClause(keyValue, rangeKey, false,
                              !mKeyRange->IsLowerOpen(), keyRangeClause);
        AppendConditionClause(keyValue, rangeKey, false,
                              !mKeyRange->IsLowerOpen(),
                              continueToKeyRangeClause);
        mRangeKey = mKeyRange->Lower();
      }
      break;

    default:
      NS_NOTREACHED("Unknown direction type!");
  }

  NS_NAMED_LITERAL_CSTRING(queryStart, "SELECT key_value, data, file_ids "
                                       "FROM object_data "
                                       "WHERE object_store_id = :id");

  mContinueQuery = queryStart + keyRangeClause + directionClause +
                   NS_LITERAL_CSTRING(" LIMIT ");

  mContinueToQuery = queryStart + continueToKeyRangeClause + directionClause +
                     NS_LITERAL_CSTRING(" LIMIT ");

  return NS_OK;
}

nsresult
OpenCursorHelper::GetSuccessResult(JSContext* aCx,
                                   jsval* aVal)
{
  if (mKey.IsUnset()) {
    *aVal = JSVAL_VOID;
    return NS_OK;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mObjectStore, mDirection,
                      mRangeKey, mContinueQuery, mContinueToQuery, mKey,
                      mCloneReadInfo);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(!mCloneReadInfo.mCloneBuffer.data(), "Should have swapped!");

  return WrapNative(aCx, cursor, aVal);
}

class ThreadLocalJSRuntime
{
  JSRuntime* mRuntime;
  JSContext* mContext;
  JSObject* mGlobal;

  static JSClass sGlobalClass;
  static const unsigned sRuntimeHeapSize = 256 * 1024;  // should be enough for anyone

  ThreadLocalJSRuntime()
  : mRuntime(NULL), mContext(NULL), mGlobal(NULL)
  {
      MOZ_COUNT_CTOR(ThreadLocalJSRuntime);
  }

  nsresult Init()
  {
    mRuntime = JS_NewRuntime(sRuntimeHeapSize);
    NS_ENSURE_TRUE(mRuntime, NS_ERROR_OUT_OF_MEMORY);

    mContext = JS_NewContext(mRuntime, 0);
    NS_ENSURE_TRUE(mContext, NS_ERROR_OUT_OF_MEMORY);

    JSAutoRequest ar(mContext);

    mGlobal = JS_NewCompartmentAndGlobalObject(mContext, &sGlobalClass, NULL);
    NS_ENSURE_TRUE(mGlobal, NS_ERROR_OUT_OF_MEMORY);

    JS_SetGlobalObject(mContext, mGlobal);
    return NS_OK;
  }

 public:
  static ThreadLocalJSRuntime *Create()
  {
    ThreadLocalJSRuntime *entry = new ThreadLocalJSRuntime();
    NS_ENSURE_TRUE(entry, nsnull);

    if (NS_FAILED(entry->Init())) {
      delete entry;
      return nsnull;
    }

    return entry;
  }

  JSContext *Context() const
  {
    return mContext;
  }

  ~ThreadLocalJSRuntime()
  {
    MOZ_COUNT_DTOR(ThreadLocalJSRuntime);

    if (mContext) {
      JS_DestroyContext(mContext);
    }

    if (mRuntime) {
      JS_DestroyRuntime(mRuntime);
    }
  }
};

JSClass ThreadLocalJSRuntime::sGlobalClass = {
  "IndexedDBTransactionThreadGlobal",
  JSCLASS_GLOBAL_FLAGS,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

CreateIndexHelper::CreateIndexHelper(IDBTransaction* aTransaction,
                                     IDBIndex* aIndex)
  : AsyncConnectionHelper(aTransaction, nsnull), mIndex(aIndex)
{
  if (sTLSIndex == BAD_TLS_INDEX) {
    PR_NewThreadPrivateIndex(&sTLSIndex, DestroyTLSEntry);
  }
}

void
CreateIndexHelper::DestroyTLSEntry(void* aPtr)
{
  delete reinterpret_cast<ThreadLocalJSRuntime *>(aPtr);
}

nsresult
CreateIndexHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  // Insert the data into the database.
  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(
    "INSERT INTO object_store_index (id, name, key_path, unique_index, "
      "multientry, object_store_id) "
    "VALUES (:id, :name, :key_path, :unique, :multientry, :osid)"
  );
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mIndex->Name());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mIndex->UsesKeyPathArray()) {
    // We use a comma in the beginning to indicate that it's an array of
    // key paths. This is to be able to tell a string-keypath from an
    // array-keypath which contains only one item.
    // It also makes serializing easier :-)
    nsAutoString keyPath;
    const nsTArray<nsString>& keyPaths = mIndex->KeyPathArray();
    for (PRUint32 i = 0; i < keyPaths.Length(); ++i) {
      keyPath.Append(NS_LITERAL_STRING(",") + keyPaths[i]);
    }
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"),
                                keyPath);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"),
                                mIndex->KeyPath());
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("unique"),
                             mIndex->IsUnique() ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("multientry"),
                             mIndex->IsMultiEntry() ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                             mIndex->ObjectStore()->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
  }

#ifdef DEBUG
  {
    PRInt64 id;
    aConnection->GetLastInsertRowID(&id);
    NS_ASSERTION(mIndex->Id() == id, "Bad index id!");
  }
#endif

  // Now we need to populate the index with data from the object store.
  rv = InsertDataFromObjectStore(aConnection);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

nsresult
CreateIndexHelper::InsertDataFromObjectStore(mozIStorageConnection* aConnection)
{
  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT id, data, file_ids, key_value FROM object_data "
                         "WHERE object_store_id = :osid"));
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                                      mIndex->ObjectStore()->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ENSURE_TRUE(sTLSIndex != BAD_TLS_INDEX, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  if (!hasResult) {
    // Bail early if we have no data to avoid creating the below runtime
    return NS_OK;
  }

  ThreadLocalJSRuntime* tlsEntry =
    reinterpret_cast<ThreadLocalJSRuntime*>(PR_GetThreadPrivate(sTLSIndex));

  if (!tlsEntry) {
    tlsEntry = ThreadLocalJSRuntime::Create();
    NS_ENSURE_TRUE(tlsEntry, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    PR_SetThreadPrivate(sTLSIndex, tlsEntry);
  }

  JSContext* cx = tlsEntry->Context();
  JSAutoRequest ar(cx);

  do {
    StructuredCloneReadInfo cloneReadInfo;
    rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(stmt, 1, 2,
      mDatabase->Manager(), cloneReadInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    JSAutoStructuredCloneBuffer& buffer = cloneReadInfo.mCloneBuffer;

    JSStructuredCloneCallbacks callbacks = {
      IDBObjectStore::StructuredCloneReadCallback,
      nsnull,
      nsnull
    };

    jsval clone;
    if (!buffer.read(cx, &clone, &callbacks, &cloneReadInfo)) {
      NS_WARNING("Failed to deserialize structured clone data!");
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }

    nsTArray<IndexUpdateInfo> updateInfo;
    rv = IDBObjectStore::AppendIndexUpdateInfo(mIndex->Id(),
                                               mIndex->KeyPath(),
                                               mIndex->KeyPathArray(),
                                               mIndex->IsUnique(),
                                               mIndex->IsMultiEntry(),
                                               tlsEntry->Context(),
                                               clone, updateInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt64 objectDataID = stmt->AsInt64(0);

    Key key;
    rv = key.SetFromStatement(stmt, 3);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = IDBObjectStore::UpdateIndexes(mTransaction, mIndex->Id(),
                                       key, false, objectDataID, updateInfo);
    NS_ENSURE_SUCCESS(rv, rv);

  } while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&hasResult)) && hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
DeleteIndexHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(
      "DELETE FROM object_store_index "
      "WHERE name = :name "
    );
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR;
  }

  return NS_OK;
}

nsresult
GetAllHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_NAMED_LITERAL_CSTRING(lowerKeyName, "lower_key");
  NS_NAMED_LITERAL_CSTRING(upperKeyName, "upper_key");

  nsCAutoString keyRangeClause;
  if (mKeyRange) {
    if (!mKeyRange->Lower().IsUnset()) {
      keyRangeClause = NS_LITERAL_CSTRING(" AND key_value");
      if (mKeyRange->IsLowerOpen()) {
        keyRangeClause.AppendLiteral(" > :");
      }
      else {
        keyRangeClause.AppendLiteral(" >= :");
      }
      keyRangeClause.Append(lowerKeyName);
    }

    if (!mKeyRange->Upper().IsUnset()) {
      keyRangeClause += NS_LITERAL_CSTRING(" AND key_value");
      if (mKeyRange->IsUpperOpen()) {
        keyRangeClause.AppendLiteral(" < :");
      }
      else {
        keyRangeClause.AppendLiteral(" <= :");
      }
      keyRangeClause.Append(upperKeyName);
    }
  }

  nsCAutoString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT data, file_ids FROM object_data "
                                       "WHERE object_store_id = :osid") +
                    keyRangeClause +
                    NS_LITERAL_CSTRING(" ORDER BY key_value ASC") +
                    limitClause;

  mCloneReadInfos.SetCapacity(50);

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                                      mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    if (!mKeyRange->Lower().IsUnset()) {
      rv = mKeyRange->Lower().BindToStatement(stmt, lowerKeyName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (!mKeyRange->Upper().IsUnset()) {
      rv = mKeyRange->Upper().BindToStatement(stmt, upperKeyName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mCloneReadInfos.Capacity() == mCloneReadInfos.Length()) {
      if (!mCloneReadInfos.SetCapacity(mCloneReadInfos.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    StructuredCloneReadInfo* readInfo = mCloneReadInfos.AppendElement();
    NS_ASSERTION(readInfo, "Shouldn't fail if SetCapacity succeeded!");

    rv = IDBObjectStore::GetStructuredCloneReadInfoFromStatement(stmt, 0, 1,
      mDatabase->Manager(), *readInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
GetAllHelper::GetSuccessResult(JSContext* aCx,
                               jsval* aVal)
{
  NS_ASSERTION(mCloneReadInfos.Length() <= mLimit, "Too many results!");

  nsresult rv = ConvertCloneReadInfosToArray(aCx, mCloneReadInfos, aVal);

  for (PRUint32 index = 0; index < mCloneReadInfos.Length(); index++) {
    mCloneReadInfos[index].mCloneBuffer.clear();
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
CountHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_NAMED_LITERAL_CSTRING(lowerKeyName, "lower_key");
  NS_NAMED_LITERAL_CSTRING(upperKeyName, "upper_key");

  nsCAutoString keyRangeClause;
  if (mKeyRange) {
    if (!mKeyRange->Lower().IsUnset()) {
      keyRangeClause = NS_LITERAL_CSTRING(" AND key_value");
      if (mKeyRange->IsLowerOpen()) {
        keyRangeClause.AppendLiteral(" > :");
      }
      else {
        keyRangeClause.AppendLiteral(" >= :");
      }
      keyRangeClause.Append(lowerKeyName);
    }

    if (!mKeyRange->Upper().IsUnset()) {
      keyRangeClause += NS_LITERAL_CSTRING(" AND key_value");
      if (mKeyRange->IsUpperOpen()) {
        keyRangeClause.AppendLiteral(" < :");
      }
      else {
        keyRangeClause.AppendLiteral(" <= :");
      }
      keyRangeClause.Append(upperKeyName);
    }
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT count(*) FROM object_data "
                                       "WHERE object_store_id = :osid") +
                    keyRangeClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                                      mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mKeyRange) {
    if (!mKeyRange->Lower().IsUnset()) {
      rv = mKeyRange->Lower().BindToStatement(stmt, lowerKeyName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (!mKeyRange->Upper().IsUnset()) {
      rv = mKeyRange->Upper().BindToStatement(stmt, upperKeyName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  NS_ENSURE_TRUE(hasResult, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mCount = stmt->AsInt64(0);
  return NS_OK;
}

nsresult
CountHelper::GetSuccessResult(JSContext* aCx,
                              jsval* aVal)
{
  return JS_NewNumberValue(aCx, static_cast<double>(mCount), aVal);
}
