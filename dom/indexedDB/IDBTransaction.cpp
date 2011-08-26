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

#include "IDBTransaction.h"

#include "nsIScriptContext.h"

#include "mozilla/storage.h"
#include "nsDOMClassInfo.h"
#include "nsEventDispatcher.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBObjectStore.h"
#include "IndexedDatabaseManager.h"
#include "TransactionThreadPool.h"

#define SAVEPOINT_NAME "savepoint"

USING_INDEXEDDB_NAMESPACE

namespace {

PLDHashOperator
DoomCachedStatements(const nsACString& aQuery,
                     nsCOMPtr<mozIStorageStatement>& aStatement,
                     void* aUserArg)
{
  CommitHelper* helper = static_cast<CommitHelper*>(aUserArg);
  helper->AddDoomedObject(aStatement);
  return PL_DHASH_REMOVE;
}

} // anonymous namespace

// static
already_AddRefed<IDBTransaction>
IDBTransaction::Create(IDBDatabase* aDatabase,
                       nsTArray<nsString>& aObjectStoreNames,
                       PRUint16 aMode,
                       PRUint32 aTimeout,
                       bool aDispatchDelayed)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBTransaction> transaction = new IDBTransaction();

  transaction->mScriptContext = aDatabase->ScriptContext();
  transaction->mOwner = aDatabase->Owner();

  transaction->mDatabase = aDatabase;
  transaction->mMode = aMode;
  transaction->mTimeout = aTimeout;

  if (!transaction->mObjectStoreNames.AppendElements(aObjectStoreNames)) {
    NS_ERROR("Out of memory!");
    return nsnull;
  }

  if (!transaction->mCachedStatements.Init()) {
    NS_ERROR("Failed to initialize hash!");
    return nsnull;
  }

  if (!aDispatchDelayed) {
    nsCOMPtr<nsIThreadInternal> thread =
      do_QueryInterface(NS_GetCurrentThread());
    NS_ENSURE_TRUE(thread, nsnull);

    // We need the current recursion depth first.
    PRUint32 depth;
    nsresult rv = thread->GetRecursionDepth(&depth);
    NS_ENSURE_SUCCESS(rv, nsnull);

    NS_ASSERTION(depth, "This should never be 0!");
    transaction->mCreatedRecursionDepth = depth - 1;

    rv = thread->AddObserver(transaction);
    NS_ENSURE_SUCCESS(rv, nsnull);

    transaction->mCreating = true;
  }

  return transaction.forget();
}

IDBTransaction::IDBTransaction()
: mReadyState(nsIIDBTransaction::INITIAL),
  mMode(nsIIDBTransaction::READ_ONLY),
  mTimeout(0),
  mPendingRequests(0),
  mCreatedRecursionDepth(0),
  mSavepointCount(0),
  mAborted(false),
  mCreating(false)
#ifdef DEBUG
  , mFiredCompleteOrAbort(false)
#endif
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBTransaction::~IDBTransaction()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mPendingRequests, "Should have no pending requests here!");
  NS_ASSERTION(!mSavepointCount, "Should have released them all!");
  NS_ASSERTION(!mConnection, "Should have called CommitOrRollback!");
  NS_ASSERTION(!mCreating, "Should have been cleared already!");
  NS_ASSERTION(mFiredCompleteOrAbort, "Should have fired event!");

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

void
IDBTransaction::OnNewRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (!mPendingRequests) {
    NS_ASSERTION(mReadyState == nsIIDBTransaction::INITIAL,
                 "Reusing a transaction!");
    mReadyState = nsIIDBTransaction::LOADING;
  }
  ++mPendingRequests;
}

void
IDBTransaction::OnRequestFinished()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mPendingRequests, "Mismatched calls!");
  --mPendingRequests;
  if (!mPendingRequests) {
    NS_ASSERTION(mAborted || mReadyState == nsIIDBTransaction::LOADING,
                 "Bad state!");
    mReadyState = IDBTransaction::COMMITTING;
    CommitOrRollback();
  }
}

nsresult
IDBTransaction::CommitOrRollback()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
  NS_ENSURE_STATE(pool);

  nsRefPtr<CommitHelper> helper(new CommitHelper(this));

  mCachedStatements.Enumerate(DoomCachedStatements, helper);
  NS_ASSERTION(!mCachedStatements.Count(), "Statements left!");

  nsresult rv = pool->Dispatch(this, helper, true, helper);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

