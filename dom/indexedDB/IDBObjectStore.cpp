/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "IDBObjectStore.h"

#include "nsIIDBDatabaseException.h"
#include "nsIJSContextStack.h"
#include "nsIUUIDGenerator.h"
#include "nsIVariant.h"

#include "jscntxt.h"
#include "mozilla/storage.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSON.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBIndex.h"
#include "IDBKeyRange.h"
#include "IDBTransaction.h"
#include "DatabaseInfo.h"
#include "Savepoint.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class AddHelper : public AsyncConnectionHelper
{
public:
  AddHelper(IDBTransaction* aTransaction,
            IDBRequest* aRequest,
            PRInt64 aObjectStoreID,
            const nsAString& aKeyPath,
            const nsAString& aValue,
            const Key& aKey,
            bool aAutoIncrement,
            bool aOverwrite,
            nsTArray<IndexUpdateInfo>& aIndexUpdateInfo)
  : AsyncConnectionHelper(aTransaction, aRequest), mOSID(aObjectStoreID),
    mKeyPath(aKeyPath), mValue(aValue), mKey(aKey),
    mAutoIncrement(aAutoIncrement), mOverwrite(aOverwrite)
  {
    mIndexUpdateInfo.SwapElements(aIndexUpdateInfo);
  }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

  nsresult ModifyValueForNewKey();
  nsresult UpdateIndexes(mozIStorageConnection* aConnection,
                         PRInt64 aObjectDataId);

private:
  // In-params.
  const PRInt64 mOSID;
  const nsString mKeyPath;
  // These may change in the autoincrement case.
  nsString mValue;
  Key mKey;
  const bool mAutoIncrement;
  const bool mOverwrite;
  nsTArray<IndexUpdateInfo> mIndexUpdateInfo;
};

class GetHelper : public AsyncConnectionHelper
{
public:
  GetHelper(IDBTransaction* aTransaction,
            IDBRequest* aRequest,
            PRInt64 aObjectStoreID,
            const Key& aKey,
            bool aAutoIncrement)
  : AsyncConnectionHelper(aTransaction, aRequest), mOSID(aObjectStoreID),
    mKey(aKey), mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  // In-params.
  const PRInt64 mOSID;
  const Key mKey;
  const bool mAutoIncrement;

private:
  // Out-params.
  nsString mValue;
};

class RemoveHelper : public GetHelper
{
public:
  RemoveHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               PRInt64 aObjectStoreID,
               const Key& aKey,
               bool aAutoIncrement)
  : GetHelper(aTransaction, aRequest, aObjectStoreID, aKey, aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);
};

class ClearHelper : public AsyncConnectionHelper
{
public:
  ClearHelper(IDBTransaction* aTransaction,
              IDBRequest* aRequest,
              PRInt64 aObjectStoreID,
              bool aAutoIncrement)
  : AsyncConnectionHelper(aTransaction, aRequest), mOSID(aObjectStoreID),
    mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);

protected:
  // In-params.
  const PRInt64 mOSID;
  const bool mAutoIncrement;
};

class OpenCursorHelper : public AsyncConnectionHelper
{
public:
  OpenCursorHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   IDBObjectStore* aObjectStore,
                   const Key& aLeftKey,
                   const Key& aRightKey,
                   PRUint16 aKeyRangeFlags,
                   PRUint16 aDirection,
                   PRBool aPreload)
  : AsyncConnectionHelper(aTransaction, aRequest), mObjectStore(aObjectStore),
    mLeftKey(aLeftKey), mRightKey(aRightKey), mKeyRangeFlags(aKeyRangeFlags),
    mDirection(aDirection), mPreload(aPreload)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBObjectStore> mObjectStore;
  const Key mLeftKey;
  const Key mRightKey;
  const PRUint16 mKeyRangeFlags;
  const PRUint16 mDirection;
  const PRBool mPreload;

  // Out-params.
  nsTArray<KeyValuePair> mData;
};

class CreateIndexHelper : public AsyncConnectionHelper
{
public:
  CreateIndexHelper(IDBTransaction* aTransaction,
                    IDBRequest* aRequest,
                    const nsAString& aName,
                    const nsAString& aKeyPath,
                    bool aUnique,
                    bool aAutoIncrement,
                    IDBObjectStore* aObjectStore)
  : AsyncConnectionHelper(aTransaction, aRequest), mName(aName),
    mKeyPath(aKeyPath), mUnique(aUnique), mAutoIncrement(aAutoIncrement),
    mObjectStore(aObjectStore), mId(LL_MININT)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  PRUint16 InsertDataFromObjectStore(mozIStorageConnection* aConnection);

  // In-params.
  nsString mName;
  nsString mKeyPath;
  const bool mUnique;
  const bool mAutoIncrement;
  nsRefPtr<IDBObjectStore> mObjectStore;

  // Out-params.
  PRInt64 mId;
};

class RemoveIndexHelper : public AsyncConnectionHelper
{
public:
  RemoveIndexHelper(IDBTransaction* aDatabase,
                    IDBRequest* aRequest,
                    const nsAString& aName,
                    IDBObjectStore* aObjectStore)
  : AsyncConnectionHelper(aDatabase, aRequest), mName(aName),
    mObjectStore(aObjectStore)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

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
               PRInt64 aObjectStoreID,
               const Key& aLeftKey,
               const Key& aRightKey,
               const PRUint16 aKeyRangeFlags,
               const PRUint32 aLimit,
               bool aAutoIncrement)
  : AsyncConnectionHelper(aTransaction, aRequest), mOSID(aObjectStoreID),
    mLeftKey(aLeftKey), mRightKey(aRightKey), mKeyRangeFlags(aKeyRangeFlags),
    mLimit(aLimit), mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  // In-params.
  const PRInt64 mOSID;
  const Key mLeftKey;
  const Key mRightKey;
  const PRUint16 mKeyRangeFlags;
  const PRUint32 mLimit;
  const bool mAutoIncrement;

private:
  // Out-params.
  nsTArray<nsString> mValues;
};

