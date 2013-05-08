/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransactionThreadPool.h"

#include "nsIObserverService.h"
#include "nsIThreadPool.h"

#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCIDInternal.h"

#include "ProfilerHelpers.h"

using mozilla::MonitorAutoLock;

USING_INDEXEDDB_NAMESPACE

namespace {

const uint32_t kThreadLimit = 20;
const uint32_t kIdleThreadLimit = 5;
const uint32_t kIdleThreadTimeoutMs = 30000;

TransactionThreadPool* gInstance = nullptr;
bool gShutdown = false;

#ifdef MOZ_ENABLE_PROFILER_SPS

class TransactionThreadPoolListener : public nsIThreadPoolListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADPOOLLISTENER

private:
  virtual ~TransactionThreadPoolListener()
  { }
};

#endif // MOZ_ENABLE_PROFILER_SPS

} // anonymous namespace

BEGIN_INDEXEDDB_NAMESPACE

class FinishTransactionRunnable MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  inline FinishTransactionRunnable(IDBTransaction* aTransaction,
                                   nsCOMPtr<nsIRunnable>& aFinishRunnable);

private:
  IDBTransaction* mTransaction;
  nsCOMPtr<nsIRunnable> mFinishRunnable;
};

END_INDEXEDDB_NAMESPACE

TransactionThreadPool::TransactionThreadPool()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "More than one instance!");
}

TransactionThreadPool::~TransactionThreadPool()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(gInstance == this, "Different instances!");
  gInstance = nullptr;
}

// static
TransactionThreadPool*
TransactionThreadPool::GetOrCreate()
{
  if (!gInstance && !gShutdown) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    nsAutoPtr<TransactionThreadPool> pool(new TransactionThreadPool());

    nsresult rv = pool->Init();
    NS_ENSURE_SUCCESS(rv, nullptr);

    gInstance = pool.forget();
  }
  return gInstance;
}

// static
TransactionThreadPool*
TransactionThreadPool::Get()
{
  return gInstance;
}

// static
void
TransactionThreadPool::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  gShutdown = true;

  if (gInstance) {
    if (NS_FAILED(gInstance->Cleanup())) {
      NS_WARNING("Failed to shutdown thread pool!");
    }
    delete gInstance;
    gInstance = nullptr;
  }
}