bool
IDBTransaction::StartSavepoint()
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  NS_PRECONDITION(mConnection, "No connection!");

  nsCOMPtr<mozIStorageStatement> stmt = GetCachedStatement(NS_LITERAL_CSTRING(
    "SAVEPOINT " SAVEPOINT_NAME
  ));
  NS_ENSURE_TRUE(stmt, false);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, false);

  ++mSavepointCount;

  return true;
}

nsresult
IDBTransaction::ReleaseSavepoint()
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  NS_PRECONDITION(mConnection, "No connection!");

  NS_ASSERTION(mSavepointCount, "Mismatch!");

  nsCOMPtr<mozIStorageStatement> stmt = GetCachedStatement(NS_LITERAL_CSTRING(
    "RELEASE SAVEPOINT " SAVEPOINT_NAME
  ));
  NS_ENSURE_TRUE(stmt, false);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, false);

  --mSavepointCount;

  return NS_OK;
}

void
IDBTransaction::RollbackSavepoint()
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  NS_PRECONDITION(mConnection, "No connection!");

  NS_ASSERTION(mSavepointCount == 1, "Mismatch!");
  mSavepointCount = 0;

  nsCOMPtr<mozIStorageStatement> stmt = GetCachedStatement(NS_LITERAL_CSTRING(
    "ROLLBACK TO SAVEPOINT " SAVEPOINT_NAME
  ));
  NS_ENSURE_TRUE(stmt,);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv,);
}

nsresult
IDBTransaction::GetOrCreateConnection(mozIStorageConnection** aResult)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mDatabase->IsInvalidated()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mConnection) {
    nsCOMPtr<mozIStorageConnection> connection =
      IDBFactory::GetConnection(mDatabase->FilePath());
    NS_ENSURE_TRUE(connection, NS_ERROR_FAILURE);

    nsCString beginTransaction;
    if (mMode == nsIIDBTransaction::READ_WRITE) {
      beginTransaction.AssignLiteral("BEGIN IMMEDIATE TRANSACTION;");
    }
    else {
      beginTransaction.AssignLiteral("BEGIN TRANSACTION;");
    }

    nsCOMPtr<mozIStorageStatement> stmt;
    nsresult rv = connection->CreateStatement(beginTransaction,
                                              getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, false);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, false);

    connection.swap(mConnection);
  }

  nsCOMPtr<mozIStorageConnection> result(mConnection);
  result.forget(aResult);
  return NS_OK;
}

already_AddRefed<mozIStorageStatement>
IDBTransaction::AddStatement(bool aCreate,
                             bool aOverwrite,
                             bool aAutoIncrement)
{
#ifdef DEBUG
  if (!aCreate) {
    NS_ASSERTION(aOverwrite, "Bad param combo!");
  }
#endif

  if (aAutoIncrement) {
    if (aCreate) {
      if (aOverwrite) {
        return GetCachedStatement(
          "INSERT OR FAIL INTO ai_object_data (object_store_id, id, data) "
          "VALUES (:osid, :key_value, :data)"
        );
      }
      return GetCachedStatement(
        "INSERT INTO ai_object_data (object_store_id, data) "
        "VALUES (:osid, :data)"
      );
    }
    return GetCachedStatement(
      "UPDATE ai_object_data "
      "SET data = :data "
      "WHERE object_store_id = :osid "
      "AND id = :key_value"
    );
  }
  if (aCreate) {
    if (aOverwrite) {
      return GetCachedStatement(
        "INSERT OR FAIL INTO object_data (object_store_id, key_value, data) "
        "VALUES (:osid, :key_value, :data)"
      );
    }
    return GetCachedStatement(
      "INSERT INTO object_data (object_store_id, key_value, data) "
      "VALUES (:osid, :key_value, :data)"
    );
  }
  return GetCachedStatement(
    "UPDATE object_data "
    "SET data = :data "
    "WHERE object_store_id = :osid "
    "AND key_value = :key_value"
  );
}

already_AddRefed<mozIStorageStatement>
IDBTransaction::DeleteStatement(bool aAutoIncrement)
{
  if (aAutoIncrement) {
    return GetCachedStatement(
      "DELETE FROM ai_object_data "
      "WHERE id = :key_value "
      "AND object_store_id = :osid"
    );
  }
  return GetCachedStatement(
    "DELETE FROM object_data "
    "WHERE key_value = :key_value "
    "AND object_store_id = :osid"
  );
}

