/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IDBTransaction.h"

#include "nsIAppShell.h"
#include "nsIScriptContext.h"

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/storage.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMLists.h"
#include "nsEventDispatcher.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsWidgetsCID.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBObjectStore.h"
#include "IndexedDatabaseManager.h"
#include "ProfilerHelpers.h"
#include "TransactionThreadPool.h"

#include "ipc/IndexedDBChild.h"

#define SAVEPOINT_NAME "savepoint"

using namespace mozilla::dom;
USING_INDEXEDDB_NAMESPACE
using mozilla::dom::quota::QuotaManager;
using mozilla::ErrorResult;

namespace {

NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef MOZ_ENABLE_PROFILER_SPS
uint64_t gNextSerialNumber = 1;
#endif

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

} // anonymous namespace


// static
already_AddRefed<IDBTransaction>
IDBTransaction::CreateInternal(IDBDatabase* aDatabase,
                               nsTArray<nsString>& aObjectStoreNames,
                               Mode aMode,
                               bool aDispatchDelayed,
                               bool aIsVersionChangeTransactionChild)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess() || !aDispatchDelayed,
               "No support for delayed-dispatch transactions in child "
               "process!");
  NS_ASSERTION(!aIsVersionChangeTransactionChild ||
               (!IndexedDatabaseManager::IsMainProcess() &&
                aMode == IDBTransaction::VERSION_CHANGE),
               "Busted logic!");

  nsRefPtr<IDBTransaction> transaction = new IDBTransaction();

  transaction->BindToOwner(aDatabase);
  transaction->SetScriptOwner(aDatabase->GetScriptOwner());
  transaction->mDatabase = aDatabase;
  transaction->mMode = aMode;
  transaction->mDatabaseInfo = aDatabase->Info();
  transaction->mObjectStoreNames.AppendElements(aObjectStoreNames);
  transaction->mObjectStoreNames.Sort();

  IndexedDBTransactionChild* actor = nullptr;

  transaction->mCreatedFileInfos.Init();

  if (IndexedDatabaseManager::IsMainProcess()) {
    transaction->mCachedStatements.Init();

    if (aMode != IDBTransaction::VERSION_CHANGE) {
      TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
      NS_ENSURE_TRUE(pool, nullptr);

      static StartTransactionRunnable sStartTransactionRunnable;
      pool->Dispatch(transaction, &sStartTransactionRunnable, false, nullptr);
    }
  }
  else if (!aIsVersionChangeTransactionChild) {
    IndexedDBDatabaseChild* dbActor = aDatabase->GetActorChild();
    NS_ASSERTION(dbActor, "Must have an actor here!");

    ipc::NormalTransactionParams params;
    params.names().AppendElements(aObjectStoreNames);
    params.mode() = aMode;

    actor = new IndexedDBTransactionChild();

    dbActor->SendPIndexedDBTransactionConstructor(actor, params);
  }

  if (!aDispatchDelayed) {
    nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
    NS_ENSURE_TRUE(appShell, nullptr);

    nsresult rv = appShell->RunBeforeNextEvent(transaction);
    NS_ENSURE_SUCCESS(rv, nullptr);

    transaction->mCreating = true;
  }

  if (actor) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    actor->SetTransaction(transaction);
  }

  return transaction.forget();
}

IDBTransaction::IDBTransaction()
: mReadyState(IDBTransaction::INITIAL),
  mMode(IDBTransaction::READ_ONLY),
  mPendingRequests(0),
  mSavepointCount(0),
  mActorChild(nullptr),
  mActorParent(nullptr),
  mAbortCode(NS_OK),
#ifdef MOZ_ENABLE_PROFILER_SPS
  mSerialNumber(gNextSerialNumber++),
#endif
  mCreating(false)
#ifdef DEBUG
  , mFiredCompleteOrAbort(false)
#endif
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SetIsDOMBinding();
}

IDBTransaction::~IDBTransaction()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mPendingRequests, "Should have no pending requests here!");
  NS_ASSERTION(!mSavepointCount, "Should have released them all!");
  NS_ASSERTION(!mConnection, "Should have called CommitOrRollback!");
  NS_ASSERTION(!mCreating, "Should have been cleared already!");
  NS_ASSERTION(mFiredCompleteOrAbort, "Should have fired event!");

  NS_ASSERTION(!mActorParent, "Actor parent owns us, how can we be dying?!");
  if (mActorChild) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