nsresult
TransactionThreadPool::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mTransactionsInProgress.Init();

  nsresult rv;
  mThreadPool = do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetName(NS_LITERAL_CSTRING("IndexedDB Trans"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetThreadLimit(kThreadLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetIdleThreadLimit(kIdleThreadLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetIdleThreadTimeout(kIdleThreadTimeoutMs);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_ENABLE_PROFILER_SPS
  nsCOMPtr<nsIThreadPoolListener> listener =
    new TransactionThreadPoolListener();

  rv = mThreadPool->SetListener(listener);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  return NS_OK;
}

nsresult
TransactionThreadPool::Cleanup()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB", "TransactionThreadPool::Cleanup");

  nsresult rv = mThreadPool->Shutdown();
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure the pool is still accessible while any callbacks generated from
  // the other threads are processed.
  rv = NS_ProcessPendingEvents(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mCompleteCallbacks.IsEmpty()) {
    // Run all callbacks manually now.
    for (uint32_t index = 0; index < mCompleteCallbacks.Length(); index++) {
      mCompleteCallbacks[index].mCallback->Run();
    }
    mCompleteCallbacks.Clear();

    // And make sure they get processed.
    rv = NS_ProcessPendingEvents(nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// static
PLDHashOperator
TransactionThreadPool::MaybeUnblockTransaction(nsPtrHashKey<TransactionInfo>* aKey,
                                               void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  TransactionInfo* maybeUnblockedInfo = aKey->GetKey();
  TransactionInfo* finishedInfo = static_cast<TransactionInfo*>(aUserArg);

  NS_ASSERTION(maybeUnblockedInfo->blockedOn.Contains(finishedInfo),
               "Huh?");
  maybeUnblockedInfo->blockedOn.RemoveEntry(finishedInfo);
  if (!maybeUnblockedInfo->blockedOn.Count()) {
    // Let this transaction run.
    maybeUnblockedInfo->queue->Unblock();
  }

  return PL_DHASH_NEXT;
}

void
TransactionThreadPool::FinishTransaction(IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "TransactionThreadPool::FinishTransaction");

  // AddRef here because removing from the hash will call Release.
  nsRefPtr<IDBTransaction> transaction(aTransaction);

  nsIAtom* databaseId = aTransaction->mDatabase->Id();

  DatabaseTransactionInfo* dbTransactionInfo;
  if (!mTransactionsInProgress.Get(databaseId, &dbTransactionInfo)) {
    NS_ERROR("We don't know anyting about this database?!");
    return;
  }

  DatabaseTransactionInfo::TransactionHashtable& transactionsInProgress =
    dbTransactionInfo->transactions;

  uint32_t transactionCount = transactionsInProgress.Count();

#ifdef DEBUG
  if (aTransaction->mMode == IDBTransaction::VERSION_CHANGE) {
    NS_ASSERTION(transactionCount == 1,
                 "More transactions running than should be!");
  }
#endif

  if (transactionCount == 1) {
#ifdef DEBUG
    {
      const TransactionInfo* info = transactionsInProgress.Get(aTransaction);
      NS_ASSERTION(info->transaction == aTransaction, "Transaction mismatch!");
    }
#endif
    mTransactionsInProgress.Remove(databaseId);

    // See if we need to fire any complete callbacks.
    uint32_t index = 0;
    while (index < mCompleteCallbacks.Length()) {
      if (MaybeFireCallback(mCompleteCallbacks[index])) {
        mCompleteCallbacks.RemoveElementAt(index);
      }
      else {
        index++;
      }
    }

    return;
  }
  TransactionInfo* info = transactionsInProgress.Get(aTransaction);
  NS_ASSERTION(info, "We've never heard of this transaction?!?");

  const nsTArray<nsString>& objectStoreNames = aTransaction->mObjectStoreNames;
  for (uint32_t index = 0, count = objectStoreNames.Length(); index < count;
       index++) {
    TransactionInfoPair* blockInfo =
      dbTransactionInfo->blockingTransactions.Get(objectStoreNames[index]);
    NS_ASSERTION(blockInfo, "Huh?");

    if (aTransaction->mMode == IDBTransaction::READ_WRITE &&
        blockInfo->lastBlockingReads == info) {
      blockInfo->lastBlockingReads = nullptr;
    }

    uint32_t i = blockInfo->lastBlockingWrites.IndexOf(info);
    if (i != blockInfo->lastBlockingWrites.NoIndex) {
      blockInfo->lastBlockingWrites.RemoveElementAt(i);
    }
  }

  info->blocking.EnumerateEntries(MaybeUnblockTransaction, info);

  transactionsInProgress.Remove(aTransaction);
}

TransactionThreadPool::TransactionQueue&
TransactionThreadPool::GetQueueForTransaction(IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");

  nsIAtom* databaseId = aTransaction->mDatabase->Id();
  const nsTArray<nsString>& objectStoreNames = aTransaction->mObjectStoreNames;
  const uint16_t mode = aTransaction->mMode;

  // See if we can run this transaction now.
  DatabaseTransactionInfo* dbTransactionInfo;
  if (!mTransactionsInProgress.Get(databaseId, &dbTransactionInfo)) {
    // First transaction for this database.
    dbTransactionInfo = new DatabaseTransactionInfo();
    mTransactionsInProgress.Put(databaseId, dbTransactionInfo);
  }

  DatabaseTransactionInfo::TransactionHashtable& transactionsInProgress =
    dbTransactionInfo->transactions;
  TransactionInfo* info = transactionsInProgress.Get(aTransaction);
  if (info) {
    // We recognize this one.
    return *info->queue;
  }

  TransactionInfo* transactionInfo = new TransactionInfo(aTransaction);

  dbTransactionInfo->transactions.Put(aTransaction, transactionInfo);;

  for (uint32_t index = 0, count = objectStoreNames.Length(); index < count;
       index++) {
    TransactionInfoPair* blockInfo =
      dbTransactionInfo->blockingTransactions.Get(objectStoreNames[index]);
    if (!blockInfo) {
      blockInfo = new TransactionInfoPair();
      blockInfo->lastBlockingReads = nullptr;
      dbTransactionInfo->blockingTransactions.Put(objectStoreNames[index],
                                                  blockInfo);
    }

    // Mark what we are blocking on.
    if (blockInfo->lastBlockingReads) {
      TransactionInfo* blockingInfo = blockInfo->lastBlockingReads;
      transactionInfo->blockedOn.PutEntry(blockingInfo);
      blockingInfo->blocking.PutEntry(transactionInfo);
    }

    if (mode == IDBTransaction::READ_WRITE &&
        blockInfo->lastBlockingWrites.Length()) {
      for (uint32_t index = 0,
           count = blockInfo->lastBlockingWrites.Length(); index < count;
           index++) {
        TransactionInfo* blockingInfo = blockInfo->lastBlockingWrites[index];
        transactionInfo->blockedOn.PutEntry(blockingInfo);
        blockingInfo->blocking.PutEntry(transactionInfo);
      }
    }

    if (mode == IDBTransaction::READ_WRITE) {
      blockInfo->lastBlockingReads = transactionInfo;
      blockInfo->lastBlockingWrites.Clear();
    }
    else {
      blockInfo->lastBlockingWrites.AppendElement(transactionInfo);
    }
  }

  if (!transactionInfo->blockedOn.Count()) {
    transactionInfo->queue->Unblock();
  }

  return *transactionInfo->queue;
}

nsresult
TransactionThreadPool::Dispatch(IDBTransaction* aTransaction,
                                nsIRunnable* aRunnable,
                                bool aFinish,
                                nsIRunnable* aFinishRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  if (aTransaction->mDatabase->IsInvalidated() && !aFinish) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  TransactionQueue& queue = GetQueueForTransaction(aTransaction);

  queue.Dispatch(aRunnable);
  if (aFinish) {
    queue.Finish(aFinishRunnable);
  }
  return NS_OK;
}

void
TransactionThreadPool::WaitForDatabasesToComplete(
                                   nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
                                   nsIRunnable* aCallback)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aDatabases.IsEmpty(), "No databases to wait on!");
  NS_ASSERTION(aCallback, "Null pointer!");

  DatabasesCompleteCallback* callback = mCompleteCallbacks.AppendElement();

  callback->mCallback = aCallback;
  callback->mDatabases.SwapElements(aDatabases);

  if (MaybeFireCallback(*callback)) {
    mCompleteCallbacks.RemoveElementAt(mCompleteCallbacks.Length() - 1);
  }
}

// static
PLDHashOperator
TransactionThreadPool::CollectTransactions(IDBTransaction* aKey,
                                           TransactionInfo* aValue,
                                           void* aUserArg)
{
  nsAutoTArray<nsRefPtr<IDBTransaction>, 50>* transactionArray =
    static_cast<nsAutoTArray<nsRefPtr<IDBTransaction>, 50>*>(aUserArg);
  transactionArray->AppendElement(aKey);

  return PL_DHASH_NEXT;
}

void
TransactionThreadPool::AbortTransactionsForDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "TransactionThreadPool::"
                             "AbortTransactionsForDatabase");

  // Get list of transactions for this database id
  DatabaseTransactionInfo* dbTransactionInfo;
  if (!mTransactionsInProgress.Get(aDatabase->Id(), &dbTransactionInfo)) {
    // If there are no transactions, we're done.
    return;
  }

  // Collect any running transactions
  DatabaseTransactionInfo::TransactionHashtable& transactionsInProgress =
    dbTransactionInfo->transactions;

  NS_ASSERTION(transactionsInProgress.Count(), "Should never be 0!");

  nsAutoTArray<nsRefPtr<IDBTransaction>, 50> transactions;
  transactionsInProgress.EnumerateRead(CollectTransactions, &transactions);

  // Abort transactions. Do this after collecting the transactions in case
  // calling Abort() modifies the data structures we're iterating above.
  for (uint32_t index = 0; index < transactions.Length(); index++) {
    if (transactions[index]->Database() != aDatabase) {
      continue;
    }

    // This can fail, for example if the transaction is in the process of
    // being comitted. That is expected and fine, so we ignore any returned
    // errors.
    transactions[index]->Abort();
  }
}