already_AddRefed<mozIStorageStatement>
IDBTransaction::GetStatement(bool aAutoIncrement)
{
  if (aAutoIncrement) {
    return GetCachedStatement(
      "SELECT data "
      "FROM ai_object_data "
      "WHERE id = :id "
      "AND object_store_id = :osid"
    );
  }
  return GetCachedStatement(
    "SELECT data "
    "FROM object_data "
    "WHERE key_value = :id "
    "AND object_store_id = :osid"
  );
}

already_AddRefed<mozIStorageStatement>
IDBTransaction::IndexGetStatement(bool aUnique,
                                  bool aAutoIncrement)
{
  if (aAutoIncrement) {
    if (aUnique) {
      return GetCachedStatement(
        "SELECT ai_object_data_id "
        "FROM ai_unique_index_data "
        "WHERE index_id = :index_id "
        "AND value = :value"
      );
    }
    return GetCachedStatement(
      "SELECT ai_object_data_id "
      "FROM ai_index_data "
      "WHERE index_id = :index_id "
      "AND value = :value"
    );
  }
  if (aUnique) {
    return GetCachedStatement(
      "SELECT object_data_key "
      "FROM unique_index_data "
      "WHERE index_id = :index_id "
      "AND value = :value"
    );
  }
  return GetCachedStatement(
    "SELECT object_data_key "
    "FROM index_data "
    "WHERE index_id = :index_id "
    "AND value = :value"
  );
}

already_AddRefed<mozIStorageStatement>
IDBTransaction::IndexGetObjectStatement(bool aUnique,
                                        bool aAutoIncrement)
{
  if (aAutoIncrement) {
    if (aUnique) {
      return GetCachedStatement(
        "SELECT data "
        "FROM ai_object_data "
        "INNER JOIN ai_unique_index_data "
        "ON ai_object_data.id = ai_unique_index_data.ai_object_data_id "
        "WHERE index_id = :index_id "
        "AND value = :value"
      );
    }
    return GetCachedStatement(
      "SELECT data "
      "FROM ai_object_data "
      "INNER JOIN ai_index_data "
      "ON ai_object_data.id = ai_index_data.ai_object_data_id "
      "WHERE index_id = :index_id "
      "AND value = :value"
    );
  }
  if (aUnique) {
    return GetCachedStatement(
      "SELECT data "
      "FROM object_data "
      "INNER JOIN unique_index_data "
      "ON object_data.id = unique_index_data.object_data_id "
      "WHERE index_id = :index_id "
      "AND value = :value"
    );
  }
  return GetCachedStatement(
    "SELECT data "
    "FROM object_data "
    "INNER JOIN index_data "
    "ON object_data.id = index_data.object_data_id "
    "WHERE index_id = :index_id "
    "AND value = :value"
  );
}

already_AddRefed<mozIStorageStatement>
IDBTransaction::IndexUpdateStatement(bool aAutoIncrement,
                                     bool aUnique,
                                     bool aOverwrite)
{
  if (aAutoIncrement) {
    if (aUnique) {
      if (aOverwrite) {
        return GetCachedStatement(
          "INSERT OR REPLACE INTO ai_unique_index_data "
            "(index_id, ai_object_data_id, value) "
          "VALUES (:index_id, :object_data_id, :value)"
        );
      }
      return GetCachedStatement(
        "INSERT INTO ai_unique_index_data "
          "(index_id, aI_object_data_id, value) "
        "VALUES (:index_id, :object_data_id, :value)"
      );
    }
    if (aOverwrite) {
      return GetCachedStatement(
        "INSERT OR REPLACE INTO ai_index_data "
          "(index_id, ai_object_data_id, value) "
        "VALUES (:index_id, :object_data_id, :value)"
      );
    }
    return GetCachedStatement(
      "INSERT INTO ai_index_data "
        "(index_id, ai_object_data_id, value) "
      "VALUES (:index_id, :object_data_id, :value)"
    );
  }
  if (aUnique) {
    if (aOverwrite) {
      return GetCachedStatement(
        "INSERT OR REPLACE INTO unique_index_data "
          "(index_id, object_data_id, object_data_key, value) "
        "VALUES (:index_id, :object_data_id, :object_data_key, :value)"
      );
    }
    return GetCachedStatement(
      "INSERT INTO unique_index_data "
        "(index_id, object_data_id, object_data_key, value) "
      "VALUES (:index_id, :object_data_id, :object_data_key, :value)"
    );
  }
  if (aOverwrite) {
    return GetCachedStatement(
      "INSERT INTO index_data ("
        "index_id, object_data_id, object_data_key, value) "
      "VALUES (:index_id, :object_data_id, :object_data_key, :value)"
    );
  }
  return GetCachedStatement(
    "INSERT INTO index_data ("
      "index_id, object_data_id, object_data_key, value) "
    "VALUES (:index_id, :object_data_id, :object_data_key, :value)"
  );
}