void
IDBTransaction::OnNewRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (!mPendingRequests) {
    NS_ASSERTION(mReadyState == IDBTransaction::INITIAL,
                 "Reusing a transaction!");
    mReadyState = IDBTransaction::LOADING;
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
    NS_ASSERTION(NS_FAILED(mAbortCode) || mReadyState == IDBTransaction::LOADING,
                 "Bad state!");
    mReadyState = IDBTransaction::COMMITTING;
    CommitOrRollback();
  }
}

void
IDBTransaction::OnRequestDisconnected()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mPendingRequests, "Mismatched calls!");
  --mPendingRequests;
}

void
IDBTransaction::RemoveObjectStore(const nsAString& aName)
{
  NS_ASSERTION(mMode == IDBTransaction::VERSION_CHANGE,
               "Only remove object stores on VERSION_CHANGE transactions");

  mDatabaseInfo->RemoveObjectStore(aName);

  for (uint32_t i = 0; i < mCreatedObjectStores.Length(); i++) {
    if (mCreatedObjectStores[i]->Name() == aName) {
      nsRefPtr<IDBObjectStore> objectStore = mCreatedObjectStores[i];
      mCreatedObjectStores.RemoveElementAt(i);
      mDeletedObjectStores.AppendElement(objectStore);
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

  if (!IndexedDatabaseManager::IsMainProcess()) {
    if (mActorChild) {
      mActorChild->SendAllRequestsFinished();
    }

    return NS_OK;
  }

  nsRefPtr<CommitHelper> helper =
    new CommitHelper(this, mListener, mCreatedObjectStores);

  TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
  NS_ENSURE_STATE(pool);

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
  NS_ENSURE_TRUE(stmt, NS_OK);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, NS_OK);

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
  NS_ENSURE_TRUE_VOID(stmt);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->Execute();
  NS_ENSURE_SUCCESS_VOID(rv);
}

nsresult
IDBTransaction::GetOrCreateConnection(mozIStorageConnection** aResult)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("IndexedDB", "IDBTransaction::GetOrCreateConnection");

  if (mDatabase->IsInvalidated()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mConnection) {
    nsCOMPtr<mozIStorageConnection> connection =
      IDBFactory::GetConnection(mDatabase->FilePath(),
                                mDatabase->Origin());
    NS_ENSURE_TRUE(connection, NS_ERROR_FAILURE);

    nsresult rv;

    nsRefPtr<UpdateRefcountFunction> function;
    nsCString beginTransaction;
    if (mMode != IDBTransaction::READ_ONLY) {
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
    NS_ENSURE_SUCCESS(rv, nullptr);

    mCachedStatements.Put(aQuery, stmt);
  }

  return stmt.forget();
}

bool
IDBTransaction::IsOpen() const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // If we haven't started anything then we're open.
  if (mReadyState == IDBTransaction::INITIAL) {
    return true;
  }

  // If we've already started then we need to check to see if we still have the
  // mCreating flag set. If we do (i.e. we haven't returned to the event loop
  // from the time we were created) then we are open. Otherwise check the
  // currently running transaction to see if it's the same. We only allow other
  // requests to be made if this transaction is currently running.
  if (mReadyState == IDBTransaction::LOADING) {
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
                                       ObjectStoreInfo* aObjectStoreInfo,
                                       bool aCreating)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aObjectStoreInfo, "Null pointer!");
  NS_ASSERTION(!aCreating || GetMode() == IDBTransaction::VERSION_CHANGE,
               "How else can we create here?!");

  nsRefPtr<IDBObjectStore> retval;

  for (uint32_t index = 0; index < mCreatedObjectStores.Length(); index++) {
    nsRefPtr<IDBObjectStore>& objectStore = mCreatedObjectStores[index];
    if (objectStore->Name() == aName) {
      retval = objectStore;
      return retval.forget();
    }
  }

  retval = IDBObjectStore::Create(this, aObjectStoreInfo, mDatabaseInfo->id,
                                  aCreating);

  mCreatedObjectStores.AppendElement(retval);

  return retval.forget();
}

