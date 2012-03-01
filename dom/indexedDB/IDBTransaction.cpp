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
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMLists.h"
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

// This runnable doesn't actually do anything beyond "prime the pump" and get
// transactions in the right order on the transaction thread pool.
class StartTransactionRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Run()
  {
    // NOP
    return NS_OK;
  }
};

// Could really use those NS_REFCOUNTING_HAHA_YEAH_RIGHT macros here.
NS_IMETHODIMP_(nsrefcnt) StartTransactionRunnable::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(nsrefcnt) StartTransactionRunnable::Release()
{
  return 1;
}

NS_IMPL_QUERY_INTERFACE1(StartTransactionRunnable, nsIRunnable)

StartTransactionRunnable gStartTransactionRunnable;

} // anonymous namespace

// static
already_AddRefed<IDBTransaction>
IDBTransaction::Create(IDBDatabase* aDatabase,
                       nsTArray<nsString>& aObjectStoreNames,
                       PRUint16 aMode,
                       bool aDispatchDelayed)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBTransaction> transaction = new IDBTransaction();

  transaction->mScriptContext = aDatabase->GetScriptContext();
  transaction->mOwner = aDatabase->GetOwner();
  if (!transaction->SetScriptOwner(aDatabase->GetScriptOwner())) {
    return nsnull;
  }

  transaction->mDatabase = aDatabase;
  transaction->mMode = aMode;
  
  transaction->mDatabaseInfo = aDatabase->Info();

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

  if (aMode != nsIIDBTransaction::VERSION_CHANGE) {
    TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
    pool->Dispatch(transaction, &gStartTransactionRunnable, false, nsnull);
  }

  return transaction.forget();
}

IDBTransaction::IDBTransaction()
: mReadyState(nsIIDBTransaction::INITIAL),
  mMode(nsIIDBTransaction::READ_ONLY),
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

  nsContentUtils::ReleaseWrapper(static_cast<nsIDOMEventTarget*>(this), this);
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

void
IDBTransaction::RemoveObjectStore(const nsAString& aName)
{
  NS_ASSERTION(mMode == nsIIDBTransaction::VERSION_CHANGE,
               "Only remove object stores on VERSION_CHANGE transactions");

  mDatabaseInfo->RemoveObjectStore(aName);

  for (PRUint32 i = 0; i < mCreatedObjectStores.Length(); i++) {
    if (mCreatedObjectStores[i]->Name() == aName) {
      mCreatedObjectStores.RemoveElementAt(i);
      break;
    }
  }
}

void
IDBTransaction::SetTransactionListener(IDBTransactionListener* aListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mListener, "Shouldn't already have a listener!");
  mListener = aListener;
}

nsresult
IDBTransaction::CommitOrRollback()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
  NS_ENSURE_STATE(pool);

  nsRefPtr<CommitHelper> helper(new CommitHelper(this, mListener,
                                                 mCreatedObjectStores));

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

    nsresult rv;

    nsRefPtr<UpdateRefcountFunction> function;
    nsCString beginTransaction;
    if (mMode != nsIIDBTransaction::READ_ONLY) {
      function = new UpdateRefcountFunction(Database()->Manager());
      NS_ENSURE_TRUE(function, NS_ERROR_OUT_OF_MEMORY);

      rv = function->Init();
      NS_ENSURE_SUCCESS(rv, rv);

      rv = connection->CreateFunction(
        NS_LITERAL_CSTRING("update_refcount"), 2, function);
      NS_ENSURE_SUCCESS(rv, rv);

      beginTransaction.AssignLiteral("BEGIN IMMEDIATE TRANSACTION;");
    }
    else {
      beginTransaction.AssignLiteral("BEGIN TRANSACTION;");
    }

    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(beginTransaction, getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    function.swap(mUpdateFileRefcountFunction);
    connection.swap(mConnection);
  }

  nsCOMPtr<mozIStorageConnection> result(mConnection);
  result.forget(aResult);
  return NS_OK;
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
  NS_ASSERTION(aObjectStoreInfo, "Null pointer!");

  nsRefPtr<IDBObjectStore> retval;

  for (PRUint32 index = 0; index < mCreatedObjectStores.Length(); index++) {
    nsRefPtr<IDBObjectStore>& objectStore = mCreatedObjectStores[index];
    if (objectStore->Name() == aName) {
      retval = objectStore;
      return retval.forget();
    }
  }

  retval = IDBObjectStore::Create(this, aObjectStoreInfo, mDatabaseInfo->id);

  mCreatedObjectStores.AppendElement(retval);

  return retval.forget();
}