already_AddRefed<mozIStorageStatement>
IDBTransaction::GetCachedStatement(const nsACString& aQuery)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aQuery.IsEmpty(), "Empty sql statement!");
  NS_ASSERTION(mConnection, "No connection!");

  nsCOMPtr<mozIStorageStatement> stmt;

  if (!mCachedStatements.Get(aQuery, getter_AddRefs(stmt))) {
    nsresult rv = mConnection->CreateStatement(aQuery, getter_AddRefs(stmt));
#ifdef DEBUG
    if (NS_FAILED(rv)) {
      nsCString error;
      error.AppendLiteral("The statement `");
      error.Append(aQuery);
      error.AppendLiteral("` failed to compile with the error message `");
      nsCString msg;
      (void)mConnection->GetLastErrorString(msg);
      error.Append(msg);
      error.AppendLiteral("`.");
      NS_ERROR(error.get());
    }
#endif
    NS_ENSURE_SUCCESS(rv, nsnull);

    if (!mCachedStatements.Put(aQuery, stmt)) {
      NS_ERROR("Out of memory?!");
    }
  }

  return stmt.forget();
}

bool
IDBTransaction::IsOpen() const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // If we haven't started anything then we're open.
  if (mReadyState == nsIIDBTransaction::INITIAL) {
    NS_ASSERTION(AsyncConnectionHelper::GetCurrentTransaction() != this,
                 "This should be some other transaction (or null)!");
    return true;
  }

  // If we've already started then we need to check to see if we still have the
  // mCreating flag set. If we do (i.e. we haven't returned to the event loop
  // from the time we were created) then we are open. Otherwise check the
  // currently running transaction to see if it's the same. We only allow other
  // requests to be made if this transaction is currently running.
  if (mReadyState == nsIIDBTransaction::LOADING) {
    if (mCreating) {
      return true;
    }

    if (AsyncConnectionHelper::GetCurrentTransaction() == this) {
      return true;
    }
  }

  return false;
}

already_AddRefed<IDBObjectStore>
IDBTransaction::GetOrCreateObjectStore(const nsAString& aName,
                                       ObjectStoreInfo* aObjectStoreInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aName.IsEmpty(), "Empty name!");
  NS_ASSERTION(aObjectStoreInfo, "Null pointer!");

  nsRefPtr<IDBObjectStore> retval;

  for (PRUint32 index = 0; index < mCreatedObjectStores.Length(); index++) {
    nsRefPtr<IDBObjectStore>& objectStore = mCreatedObjectStores[index];
    if (objectStore->Name() == aName) {
      retval = objectStore;
      return retval.forget();
    }
  }

  retval = IDBObjectStore::Create(this, aObjectStoreInfo);
  NS_ENSURE_TRUE(retval, nsnull);

  if (!mCreatedObjectStores.AppendElement(retval)) {
    NS_WARNING("Out of memory!");
    return nsnull;
  }

  return retval.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBTransaction)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBTransaction,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mDatabase,
                                                       nsIDOMEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnCompleteListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnTimeoutListener)

  for (PRUint32 i = 0; i < tmp->mCreatedObjectStores.Length(); i++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCreatedObjectStores[i]");
    cb.NoteXPCOMChild(static_cast<nsIIDBObjectStore*>(
                      tmp->mCreatedObjectStores[i].get()));
  }

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBTransaction,
                                                nsDOMEventTargetHelper)
  // Don't unlink mDatabase!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnCompleteListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnTimeoutListener)

  tmp->mCreatedObjectStores.Clear();

NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBTransaction)
  NS_INTERFACE_MAP_ENTRY(nsIIDBTransaction)
  NS_INTERFACE_MAP_ENTRY(nsIThreadObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBTransaction)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBTransaction, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBTransaction, nsDOMEventTargetHelper)

DOMCI_DATA(IDBTransaction, IDBTransaction)