already_AddRefed<FileInfo>
IDBTransaction::GetFileInfo(nsIDOMBlob* aBlob)
{
  nsRefPtr<FileInfo> fileInfo;
  mCreatedFileInfos.Get(aBlob, getter_AddRefs(fileInfo));
  return fileInfo.forget();
}

void
IDBTransaction::AddFileInfo(nsIDOMBlob* aBlob, FileInfo* aFileInfo)
{
  mCreatedFileInfos.Put(aBlob, aFileInfo);
}

void
IDBTransaction::ClearCreatedFileInfos()
{
  mCreatedFileInfos.Clear();
}

nsresult
IDBTransaction::AbortInternal(nsresult aAbortCode,
                              already_AddRefed<DOMError> aError)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<DOMError> error = aError;

  if (IsFinished()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  if (mActorChild) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorChild->SendAbort(aAbortCode);
  }

  bool needToCommitOrRollback = mReadyState == IDBTransaction::INITIAL;

  mAbortCode = aAbortCode;
  mReadyState = IDBTransaction::DONE;
  mError = error.forget();

  if (GetMode() == IDBTransaction::VERSION_CHANGE) {
    // If a version change transaction is aborted, we must revert the world
    // back to its previous state.
    mDatabase->RevertToPreviousState();

    DatabaseInfo* dbInfo = mDatabase->Info();

    for (uint32_t i = 0; i < mCreatedObjectStores.Length(); i++) {
      nsRefPtr<IDBObjectStore>& objectStore = mCreatedObjectStores[i];
      ObjectStoreInfo* info = dbInfo->GetObjectStore(objectStore->Name());

      if (!info) {
        info = new ObjectStoreInfo(*objectStore->Info());
        info->indexes.Clear();
      }

      objectStore->SetInfo(info);
    }

    for (uint32_t i = 0; i < mDeletedObjectStores.Length(); i++) {
      nsRefPtr<IDBObjectStore>& objectStore = mDeletedObjectStores[i];
      ObjectStoreInfo* info = dbInfo->GetObjectStore(objectStore->Name());

      if (!info) {
        info = new ObjectStoreInfo(*objectStore->Info());
        info->indexes.Clear();
      }

      objectStore->SetInfo(info);
    }

    // and then the db must be closed
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
IDBTransaction::Abort(IDBRequest* aRequest)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRequest, "This is undesirable.");

  ErrorResult rv;
  nsRefPtr<DOMError> error = aRequest->GetError(rv);

  return AbortInternal(aRequest->GetErrorCode(), error.forget());
}

nsresult
IDBTransaction::Abort(nsresult aErrorCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<DOMError> error = new DOMError(GetOwner(), aErrorCode);
  return AbortInternal(aErrorCode, error.forget());
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBTransaction,
                                                  IDBWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDatabase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCreatedObjectStores)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeletedObjectStores)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBTransaction, IDBWrapperCache)
  // Don't unlink mDatabase!
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCreatedObjectStores)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDeletedObjectStores)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBTransaction)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END_INHERITING(IDBWrapperCache)

NS_IMPL_ADDREF_INHERITED(IDBTransaction, IDBWrapperCache)
NS_IMPL_RELEASE_INHERITED(IDBTransaction, IDBWrapperCache)

DOMCI_DATA(IDBTransaction, IDBTransaction)

JSObject*
IDBTransaction::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return IDBTransactionBinding::Wrap(aCx, aScope, this);
}

mozilla::dom::IDBTransactionMode
IDBTransaction::GetMode(ErrorResult& aRv) const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  switch (mMode) {
    case READ_ONLY:
      return mozilla::dom::IDBTransactionMode::Readonly;

    case READ_WRITE:
      return mozilla::dom::IDBTransactionMode::Readwrite;

    case VERSION_CHANGE:
      return mozilla::dom::IDBTransactionMode::Versionchange;

    case MODE_INVALID:
    default:
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return mozilla::dom::IDBTransactionMode::Readonly;
  }
}

DOMError*
IDBTransaction::GetError(ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return mError;
}

already_AddRefed<nsIDOMDOMStringList>
IDBTransaction::GetObjectStoreNames(ErrorResult& aRv)
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

  uint32_t count = arrayOfNames->Length();
  for (uint32_t index = 0; index < count; index++) {
    if (!list->Add(arrayOfNames->ElementAt(index))) {
      NS_WARNING("Failed to add element!");
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }
  }

  return list.forget();
}