struct MOZ_STACK_CLASS TransactionSearchInfo
{
  TransactionSearchInfo(nsIOfflineStorage* aDatabase)
    : db(aDatabase), found(false)
  {
  }

  nsIOfflineStorage* db;
  bool found;
};

// static
PLDHashOperator
TransactionThreadPool::FindTransaction(IDBTransaction* aKey,
                                       TransactionInfo* aValue,
                                       void* aUserArg)
{
  TransactionSearchInfo* info = static_cast<TransactionSearchInfo*>(aUserArg);

  if (aKey->Database() == info->db) {
    info->found = true;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}
bool
TransactionThreadPool::HasTransactionsForDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  DatabaseTransactionInfo* dbTransactionInfo = nullptr;
  dbTransactionInfo = mTransactionsInProgress.Get(aDatabase->Id());
  if (!dbTransactionInfo) {
    return false;
  }

  TransactionSearchInfo info(aDatabase);
  dbTransactionInfo->transactions.EnumerateRead(FindTransaction, &info);

  return info.found;
}

bool
TransactionThreadPool::MaybeFireCallback(DatabasesCompleteCallback aCallback)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB",
                             "TransactionThreadPool::MaybeFireCallback");

  for (uint32_t index = 0; index < aCallback.mDatabases.Length(); index++) {
    IDBDatabase* database = aCallback.mDatabases[index];
    if (!database) {
      MOZ_CRASH();
    }

    if (mTransactionsInProgress.Get(database->Id(),
                                    nullptr)) {
      return false;
    }
  }

  aCallback.mCallback->Run();
  return true;
}

