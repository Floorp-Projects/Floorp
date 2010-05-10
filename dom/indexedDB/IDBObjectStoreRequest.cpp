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

// XXX remove once we can get jsvals out of nsIVariant
#include "jscntxt.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "nsJSON.h"
#include "IDBEvents.h"

#include "IDBObjectStoreRequest.h"

#include "nsIIDBDatabaseException.h"

#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"
#include "mozilla/Storage.h"

#include "AsyncConnectionHelper.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class PutHelper : public AsyncConnectionHelper
{
public:
  PutHelper(IDBDatabaseRequest* aDatabase,
            IDBRequest* aRequest,
            PRInt64 aObjectStoreID,
            const nsAString& aValue,
            const nsAString& aKeyString,
            PRInt64 aKeyInt,
            bool aAutoIncrement,
            bool aOverwrite)
  : AsyncConnectionHelper(aDatabase, aRequest), mOSID(aObjectStoreID),
    mValue(aValue), mKeyInt(aKeyInt), mAutoIncrement(aAutoIncrement),
    mOverwrite(aOverwrite)
  {
    if (!mAutoIncrement) {
      if (aKeyString.IsVoid()) {
        mKeyString.SetIsVoid(PR_TRUE);
      }
      else {
        mKeyString.Assign(aKeyString);
      }
    }
  }

  PRUint16 DoDatabaseWork();
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  const PRInt64 mOSID;
  nsString mValue;
  nsString mKeyString;
  PRInt64 mKeyInt;
  const bool mAutoIncrement;
  const bool mOverwrite;
};

class GetHelper : public AsyncConnectionHelper
{
public:
  GetHelper(IDBDatabaseRequest* aDatabase,
            IDBRequest* aRequest,
            PRInt64 aObjectStoreID,
            const nsAString& aKeyString,
            PRInt64 aKeyInt,
            bool aAutoIncrement)
  : AsyncConnectionHelper(aDatabase, aRequest), mOSID(aObjectStoreID),
    mKeyInt(aKeyInt), mAutoIncrement(aAutoIncrement)
  {
    if (aKeyString.IsVoid()) {
      mKeyString.SetIsVoid(PR_TRUE);
    }
    else {
      mKeyString.Assign(aKeyString);
    }
  }

  PRUint16 DoDatabaseWork();
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

  // Disabled until we can use nsIVariants with jsvals
  //PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

protected:
  // In-params.
  const PRInt64 mOSID;
  nsString mKeyString;
  const PRInt64 mKeyInt;
  const bool mAutoIncrement;

private:
  // Out-params.
  nsString mValue;
};

class RemoveHelper : public GetHelper
{
public:
  RemoveHelper(IDBDatabaseRequest* aDatabase,
               IDBRequest* aRequest,
               PRInt64 aObjectStoreID,
               const nsAString& aKeyString,
               PRInt64 aKeyInt,
               bool aAutoIncrement)
  : GetHelper(aDatabase, aRequest, aObjectStoreID, aKeyString, aKeyInt,
              aAutoIncrement)
  {
  }

  PRUint16 DoDatabaseWork();
};

// Remove once nsIVariant can handle jsvals
class GetSuccessEvent : public IDBSuccessEvent
{
public:
  GetSuccessEvent(const nsAString& aValue)
  : mValue(aValue)
  { }

  NS_IMETHOD GetResult(nsIVariant** aResult);