void
IDBTransaction::OnNewFileInfo(FileInfo* aFileInfo)
{
  mCreatedFileInfos.AppendElement(aFileInfo);
}

void
IDBTransaction::ClearCreatedFileInfos()
{
  mCreatedFileInfos.Clear();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBTransaction)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBTransaction,
                                                  IDBWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mDatabase,
                                                       nsIDOMEventTarget)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(error)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(complete)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(abort)

  for (PRUint32 i = 0; i < tmp->mCreatedObjectStores.Length(); i++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCreatedObjectStores[i]");
    cb.NoteXPCOMChild(static_cast<nsIIDBObjectStore*>(
                      tmp->mCreatedObjectStores[i].get()));
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBTransaction, IDBWrapperCache)
  // Don't unlink mDatabase!
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(error)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(complete)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(abort)

  tmp->mCreatedObjectStores.Clear();

NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBTransaction)
  NS_INTERFACE_MAP_ENTRY(nsIIDBTransaction)
  NS_INTERFACE_MAP_ENTRY(nsIThreadObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBTransaction)
NS_INTERFACE_MAP_END_INHERITING(IDBWrapperCache)

NS_IMPL_ADDREF_INHERITED(IDBTransaction, IDBWrapperCache)
NS_IMPL_RELEASE_INHERITED(IDBTransaction, IDBWrapperCache)

DOMCI_DATA(IDBTransaction, IDBTransaction)

NS_IMPL_EVENT_HANDLER(IDBTransaction, error);
NS_IMPL_EVENT_HANDLER(IDBTransaction, complete);
NS_IMPL_EVENT_HANDLER(IDBTransaction, abort);

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
    mDatabaseInfo->GetObjectStoreNames(stackArray);

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
    info = mDatabaseInfo->GetObjectStore(aName);
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

  if (Mode() == nsIIDBTransaction::VERSION_CHANGE) {
    // If a version change transaction is aborted, the db must be closed
    mDatabase->Close();
  }

  // Fire the abort event if there are no outstanding requests. Otherwise the
  // abort event will be fired when all outstanding requests finish.
  if (needToCommitOrRollback) {
    return CommitOrRollback();
  }

  return NS_OK;
}

nsresult
IDBTransaction::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
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
                                   bool aMayWait,
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

CommitHelper::CommitHelper(
              IDBTransaction* aTransaction,
              IDBTransactionListener* aListener,
              const nsTArray<nsRefPtr<IDBObjectStore> >& aUpdatedObjectStores)