TransactionThreadPool::
TransactionQueue::TransactionQueue(IDBTransaction* aTransaction)
: mMonitor("TransactionQueue::mMonitor"),
  mTransaction(aTransaction),
  mShouldFinish(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");
}

void
TransactionThreadPool::TransactionQueue::Unblock()
{
  MonitorAutoLock lock(mMonitor);

  // NB: Finish may be called before Unblock.

  TransactionThreadPool::Get()->mThreadPool->
    Dispatch(this, NS_DISPATCH_NORMAL);
}

void
TransactionThreadPool::TransactionQueue::Dispatch(nsIRunnable* aRunnable)
{
  MonitorAutoLock lock(mMonitor);

  NS_ASSERTION(!mShouldFinish, "Dispatch called after Finish!");

  mQueue.AppendElement(aRunnable);

  mMonitor.Notify();
}

void
TransactionThreadPool::TransactionQueue::Finish(nsIRunnable* aFinishRunnable)
{
  MonitorAutoLock lock(mMonitor);

  NS_ASSERTION(!mShouldFinish, "Finish called more than once!");

  mShouldFinish = true;
  mFinishRunnable = aFinishRunnable;

  mMonitor.Notify();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(TransactionThreadPool::TransactionQueue,
                              nsIRunnable)

NS_IMETHODIMP
TransactionThreadPool::TransactionQueue::Run()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("IndexedDB", "TransactionQueue::Run");

  IDB_PROFILER_MARK("IndexedDB Transaction %llu: Beginning database work",
                    "IDBTransaction[%llu] DT Start",
                    mTransaction->GetSerialNumber());

  nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> queue;
  nsCOMPtr<nsIRunnable> finishRunnable;
  bool shouldFinish = false;

  do {
    NS_ASSERTION(queue.IsEmpty(), "Should have cleared this!");

    {
      MonitorAutoLock lock(mMonitor);
      while (!mShouldFinish && mQueue.IsEmpty()) {
        if (NS_FAILED(mMonitor.Wait())) {
          NS_ERROR("Failed to wait!");
        }
      }

      mQueue.SwapElements(queue);
      if (mShouldFinish) {
        mFinishRunnable.swap(finishRunnable);
        shouldFinish = true;
      }
    }

    uint32_t count = queue.Length();
    for (uint32_t index = 0; index < count; index++) {
      nsCOMPtr<nsIRunnable>& runnable = queue[index];
      runnable->Run();
      runnable = nullptr;
    }

    if (count) {
      queue.Clear();
    }
  } while (!shouldFinish);

  IDB_PROFILER_MARK("IndexedDB Transaction %llu: Finished database work",
                    "IDBTransaction[%llu] DT Done",
                    mTransaction->GetSerialNumber());

  nsCOMPtr<nsIRunnable> finishTransactionRunnable =
    new FinishTransactionRunnable(mTransaction, finishRunnable);
  if (NS_FAILED(NS_DispatchToMainThread(finishTransactionRunnable,
                                        NS_DISPATCH_NORMAL))) {
    NS_WARNING("Failed to dispatch finishTransactionRunnable!");
  }

  return NS_OK;
}

FinishTransactionRunnable::FinishTransactionRunnable(
                                         IDBTransaction* aTransaction,
                                         nsCOMPtr<nsIRunnable>& aFinishRunnable)
: mTransaction(aTransaction)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");
  mFinishRunnable.swap(aFinishRunnable);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(FinishTransactionRunnable, nsIRunnable)

NS_IMETHODIMP
FinishTransactionRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PROFILER_MAIN_THREAD_LABEL("IndexedDB", "FinishTransactionRunnable::Run");

  if (!gInstance) {
    NS_ERROR("Running after shutdown!");
    return NS_ERROR_FAILURE;
  }

  gInstance->FinishTransaction(mTransaction);

  if (mFinishRunnable) {
    mFinishRunnable->Run();
    mFinishRunnable = nullptr;
  }

  return NS_OK;
}

#ifdef MOZ_ENABLE_PROFILER_SPS

NS_IMPL_THREADSAFE_ISUPPORTS1(TransactionThreadPoolListener,
                              nsIThreadPoolListener)

NS_IMETHODIMP
TransactionThreadPoolListener::OnThreadCreated()
{
  MOZ_ASSERT(!NS_IsMainThread());
  profiler_register_thread("IndexedDB Transaction");
  return NS_OK;
}

NS_IMETHODIMP
TransactionThreadPoolListener::OnThreadShuttingDown()
{
  MOZ_ASSERT(!NS_IsMainThread());
  profiler_unregister_thread();
  return NS_OK;
}

#endif // MOZ_ENABLE_PROFILER_SPS