already_AddRefed<nsIIDBObjectStore>
IDBTransaction::ObjectStore(const nsAString& aName, ErrorResult& aRv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsFinished()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  ObjectStoreInfo* info = nullptr;

  if (mMode == IDBTransaction::VERSION_CHANGE ||
      mObjectStoreNames.Contains(aName)) {
    info = mDatabaseInfo->GetObjectStore(aName);
  }

  if (!info) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
    return nullptr;
  }

  nsRefPtr<IDBObjectStore> objectStore =
    GetOrCreateObjectStore(aName, info, false);
  if (!objectStore) {
    NS_WARNING("Failed to get or create object store!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  return objectStore.forget();
}

nsresult
IDBTransaction::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mDatabase;
  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // We're back at the event loop, no longer newborn.
  mCreating = false;

  // Maybe set the readyState to DONE if there were no requests generated.
  if (mReadyState == IDBTransaction::INITIAL) {
    mReadyState = IDBTransaction::DONE;

    if (NS_FAILED(CommitOrRollback())) {
      NS_WARNING("Failed to commit!");
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
  mAbortCode(aTransaction->mAbortCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mConnection.swap(aTransaction->mConnection);
  mUpdateFileRefcountFunction.swap(aTransaction->mUpdateFileRefcountFunction);

  for (uint32_t i = 0; i < aUpdatedObjectStores.Length(); i++) {
    ObjectStoreInfo* info = aUpdatedObjectStores[i]->Info();
    if (info->comittedAutoIncrementId != info->nextAutoIncrementId) {
      mAutoIncrementObjectStores.AppendElement(aUpdatedObjectStores[i]);
    }
  }
}

CommitHelper::CommitHelper(IDBTransaction* aTransaction,
                           nsresult aAbortCode)
: mTransaction(aTransaction),
  mAbortCode(aAbortCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

CommitHelper::~CommitHelper()
{
}

NS_IMPL_ISUPPORTS1(CommitHelper, nsIRunnable)

NS_IMETHODIMP
CommitHelper::Run()
{
  if (NS_IsMainThread()) {
    PROFILER_MAIN_THREAD_LABEL("IndexedDB", "CommitHelper::Run");

    NS_ASSERTION(mDoomedObjects.IsEmpty(), "Didn't release doomed objects!");

    mTransaction->mReadyState = IDBTransaction::DONE;

    // Release file infos on the main thread, so they will eventually get
    // destroyed on correct thread.
    mTransaction->ClearCreatedFileInfos();
    if (mUpdateFileRefcountFunction) {
      mUpdateFileRefcountFunction->ClearFileInfoEntries();
      mUpdateFileRefcountFunction = nullptr;
    }

    nsCOMPtr<nsIDOMEvent> event;
    if (NS_FAILED(mAbortCode)) {
      if (mTransaction->GetMode() == IDBTransaction::VERSION_CHANGE) {
        // This will make the database take a snapshot of it's DatabaseInfo
        mTransaction->Database()->Close();
        // Then remove the info from the hash as it contains invalid data.
        DatabaseInfo::Remove(mTransaction->Database()->Id());
      }

      event = CreateGenericEvent(mTransaction,
                                 NS_LITERAL_STRING(ABORT_EVT_STR),
                                 eDoesBubble, eNotCancelable);

      // The transaction may already have an error object (e.g. if one of the
      // requests failed).  If it doesn't, and it wasn't aborted
      // programmatically, create one now.
      if (!mTransaction->mError &&
          mAbortCode != NS_ERROR_DOM_INDEXEDDB_ABORT_ERR) {
        mTransaction->mError = new DOMError(mTransaction->GetOwner(), mAbortCode);
      }
    }
    else {
      event = CreateGenericEvent(mTransaction,
                                 NS_LITERAL_STRING(COMPLETE_EVT_STR),
                                 eDoesNotBubble, eNotCancelable);
    }
    NS_ENSURE_TRUE(event, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (mListener) {
      mListener->NotifyTransactionPreComplete(mTransaction);
    }

    IDB_PROFILER_MARK("IndexedDB Transaction %llu: Complete (rv = %lu)",
                      "IDBTransaction[%llu] MT Complete",
                      mTransaction->GetSerialNumber(), mAbortCode);

    bool dummy;
    if (NS_FAILED(mTransaction->DispatchEvent(event, &dummy))) {
      NS_WARNING("Dispatch failed!");
    }

#ifdef DEBUG
    mTransaction->mFiredCompleteOrAbort = true;
#endif

    if (mListener) {
      mListener->NotifyTransactionPostComplete(mTransaction);
    }

    mTransaction = nullptr;

    return NS_OK;
  }

  PROFILER_LABEL("IndexedDB", "CommitHelper::Run");

  IDBDatabase* database = mTransaction->Database();
  if (database->IsInvalidated()) {
    mAbortCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (mConnection) {
    QuotaManager::SetCurrentWindow(database->GetOwner());

    if (NS_SUCCEEDED(mAbortCode) && mUpdateFileRefcountFunction &&
        NS_FAILED(mUpdateFileRefcountFunction->WillCommit(mConnection))) {
      mAbortCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (NS_SUCCEEDED(mAbortCode) && NS_FAILED(WriteAutoIncrementCounts())) {
      mAbortCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (NS_SUCCEEDED(mAbortCode)) {
      NS_NAMED_LITERAL_CSTRING(release, "COMMIT TRANSACTION");
      nsresult rv = mConnection->ExecuteSimpleSQL(release);
      if (NS_SUCCEEDED(rv)) {
        if (mUpdateFileRefcountFunction) {
          mUpdateFileRefcountFunction->DidCommit();
        }
        CommitAutoIncrementCounts();
      }
      else if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
        // mozstorage translates SQLITE_FULL to NS_ERROR_FILE_NO_DEVICE_SPACE,
        // which we know better as NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR.
        mAbortCode = NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
      }
      else {
        mAbortCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    if (NS_FAILED(mAbortCode)) {
      if (mUpdateFileRefcountFunction) {
        mUpdateFileRefcountFunction->DidAbort();
      }
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
    mConnection = nullptr;

    QuotaManager::SetCurrentWindow(nullptr);
  }

  return NS_OK;
}

nsresult
CommitHelper::WriteAutoIncrementCounts()
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv;
  for (uint32_t i = 0; i < mAutoIncrementObjectStores.Length(); i++) {
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
  for (uint32_t i = 0; i < mAutoIncrementObjectStores.Length(); i++) {
    ObjectStoreInfo* info = mAutoIncrementObjectStores[i]->Info();
    info->comittedAutoIncrementId = info->nextAutoIncrementId;
  }
}

void
CommitHelper::RevertAutoIncrementCounts()
{
  for (uint32_t i = 0; i < mAutoIncrementObjectStores.Length(); i++) {
    ObjectStoreInfo* info = mAutoIncrementObjectStores[i]->Info();
    info->nextAutoIncrementId = info->comittedAutoIncrementId;
  }
}

nsresult
UpdateRefcountFunction::Init()
{
  mFileInfoEntries.Init();

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(UpdateRefcountFunction, mozIStorageFunction)

NS_IMETHODIMP
UpdateRefcountFunction::OnFunctionCall(mozIStorageValueArray* aValues,
                                       nsIVariant** _retval)
{
  *_retval = nullptr;

  uint32_t numEntries;
  nsresult rv = aValues->GetNumEntries(&numEntries);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(numEntries == 2, "unexpected number of arguments");

#ifdef DEBUG
  int32_t type1 = mozIStorageValueArray::VALUE_TYPE_NULL;
  aValues->GetTypeOfIndex(0, &type1);

  int32_t type2 = mozIStorageValueArray::VALUE_TYPE_NULL;
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
UpdateRefcountFunction::WillCommit(mozIStorageConnection* aConnection)
{
  DatabaseUpdateFunction function(aConnection, this);

  mFileInfoEntries.EnumerateRead(DatabaseUpdateCallback, &function);

  nsresult rv = function.ErrorCode();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CreateJournals();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
UpdateRefcountFunction::DidCommit()
{
  mFileInfoEntries.EnumerateRead(FileInfoUpdateCallback, nullptr);

  nsresult rv = RemoveJournals(mJournalsToRemoveAfterCommit);
  NS_ENSURE_SUCCESS_VOID(rv);
}

void
UpdateRefcountFunction::DidAbort()
{
  nsresult rv = RemoveJournals(mJournalsToRemoveAfterAbort);
  NS_ENSURE_SUCCESS_VOID(rv);
}

nsresult
UpdateRefcountFunction::ProcessValue(mozIStorageValueArray* aValues,
                                     int32_t aIndex,
                                     UpdateType aUpdateType)
{
  int32_t type;
  aValues->GetTypeOfIndex(aIndex, &type);
  if (type == mozIStorageValueArray::VALUE_TYPE_NULL) {
    return NS_OK;
  }

  nsString ids;
  aValues->GetString(aIndex, ids);

  nsTArray<int64_t> fileIds;
  nsresult rv = IDBObjectStore::ConvertFileIdsToArray(ids, fileIds);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < fileIds.Length(); i++) {
    int64_t id = fileIds.ElementAt(i);

    FileInfoEntry* entry;
    if (!mFileInfoEntries.Get(id, &entry)) {
      nsRefPtr<FileInfo> fileInfo = mFileManager->GetFileInfo(id);
      NS_ASSERTION(fileInfo, "Shouldn't be null!");

      nsAutoPtr<FileInfoEntry> newEntry(new FileInfoEntry(fileInfo));
      mFileInfoEntries.Put(id, newEntry);
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

nsresult
UpdateRefcountFunction::CreateJournals()
{
  nsCOMPtr<nsIFile> journalDirectory = mFileManager->GetJournalDirectory();
  NS_ENSURE_TRUE(journalDirectory, NS_ERROR_FAILURE);

  for (uint32_t i = 0; i < mJournalsToCreateBeforeCommit.Length(); i++) {
    int64_t id = mJournalsToCreateBeforeCommit[i];

    nsCOMPtr<nsIFile> file =
      mFileManager->GetFileForId(journalDirectory, id);
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    nsresult rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    NS_ENSURE_SUCCESS(rv, rv);

    mJournalsToRemoveAfterAbort.AppendElement(id);
  }

  return NS_OK;
}

nsresult
UpdateRefcountFunction::RemoveJournals(const nsTArray<int64_t>& aJournals)
{
  nsCOMPtr<nsIFile> journalDirectory = mFileManager->GetJournalDirectory();
  NS_ENSURE_TRUE(journalDirectory, NS_ERROR_FAILURE);

  for (uint32_t index = 0; index < aJournals.Length(); index++) {
    nsCOMPtr<nsIFile> file =
      mFileManager->GetFileForId(journalDirectory, aJournals[index]);
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    if (NS_FAILED(file->Remove(false))) {
      NS_WARNING("Failed to removed journal!");
    }
  }

  return NS_OK;
}

PLDHashOperator
UpdateRefcountFunction::DatabaseUpdateCallback(const uint64_t& aKey,
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
UpdateRefcountFunction::FileInfoUpdateCallback(const uint64_t& aKey,
                                               FileInfoEntry* aValue,
                                               void* aUserArg)
{
  if (aValue->mDelta) {
    aValue->mFileInfo->UpdateDBRefs(aValue->mDelta);
  }

  return PL_DHASH_NEXT;
}

bool
UpdateRefcountFunction::DatabaseUpdateFunction::Update(int64_t aId,
                                                       int32_t aDelta)
{
  nsresult rv = UpdateInternal(aId, aDelta);
  if (NS_FAILED(rv)) {
    mErrorCode = rv;
    return false;
  }

  return true;
}

nsresult
UpdateRefcountFunction::DatabaseUpdateFunction::UpdateInternal(int64_t aId,
                                                               int32_t aDelta)
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

  int32_t rows;
  rv = mConnection->GetAffectedRows(&rows);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rows > 0) {
    if (!mSelectStatement) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT id FROM file where id = :id"
      ), getter_AddRefs(mSelectStatement));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    mozStorageStatementScoper selectScoper(mSelectStatement);

    rv = mSelectStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasResult;
    rv = mSelectStatement->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!hasResult) {
      // Don't have to create the journal here, we can create all at once,
      // just before commit
      mFunction->mJournalsToCreateBeforeCommit.AppendElement(aId);
    }

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

  mFunction->mJournalsToRemoveAfterCommit.AppendElement(aId);

  return NS_OK;
}
