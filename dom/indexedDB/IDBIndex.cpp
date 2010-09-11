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


#include "IDBIndex.h"

#include "nsIIDBDatabaseException.h"
#include "nsIIDBKeyRange.h"

#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"
#include "mozilla/storage.h"

#include "AsyncConnectionHelper.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "DatabaseInfo.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class GetHelper : public AsyncConnectionHelper
{
public:
  GetHelper(IDBTransaction* aTransaction,
            IDBRequest* aRequest,
            const Key& aKey,
            PRInt64 aId,
            bool aUnique,
            bool aAutoIncrement)
  : AsyncConnectionHelper(aTransaction, aRequest), mKey(aKey), mId(aId),
    mUnique(aUnique), mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

protected:
  // In-params.
  Key mKey;
  const PRInt64 mId;
  const bool mUnique;
  const bool mAutoIncrement;
};

class GetObjectHelper : public GetHelper
{
public:
  GetObjectHelper(IDBTransaction* aTransaction,
                  IDBRequest* aRequest,
                  const Key& aKey,
                  PRInt64 aId,
                  bool aUnique,
                  bool aAutoIncrement)
  : GetHelper(aTransaction, aRequest, aKey, aId, aUnique, aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  nsString mValue;
};

class GetAllHelper : public GetHelper
{
public:
  GetAllHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               const Key& aKey,
               PRInt64 aId,
               bool aUnique,
               bool aAutoIncrement,
               const PRUint32 aLimit)
  : GetHelper(aTransaction, aRequest, aKey, aId, aUnique, aAutoIncrement),
    mLimit(aLimit)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  const PRUint32 mLimit;
  nsTArray<Key> mKeys;
};

class GetAllObjectsHelper : public GetHelper
{
public:
  GetAllObjectsHelper(IDBTransaction* aTransaction,
                      IDBRequest* aRequest,
                      const Key& aKey,
                      PRInt64 aId,
                      bool aUnique,
                      bool aAutoIncrement,
                      const PRUint32 aLimit)
  : GetHelper(aTransaction, aRequest, aKey, aId, aUnique, aAutoIncrement),
    mLimit(aLimit)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  const PRUint32 mLimit;
  nsTArray<nsString> mValues;
};

class OpenCursorHelper : public AsyncConnectionHelper
{
public:
  OpenCursorHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   IDBIndex* aIndex,
                   PRInt64 aId,
                   bool aUnique,
                   bool aAutoIncrement,
                   const Key& aLeftKey,
                   const Key& aRightKey,
                   PRUint16 aKeyRangeFlags,
                   PRUint16 aDirection,
                   PRBool aPreload)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex), mId(aId),
    mUnique(aUnique), mAutoIncrement(aAutoIncrement), mLeftKey(aLeftKey),
    mRightKey(aRightKey), mKeyRangeFlags(aKeyRangeFlags),
    mDirection(aDirection), mPreload(aPreload)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  const PRInt64 mId;
  const bool mUnique;
  const bool mAutoIncrement;
  const Key mLeftKey;
  const Key mRightKey;
  const PRUint16 mKeyRangeFlags;
  const PRUint16 mDirection;
  const PRBool mPreload;

  // Out-params.
  nsTArray<KeyKeyPair> mData;
};

class OpenObjectCursorHelper : public AsyncConnectionHelper
{
public:
  OpenObjectCursorHelper(IDBTransaction* aTransaction,
                         IDBRequest* aRequest,
                         IDBIndex* aIndex,
                         PRInt64 aId,
                         bool aUnique,
                         bool aAutoIncrement,
                         const Key& aLeftKey,
                         const Key& aRightKey,
                         PRUint16 aKeyRangeFlags,
                         PRUint16 aDirection,
                         PRBool aPreload)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex), mId(aId),
    mUnique(aUnique), mAutoIncrement(aAutoIncrement), mLeftKey(aLeftKey),
    mRightKey(aRightKey), mKeyRangeFlags(aKeyRangeFlags),
    mDirection(aDirection), mPreload(aPreload)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  const PRInt64 mId;
  const bool mUnique;
  const bool mAutoIncrement;
  const Key mLeftKey;
  const Key mRightKey;
  const PRUint16 mKeyRangeFlags;
  const PRUint16 mDirection;
  const PRBool mPreload;

  // Out-params.
  nsTArray<KeyValuePair> mData;
};

inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBIndex* aIndex)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  IDBDatabase* database = aIndex->ObjectStore()->Transaction()->Database();
  return IDBRequest::Create(static_cast<nsPIDOMEventTarget*>(aIndex),
                            database->ScriptContext(), database->Owner());
}

} // anonymous namespace

// static
already_AddRefed<IDBIndex>
IDBIndex::Create(IDBObjectStore* aObjectStore,
                 const IndexInfo* aIndexInfo)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aObjectStore, "Null pointer!");
  NS_ASSERTION(aIndexInfo, "Null pointer!");

  IDBDatabase* database = aObjectStore->Transaction()->Database();

  nsRefPtr<IDBIndex> index = new IDBIndex();

  index->mScriptContext = database->ScriptContext();
  index->mOwner = database->Owner();

  index->mObjectStore = aObjectStore;
  index->mId = aIndexInfo->id;
  index->mName = aIndexInfo->name;
  index->mKeyPath = aIndexInfo->keyPath;
  index->mUnique = aIndexInfo->unique;
  index->mAutoIncrement = aIndexInfo->autoIncrement;

  return index.forget();
}

IDBIndex::IDBIndex()
: mId(LL_MININT),
  mUnique(false),
  mAutoIncrement(false)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
}

IDBIndex::~IDBIndex()
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBIndex)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBIndex,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mObjectStore,
                                                       nsPIDOMEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBIndex,
                                                nsDOMEventTargetHelper)
  // Don't unlink mObjectStore!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBIndex)
  NS_INTERFACE_MAP_ENTRY(nsIIDBIndex)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBIndex)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBIndex, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBIndex, nsDOMEventTargetHelper)

DOMCI_DATA(IDBIndex, IDBIndex)

NS_IMETHODIMP
IDBIndex::GetName(nsAString& aName)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetStoreName(nsAString& aStoreName)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  return mObjectStore->GetName(aStoreName);
}