: mTransaction(aTransaction),
  mListener(aListener),
  mAborted(!!aTransaction->mAborted)
{
  mConnection.swap(aTransaction->mConnection);
  mUpdateFileRefcountFunction.swap(aTransaction->mUpdateFileRefcountFunction);

  for (PRUint32 i = 0; i < aUpdatedObjectStores.Length(); i++) {
    ObjectStoreInfo* info = aUpdatedObjectStores[i]->Info();
    if (info->comittedAutoIncrementId != info->nextAutoIncrementId) {
      mAutoIncrementObjectStores.AppendElement(aUpdatedObjectStores[i]);
    }
  }
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

    // Release file infos on the main thread, so they will eventually get
    // destroyed on correct thread.
    mTransaction->ClearCreatedFileInfos();
    if (mUpdateFileRefcountFunction) {
      mUpdateFileRefcountFunction->ClearFileInfoEntries();
      mUpdateFileRefcountFunction = nsnull;
    }

    nsCOMPtr<nsIDOMEvent> event;
    if (mAborted) {
      if (mTransaction->Mode() == nsIIDBTransaction::VERSION_CHANGE) {
        // This will make the database take a snapshot of it's DatabaseInfo
        mTransaction->Database()->Close();
        // Then remove the info from the hash as it contains invalid data.
        DatabaseInfo::Remove(mTransaction->Database()->Id());
      }

      event = CreateGenericEvent(NS_LITERAL_STRING(ABORT_EVT_STR),
                                 eDoesBubble, eNotCancelable);
    }
    else {
      event = CreateGenericEvent(NS_LITERAL_STRING(COMPLETE_EVT_STR),
                                 eDoesNotBubble, eNotCancelable);
    }
    NS_ENSURE_TRUE(event, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    bool dummy;
    if (NS_FAILED(mTransaction->DispatchEvent(event, &dummy))) {
      NS_WARNING("Dispatch failed!");
    }

#ifdef DEBUG
    mTransaction->mFiredCompleteOrAbort = true;
#endif

    // Tell the listener (if we have one) that we're done
    if (mListener) {
      mListener->NotifyTransactionComplete(mTransaction);
    }

    mTransaction = nsnull;

    return NS_OK;
  }

  IDBDatabase* database = mTransaction->Database();
  if (database->IsInvalidated()) {
    mAborted = true;
  }

  if (mConnection) {
    IndexedDatabaseManager::SetCurrentWindow(database->GetOwner());

    if (!mAborted && mUpdateFileRefcountFunction &&
        NS_FAILED(mUpdateFileRefcountFunction->UpdateDatabase(mConnection))) {
      mAborted = true;
    }

    if (!mAborted && NS_FAILED(WriteAutoIncrementCounts())) {
      mAborted = true;
    }

    if (!mAborted) {
      NS_NAMED_LITERAL_CSTRING(release, "COMMIT TRANSACTION");
      if (NS_SUCCEEDED(mConnection->ExecuteSimpleSQL(release))) {
        if (mUpdateFileRefcountFunction) {
          mUpdateFileRefcountFunction->UpdateFileInfos();
        }
        CommitAutoIncrementCounts();
      }
      else {
        mAborted = true;
      }
    }

    if (mAborted) {
      RevertAutoIncrementCounts();
      NS_NAMED_LITERAL_CSTRING(rollback, "ROLLBACK TRANSACTION");
      if (NS_FAILED(mConnection->ExecuteSimpleSQL(rollback))) {
        NS_WARNING("Failed to rollback transaction!");
      }
    }
  }

  mDoomedObjects.Clear();

  if (mConnection) {
    if (mUpdateFileRefcountFunction) {
      nsresult rv = mConnection->RemoveFunction(
        NS_LITERAL_CSTRING("update_refcount"));
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to remove function!");
      }
    }

    mConnection->Close();
    mConnection = nsnull;

    IndexedDatabaseManager::SetCurrentWindow(nsnull);
  }

  return NS_OK;
}

nsresult
CommitHelper::WriteAutoIncrementCounts()
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv;
  for (PRUint32 i = 0; i < mAutoIncrementObjectStores.Length(); i++) {
    ObjectStoreInfo* info = mAutoIncrementObjectStores[i]->Info();
    if (!stmt) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "UPDATE object_store SET auto_increment = :ai "
        "WHERE id = :osid;"), getter_AddRefs(stmt));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      stmt->Reset();
    }

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), info->id);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("ai"),
                               info->nextAutoIncrementId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  return NS_OK;
}

void
CommitHelper::CommitAutoIncrementCounts()
{
  for (PRUint32 i = 0; i < mAutoIncrementObjectStores.Length(); i++) {
    ObjectStoreInfo* info = mAutoIncrementObjectStores[i]->Info();
    info->comittedAutoIncrementId = info->nextAutoIncrementId;
  }
}

void
CommitHelper::RevertAutoIncrementCounts()
{
  for (PRUint32 i = 0; i < mAutoIncrementObjectStores.Length(); i++) {
    ObjectStoreInfo* info = mAutoIncrementObjectStores[i]->Info();
    info->nextAutoIncrementId = info->comittedAutoIncrementId;
  }
}

