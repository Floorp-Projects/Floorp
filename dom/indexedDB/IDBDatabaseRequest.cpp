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

#include "IDBDatabaseRequest.h"

#include "nsIIDBDatabaseException.h"

#include "mozilla/Storage.h"
#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBObjectStoreRequest.h"
#include "IndexedDatabaseRequest.h"
#include "LazyIdleThread.h"

USING_INDEXEDDB_NAMESPACE

namespace {

const PRUint32 kDefaultDatabaseTimeoutMS = 5000;

inline
nsISupports*
isupports_cast(IDBDatabaseRequest* aClassPtr)
{
  return static_cast<nsISupports*>(
    static_cast<IDBRequest::Generator*>(aClassPtr));
}

template<class T>
inline
already_AddRefed<nsISupports>
do_QIAndNull(nsCOMPtr<T>& aCOMPtr)
{
  nsCOMPtr<nsISupports> temp(do_QueryInterface(aCOMPtr));
  aCOMPtr = nsnull;
  return temp.forget();
}

class CloseConnectionRunnable : public nsRunnable
{
public:
  CloseConnectionRunnable(nsTArray<nsCOMPtr<nsISupports> >& aDoomedObjects)
  {
    mDoomedObjects.SwapElements(aDoomedObjects);
  }

  NS_IMETHOD Run()
  {
    mDoomedObjects.Clear();
    return NS_OK;
  }

private:
  nsTArray<nsCOMPtr<nsISupports> > mDoomedObjects;
};

class CreateObjectStoreHelper : public AsyncConnectionHelper
{
public:
  CreateObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                          IDBRequest* aRequest,
                          const nsAString& aName,
                          const nsAString& aKeyPath,
                          bool aAutoIncrement)
  : AsyncConnectionHelper(aDatabase, aRequest), mName(aName),
    mKeyPath(aKeyPath), mAutoIncrement(aAutoIncrement), mId(LL_MININT)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

protected:
  // In-params.
  nsString mName;
  nsString mKeyPath;
  bool mAutoIncrement;

  // Created during Init.
  nsRefPtr<IDBObjectStoreRequest> mObjectStore;

  // Out-params.
  PRInt64 mId;
};

class OpenObjectStoreHelper : public CreateObjectStoreHelper
{
public:
  OpenObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                        IDBRequest* aRequest,
                        const nsAString& aName,
                        PRUint16 aMode)
  : CreateObjectStoreHelper(aDatabase, aRequest, aName, EmptyString(), false),
    mMode(aMode)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

protected:
  // In-params.
  PRUint16 mMode;
};

class RemoveObjectStoreHelper : public AsyncConnectionHelper
{
public:
  RemoveObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                          IDBRequest* aRequest,
                          const nsAString& aName)
  : AsyncConnectionHelper(aDatabase, aRequest), mName(aName)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  nsString mName;
};

} // anonymous namespace

// static
already_AddRefed<IDBDatabaseRequest>
IDBDatabaseRequest::Create(const nsAString& aName,
                           const nsAString& aDescription,
                           PRBool aReadOnly,
                           nsTArray<nsString>& aObjectStoreNames,
                           nsTArray<nsString>& aIndexNames,
                           const nsAString& aVersion,
                           LazyIdleThread* aThread,
                           const nsAString& aDatabaseFilePath,
                           nsCOMPtr<mozIStorageConnection>& aConnection)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBDatabaseRequest> db(new IDBDatabaseRequest());

  db->mName.Assign(aName);
  db->mDescription.Assign(aDescription);
  db->mReadOnly = aReadOnly;
  db->mObjectStoreNames.SwapElements(aObjectStoreNames);
  db->mIndexNames.SwapElements(aIndexNames);
  db->mVersion.Assign(aVersion);
  db->mDatabaseFilePath.Assign(aDatabaseFilePath);

  aThread->SetWeakIdleObserver(db);
  db->mConnectionThread = aThread;

  db->mConnection.swap(aConnection);

  return db.forget();
}

IDBDatabaseRequest::IDBDatabaseRequest()
: mReadOnly(PR_FALSE)
{
  
}

IDBDatabaseRequest::~IDBDatabaseRequest()
{
  mConnectionThread->SetWeakIdleObserver(nsnull);
  FireCloseConnectionRunnable();
}