inline
nsresult
GetKeyFromObject(JSContext* aCx,
                 JSObject* aObj,
                 const nsString& aKeyPath,
                 Key& aKey)
{
  NS_PRECONDITION(aCx && aObj, "Null pointers!");
  NS_ASSERTION(!aKeyPath.IsVoid(), "This will explode!");

  const jschar* keyPathChars = reinterpret_cast<const jschar*>(aKeyPath.get());
  const size_t keyPathLen = aKeyPath.Length();

  jsval key;
  JSBool ok = JS_GetUCProperty(aCx, aObj, keyPathChars, keyPathLen, &key);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  nsresult rv = IDBObjectStore::GetKeyFromJSVal(key, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBObjectStore* aObjectStore)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  IDBDatabase* database = aObjectStore->Transaction()->Database();
  return IDBRequest::Create(static_cast<nsPIDOMEventTarget*>(aObjectStore),
                            database->ScriptContext(), database->Owner());
}

} // anonymous namespace

// static
already_AddRefed<IDBObjectStore>
IDBObjectStore::Create(IDBTransaction* aTransaction,
                       const ObjectStoreInfo* aStoreInfo,
                       PRUint16 aMode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBObjectStore> objectStore = new IDBObjectStore();

  objectStore->mScriptContext = aTransaction->Database()->ScriptContext();
  objectStore->mOwner = aTransaction->Database()->Owner();

  objectStore->mTransaction = aTransaction;
  objectStore->mName = aStoreInfo->name;
  objectStore->mId = aStoreInfo->id;
  objectStore->mKeyPath = aStoreInfo->keyPath;
  objectStore->mAutoIncrement = aStoreInfo->autoIncrement;
  objectStore->mDatabaseId = aStoreInfo->databaseId;
  objectStore->mMode = aMode;

  return objectStore.forget();
}

