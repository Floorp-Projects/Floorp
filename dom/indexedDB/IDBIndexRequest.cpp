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
 *   Shawn Wilsher <me@shawnwilsher.com>
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


#include "IDBIndexRequest.h"

#include "nsIIDBDatabaseException.h"

#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"
#include "mozilla/Storage.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBObjectStoreRequest.h"
#include "IDBTransactionRequest.h"
#include "DatabaseInfo.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class GetHelper : public AsyncConnectionHelper
{
public:
  GetHelper(IDBTransactionRequest* aTransaction,
            IDBRequest* aRequest,
            const nsAString& aValue,
            PRInt64 aId,
            bool aUnique,
            bool aAutoIncrement)
  : AsyncConnectionHelper(aTransaction, aRequest), mValue(aValue), mId(aId),
    mUnique(aUnique), mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  const nsString mValue;
  const PRInt64 mId;
  const bool mUnique;
  const bool mAutoIncrement;

  // Out-params.
  Key mKey;
};

} // anonymous namespace

// static
already_AddRefed<IDBIndexRequest>
IDBIndexRequest::Create(IDBObjectStoreRequest* aObjectStore,
                        const IndexInfo* aIndexInfo)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aObjectStore, "Null pointer!");
  NS_ASSERTION(aIndexInfo, "Null pointer!");

  nsRefPtr<IDBIndexRequest> index = new IDBIndexRequest();

  index->mObjectStore = aObjectStore;
  index->mId = aIndexInfo->id;
  index->mName = aIndexInfo->name;
  index->mKeyPath = aIndexInfo->keyPath;
  index->mUnique = aIndexInfo->unique;

  return index.forget();
}

IDBIndexRequest::IDBIndexRequest()
: mId(LL_MININT),
  mUnique(false),
  mAutoIncrement(false)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
}

IDBIndexRequest::~IDBIndexRequest()
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
}

NS_IMPL_ADDREF(IDBIndexRequest)
NS_IMPL_RELEASE(IDBIndexRequest)

NS_INTERFACE_MAP_BEGIN(IDBIndexRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, IDBRequest::Generator)
  NS_INTERFACE_MAP_ENTRY(nsIIDBIndexRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBIndex)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBIndexRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBIndexRequest, IDBIndexRequest)

////////////////////////////////////////////////////////////////////////////////
//// nsIIDBIndex

NS_IMETHODIMP
IDBIndexRequest::GetName(nsAString& aName)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndexRequest::GetStoreName(nsAString& aStoreName)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  return mObjectStore->GetName(aStoreName);
}

NS_IMETHODIMP
IDBIndexRequest::GetKeyPath(nsAString& aKeyPath)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  aKeyPath.Assign(mKeyPath);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndexRequest::GetUnique(PRBool* aUnique)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  *aUnique = mUnique;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIIDBIndexRequest

NS_IMETHODIMP
IDBIndexRequest::OpenObjectCursor(nsIIDBKeyRange* aRange,
                                  PRUint16 aDirection,
                                  nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IDBIndexRequest::OpenCursor(nsIIDBKeyRange* aRange,
                            PRUint16 aDirection,
                            nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IDBIndexRequest::Put(nsIVariant* aValue,
                     nsIVariant* aKey,
                     PRBool aOverwrite,
                     nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IDBIndexRequest::GetObject(nsIVariant* /* aKey */,
                           nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  NS_WARNING("Using a slow path for GetObject! Fix this now!");

  nsString jsonValue;
  nsresult rv = IDBObjectStoreRequest::GetJSONFromArg0(jsonValue);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IDBIndexRequest::Get(nsIVariant* /* aKey */,
                     nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  NS_WARNING("Using a slow path for Get! Fix this now!");

  nsString jsonValue;
  nsresult rv = IDBObjectStoreRequest::GetJSONFromArg0(jsonValue);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetHelper> helper =
    new GetHelper(mObjectStore->Transaction(), request, jsonValue, mId, mUnique,
                  mAutoIncrement);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndexRequest::Remove(nsIVariant* aKey,
                        nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

PRUint16
GetHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->IndexGetStatement(mUnique, mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"), mId);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("value"), mValue);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (hasResult) {
    PRInt32 keyType;
    rv = stmt->GetTypeOfIndex(0, &keyType);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      mKey = stmt->AsInt64(0);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(0, mKey.ToString());
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
    }
  }

  return OK;
}

PRUint16
GetHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_ASSERTION(!mKey.IsNull(), "Badness!");

  if (mKey.IsUnset()) {
    aResult->SetAsEmpty();
  }
  else if (mKey.IsString()) {
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