nsCOMPtr<mozIStorageConnection>&
IDBDatabaseRequest::Connection()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  return mConnection;
}

nsresult
IDBDatabaseRequest::EnsureConnection()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (!mConnection) {
    mConnection = IndexedDatabaseRequest::GetConnection(mDatabaseFilePath);
    NS_ENSURE_TRUE(mConnection, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

already_AddRefed<mozIStorageStatement>
IDBDatabaseRequest::PutStatement(bool aOverwrite,
                                 bool aAutoIncrement)
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  nsresult rv = EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<mozIStorageStatement> result;

  if (aOverwrite) {
    if (aAutoIncrement) {
      if (!mPutOverwriteAutoIncrementStmt) {
        rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
          "INSERT OR REPLACE INTO ai_object_data (object_store_id, id, data) "
          "VALUES (:osid, :key_value, :data)"
        ), getter_AddRefs(mPutOverwriteAutoIncrementStmt));
        NS_ENSURE_SUCCESS(rv, nsnull);
      }
      result = mPutOverwriteAutoIncrementStmt;
    }
    else {
      if (!mPutOverwriteStmt) {
        rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
          "INSERT OR REPLACE INTO object_data (object_store_id, key_value, "
                                              "data) "
          "VALUES (:osid, :key_value, :data)"
        ), getter_AddRefs(mPutOverwriteStmt));
        NS_ENSURE_SUCCESS(rv, nsnull);
      }
      result = mPutOverwriteStmt;
    }
  }
  else {
    if (aAutoIncrement) {
      if (!mPutAutoIncrementStmt) {
        rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
          "INSERT INTO ai_object_data (object_store_id, data) "
          "VALUES (:osid, :data)"
        ), getter_AddRefs(mPutAutoIncrementStmt));
        NS_ENSURE_SUCCESS(rv, nsnull);
      }
      result = mPutAutoIncrementStmt;
    }
    else {
      if (!mPutStmt) {
        rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
          "INSERT INTO object_data (object_store_id, key_value, data) "
          "VALUES (:osid, :key_value, :data)"
        ), getter_AddRefs(mPutStmt));
        NS_ENSURE_SUCCESS(rv, nsnull);
      }
      result = mPutStmt;
    }
  }

  return result.forget();
}

already_AddRefed<mozIStorageStatement>
IDBDatabaseRequest::RemoveStatement(bool aAutoIncrement)
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  nsresult rv = EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<mozIStorageStatement> result;

  if (aAutoIncrement) {
    if (!mRemoveAutoIncrementStmt) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM ai_object_data "
        "WHERE id = :key_value "
        "AND object_store_id = :osid"
      ), getter_AddRefs(mRemoveAutoIncrementStmt));
      NS_ENSURE_SUCCESS(rv, nsnull);
    }
    result = mRemoveAutoIncrementStmt;
  }
  else {
    if (!mRemoveStmt) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM object_data "
        "WHERE key_value = :key_value "
        "AND object_store_id = :osid"
      ), getter_AddRefs(mRemoveStmt));
      NS_ENSURE_SUCCESS(rv, nsnull);
    }
    result = mRemoveStmt;
  }

  return result.forget();
}

void
IDBDatabaseRequest::FireCloseConnectionRunnable()
{
  nsTArray<nsCOMPtr<nsISupports> > doomedObjects;

  doomedObjects.AppendElement(do_QIAndNull(mConnection));
  doomedObjects.AppendElement(do_QIAndNull(mPutStmt));
  doomedObjects.AppendElement(do_QIAndNull(mPutAutoIncrementStmt));
  doomedObjects.AppendElement(do_QIAndNull(mPutOverwriteStmt));
  doomedObjects.AppendElement(do_QIAndNull(mPutOverwriteAutoIncrementStmt));
  doomedObjects.AppendElement(do_QIAndNull(mRemoveStmt));
  doomedObjects.AppendElement(do_QIAndNull(mRemoveAutoIncrementStmt));

  nsCOMPtr<nsIRunnable> runnable(new CloseConnectionRunnable(doomedObjects));
  mConnectionThread->Dispatch(runnable, NS_DISPATCH_NORMAL);

  NS_ASSERTION(doomedObjects.Length() == 0, "Should have swapped!");
}

