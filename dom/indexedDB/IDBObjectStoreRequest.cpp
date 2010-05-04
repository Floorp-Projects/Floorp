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

class PutHelper : public AsyncConnectionHelper
{
public:
  PutHelper(IDBDatabaseRequest* aDatabase,
            nsIDOMEventTarget* aTarget,
            PRInt64 aObjectStoreID,
            const nsAString& aValue,
            nsIVariant* aKey,
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
  nsCOMPtr<nsIVariant> mKey;
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

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<PutHelper> helper =
    new PutHelper(mDatabase, request, mId, jsonString, aKey, mAutoIncrement,
                  aNoOverwrite);
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
    new GetHelper(mDatabase, request, mId, aKey, mAutoIncrement);
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

    // TODO get string rep
    // stmt->BindStringByName(NS_LITERAL_CSTRING("key_value"), mKey);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  // TODO bind data

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseError::CONSTRAINT_ERR;
  }

  // TODO update indexes if needed

  return NS_SUCCEEDED(transaction.Commit()) ?
         OK :
         nsIIDBDatabaseError::UNKNOWN_ERR;
}

void
PutHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  aResult->SetFromVariant(mKey);
}

PRUint16
GetHelper::DoDatabaseWork()
{
  // Put value into mValue.
  mValue.AssignLiteral("\"foo\"");
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
