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

#include "nsIIDBDatabaseError.h"

#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"
#include "mozilla/Storage.h"

#include "AsyncConnectionHelper.h"

USING_INDEXEDDB_NAMESPACE

namespace {

// XXX this goes away when we use jsvals
void
VariantToString(nsIVariant* aValue,
                nsString& _retval)
{
  _retval.SetIsVoid(PR_TRUE);

  PRUint16 type;
  (void)aValue->GetDataType(&type);
  switch (type) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    {
      PRInt32 value;
      nsresult rv = aValue->GetAsInt32(&value);
      if (NS_SUCCEEDED(rv)) {
        _retval.AppendInt(value);
      }
    }
    case nsIDataType::VTYPE_UINT32: // Try to preserve full range
    case nsIDataType::VTYPE_INT64:
    // Data loss possible, but there is no unsigned types in SQLite
    case nsIDataType::VTYPE_UINT64:
    {
      PRInt64 value;
      nsresult rv = aValue->GetAsInt64(&value);
      if (NS_SUCCEEDED(rv)) {
        _retval.AppendInt(value);
      }
    }
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    {
      double value;
      nsresult rv = aValue->GetAsDouble(&value);
      if (NS_SUCCEEDED(rv)) {
        _retval.AppendFloat(value);
      }
    }
    case nsIDataType::VTYPE_BOOL:
    {
      PRBool value;
      nsresult rv = aValue->GetAsBool(&value);
      if (NS_SUCCEEDED(rv)) {
        _retval.AppendInt(value ? 1 : 0);
      }
    }
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    {
      nsCAutoString value;
      nsresult rv = aValue->GetAsAUTF8String(value);
      if (NS_SUCCEEDED(rv)) {
        _retval = NS_ConvertUTF8toUTF16(value);
      }
    }
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_ASTRING:
    {
      // GetAsAString does proper conversion to UCS2 from all string-like types.
      // It can be used universally without problems (unless someone implements
      // their own variant, but that's their problem).
      (void)aValue->GetAsAString(_retval);
    }
    default:
      NS_WARNING("Unknown type; cannot handle!");
  }
}

class PutHelper : public AsyncConnectionHelper
{
public:
  PutHelper(IDBDatabaseRequest* aDatabase,
            nsIDOMEventTarget* aTarget,
            PRInt64 aObjectStoreID,
            const nsAString& aValue,
            const nsAString& aKey,
            bool aAutoIncrement,
            bool aNoOverwrite)
  : AsyncConnectionHelper(aDatabase, aTarget), mOSID(aObjectStoreID),
    mValue(aValue), mKey(aKey), mAutoIncrement(aAutoIncrement),
    mNoOverwrite(aNoOverwrite)
  { }

  PRUint16 DoDatabaseWork();
  void GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  const PRInt64 mOSID;
  nsString mValue;
  nsString mKey;
  const bool mAutoIncrement;
  const bool mNoOverwrite;
};

class GetHelper : public AsyncConnectionHelper
{
public:
  GetHelper(IDBDatabaseRequest* aDatabase,
            nsIDOMEventTarget* aTarget,
            PRInt64 aObjectStoreID,
            nsIVariant* aKey,
            bool aAutoIncrement)
  : AsyncConnectionHelper(aDatabase, aTarget), mOSID(aObjectStoreID),
    mKey(aKey), mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork();
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

  // Disabled until we can use nsIVariants with jsvals
  //void GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  const PRInt64 mOSID;
  nsCOMPtr<nsIVariant> mKey;
  const bool mAutoIncrement;

  // Out-params.
  nsString mValue;
};

// Remove once nsIVariant can handle jsvals
class GetSuccessEvent : public IDBSuccessEvent
{
public:
  GetSuccessEvent(const nsAString& aValue)
  : mValue(aValue)
  { }

  NS_IMETHOD GetResult(nsIVariant** aResult);

  void Init() {
    IDBSuccessEvent::Init();
  }

private:
  nsString mValue;
};

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
                           PRBool aNoOverwrite,
                           nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

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

  if (argc < 2) {
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

  nsString keyString;
  VariantToString(aKey, keyString);
  NS_ENSURE_TRUE(mAutoIncrement == keyString.IsVoid(), NS_ERROR_INVALID_ARG);

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<PutHelper> helper =
    new PutHelper(mDatabase, request, mId, jsonString, keyString,
                  !!mAutoIncrement, !!aNoOverwrite);
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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IDBObjectStoreRequest::Get(nsIVariant* aKey,
                           nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetHelper> helper =
    new GetHelper(mDatabase, request, mId, aKey, !!mAutoIncrement);
  nsresult rv = helper->Dispatch(mDatabase->ConnectionThread());
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
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection = mDatabase->Connection();
  return OK;

  // Rollback on any errors.
  mozStorageTransaction transaction(connection, PR_FALSE);

  // TODO handle overwrite = true (use OR REPLACE?)

  // XXX pull statement creation into mDatabase or something for efficiency.
  nsCOMPtr<mozIStorageStatement> stmt;
  if (mAutoIncrement) {
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO ai_object_data (object_store_id, data)"
      "VALUES (:osid, :data)"
    ), getter_AddRefs(stmt));
  }
  else {
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO object_data (object_store_id, key_value, data)"
      "VALUES (:osid, :key_value, :data)"
    ), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

    stmt->BindStringByName(NS_LITERAL_CSTRING("key_value"), mKey);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("data"), mValue);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseError::CONSTRAINT_ERR;
  }

  // TODO set mKey to the key we used

  // TODO update indexes if needed

  return NS_SUCCEEDED(transaction.Commit()) ?
         OK :
         nsIIDBDatabaseError::UNKNOWN_ERR;
}

void
PutHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  // XXX this is wrong if they pass a number; we are giving back a string
  aResult->SetAsAString(mKey);
}

PRUint16
GetHelper::DoDatabaseWork()
{
  nsresult rv = mDatabase->EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection = mDatabase->Connection();

  // XXX pull statement creation into mDatabase or something for efficiency.
  nsCOMPtr<mozIStorageStatement> stmt;
  if (mAutoIncrement) {
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT data "
      "FROM ai_object_data "
      "WERE id = :id "
      "AND object_store_id = :osid"
    ), getter_AddRefs(stmt));
  }
  else {
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT data "
      "FROM object_data "
      "WERE key_value = :id "
      "AND object_store_id = :osid"
    ), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  // TODO probably should switch on type for this
  nsString id;
  VariantToString(mKey, id);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("id"), id);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  // Search for it!
  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);
  NS_ENSURE_TRUE(hasResult, nsIIDBDatabaseError::NOT_FOUND_ERR);

  // Set the value based on results.
  (void)stmt->GetString(0, mValue);

  return OK;
}

PRUint16
GetHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetSuccessEvent> event(new GetSuccessEvent(mValue));
  event->Init();

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
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