nsresult
UpdateRefcountFunction::Init()
{
  NS_ENSURE_TRUE(mFileInfoEntries.Init(), NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(UpdateRefcountFunction, mozIStorageFunction)

NS_IMETHODIMP
UpdateRefcountFunction::OnFunctionCall(mozIStorageValueArray* aValues,
                                       nsIVariant** _retval)
{
  *_retval = nsnull;

  PRUint32 numEntries;
  nsresult rv = aValues->GetNumEntries(&numEntries);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(numEntries == 2, "unexpected number of arguments");

#ifdef DEBUG
  PRInt32 type1 = mozIStorageValueArray::VALUE_TYPE_NULL;
  aValues->GetTypeOfIndex(0, &type1);

  PRInt32 type2 = mozIStorageValueArray::VALUE_TYPE_NULL;
  aValues->GetTypeOfIndex(1, &type2);

  NS_ASSERTION(!(type1 == mozIStorageValueArray::VALUE_TYPE_NULL &&
                 type2 == mozIStorageValueArray::VALUE_TYPE_NULL),
               "Shouldn't be called!");
#endif

  rv = ProcessValue(aValues, 0, eDecrement);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ProcessValue(aValues, 1, eIncrement);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpdateRefcountFunction::ProcessValue(mozIStorageValueArray* aValues,
                                     PRInt32 aIndex,
                                     UpdateType aUpdateType)
{
  PRInt32 type;
  aValues->GetTypeOfIndex(aIndex, &type);
  if (type == mozIStorageValueArray::VALUE_TYPE_NULL) {
    return NS_OK;
  }

  nsString ids;
  aValues->GetString(aIndex, ids);

  nsTArray<PRInt64> fileIds;
  nsresult rv = IDBObjectStore::ConvertFileIdsToArray(ids, fileIds);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < fileIds.Length(); i++) {
    PRInt64 id = fileIds.ElementAt(i);

    FileInfoEntry* entry;
    if (!mFileInfoEntries.Get(id, &entry)) {
      nsRefPtr<FileInfo> fileInfo = mFileManager->GetFileInfo(id);
      NS_ASSERTION(fileInfo, "Shouldn't be null!");

      nsAutoPtr<FileInfoEntry> newEntry(new FileInfoEntry(fileInfo));
      if (!mFileInfoEntries.Put(id, newEntry)) {
        NS_WARNING("Out of memory?");
        return NS_ERROR_OUT_OF_MEMORY;
      }
      entry = newEntry.forget();
    }

    switch (aUpdateType) {
      case eIncrement:
        entry->mDelta++;
        break;
      case eDecrement:
        entry->mDelta--;
        break;
      default:
        NS_NOTREACHED("Unknown update type!");
    }
  }

  return NS_OK;
}

PLDHashOperator
UpdateRefcountFunction::DatabaseUpdateCallback(const PRUint64& aKey,
                                               FileInfoEntry* aValue,
                                               void* aUserArg)
{
  if (!aValue->mDelta) {
    return PL_DHASH_NEXT;
  }

  DatabaseUpdateFunction* function =
    static_cast<DatabaseUpdateFunction*>(aUserArg);

  if (!function->Update(aKey, aValue->mDelta)) {
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

PLDHashOperator
UpdateRefcountFunction::FileInfoUpdateCallback(const PRUint64& aKey,
                                               FileInfoEntry* aValue,
                                               void* aUserArg)
{
  if (aValue->mDelta) {
    aValue->mFileInfo->UpdateDBRefs(aValue->mDelta);
  }

  return PL_DHASH_NEXT;
}

bool
UpdateRefcountFunction::DatabaseUpdateFunction::Update(PRInt64 aId,
                                                       PRInt32 aDelta)
{
  nsresult rv = UpdateInternal(aId, aDelta);
  if (NS_FAILED(rv)) {
    mErrorCode = rv;
    return false;
  }

  return true;
}

nsresult
UpdateRefcountFunction::DatabaseUpdateFunction::UpdateInternal(PRInt64 aId,
                                                               PRInt32 aDelta)
{
  nsresult rv;

  if (!mUpdateStatement) {
    rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE file SET refcount = refcount + :delta WHERE id = :id"
    ), getter_AddRefs(mUpdateStatement));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mozStorageStatementScoper updateScoper(mUpdateStatement);

  rv = mUpdateStatement->BindInt32ByName(NS_LITERAL_CSTRING("delta"), aDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mUpdateStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mUpdateStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rows;
  rv = mConnection->GetAffectedRows(&rows);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rows > 0) {
    return NS_OK;
  }

  if (!mInsertStatement) {
    rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO file (id, refcount) VALUES(:id, :delta)"
    ), getter_AddRefs(mInsertStatement));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mozStorageStatementScoper insertScoper(mInsertStatement);

  rv = mInsertStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mInsertStatement->BindInt32ByName(NS_LITERAL_CSTRING("delta"), aDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mInsertStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