NS_IMETHODIMP
IDBTransaction::GetDb(nsIIDBDatabase** aDB)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ADDREF(*aDB = mDatabase);
  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::GetReadyState(PRUint16* aReadyState)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aReadyState = mReadyState;
  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::GetMode(PRUint16* aMode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aMode = mMode;
  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::GetObjectStoreNames(nsIDOMDOMStringList** aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());

  nsAutoTArray<nsString, 10> stackArray;
  nsTArray<nsString>* arrayOfNames;

  if (mMode == IDBTransaction::VERSION_CHANGE) {
    DatabaseInfo* info;
    if (!DatabaseInfo::Get(mDatabase->Id(), &info)) {
      NS_ERROR("This should never fail!");
    }

    if (!info->GetObjectStoreNames(stackArray)) {
      NS_ERROR("Out of memory!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    arrayOfNames = &stackArray;
  }
  else {
    arrayOfNames = &mObjectStoreNames;
  }

  PRUint32 count = arrayOfNames->Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(arrayOfNames->ElementAt(index)),
                   NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  list.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::ObjectStore(const nsAString& aName,
                            nsIIDBObjectStore** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  ObjectStoreInfo* info = nsnull;

  if (mMode == nsIIDBTransaction::VERSION_CHANGE ||
      mObjectStoreNames.Contains(aName)) {
    ObjectStoreInfo::Get(mDatabase->Id(), aName, &info);
  }

  if (!info) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR;
  }

  nsRefPtr<IDBObjectStore> objectStore = GetOrCreateObjectStore(aName, info);
  NS_ENSURE_TRUE(objectStore, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  objectStore.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::Abort()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // We can't use IsOpen here since we need it to be possible to call Abort()
  // even from outside of transaction callbacks.
  if (mReadyState != IDBTransaction::INITIAL &&
      mReadyState != IDBTransaction::LOADING) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  bool needToCommitOrRollback = mReadyState == nsIIDBTransaction::INITIAL;

  mAborted = true;
  mReadyState = nsIIDBTransaction::DONE;

  // Fire the abort event if there are no outstanding requests. Otherwise the
  // abort event will be fired when all outdtanding requests finish.
  if (needToCommitOrRollback) {
    return CommitOrRollback();
  }

  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::SetOnerror(nsIDOMEventListener* aErrorListener)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(ERROR_EVT_STR),
                                mOnErrorListener, aErrorListener);
}

NS_IMETHODIMP
IDBTransaction::GetOnerror(nsIDOMEventListener** aErrorListener)
{
  return GetInnerEventListener(mOnErrorListener, aErrorListener);
}

NS_IMETHODIMP
IDBTransaction::GetOncomplete(nsIDOMEventListener** aOncomplete)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return GetInnerEventListener(mOnCompleteListener, aOncomplete);
}

NS_IMETHODIMP
IDBTransaction::SetOncomplete(nsIDOMEventListener* aOncomplete)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return RemoveAddEventListener(NS_LITERAL_STRING(COMPLETE_EVT_STR),
                                mOnCompleteListener, aOncomplete);
}

NS_IMETHODIMP
IDBTransaction::GetOnabort(nsIDOMEventListener** aOnabort)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return GetInnerEventListener(mOnAbortListener, aOnabort);
}

NS_IMETHODIMP
IDBTransaction::SetOnabort(nsIDOMEventListener* aOnabort)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return RemoveAddEventListener(NS_LITERAL_STRING(ABORT_EVT_STR),
                                mOnAbortListener, aOnabort);
}

NS_IMETHODIMP
IDBTransaction::GetOntimeout(nsIDOMEventListener** aOntimeout)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return GetInnerEventListener(mOnTimeoutListener, aOntimeout);
}

NS_IMETHODIMP
IDBTransaction::SetOntimeout(nsIDOMEventListener* aOntimeout)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return RemoveAddEventListener(NS_LITERAL_STRING(TIMEOUT_EVT_STR),
                                mOnTimeoutListener, aOntimeout);
}

nsresult
IDBTransaction::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = PR_TRUE;
  aVisitor.mParentTarget = mDatabase;
  return NS_OK;
}