NS_IMETHODIMP
IDBIndex::GetKeyPath(nsAString& aKeyPath)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  aKeyPath.Assign(mKeyPath);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetUnique(PRBool* aUnique)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  *aUnique = mUnique;
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::Get(nsIVariant* aKey,
              nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (key.IsUnset() || key.IsNull()) {
    return NS_ERROR_INVALID_ARG;
  }

  IDBTransaction* transaction = mObjectStore->Transaction();

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetHelper> helper =
    new GetHelper(transaction, request, key, mId, mUnique, mAutoIncrement);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetObject(nsIVariant* aKey,
                    nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (key.IsUnset() || key.IsNull()) {
    return NS_ERROR_INVALID_ARG;
  }

  IDBTransaction* transaction = mObjectStore->Transaction();

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetObjectHelper> helper =
    new GetObjectHelper(transaction, request, key, mId, mUnique,
                        mAutoIncrement);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetAll(nsIVariant* aKey,
                 PRUint32 aLimit,
                 PRUint8 aOptionalArgCount,
                 nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (key.IsNull()) {
    key = Key::UNSETKEY;
  }

  if (aOptionalArgCount < 2) {
    aLimit = PR_UINT32_MAX;
  }

  IDBTransaction* transaction = mObjectStore->Transaction();

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(transaction, request, key, mId, mUnique, mAutoIncrement,
                     aLimit);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetAllObjects(nsIVariant* aKey,
                        PRUint32 aLimit,
                        PRUint8 aOptionalArgCount,
                        nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (key.IsNull()) {
    key = Key::UNSETKEY;
  }

  if (aOptionalArgCount < 2) {
    aLimit = PR_UINT32_MAX;
  }

  IDBTransaction* transaction = mObjectStore->Transaction();

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<GetAllObjectsHelper> helper =
    new GetAllObjectsHelper(transaction, request, key, mId, mUnique,
                            mAutoIncrement, aLimit);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::OpenCursor(nsIIDBKeyRange* aKeyRange,
                     PRUint16 aDirection,
                     PRBool aPreload,
                     PRUint8 aOptionalArgCount,
                     nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mObjectStore->TransactionIsOpen()) {
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

    rv = IDBObjectStore::GetKeyFromVariant(variant, leftKey);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aKeyRange->GetRight(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = IDBObjectStore::GetKeyFromVariant(variant, rightKey);
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

  IDBTransaction* transaction = mObjectStore->Transaction();

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(transaction, request, this, mId, mUnique,
                         mAutoIncrement, leftKey, rightKey, keyRangeFlags,
                         aDirection, aPreload);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::OpenObjectCursor(nsIIDBKeyRange* aKeyRange,
                           PRUint16 aDirection,
                           PRBool aPreload,
                           PRUint8 aOptionalArgCount,
                           nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mObjectStore->TransactionIsOpen()) {
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

    rv = IDBObjectStore::GetKeyFromVariant(variant, leftKey);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aKeyRange->GetRight(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = IDBObjectStore::GetKeyFromVariant(variant, rightKey);
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

  IDBTransaction* transaction = mObjectStore->Transaction();

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<OpenObjectCursorHelper> helper =
    new OpenObjectCursorHelper(transaction, request, this, mId, mUnique,
                               mAutoIncrement, leftKey, rightKey, keyRangeFlags,
                               aDirection, aPreload);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
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

  NS_NAMED_LITERAL_CSTRING(value, "value");

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(value, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(value, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Bad key type!");
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  mKey = Key::UNSETKEY;

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
    else {
      NS_NOTREACHED("Bad SQLite type!");
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

PRUint16
GetObjectHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->IndexGetObjectStatement(mUnique, mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"), mId);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(value, "value");

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(value, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(value, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Bad key type!");
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  mKey = Key::UNSETKEY;

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (hasResult) {
    rv = stmt->GetString(0, mValue);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }
  else {
    mValue.SetIsVoid(PR_TRUE);
  }

  return OK;
}

PRUint16
GetObjectHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetSuccessEvent> event(new GetSuccessEvent(mValue));
  nsresult rv = event->Init(mRequest, mTransaction);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return OK;
}

PRUint16
GetAllHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mKeys.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsCString keyColumn;
  nsCString tableName;

  if (mAutoIncrement) {
    keyColumn.AssignLiteral("ai_object_data_id");
    if (mUnique) {
      tableName.AssignLiteral("ai_unique_index_data");
    }
    else {
      tableName.AssignLiteral("ai_index_data");
    }
  }
  else {
    keyColumn.AssignLiteral("object_data_key");
    if (mUnique) {
      tableName.AssignLiteral("unique_index_data");
    }
    else {
      tableName.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(value, "value");

  nsCString keyClause;
  if (!mKey.IsUnset()) {
    keyClause = NS_LITERAL_CSTRING(" AND ") + value +
                NS_LITERAL_CSTRING(" = :") + value;
  }

  nsCString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT ") + keyColumn +
                    NS_LITERAL_CSTRING(" FROM ") + tableName +
                    NS_LITERAL_CSTRING(" WHERE ") + indexId  +
                    NS_LITERAL_CSTRING(" = :") + indexId + keyClause +
                    limitClause;
  
  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mId);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (!mKey.IsUnset()) {
    if (mKey.IsInt()) {
      rv = stmt->BindInt64ByName(value, mKey.IntValue());
    }
    else if (mKey.IsString()) {
      rv = stmt->BindStringByName(value, mKey.StringValue());
    }
    else {
      NS_NOTREACHED("Bad key type!");
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  PRBool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mKeys.Capacity() == mKeys.Length()) {
      if (!mKeys.SetCapacity(mKeys.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return nsIIDBDatabaseException::UNKNOWN_ERR;
      }
    }

    Key* key = mKeys.AppendElement();
    NS_ASSERTION(key, "This shouldn't fail!");

    PRInt32 keyType;
    rv = stmt->GetTypeOfIndex(0, &keyType);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      *key = stmt->AsInt64(0);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(0, key->ToString());
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
    }
    else {
      NS_NOTREACHED("Bad SQLite type!");
    }
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
GetAllHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetAllKeySuccessEvent> event(new GetAllKeySuccessEvent(mKeys));

  NS_ASSERTION(mKeys.IsEmpty(), "Should have swapped!");

  nsresult rv = event->Init(mRequest, mTransaction);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return OK;
}

PRUint16
GetAllObjectsHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mValues.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsCString dataTableName;
  nsCString objectDataId;
  nsCString indexTableName;

  if (mAutoIncrement) {
    dataTableName.AssignLiteral("ai_object_data");
    objectDataId.AssignLiteral("ai_object_data_id");
    if (mUnique) {
      indexTableName.AssignLiteral("ai_unique_index_data");
    }
    else {
      indexTableName.AssignLiteral("ai_index_data");
    }
  }
  else {
    dataTableName.AssignLiteral("object_data");
    objectDataId.AssignLiteral("object_data_id");
    if (mUnique) {
      indexTableName.AssignLiteral("unique_index_data");
    }
    else {
      indexTableName.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(value, "value");

  nsCString keyClause;
  if (!mKey.IsUnset()) {
    keyClause = NS_LITERAL_CSTRING(" AND ") + value +
                NS_LITERAL_CSTRING(" = :") + value;
  }

  nsCString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT data FROM ") + dataTableName +
                    NS_LITERAL_CSTRING(" INNER JOIN ") + indexTableName  +
                    NS_LITERAL_CSTRING(" ON ") + dataTableName +
                    NS_LITERAL_CSTRING(".id = ") + indexTableName +
                    NS_LITERAL_CSTRING(".") + objectDataId +
                    NS_LITERAL_CSTRING(" WHERE ") + indexId  +
                    NS_LITERAL_CSTRING(" = :") + indexId + keyClause +
                    limitClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mId);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (!mKey.IsUnset()) {
    if (mKey.IsInt()) {
      rv = stmt->BindInt64ByName(value, mKey.IntValue());
    }
    else if (mKey.IsString()) {
      rv = stmt->BindStringByName(value, mKey.StringValue());
    }
    else {
      NS_NOTREACHED("Bad key type!");
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  PRBool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mValues.Capacity() == mValues.Length()) {
      if (!mValues.SetCapacity(mValues.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return nsIIDBDatabaseException::UNKNOWN_ERR;
      }
    }

    nsString* value = mValues.AppendElement();
    NS_ASSERTION(value, "This shouldn't fail!");

#ifdef DEBUG
    {
      PRInt32 keyType;
      NS_ASSERTION(NS_SUCCEEDED(stmt->GetTypeOfIndex(0, &keyType)) &&
                   keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                   "Bad SQLITE type!");
    }
#endif

    rv = stmt->GetString(0, *value);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
GetAllObjectsHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetAllSuccessEvent> event(new GetAllSuccessEvent(mValues));

  NS_ASSERTION(mValues.IsEmpty(), "Should have swapped!");

  nsresult rv = event->Init(mRequest, mTransaction);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return OK;
}

PRUint16
OpenCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mData.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsCString table;
  nsCString keyColumn;

  if (mAutoIncrement) {
    keyColumn.AssignLiteral("ai_object_data_id");
    if (mUnique) {
      table.AssignLiteral("ai_unique_index_data");
    }
    else {
      table.AssignLiteral("ai_index_data");
    }
  }
  else {
    keyColumn.AssignLiteral("object_data_key");
    if (mUnique) {
      table.AssignLiteral("unique_index_data");
    }
    else {
      table.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(leftKeyName, "left_key");
  NS_NAMED_LITERAL_CSTRING(rightKeyName, "right_key");

  nsCAutoString keyRangeClause;
  if (!mLeftKey.IsUnset()) {
    keyRangeClause = NS_LITERAL_CSTRING(" AND value");
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
    keyRangeClause.AppendLiteral(" AND value");
    if (mKeyRangeFlags & nsIIDBKeyRange::RIGHT_OPEN) {
      keyRangeClause.AppendLiteral(" < :");
    }
    else {
      NS_ASSERTION(mKeyRangeFlags & nsIIDBKeyRange::RIGHT_BOUND, "Bad flags!");
      keyRangeClause.AppendLiteral(" <= :");
    }
    keyRangeClause.Append(rightKeyName);
  }

  nsCString groupClause;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::PREV:
      break;

    case nsIIDBCursor::NEXT_NO_DUPLICATE:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      groupClause = NS_LITERAL_CSTRING(" GROUP BY value");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }

  nsCString directionClause;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::NEXT_NO_DUPLICATE:
      directionClause = NS_LITERAL_CSTRING("DESC");
      break;

    case nsIIDBCursor::PREV:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      directionClause = NS_LITERAL_CSTRING("ASC");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT value, ") + keyColumn +
                    NS_LITERAL_CSTRING(" FROM ") + table +
                    NS_LITERAL_CSTRING(" WHERE index_id = :") + indexId +
                    keyRangeClause + groupClause +
                    NS_LITERAL_CSTRING(" ORDER BY value ") + directionClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mId);
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
    if (mData.Capacity() == mData.Length()) {
      if (!mData.SetCapacity(mData.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return nsIIDBDatabaseException::UNKNOWN_ERR;
      }
    }

    KeyKeyPair* pair = mData.AppendElement();
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

    rv = stmt->GetTypeOfIndex(1, &keyType);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      pair->value = stmt->AsInt64(1);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(1, pair->value.ToString());
      NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
    }
    else {
      NS_NOTREACHED("Bad SQLite type!");
    }
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
    IDBCursor::Create(mRequest, mTransaction, mIndex, mDirection, mData);
  NS_ENSURE_TRUE(cursor, nsIIDBDatabaseException::UNKNOWN_ERR);

  aResult->SetAsISupports(static_cast<nsPIDOMEventTarget*>(cursor));
  return OK;
}

PRUint16
OpenObjectCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mData.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsCString indexTable;
  nsCString objectTable;
  nsCString objectDataId;

  if (mAutoIncrement) {
    objectTable.AssignLiteral("ai_object_data");
    objectDataId.AssignLiteral("ai_object_data_id");
    if (mUnique) {
      indexTable.AssignLiteral("ai_unique_index_data");
    }
    else {
      indexTable.AssignLiteral("ai_index_data");
    }
  }
  else {
    objectTable.AssignLiteral("object_data");
    objectDataId.AssignLiteral("object_data_id");
    if (mUnique) {
      indexTable.AssignLiteral("unique_index_data");
    }
    else {
      indexTable.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(leftKeyName, "left_key");
  NS_NAMED_LITERAL_CSTRING(rightKeyName, "right_key");
  NS_NAMED_LITERAL_CSTRING(value, "value");
  NS_NAMED_LITERAL_CSTRING(data, "data");

  nsCAutoString keyRangeClause;
  if (!mLeftKey.IsUnset()) {
    keyRangeClause = NS_LITERAL_CSTRING(" AND value");
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
    keyRangeClause += NS_LITERAL_CSTRING(" AND value");
    if (mKeyRangeFlags & nsIIDBKeyRange::RIGHT_OPEN) {
      keyRangeClause.AppendLiteral(" < :");
    }
    else {
      NS_ASSERTION(mKeyRangeFlags & nsIIDBKeyRange::RIGHT_BOUND, "Bad flags!");
      keyRangeClause.AppendLiteral(" <= :");
    }
    keyRangeClause.Append(rightKeyName);
  }

  nsCString groupClause;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::PREV:
      break;

    case nsIIDBCursor::NEXT_NO_DUPLICATE:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      groupClause = NS_LITERAL_CSTRING(" GROUP BY ") + indexTable +
                    NS_LITERAL_CSTRING(".") + value;
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
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
      NS_NOTREACHED("Unknown direction!");
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT ") + indexTable +
                    NS_LITERAL_CSTRING(".") + value +
                    NS_LITERAL_CSTRING(", ") + objectTable +
                    NS_LITERAL_CSTRING(".") + data +
                    NS_LITERAL_CSTRING(" FROM ") + objectTable +
                    NS_LITERAL_CSTRING(" INNER JOIN ") + indexTable +
                    NS_LITERAL_CSTRING(" ON ") + indexTable +
                    NS_LITERAL_CSTRING(".") + objectDataId +
                    NS_LITERAL_CSTRING(" = ") + objectTable +
                    NS_LITERAL_CSTRING(".id WHERE ") + indexId +
                    NS_LITERAL_CSTRING(" = :") + indexId + keyRangeClause +
                    groupClause + NS_LITERAL_CSTRING(" ORDER BY ") +
                    indexTable + NS_LITERAL_CSTRING(".") + value +
                    directionClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mId);
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

    rv = stmt->GetString(1, pair->value);
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
OpenObjectCursorHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  if (mData.IsEmpty()) {
    aResult->SetAsEmpty();
    return OK;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mIndex, mDirection, mData);
  NS_ENSURE_TRUE(cursor, nsIIDBDatabaseException::UNKNOWN_ERR);

  aResult->SetAsISupports(static_cast<nsPIDOMEventTarget*>(cursor));
  return OK;
}