// static
nsresult
IDBObjectStore::GetKeyFromVariant(nsIVariant* aKeyVariant,
                                  Key& aKey)
{
  if (!aKeyVariant) {
    aKey = Key::UNSETKEY;
    return NS_OK;
  }

  PRUint16 type;
  nsresult rv = aKeyVariant->GetDataType(&type);
  NS_ENSURE_SUCCESS(rv, rv);

  // See xpcvariant.cpp, these are the only types we should expect.
  switch (type) {
    case nsIDataType::VTYPE_VOID:
      aKey = Key::UNSETKEY;
      break;

    case nsIDataType::VTYPE_EMPTY:
      aKey = Key::NULLKEY;
      break;

    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      rv = aKeyVariant->GetAsAString(aKey.ToString());
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_DOUBLE:
      rv = aKeyVariant->GetAsInt64(aKey.ToIntPtr());
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    default:
      return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

// static
nsresult
IDBObjectStore::GetKeyFromJSVal(jsval aKeyVal,
                                Key& aKey)
{
  if (JSVAL_IS_VOID(aKeyVal)) {
    aKey = Key::UNSETKEY;
  }
  else if (JSVAL_IS_NULL(aKeyVal)) {
    aKey = Key::NULLKEY;
  }
  else if (JSVAL_IS_STRING(aKeyVal)) {
    aKey = nsDependentJSString(aKeyVal);
  }
  else if (JSVAL_IS_INT(aKeyVal)) {
    aKey = JSVAL_TO_INT(aKeyVal);
  }
  else if (JSVAL_IS_DOUBLE(aKeyVal)) {
    aKey = JSVAL_TO_DOUBLE(aKeyVal);
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

// static
nsresult
IDBObjectStore::GetJSValFromKey(const Key& aKey,
                                JSContext* aCx,
                                jsval* aKeyVal)
{
  if (aKey.IsUnset()) {
    *aKeyVal = JSVAL_VOID;
    return NS_OK;
  }

  if (aKey.IsNull()) {
    *aKeyVal = JSVAL_NULL;
    return NS_OK;
  }

  if (aKey.IsInt()) {
    JSBool ok = JS_NewNumberValue(aCx, aKey.IntValue(), aKeyVal);
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
    return NS_OK;
  }

  if (aKey.IsString()) {
    const nsString& keyString = aKey.StringValue();
    JSString* str =
      JS_NewUCStringCopyN(aCx,
                          reinterpret_cast<const jschar*>(keyString.get()),
                          keyString.Length());
    NS_ENSURE_TRUE(str, NS_ERROR_FAILURE);

    *aKeyVal = STRING_TO_JSVAL(str);
    return NS_OK;
  }

  NS_NOTREACHED("Unknown key type!");
  return NS_ERROR_INVALID_ARG;
}

// static
nsresult
IDBObjectStore::GetJSONFromArg0(/* jsval arg0, */
                                nsAString& aJSON)
{
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

  nsAXPCNativeCallContext* cc;
  nsresult rv = xpc->GetCurrentNativeCallContext(&cc);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(cc, NS_ERROR_UNEXPECTED);

  PRUint32 argc;
  rv = cc->GetArgc(&argc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (argc < 1) {
    return NS_ERROR_XPC_NOT_ENOUGH_ARGS;
  }

  jsval* argv;
  rv = cc->GetArgvPtr(&argv);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext* cx;
  rv = cc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  js::AutoValueRooter clone(cx);
  rv = nsContentUtils::CreateStructuredClone(cx, argv[0], clone.jsval_addr());
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIJSON> json(new nsJSON());

  rv = json->EncodeFromJSVal(clone.jsval_addr(), cx, aJSON);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
nsresult
IDBObjectStore::GetKeyPathValueFromJSON(const nsAString& aJSON,
                                        const nsAString& aKeyPath,
                                        JSContext** aCx,
                                        Key& aValue)
{
  NS_ASSERTION(!aJSON.IsEmpty(), "Empty JSON!");
  NS_ASSERTION(!aKeyPath.IsEmpty(), "Empty keyPath!");
  NS_ASSERTION(aCx, "Null pointer!");

  nsresult rv;

  if (!*aCx) {
    rv = nsContentUtils::ThreadJSContextStack()->GetSafeJSContext(aCx);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JSAutoRequest ar(*aCx);

  js::AutoValueRooter clone(*aCx);

  nsCOMPtr<nsIJSON> json(new nsJSON());
  rv = json->DecodeToJSVal(aJSON, *aCx, clone.jsval_addr());
  NS_ENSURE_SUCCESS(rv, rv);

  if (JSVAL_IS_PRIMITIVE(clone.jsval_value())) {
    // This isn't an object, so just leave the key unset.
    aValue = Key::UNSETKEY;
    return NS_OK;
  }

  JSObject* obj = JSVAL_TO_OBJECT(clone.jsval_value());

  const jschar* keyPathChars =
    reinterpret_cast<const jschar*>(aKeyPath.BeginReading());
  const size_t keyPathLen = aKeyPath.Length();

  js::AutoValueRooter value(*aCx);
  JSBool ok = JS_GetUCProperty(*aCx, obj, keyPathChars, keyPathLen,
                               value.jsval_addr());
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  rv = GetKeyFromJSVal(value.jsval_value(), aValue);
  if (NS_FAILED(rv) || aValue.IsNull()) {
    // If the object doesn't have a value that we can use for our index then we
    // leave it unset.
    aValue = Key::UNSETKEY;
  }

  return NS_OK;
}

/* static */
nsresult
IDBObjectStore::GetIndexUpdateInfo(ObjectStoreInfo* aObjectStoreInfo,
                                   JSContext* aCx,
                                   jsval aObject,
                                   nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
  JSObject* cloneObj = nsnull;

  PRUint32 count = aObjectStoreInfo->indexes.Length();
  if (count && !JSVAL_IS_PRIMITIVE(aObject)) {
    if (!aUpdateInfoArray.SetCapacity(count)) {
      NS_ERROR("Out of memory!");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    cloneObj = JSVAL_TO_OBJECT(aObject);

    for (PRUint32 indexesIndex = 0; indexesIndex < count; indexesIndex++) {
      const IndexInfo& indexInfo = aObjectStoreInfo->indexes[indexesIndex];

      const jschar* keyPathChars =
        reinterpret_cast<const jschar*>(indexInfo.keyPath.BeginReading());
      const size_t keyPathLen = indexInfo.keyPath.Length();

      jsval keyPathValue;
      JSBool ok = JS_GetUCProperty(aCx, cloneObj, keyPathChars, keyPathLen,
                                   &keyPathValue);
      NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

      Key value;
      nsresult rv = GetKeyFromJSVal(keyPathValue, value);
      if (NS_FAILED(rv) || value.IsUnset() || value.IsNull()) {
        // Not a value we can do anything with, ignore it.
        continue;
      }

      IndexUpdateInfo* updateInfo = aUpdateInfoArray.AppendElement();
      updateInfo->info = indexInfo;
      updateInfo->value = value;
    }
  }
  else {
    aUpdateInfoArray.Clear();
  }

  return NS_OK;
}

/* static */
nsresult
IDBObjectStore::UpdateIndexes(IDBTransaction* aTransaction,
                              PRInt64 aObjectStoreId,
                              const Key& aObjectStoreKey,
                              bool aAutoIncrement,
                              bool aOverwrite,
                              PRInt64 aObjectDataId,
                              const nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
#ifdef DEBUG
  if (aAutoIncrement) {
    NS_ASSERTION(aObjectDataId != LL_MININT, "Bad objectData id!");
  }
  else {
    NS_ASSERTION(aObjectDataId == LL_MININT, "Bad objectData id!");
  }
#endif

  PRUint32 indexCount = aUpdateInfoArray.Length();
  NS_ASSERTION(indexCount, "Don't call me!");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv;

  if (!aAutoIncrement) {
    stmt = aTransaction->GetCachedStatement(
      "SELECT id "
      "FROM object_data "
      "WHERE object_store_id = :osid "
      "AND key_value = :key_value"
    );
    NS_ENSURE_TRUE(stmt, NS_ERROR_FAILURE);

    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), aObjectStoreId);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(!aObjectStoreKey.IsUnset(), "This shouldn't happen!");

    NS_NAMED_LITERAL_CSTRING(keyValue, "key_value");

    if (aObjectStoreKey.IsInt()) {
      rv = stmt->BindInt64ByName(keyValue, aObjectStoreKey.IntValue());
    }
    else if (aObjectStoreKey.IsString()) {
      rv = stmt->BindStringByName(keyValue, aObjectStoreKey.StringValue());
    }
    else {
      NS_NOTREACHED("Unknown key type!");
    }
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!hasResult) {
      NS_ERROR("This is bad, we just added this value! Where'd it go?!");
      return NS_ERROR_FAILURE;
    }

    aObjectDataId = stmt->AsInt64(0);
  }

  NS_ASSERTION(aObjectDataId != LL_MININT, "Bad objectData id!");

  for (PRUint32 indexIndex = 0; indexIndex < indexCount; indexIndex++) {
    const IndexUpdateInfo& updateInfo = aUpdateInfoArray[indexIndex];

    stmt = aTransaction->IndexUpdateStatement(updateInfo.info.autoIncrement,
                                              updateInfo.info.unique,
                                              aOverwrite);
    NS_ENSURE_TRUE(stmt, NS_ERROR_FAILURE);

    mozStorageStatementScoper scoper2(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                               updateInfo.info.id);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("object_data_id"),
                               aObjectDataId);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_NAMED_LITERAL_CSTRING(objectDataKey, "object_data_key");

    if (aObjectStoreKey.IsInt()) {
      rv = stmt->BindInt64ByName(objectDataKey, aObjectStoreKey.IntValue());
    }
    else if (aObjectStoreKey.IsString()) {
      rv = stmt->BindStringByName(objectDataKey, aObjectStoreKey.StringValue());
    }
    else {
      NS_NOTREACHED("Unknown key type!");
    }
    NS_ENSURE_SUCCESS(rv, rv);

    NS_NAMED_LITERAL_CSTRING(value, "value");

    if (updateInfo.value.IsInt()) {
      rv = stmt->BindInt64ByName(value, updateInfo.value.IntValue());
    }
    else if (updateInfo.value.IsString()) {
      rv = stmt->BindStringByName(value, updateInfo.value.StringValue());
    }
    else if (updateInfo.value.IsUnset()) {
      rv = stmt->BindStringByName(value, updateInfo.value.StringValue());
    }
    else {
      NS_NOTREACHED("Unknown key type!");
    }
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

ObjectStoreInfo*
IDBObjectStore::GetObjectStoreInfo()
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  ObjectStoreInfo* info;
  if (!ObjectStoreInfo::Get(mDatabaseId, mName, &info)) {
    NS_ERROR("This should never fail!");
    return nsnull;
  }
  return info;
}

IDBObjectStore::IDBObjectStore()
: mId(LL_MININT),
  mAutoIncrement(PR_FALSE),
  mMode(nsIIDBTransaction::READ_WRITE)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBObjectStore::~IDBObjectStore()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

nsresult
IDBObjectStore::GetAddInfo(JSContext* aCx,
                           jsval aValue,
                           jsval aKeyVal,
                           nsString& aJSON,
                           Key& aKey,
                           nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
  JSAutoRequest ar(aCx);

  js::AutoValueRooter clone(aCx);
  nsresult rv = nsContentUtils::CreateStructuredClone(aCx, aValue,
                                                      clone.jsval_addr());
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mKeyPath.IsEmpty()) {
    rv = GetKeyFromJSVal(aKeyVal, aKey);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Inline keys live on the object. Make sure it is an object.
    if (JSVAL_IS_PRIMITIVE(clone.jsval_value())) {
      return NS_ERROR_INVALID_ARG;
    }

    rv = GetKeyFromObject(aCx, JSVAL_TO_OBJECT(clone.jsval_value()), mKeyPath, aKey);
    NS_ENSURE_SUCCESS(rv, rv);

    // Except if null was passed, in which case we're supposed to generate the
    // key.
    if (aKey.IsUnset() && JSVAL_IS_NULL(aKeyVal)) {
      aKey = Key::NULLKEY;
    }
  }

  if (aKey.IsUnset() && !mAutoIncrement) {
    return NS_ERROR_INVALID_ARG;
  }

  // Figure out indexes and the index values to update here.
  ObjectStoreInfo* objectStoreInfo = GetObjectStoreInfo();
  NS_ENSURE_TRUE(objectStoreInfo, NS_ERROR_FAILURE);

  rv = GetIndexUpdateInfo(objectStoreInfo, aCx, clone.jsval_value(), aUpdateInfoArray);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIJSON> json(new nsJSON());
  rv = json->EncodeFromJSVal(clone.jsval_addr(), aCx, aJSON);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBObjectStore)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBObjectStore,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mTransaction,
                                                       nsPIDOMEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBObjectStore,
                                                nsDOMEventTargetHelper)
  // Don't unlink mTransaction!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBObjectStore)
  NS_INTERFACE_MAP_ENTRY(nsIIDBObjectStore)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBObjectStore)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBObjectStore, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBObjectStore, nsDOMEventTargetHelper)

DOMCI_DATA(IDBObjectStore, IDBObjectStore)

NS_IMETHODIMP
IDBObjectStore::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::GetKeyPath(nsAString& aKeyPath)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aKeyPath.Assign(mKeyPath);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::GetIndexNames(nsIDOMDOMStringList** aIndexNames)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  ObjectStoreInfo* info = GetObjectStoreInfo();
  NS_ENSURE_TRUE(info, NS_ERROR_UNEXPECTED);

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());

  PRUint32 count = info->indexes.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(info->indexes[index].name),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  list.forget(aIndexNames);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Get(nsIVariant* aKey,
                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  Key key;
  nsresult rv = GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (key.IsUnset()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetHelper> helper =
    new GetHelper(mTransaction, request, mId, key, !!mAutoIncrement);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::GetAll(nsIIDBKeyRange* aKeyRange,
                       PRUint32 aLimit,
                       PRUint8 aOptionalArgCount,
                       nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aOptionalArgCount < 2) {
    aLimit = PR_UINT32_MAX;
  }

  Key leftKey, rightKey;
  PRUint16 keyRangeFlags = 0;

  nsresult rv;
  if (aKeyRange) {
    rv = aKeyRange->GetFlags(&keyRangeFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIVariant> variant;
    rv = aKeyRange->GetLeft(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetKeyFromVariant(variant, leftKey);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aKeyRange->GetRight(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetKeyFromVariant(variant, rightKey);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(mTransaction, request, mId, leftKey, rightKey,
                     keyRangeFlags, aLimit, mAutoIncrement);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Add(const jsval &aValue,
                    const jsval &aKey,
                    JSContext* aCx,
                    PRUint8 aOptionalArgCount,
                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mMode != nsIIDBTransaction::READ_WRITE) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  jsval keyval;
  if (aOptionalArgCount >= 1) {
    keyval = aKey;
    if (mAutoIncrement && JSVAL_IS_NULL(keyval)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
  }
  else {
    keyval = JSVAL_VOID;
  }

  nsString jsonValue;
  Key key;
  nsTArray<IndexUpdateInfo> updateInfo;

  nsresult rv = GetAddInfo(aCx, aValue, keyval, jsonValue, key, updateInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (key.IsUnset() && !mAutoIncrement) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<AddHelper> helper =
    new AddHelper(mTransaction, request, mId, mKeyPath, jsonValue, key,
                  !!mAutoIncrement, false, updateInfo);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Put(const jsval &aValue,
                    const jsval &aKey,
                    JSContext* aCx,
                    PRUint8 aOptionalArgCount,
                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mMode != nsIIDBTransaction::READ_WRITE) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  jsval keyval = (aOptionalArgCount >= 1) ? aKey : JSVAL_VOID;

  nsString jsonValue;
  Key key;
  nsTArray<IndexUpdateInfo> updateInfo;

  nsresult rv = GetAddInfo(aCx, aValue, keyval, jsonValue, key, updateInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (key.IsUnset() || key.IsNull()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<AddHelper> helper =
    new AddHelper(mTransaction, request, mId, mKeyPath, jsonValue, key,
                  !!mAutoIncrement, true, updateInfo);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Remove(nsIVariant* aKey,
                       nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mMode != nsIIDBTransaction::READ_WRITE) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  Key key;
  nsresult rv = GetKeyFromVariant(aKey, key);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (key.IsUnset() || key.IsNull()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<RemoveHelper> helper =
    new RemoveHelper(mTransaction, request, mId, key, !!mAutoIncrement);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Clear(nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mMode != nsIIDBTransaction::READ_WRITE) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<ClearHelper> helper =
    new ClearHelper(mTransaction, request, mId, !!mAutoIncrement);
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::OpenCursor(nsIIDBKeyRange* aKeyRange,
                           PRUint16 aDirection,
                           PRBool aPreload,
                           PRUint8 aOptionalArgCount,
                           nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv;

  Key leftKey, rightKey;
  PRUint16 keyRangeFlags = 0;

  if (aKeyRange) {
    rv = aKeyRange->GetFlags(&keyRangeFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIVariant> variant;
    rv = aKeyRange->GetLeft(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetKeyFromVariant(variant, leftKey);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aKeyRange->GetRight(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetKeyFromVariant(variant, rightKey);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aOptionalArgCount >= 2) {
    if (aDirection != nsIIDBCursor::NEXT &&
        aDirection != nsIIDBCursor::NEXT_NO_DUPLICATE &&
        aDirection != nsIIDBCursor::PREV &&
        aDirection != nsIIDBCursor::PREV_NO_DUPLICATE) {
      return NS_ERROR_INVALID_ARG;
    }
  }
  else {
    aDirection = nsIIDBCursor::NEXT;
  }

  if (aPreload) {
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(mTransaction, request, this, leftKey, rightKey,
                         keyRangeFlags, aDirection, aPreload);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::CreateIndex(const nsAString& aName,
                            const nsAString& aKeyPath,
                            PRBool aUnique,
                            nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  ObjectStoreInfo* info = GetObjectStoreInfo();
  NS_ENSURE_TRUE(info, NS_ERROR_UNEXPECTED);

  bool found = false;
  PRUint32 indexCount = info->indexes.Length();
  for (PRUint32 index = 0; index < indexCount; index++) {
    if (info->indexes[index].name == aName) {
      found = true;
      break;
    }
  }

  if (found) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  if (aKeyPath.IsEmpty()) {
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<CreateIndexHelper> helper =
    new CreateIndexHelper(mTransaction, request, aName, aKeyPath, !!aUnique,
                          mAutoIncrement, this);
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::Index(const nsAString& aName,
                      nsIIDBIndex** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  ObjectStoreInfo* info = GetObjectStoreInfo();
  NS_ENSURE_TRUE(info, NS_ERROR_UNEXPECTED);

  IndexInfo* indexInfo = nsnull;
  PRUint32 indexCount = info->indexes.Length();
  for (PRUint32 index = 0; index < indexCount; index++) {
    if (info->indexes[index].name == aName) {
      indexInfo = &(info->indexes[index]);
      break;
    }
  }

  if (!indexInfo) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<IDBIndex> request = IDBIndex::Create(this, indexInfo);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStore::RemoveIndex(const nsAString& aName,
                            nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransaction->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  ObjectStoreInfo* info = GetObjectStoreInfo();
  NS_ENSURE_TRUE(info, NS_ERROR_UNEXPECTED);

  bool found = false;
  PRUint32 indexCount = info->indexes.Length();
  for (PRUint32 index = 0; index < indexCount; index++) {
    if (info->indexes[index].name == aName) {
      found = true;
      break;
    }
  }

  if (!found) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<RemoveIndexHelper> helper =
    new RemoveIndexHelper(mTransaction, request, aName, this);
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

PRUint16
AddHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsresult rv;
  if (mKey.IsNull()) {
    NS_WARNING("Using a UUID for null keys, probably can do something faster!");

    nsCOMPtr<nsIUUIDGenerator> uuidGen =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    nsID id;
    rv = uuidGen->GenerateUUIDInPlace(&id);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    char idString[NSID_LENGTH] = { 0 };
    id.ToProvidedString(idString);

    mKey = NS_ConvertASCIItoUTF16(idString);
  }

  bool mayOverwrite = mOverwrite;
  bool unsetKey = mKey.IsUnset();

  if (unsetKey) {
    NS_ASSERTION(mAutoIncrement, "Must have a key for non-autoIncrement!");

    // Will need to add first and then set the key later.
    mayOverwrite = false;
  }

  if (mAutoIncrement && !unsetKey) {
    mayOverwrite = true;
  }

  Savepoint savepoint(mTransaction);

  nsCOMPtr<mozIStorageStatement> stmt;
  if (!mOverwrite && !unsetKey) {
    // Make sure the key doesn't exist already
    stmt = mTransaction->GetStatement(mAutoIncrement);
    NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    NS_NAMED_LITERAL_CSTRING(id, "id");

    if (mKey.IsInt()) {
      rv = stmt->BindInt64ByName(id, mKey.IntValue());
    }
    else if (mKey.IsString()) {
      rv = stmt->BindStringByName(id, mKey.StringValue());
    }
    else {
      NS_NOTREACHED("Unknown key type!");
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    PRBool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    if (hasResult) {
      return nsIIDBDatabaseException::CONSTRAINT_ERR;
    }
  }

  // Now we add it to the database (or update, depending on our variables).
  stmt = mTransaction->AddStatement(true, mayOverwrite, mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  NS_NAMED_LITERAL_CSTRING(keyValue, "key_value");

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (!mAutoIncrement || mayOverwrite) {
    NS_ASSERTION(!mKey.IsUnset(), "This shouldn't happen!");

    if (mKey.IsInt()) {
      rv = stmt->BindInt64ByName(keyValue, mKey.IntValue());
    }
    else if (mKey.IsString()) {
      rv = stmt->BindStringByName(keyValue, mKey.StringValue());
    }
    else {
      NS_NOTREACHED("Unknown key type!");
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("data"), mValue);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->Execute();
  if (NS_FAILED(rv)) {
    if (mayOverwrite && rv == NS_ERROR_STORAGE_CONSTRAINT) {
      scoper.Abandon();

      stmt = mTransaction->AddStatement(false, true, mAutoIncrement);
      NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

      mozStorageStatementScoper scoper2(stmt);

      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

      if (!mAutoIncrement) {
        NS_ASSERTION(!mKey.IsUnset(), "This shouldn't happen!");

        if (mKey.IsInt()) {
          rv = stmt->BindInt64ByName(keyValue, mKey.IntValue());
        }
        else if (mKey.IsString()) {
          rv = stmt->BindStringByName(keyValue, mKey.StringValue());
        }
        else {
          NS_NOTREACHED("Unknown key type!");
        }
        NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
      }

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("data"), mValue);
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

      rv = stmt->Execute();
    }

    if (NS_FAILED(rv)) {
      return nsIIDBDatabaseException::CONSTRAINT_ERR;
    }
  }

  // If we are supposed to generate a key, get the new id.
  if (mAutoIncrement && !mOverwrite) {
#ifdef DEBUG
    PRInt64 oldKey = unsetKey ? 0 : mKey.IntValue();
#endif

    rv = aConnection->GetLastInsertRowID(mKey.ToIntPtr());
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

#ifdef DEBUG
    NS_ASSERTION(mKey.IsInt(), "Bad key value!");
    if (!unsetKey) {
      NS_ASSERTION(mKey.IntValue() == oldKey, "Something went haywire!");
    }
#endif

    if (!mKeyPath.IsEmpty() && unsetKey) {
      // Special case where someone put an object into an autoIncrement'ing
      // objectStore with no key in its keyPath set. We needed to figure out
      // which row id we would get above before we could set that properly.
      rv = ModifyValueForNewKey();
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

      scoper.Abandon();
      rv = stmt->Reset();
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

      stmt = mTransaction->AddStatement(false, true, true);
      NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

      mozStorageStatementScoper scoper2(stmt);

      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

      rv = stmt->BindInt64ByName(keyValue, mKey.IntValue());
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("data"), mValue);
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
    }
  }

  // Update our indexes if needed.
  if (!mIndexUpdateInfo.IsEmpty()) {
    PRInt64 objectDataId = mAutoIncrement ? mKey.IntValue() : LL_MININT;
    rv = IDBObjectStore::UpdateIndexes(mTransaction, mOSID, mKey,
                                       mAutoIncrement, mOverwrite,
                                       objectDataId, mIndexUpdateInfo);
    if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
      return nsIIDBDatabaseException::CONSTRAINT_ERR;
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  rv = savepoint.Release();
  return NS_SUCCEEDED(rv) ? OK : nsIIDBDatabaseException::UNKNOWN_ERR;
}

PRUint16
AddHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Badness!");

  if (mKey.IsString()) {
    aResult->SetAsAString(mKey.StringValue());
  }
  else if (mKey.IsInt()) {
    aResult->SetAsInt64(mKey.IntValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  return OK;
}

nsresult
AddHelper::ModifyValueForNewKey()
{
  NS_ASSERTION(mAutoIncrement && !mKeyPath.IsEmpty() && mKey.IsInt(),
               "Don't call me!");

  JSContext* cx;
  nsresult rv = nsContentUtils::ThreadJSContextStack()->GetSafeJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  js::AutoValueRooter clone(cx);

  nsCOMPtr<nsIJSON> json(new nsJSON());
  rv = json->DecodeToJSVal(mValue, cx, clone.jsval_addr());
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* obj = JSVAL_TO_OBJECT(clone.jsval_value());
  JSBool ok;
  js::AutoValueRooter key(cx);

  const jschar* keyPathChars = reinterpret_cast<const jschar*>(mKeyPath.get());
  const size_t keyPathLen = mKeyPath.Length();

#ifdef DEBUG
  ok = JS_GetUCProperty(cx, obj, keyPathChars, keyPathLen, key.jsval_addr());
  NS_ASSERTION(ok && JSVAL_IS_VOID(key.jsval_value()), "Already has a key prop!");
#endif

  ok = JS_NewNumberValue(cx, mKey.IntValue(), key.jsval_addr());
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  ok = JS_DefineUCProperty(cx, obj, keyPathChars, keyPathLen, key.jsval_value(),
                           nsnull, nsnull, JSPROP_ENUMERATE);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  rv = json->EncodeFromJSVal(clone.jsval_addr(), cx, mValue);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

PRUint16
GetHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetStatement(mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Must have a key here!");

  NS_NAMED_LITERAL_CSTRING(id, "id");

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(id, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(id, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  // Search for it!
  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (!hasResult) {
    return nsIIDBDatabaseException::NOT_FOUND_ERR;
  }

  // Set the value based on results.
  (void)stmt->GetString(0, mValue);

  return OK;
}

PRUint16
GetHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetSuccessEvent> event(new GetSuccessEvent(mValue));
  nsresult rv = event->Init(mRequest, mTransaction);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return OK;
}

PRUint16
RemoveHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->RemoveStatement(mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Must have a key here!");

  NS_NAMED_LITERAL_CSTRING(key_value, "key_value");

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(key_value, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(key_value, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  // Search for it!
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
RemoveHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  return AsyncConnectionHelper::OnSuccess(aTarget);
}

PRUint16
RemoveHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Badness!");

  if (mKey.IsString()) {
    aResult->SetAsAString(mKey.StringValue());
  }
  else if (mKey.IsInt()) {
    aResult->SetAsInt64(mKey.IntValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  return OK;
}

PRUint16
ClearHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsCString table;
  if (mAutoIncrement) {
    table.AssignLiteral("ai_object_data");
  }
  else {
    table.AssignLiteral("object_data");
  }

  nsCString query = NS_LITERAL_CSTRING("DELETE FROM ") + table +
                    NS_LITERAL_CSTRING(" WHERE object_store_id = :osid");

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
OpenCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCString table;
  nsCString keyColumn;

  if (mObjectStore->IsAutoIncrement()) {
    table.AssignLiteral("ai_object_data");
    keyColumn.AssignLiteral("id");
  }
  else {
    table.AssignLiteral("object_data");
    keyColumn.AssignLiteral("key_value");
  }

  NS_NAMED_LITERAL_CSTRING(osid, "osid");
  NS_NAMED_LITERAL_CSTRING(leftKeyName, "left_key");
  NS_NAMED_LITERAL_CSTRING(rightKeyName, "right_key");

  nsCAutoString keyRangeClause;
  if (!mLeftKey.IsUnset()) {
    keyRangeClause = NS_LITERAL_CSTRING(" AND ") + keyColumn;
    if (mKeyRangeFlags & nsIIDBKeyRange::LEFT_OPEN) {
      keyRangeClause.AppendLiteral(" > :");
    }
    else {
      NS_ASSERTION(mKeyRangeFlags & nsIIDBKeyRange::LEFT_BOUND, "Bad flags!");
      keyRangeClause.AppendLiteral(" >= :");
    }
    keyRangeClause.Append(leftKeyName);
  }

  if (!mRightKey.IsUnset()) {
    keyRangeClause += NS_LITERAL_CSTRING(" AND ") + keyColumn;
    if (mKeyRangeFlags & nsIIDBKeyRange::RIGHT_OPEN) {
      keyRangeClause.AppendLiteral(" < :");
    }
    else {
      NS_ASSERTION(mKeyRangeFlags & nsIIDBKeyRange::RIGHT_BOUND, "Bad flags!");
      keyRangeClause.AppendLiteral(" <= :");
    }
    keyRangeClause.Append(rightKeyName);
  }

  nsCString directionClause;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::NEXT_NO_DUPLICATE:
      directionClause = NS_LITERAL_CSTRING(" DESC");
      break;

    case nsIIDBCursor::PREV:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      directionClause = NS_LITERAL_CSTRING(" ASC");
      break;

    default:
      NS_NOTREACHED("Unknown direction type!");
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT ") + keyColumn +
                    NS_LITERAL_CSTRING(", data FROM ") + table +
                    NS_LITERAL_CSTRING(" WHERE object_store_id = :") + osid +
                    keyRangeClause + NS_LITERAL_CSTRING(" ORDER BY ") +
                    keyColumn + directionClause;

  if (!mData.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(osid, mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (!mLeftKey.IsUnset()) {
    if (mLeftKey.IsString()) {
      rv = stmt->BindStringByName(leftKeyName, mLeftKey.StringValue());
    }
    else if (mLeftKey.IsInt()) {
      rv = stmt->BindInt64ByName(leftKeyName, mLeftKey.IntValue());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  if (!mRightKey.IsUnset()) {
    if (mRightKey.IsString()) {
      rv = stmt->BindStringByName(rightKeyName, mRightKey.StringValue());
    }
    else if (mRightKey.IsInt()) {
      rv = stmt->BindInt64ByName(rightKeyName, mRightKey.IntValue());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  NS_WARNING("Copying all results for cursor snapshot, do something smarter!");

  PRBool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mData.Capacity() == mData.Length()) {
      if (!mData.SetCapacity(mData.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return nsIIDBDatabaseException::UNKNOWN_ERR;
      }
    }

    KeyValuePair* pair = mData.AppendElement();
    NS_ASSERTION(pair, "Shouldn't fail if SetCapacity succeeded!");

    PRInt32 keyType;
    rv = stmt->GetTypeOfIndex(0, &keyType);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      pair->key = stmt->AsInt64(0);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(0, pair->key.ToString());
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
    }
    else {
      NS_NOTREACHED("Bad SQLite type!");
    }

#ifdef DEBUG
    {
      PRInt32 valueType;
      NS_ASSERTION(NS_SUCCEEDED(stmt->GetTypeOfIndex(1, &valueType)) &&
                   valueType == mozIStorageStatement::VALUE_TYPE_TEXT,
                   "Bad value type!");
    }
#endif

    rv = stmt->GetString(1, pair->value);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
OpenCursorHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  if (mData.IsEmpty()) {
    aResult->SetAsEmpty();
    return OK;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mObjectStore, mDirection, mData);
  NS_ENSURE_TRUE(cursor, nsIIDBDatabaseException::UNKNOWN_ERR);

  aResult->SetAsISupports(static_cast<nsPIDOMEventTarget*>(cursor));
  return OK;
}

PRUint16
CreateIndexHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  Savepoint savepoint(mTransaction);

  // Insert the data into the database.
  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(
    "INSERT INTO object_store_index (name, key_path, unique_index, "
      "object_store_id, object_store_autoincrement) "
    "VALUES (:name, :key_path, :unique, :osid, :os_auto_increment)"
  );
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"), mKeyPath);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("unique"),
                             mUnique ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("os_auto_increment"),
                             mAutoIncrement ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::CONSTRAINT_ERR;
  }

  // Get the id of this object store, and store it for future use.
  (void)aConnection->GetLastInsertRowID(&mId);

  // Now we need to populate the index with data from the object store.
  PRUint16 rc = InsertDataFromObjectStore(aConnection);
  NS_ENSURE_TRUE(rc == OK, rc);

  rv = savepoint.Release();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  return OK;
}

PRUint16
CreateIndexHelper::InsertDataFromObjectStore(mozIStorageConnection* aConnection)
{
  nsCAutoString table;
  nsCAutoString columns;
  if (mAutoIncrement) {
    table.AssignLiteral("ai_object_data");
    columns.AssignLiteral("id, data");
  }
  else {
    table.AssignLiteral("object_data");
    columns.AssignLiteral("id, data, key_value");
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT ") + columns +
                    NS_LITERAL_CSTRING(" FROM ") + table +
                    NS_LITERAL_CSTRING(" WHERE object_store_id = :osid");

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                                      mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsCOMPtr<mozIStorageStatement> insertStmt =
      mTransaction->IndexUpdateStatement(mAutoIncrement, mUnique, false);
    NS_ENSURE_TRUE(insertStmt, nsIIDBDatabaseException::UNKNOWN_ERR);

    mozStorageStatementScoper scoper2(insertStmt);

    rv = insertStmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"), mId);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    rv = insertStmt->BindInt64ByName(NS_LITERAL_CSTRING("object_data_id"),
                                     stmt->AsInt64(0));
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    if (!mAutoIncrement) {
      // XXX does this cause problems with the affinity?
      nsString key;
      rv = stmt->GetString(2, key);
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

      rv = insertStmt->BindStringByName(NS_LITERAL_CSTRING("object_data_key"),
                                        key);
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
    }

    nsString json;
    rv = stmt->GetString(1, json);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    Key key;
    JSContext* cx = nsnull;
    rv = IDBObjectStore::GetKeyPathValueFromJSON(json, mKeyPath, &cx, key);
    // XXX this should be a constraint error maybe?
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    NS_NAMED_LITERAL_CSTRING(value, "value");

    if (key.IsUnset()) {
      continue;
    }

    if (key.IsInt()) {
      rv = insertStmt->BindInt64ByName(value, key.IntValue());
    }
    else if (key.IsString()) {
      rv = insertStmt->BindStringByName(value, key.StringValue());
    }
    else {
      return nsIIDBDatabaseException::CONSTRAINT_ERR;
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    rv = insertStmt->Execute();
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  return OK;
}

PRUint16
CreateIndexHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  ObjectStoreInfo* info = mObjectStore->GetObjectStoreInfo();
  if (!info) {
    NS_ERROR("Couldn't get info!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

#ifdef DEBUG
  {
    bool found = false;
    PRUint32 indexCount = info->indexes.Length();
    for (PRUint32 index = 0; index < indexCount; index++) {
      if (info->indexes[index].name == mName) {
        found = true;
        break;
      }
    }
    NS_ASSERTION(!found, "Alreayd have this index!");
  }
#endif

  IndexInfo* newInfo = info->indexes.AppendElement();
  if (!newInfo) {
    NS_ERROR("Couldn't add index name!  Out of memory?");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  newInfo->id = mId;
  newInfo->name = mName;
  newInfo->keyPath = mKeyPath;
  newInfo->unique = mUnique;
  newInfo->autoIncrement = mAutoIncrement;

  nsCOMPtr<nsIIDBIndex> result;
  nsresult rv = mObjectStore->Index(mName, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  aResult->SetAsISupports(result);
  return OK;
}

PRUint16
RemoveIndexHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(
      "DELETE FROM object_store_index "
      "WHERE name = :name "
    );
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::NOT_FOUND_ERR;
  }

  return OK;
}

PRUint16
RemoveIndexHelper::GetSuccessResult(nsIWritableVariant* /* aResult */)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  ObjectStoreInfo* info = mObjectStore->GetObjectStoreInfo();
  if (!info) {
    NS_ERROR("Unable to get object store info!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

#ifdef DEBUG
  {
    bool found = false;
    PRUint32 indexCount = info->indexes.Length();
    for (PRUint32 index = 0; index < indexCount; index++) {
      if (info->indexes[index].name == mName) {
        found = true;
        break;
      }
    }
    NS_ASSERTION(found, "Didn't know about this one!");
  }
#endif

  PRUint32 indexCount = info->indexes.Length();
  for (PRUint32 index = 0; index < indexCount; index++) {
    if (info->indexes[index].name == mName) {
      info->indexes.RemoveElementAt(index);
      break;
    }
  }

  return OK;
}

PRUint16
GetAllHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCString table;
  nsCString keyColumn;

  if (mAutoIncrement) {
    table.AssignLiteral("ai_object_data");
    keyColumn.AssignLiteral("id");
  }
  else {
    table.AssignLiteral("object_data");
    keyColumn.AssignLiteral("key_value");
  }

  NS_NAMED_LITERAL_CSTRING(osid, "osid");
  NS_NAMED_LITERAL_CSTRING(leftKeyName, "left_key");
  NS_NAMED_LITERAL_CSTRING(rightKeyName, "right_key");

  nsCAutoString keyRangeClause;
  if (!mLeftKey.IsUnset()) {
    keyRangeClause = NS_LITERAL_CSTRING(" AND ") + keyColumn;
    if (mKeyRangeFlags & nsIIDBKeyRange::LEFT_OPEN) {
      keyRangeClause.AppendLiteral(" > :");
    }
    else {
      NS_ASSERTION(mKeyRangeFlags & nsIIDBKeyRange::LEFT_BOUND, "Bad flags!");
      keyRangeClause.AppendLiteral(" >= :");
    }
    keyRangeClause.Append(leftKeyName);
  }

  if (!mRightKey.IsUnset()) {
    keyRangeClause += NS_LITERAL_CSTRING(" AND ") + keyColumn;
    if (mKeyRangeFlags & nsIIDBKeyRange::RIGHT_OPEN) {
      keyRangeClause.AppendLiteral(" < :");
    }
    else {
      NS_ASSERTION(mKeyRangeFlags & nsIIDBKeyRange::RIGHT_BOUND, "Bad flags!");
      keyRangeClause.AppendLiteral(" <= :");
    }
    keyRangeClause.Append(rightKeyName);
  }

  nsCAutoString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT data FROM ") + table +
                    NS_LITERAL_CSTRING(" WHERE object_store_id = :") + osid +
                    keyRangeClause + NS_LITERAL_CSTRING(" ORDER BY ") +
                    keyColumn + NS_LITERAL_CSTRING(" ASC") + limitClause;

  if (!mValues.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(osid, mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (!mLeftKey.IsUnset()) {
    if (mLeftKey.IsString()) {
      rv = stmt->BindStringByName(leftKeyName, mLeftKey.StringValue());
    }
    else if (mLeftKey.IsInt()) {
      rv = stmt->BindInt64ByName(leftKeyName, mLeftKey.IntValue());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  if (!mRightKey.IsUnset()) {
    if (mRightKey.IsString()) {
      rv = stmt->BindStringByName(rightKeyName, mRightKey.StringValue());
    }
    else if (mRightKey.IsInt()) {
      rv = stmt->BindInt64ByName(rightKeyName, mRightKey.IntValue());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  PRBool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mValues.Capacity() == mValues.Length()) {
      if (!mValues.SetCapacity(mValues.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return nsIIDBDatabaseException::UNKNOWN_ERR;
      }
    }

    nsString* value = mValues.AppendElement();
    NS_ASSERTION(value, "Shouldn't fail if SetCapacity succeeded!");

    rv = stmt->GetString(0, *value);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
GetAllHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  NS_ASSERTION(mValues.Length() <= mLimit, "Too many results!");

  nsRefPtr<GetAllSuccessEvent> event(new GetAllSuccessEvent(mValues));

  NS_ASSERTION(mValues.IsEmpty(), "Should have swapped!");

  nsresult rv = event->Init(mRequest, mTransaction);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return OK;
}