void
IDBDatabaseRequest::OnObjectStoreCreated(const nsAString& aName)
{
  NS_ASSERTION(!mObjectStoreNames.Contains(aName),
               "Already got this object store in the list!");
  if (!mObjectStoreNames.AppendElement(aName)) {
    NS_ERROR("Failed to add object store name! OOM?");
  }
}

void
IDBDatabaseRequest::OnIndexCreated(const nsAString& aName)
{
  NS_ASSERTION(!mIndexNames.Contains(aName),
               "Already got this index in the list!");
  if (!mIndexNames.AppendElement(aName)) {
    NS_ERROR("Failed to add index name! OOM?");
  }
}

void
IDBDatabaseRequest::OnObjectStoreRemoved(const nsAString& aName)
{
  NS_ASSERTION(mObjectStoreNames.Contains(aName),
               "Didn't know about this object store before!");
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStoreNames[index].Equals(aName)) {
      mObjectStoreNames.RemoveElementAt(index);
      return;
    }
  }
  NS_NOTREACHED("Shouldn't get here!");
}

NS_IMPL_ADDREF(IDBDatabaseRequest)
NS_IMPL_RELEASE(IDBDatabaseRequest)

NS_INTERFACE_MAP_BEGIN(IDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabase)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabaseRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBDatabaseRequest, IDBDatabaseRequest)