  nsresult Init(IDBRequest* aRequest) {
    mSource = aRequest->GetGenerator();

    nsresult rv = InitEvent(NS_LITERAL_STRING(SUCCESS_EVT_STR), PR_FALSE,
                            PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetTrusted(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  nsString mValue;
};

inline
nsresult
GetKeyFromVariant(nsIVariant* aKey,
                  PRBool aAutoIncrement,
                  PRBool aGetting,
                  nsAString& aKeyString,
                  PRInt64* aKeyInt)
{
  NS_ASSERTION(aKey && aKeyInt, "Null pointers!");

  PRUint16 type;
  nsresult rv = aKey->GetDataType(&type);
  NS_ENSURE_SUCCESS(rv, rv);

  // See xpcvariant.cpp, these are the only types we should expect.
  switch (type) {
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
      if (!aAutoIncrement || aGetting) {
        return NS_ERROR_INVALID_ARG;
      }
      aKeyString.SetIsVoid(PR_TRUE);
      *aKeyInt = 0;
      break;

    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      if (aAutoIncrement) {
        return NS_ERROR_INVALID_ARG;
      }
      rv = aKey->GetAsAString(aKeyString);
      NS_ENSURE_SUCCESS(rv, rv);
      *aKeyInt = 0;
      break;

    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_DOUBLE:
      if (aAutoIncrement && !aGetting) {
        return NS_ERROR_INVALID_ARG;
      }
      rv = aKey->GetAsInt64(aKeyInt);
      NS_ENSURE_SUCCESS(rv, rv);
      aKeyString.SetIsVoid(PR_TRUE);
      break;

    default:
      return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

inline
nsresult
GetKeyFromValue(JSContext* aCx,
                jsval aValue,
                const nsString& aKeyPath,
                nsAString& aKeyString,
                PRInt64* aKeyInt)
{
  NS_PRECONDITION(aCx && aKeyInt, "Null pointers!");
  NS_ASSERTION(!aKeyPath.IsVoid(), "This will explode!");

  if (!JSVAL_IS_OBJECT(aValue)) {
    return NS_ERROR_INVALID_ARG;
  }

  jsval key;
  JSBool ok = JS_GetUCProperty(aCx, JSVAL_TO_OBJECT(aValue),
                               reinterpret_cast<const jschar*>(aKeyPath.get()),
                               aKeyPath.Length(), &key);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  if (JSVAL_IS_VOID(key) || JSVAL_IS_NULL(key)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (JSVAL_IS_INT(key)) {
    *aKeyInt = JSVAL_TO_INT(key);
    aKeyString.SetIsVoid(PR_TRUE);
    return NS_OK;
  }

  if (JSVAL_IS_DOUBLE(key)) {
    jsdouble d = *JSVAL_TO_DOUBLE(key);
    if (d < PR_INT32_MIN || d > PR_INT32_MAX) {
      return NS_ERROR_INVALID_ARG;
    }
    *aKeyInt = d;
    aKeyString.SetIsVoid(PR_TRUE);
    return NS_OK;
  }

  if (JSVAL_IS_STRING(key)) {
    JSString* str = JSVAL_TO_STRING(key);
    size_t len = JS_GetStringLength(str);
    if (!len) {
      return NS_ERROR_INVALID_ARG;
    }
    const PRUnichar* chars =
      reinterpret_cast<const PRUnichar*>(JS_GetStringChars(str));
    aKeyString.Assign(chars, len);
    *aKeyInt = 0;
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

} // anonymous namespace

// static
already_AddRefed<IDBObjectStoreRequest>
IDBObjectStoreRequest::Create(IDBDatabaseRequest* aDatabase,
                              const nsAString& aName,
                              const nsAString& aKeyPath,
                              PRBool aAutoIncrement,
                              PRUint16 aMode,
                              PRInt64 aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBObjectStoreRequest> objectStore = new IDBObjectStoreRequest();

  objectStore->mDatabase = aDatabase;
  objectStore->mName.Assign(aName);
  if (aKeyPath.IsVoid()) {
    objectStore->mKeyPath.SetIsVoid(PR_TRUE);
  }
  else {
    objectStore->mKeyPath.Assign(aKeyPath);
  }
  objectStore->mAutoIncrement = aAutoIncrement;
  objectStore->mMode = aMode;
  objectStore->mId = aId;

  return objectStore.forget();
}

IDBObjectStoreRequest::IDBObjectStoreRequest()
: mAutoIncrement(PR_FALSE),
  mMode(nsIIDBObjectStore::READ_WRITE),
  mId(LL_MININT)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBObjectStoreRequest::~IDBObjectStoreRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

NS_IMPL_ADDREF(IDBObjectStoreRequest)
NS_IMPL_RELEASE(IDBObjectStoreRequest)

NS_INTERFACE_MAP_BEGIN(IDBObjectStoreRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIDBObjectStoreRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBObjectStoreRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBObjectStore)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBObjectStoreRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBObjectStoreRequest, IDBObjectStoreRequest)

NS_IMETHODIMP
IDBObjectStoreRequest::GetMode(PRUint16* aMode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aMode = mMode;
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStoreRequest::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStoreRequest::GetKeyPath(nsAString& aKeyPath)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mKeyPath.IsVoid()) {
    aKeyPath.SetIsVoid(PR_TRUE);
  }
  else {
    aKeyPath.Assign(mKeyPath);
  }
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStoreRequest::GetIndexNames(nsIDOMDOMStringList** aIndexNames)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  PRUint32 count = mIndexes.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(mIndexes[index]), NS_ERROR_OUT_OF_MEMORY);
  }

  list.forget(aIndexNames);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStoreRequest::Put(nsIVariant* /* aValue */,
                           nsIVariant* aKey,
                           PRBool aOverwrite,
                           nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsString keyString;
  PRInt64 keyInt;

  // This is the slow path, need to do this better once nsIVariants can have
  // raw jsvals inside them.
  NS_WARNING("Using a slow path for Put! Fix this now!");

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
  rv = nsContentUtils::CreateStructuredClone(cx, argv[0], clone.addr());
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIJSON> json(new nsJSON());

  nsString jsonString;
  rv = json->EncodeFromJSVal(clone.addr(), cx, jsonString);
  NS_ENSURE_SUCCESS(rv, rv);

  // Inline keys should check the object first.
  if (!mKeyPath.IsVoid()) {
    rv = GetKeyFromValue(cx, clone.value(), mKeyPath, keyString, &keyInt);
  }
  else {
    rv = GetKeyFromVariant(aKey, mAutoIncrement, PR_FALSE, keyString, &keyInt);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<PutHelper> helper =
    new PutHelper(mDatabase, request, mId, jsonString, keyString, keyInt,
                  !!mAutoIncrement, !!aOverwrite);
  rv = helper->Dispatch(mDatabase->ConnectionThread());
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStoreRequest::Remove(nsIVariant* aKey,
                              nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsString keyString;
  PRInt64 keyInt;

  nsresult rv = GetKeyFromVariant(aKey, mAutoIncrement, PR_TRUE,
                                  keyString, &keyInt);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<RemoveHelper> helper =
    new RemoveHelper(mDatabase, request, mId, keyString, keyInt,
                     !!mAutoIncrement);
  rv = helper->Dispatch(mDatabase->ConnectionThread());
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStoreRequest::Get(nsIVariant* aKey,
                           nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsString keyString;
  PRInt64 keyInt;

  nsresult rv = GetKeyFromVariant(aKey, mAutoIncrement, PR_TRUE,
                                  keyString, &keyInt);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetHelper> helper =
    new GetHelper(mDatabase, request, mId, keyString, keyInt, !!mAutoIncrement);
  rv = helper->Dispatch(mDatabase->ConnectionThread());
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBObjectStoreRequest::OpenCursor(nsIIDBKeyRange* aRange,
                                  PRUint16 aDirection,
                                  nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

PRUint16
PutHelper::DoDatabaseWork()
{
  nsresult rv = mDatabase->EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection = mDatabase->Connection();

  // Rollback on any errors.
  mozStorageTransaction transaction(connection, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> stmt =
    mDatabase->PutStatement(!mOverwrite, mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);
  if (!mAutoIncrement || mAutoIncrement && mOverwrite) {
    NS_NAMED_LITERAL_CSTRING(keyValue, "key_value");

    if (mKeyString.IsVoid()) {
      rv = stmt->BindInt64ByName(keyValue, mKeyInt);
    }
    else {
      rv = stmt->BindStringByName(keyValue, mKeyString);
    }
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("data"), mValue);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::CONSTRAINT_ERR;
  }

  if (mAutoIncrement) {
    rv = connection->GetLastInsertRowID(&mKeyInt);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  // TODO update indexes if needed

  return NS_SUCCEEDED(transaction.Commit()) ?
         OK :
         nsIIDBDatabaseException::UNKNOWN_ERR;
}

PRUint16
PutHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  if (mAutoIncrement || mKeyString.IsVoid()) {
    aResult->SetAsInt64(mKeyInt);
  }
  else {
    aResult->SetAsAString(mKeyString);
  }
  return OK;
}

PRUint16
GetHelper::DoDatabaseWork()
{
  nsresult rv = mDatabase->EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection = mDatabase->Connection();

  // XXX pull statement creation into mDatabase or something for efficiency.
  nsCOMPtr<mozIStorageStatement> stmt;
  if (mAutoIncrement) {
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT data "
      "FROM ai_object_data "
      "WHERE id = :id "
      "AND object_store_id = :osid"
    ), getter_AddRefs(stmt));
  }
  else {
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT data "
      "FROM object_data "
      "WHERE key_value = :id "
      "AND object_store_id = :osid"
    ), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(id, "id");

  if (mKeyString.IsVoid()) {
    rv = stmt->BindInt64ByName(id, mKeyInt);
  }
  else {
    rv = stmt->BindStringByName(id, mKeyString);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  // Search for it!
  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  NS_ENSURE_TRUE(hasResult, nsIIDBDatabaseException::NOT_FOUND_ERR);

  // Set the value based on results.
  (void)stmt->GetString(0, mValue);

  return OK;
}

PRUint16
GetHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetSuccessEvent> event(new GetSuccessEvent(mValue));
  nsresult rv = event->Init(mRequest);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return OK;
}

PRUint16
RemoveHelper::DoDatabaseWork()
{
  nsresult rv = mDatabase->EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection = mDatabase->Connection();

  nsCOMPtr<mozIStorageStatement> stmt =
    mDatabase->RemoveStatement(mAutoIncrement);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(key_value, "key_value");

  if (mKeyString.IsVoid()) {
    rv = stmt->BindInt64ByName(key_value, mKeyInt);
  }
  else {
    rv = stmt->BindStringByName(key_value, mKeyString);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  // Search for it!
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

// Remove once nsIVariant supports jsvals!
NS_IMETHODIMP
GetSuccessEvent::GetResult(nsIVariant** aResult)
{
  // This is the slow path, need to do this better once nsIVariants can have
  // raw jsvals inside them.
  NS_WARNING("Using a slow path for Get! Fix this now!");

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

  nsAXPCNativeCallContext* cc;
  nsresult rv = xpc->GetCurrentNativeCallContext(&cc);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(cc, NS_ERROR_UNEXPECTED);

  jsval* retval;
  rv = cc->GetRetValPtr(&retval);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext* cx;
  rv = cc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  nsCOMPtr<nsIJSON> json(new nsJSON());
  rv = json->DecodeToJSVal(mValue, cx, retval);
  NS_ENSURE_SUCCESS(rv, rv);

  cc->SetReturnValueWasSet(PR_TRUE);
  return NS_OK;
}