// XXX Once nsIThreadObserver gets split this method will disappear.
NS_IMETHODIMP
IDBTransaction::OnDispatchedEvent(nsIThreadInternal* aThread)
{
  NS_NOTREACHED("Don't call me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IDBTransaction::OnProcessNextEvent(nsIThreadInternal* aThread,
                                   PRBool aMayWait,
                                   PRUint32 aRecursionDepth)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRecursionDepth > mCreatedRecursionDepth,
               "Should be impossible!");
  NS_ASSERTION(mCreating, "Should be true!");
  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::AfterProcessNextEvent(nsIThreadInternal* aThread,
                                      PRUint32 aRecursionDepth)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aThread, "This should never be null!");
  NS_ASSERTION(aRecursionDepth >= mCreatedRecursionDepth,
               "Should be impossible!");
  NS_ASSERTION(mCreating, "Should be true!");

  if (aRecursionDepth == mCreatedRecursionDepth) {
    // We're back at the event loop, no longer newborn.
    mCreating = false;

    // Maybe set the readyState to DONE if there were no requests generated.
    if (mReadyState == nsIIDBTransaction::INITIAL) {
      mReadyState = nsIIDBTransaction::DONE;

      if (NS_FAILED(CommitOrRollback())) {
        NS_WARNING("Failed to commit!");
      }
    }

    // No longer need to observe thread events.
    if(NS_FAILED(aThread->RemoveObserver(this))) {
      NS_ERROR("Failed to remove observer!");
    }
  }

  return NS_OK;
}

CommitHelper::CommitHelper(IDBTransaction* aTransaction)
: mTransaction(aTransaction),
  mAborted(!!aTransaction->mAborted),
  mHaveMetadata(false)
{
  mConnection.swap(aTransaction->mConnection);
}

CommitHelper::~CommitHelper()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(CommitHelper, nsIRunnable)

NS_IMETHODIMP
CommitHelper::Run()
{
  if (NS_IsMainThread()) {
    NS_ASSERTION(mDoomedObjects.IsEmpty(), "Didn't release doomed objects!");

    mTransaction->mReadyState = nsIIDBTransaction::DONE;

    nsCOMPtr<nsIDOMEvent> event;
    if (mAborted) {
      if (mHaveMetadata) {
        NS_ASSERTION(mTransaction->Mode() == nsIIDBTransaction::VERSION_CHANGE,
                     "Bad transaction type!");

        DatabaseInfo* dbInfo;
        if (!DatabaseInfo::Get(mTransaction->Database()->Id(), &dbInfo)) {
          NS_ERROR("This should never fail!");
        }

        if (NS_FAILED(IDBFactory::UpdateDatabaseMetadata(dbInfo, mOldVersion,
                                                         mOldObjectStores))) {
          NS_WARNING("Failed to update database metadata!");
        }
        else {
          NS_ASSERTION(mOldObjectStores.IsEmpty(), "Should have swapped!");
        }
      }

      event = CreateGenericEvent(NS_LITERAL_STRING(ABORT_EVT_STR));
    }
    else {
      event = CreateGenericEvent(NS_LITERAL_STRING(COMPLETE_EVT_STR));
    }
    NS_ENSURE_TRUE(event, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    PRBool dummy;
    if (NS_FAILED(mTransaction->DispatchEvent(event, &dummy))) {
      NS_WARNING("Dispatch failed!");
    }

#ifdef DEBUG
    mTransaction->mFiredCompleteOrAbort = true;
#endif
    mTransaction = nsnull;
    return NS_OK;
  }

  IDBDatabase* database = mTransaction->Database();
  if (database->IsInvalidated()) {
    mAborted = true;
  }

  if (mConnection) {
    IndexedDatabaseManager::SetCurrentDatabase(database);

    if (!mAborted) {
      NS_NAMED_LITERAL_CSTRING(release, "END TRANSACTION");
      if (NS_FAILED(mConnection->ExecuteSimpleSQL(release))) {
        mAborted = true;
      }
    }

    if (mAborted) {
      NS_NAMED_LITERAL_CSTRING(rollback, "ROLLBACK TRANSACTION");
      if (NS_FAILED(mConnection->ExecuteSimpleSQL(rollback))) {
        NS_WARNING("Failed to rollback transaction!");
      }

      if (mTransaction->Mode() == nsIIDBTransaction::VERSION_CHANGE) {
        nsresult rv =
          IDBFactory::LoadDatabaseInformation(mConnection,
                                              mTransaction->Database()->Id(),
                                              mOldVersion, mOldObjectStores);
        if (NS_SUCCEEDED(rv)) {
          mHaveMetadata = true;
        }
        else {
          NS_WARNING("Failed to get database information!");
        }
      }
    }
  }

  mDoomedObjects.Clear();

  if (mConnection) {
    mConnection->Close();
    mConnection = nsnull;

    IndexedDatabaseManager::SetCurrentDatabase(nsnull);
  }

  return NS_OK;
}