NS_IMETHODIMP
IDBDatabaseRequest::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetDescription(nsAString& aDescription)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aDescription.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetVersion(nsAString& aVersion)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aVersion.Assign(mVersion);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetObjectStores(nsIDOMDOMStringList** aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(mObjectStoreNames[index]), NS_ERROR_OUT_OF_MEMORY);
  }

  list.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetIndexes(nsIDOMDOMStringList** aIndexes)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  PRUint32 count = mIndexNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(mIndexNames[index]), NS_ERROR_OUT_OF_MEMORY);
  }

  
  list.forget(aIndexes);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetCurrentTransaction(nsIIDBTransaction** aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIIDBTransaction> transaction(mCurrentTransaction);
  transaction.forget(aTransaction);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::CreateObjectStore(const nsAString& aName,
                                      const nsAString& aKeyPath,
                                      PRBool aAutoIncrement,
                                      nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  bool exists = false;
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStoreNames[index].Equals(aName)) {
      exists = true;
      break;
    }
  }

  nsresult rv;
  if (NS_UNLIKELY(exists)) {
    nsCOMPtr<nsIRunnable> runnable =
      IDBErrorEvent::CreateRunnable(request,
                                    nsIIDBDatabaseException::CONSTRAINT_ERR);
    NS_ENSURE_TRUE(runnable, NS_ERROR_UNEXPECTED);
    rv = NS_DispatchToCurrentThread(runnable);
  }
  else {
    nsRefPtr<CreateObjectStoreHelper> helper =
      new CreateObjectStoreHelper(this, request, aName, aKeyPath,
                                  !!aAutoIncrement);
    rv = helper->Dispatch(mConnectionThread);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenObjectStore(const nsAString& aName,
                                    PRUint16 aMode,
                                    PRUint8 aOptionalArgCount,
                                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aOptionalArgCount) {
    if (aMode != nsIIDBObjectStore::READ_WRITE &&
        aMode != nsIIDBObjectStore::READ_ONLY &&
        aMode != nsIIDBObjectStore::SNAPSHOT_READ) {
      return NS_ERROR_INVALID_ARG;
    }
  }
  else {
    aMode = nsIIDBObjectStore::READ_WRITE;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest();

  bool exists = false;
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStoreNames[index].Equals(aName)) {
      exists = true;
      break;
    }
  }

  nsresult rv;
  if (NS_UNLIKELY(!exists)) {
    nsCOMPtr<nsIRunnable> runnable =
      IDBErrorEvent::CreateRunnable(request,
                                    nsIIDBDatabaseException::CONSTRAINT_ERR);
    NS_ENSURE_TRUE(runnable, NS_ERROR_UNEXPECTED);
    rv = NS_DispatchToCurrentThread(runnable);
  }
  else {
    nsRefPtr<OpenObjectStoreHelper> helper =
      new OpenObjectStoreHelper(this, request, aName, aMode);
    rv = helper->Dispatch(mConnectionThread);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::CreateIndex(const nsAString& aName,
                                const nsAString& aStoreName,
                                const nsAString& aKeyPath,
                                PRBool aUnique,
                                nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenIndex(const nsAString& aName,
                              nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveObjectStore(const nsAString& aName,
                                      nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest();

  bool exists = false;
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStoreNames[index].Equals(aName)) {
      exists = true;
      break;
    }
  }

  nsresult rv;
  if (NS_UNLIKELY(!exists)) {
    nsCOMPtr<nsIRunnable> runnable =
      IDBErrorEvent::CreateRunnable(request,
                                    nsIIDBDatabaseException::NOT_FOUND_ERR);
    NS_ENSURE_TRUE(runnable, NS_ERROR_UNEXPECTED);
    rv = NS_DispatchToCurrentThread(runnable);
  }
  else {
    nsRefPtr<RemoveObjectStoreHelper> helper =
      new RemoveObjectStoreHelper(this, request, aName);
    rv = helper->Dispatch(mConnectionThread);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveIndex(const nsAString& aIndexName,
                                nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::SetVersion(const nsAString& aVersion,
                               nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenTransaction(nsIDOMDOMStringList* aStoreNames,
                                    PRUint32 aTimeout,
                                    PRUint8 aOptionalArgCount,
                                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  if (!aOptionalArgCount) {
    aTimeout = kDefaultDatabaseTimeoutMS;
  }

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_FALSE(strcmp(aTopic, IDLE_THREAD_TOPIC), NS_ERROR_UNEXPECTED);

  FireCloseConnectionRunnable();

  return NS_OK;
}

PRUint16
CreateObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  // Insert the data into the database.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO object_store (name, key_path, auto_increment) "
    "VALUES (:name, :key_path, :auto_increment)"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (mKeyPath.IsVoid()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("key_path"));
  } else {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"), mKeyPath);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mAutoIncrement ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::CONSTRAINT_ERR;
  }

  // Get the id of this object store, and store it for future use.
  (void)aConnection->GetLastInsertRowID(&mId);

  return OK;
}

PRUint16
CreateObjectStoreHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  mObjectStore =
    IDBObjectStoreRequest::Create(mDatabase, mName, mKeyPath, mAutoIncrement,
                                  nsIIDBObjectStore::READ_WRITE, mId);
  NS_ENSURE_TRUE(mObjectStore, nsIIDBDatabaseException::UNKNOWN_ERR);

  mDatabase->OnObjectStoreCreated(mName);

  return AsyncConnectionHelper::OnSuccess(aTarget);
}

PRUint16
CreateObjectStoreHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  nsCOMPtr<nsISupports> result =
    do_QueryInterface(static_cast<nsIIDBObjectStoreRequest*>(mObjectStore));
  NS_ASSERTION(result, "Failed to QI!");

  aResult->SetAsISupports(result);
  return OK;
}

PRUint16
OpenObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  // TODO pull this up to the connection and cache it so opening these is
  // cheaper.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, key_path, auto_increment "
    "FROM object_store "
    "WHERE name = :name"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  NS_ENSURE_TRUE(hasResult, nsIIDBDatabaseException::NOT_FOUND_ERR);

  mId = stmt->AsInt64(0);
  (void)stmt->GetString(1, mKeyPath);
  mAutoIncrement = !!stmt->AsInt32(2);

  return OK;
}

PRUint16
OpenObjectStoreHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  mObjectStore =
    IDBObjectStoreRequest::Create(mDatabase, mName, mKeyPath, mAutoIncrement,
                                  mMode, mId);
  NS_ENSURE_TRUE(mObjectStore, nsIIDBDatabaseException::UNKNOWN_ERR);

  return AsyncConnectionHelper::OnSuccess(aTarget);
}

PRUint16
OpenObjectStoreHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  nsCOMPtr<nsISupports> result =
    do_QueryInterface(static_cast<nsIIDBObjectStoreRequest*>(mObjectStore));
  NS_ASSERTION(result, "Failed to QI!");

  aResult->SetAsISupports(result);
  return OK;
}

PRUint16
RemoveObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM object_store "
    "WHERE name = :name "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::NOT_FOUND_ERR;
  }

  return OK;
}

PRUint16
RemoveObjectStoreHelper::GetSuccessResult(nsIWritableVariant* /* aResult */)
{
  mDatabase->OnObjectStoreRemoved(mName);
  return OK;
}
