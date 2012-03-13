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

#include "TransactionThreadPool.h"

#include "nsIObserverService.h"
#include "nsIThreadPool.h"

#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCIDInternal.h"

using mozilla::MonitorAutoLock;

USING_INDEXEDDB_NAMESPACE

namespace {

const PRUint32 kThreadLimit = 20;
const PRUint32 kIdleThreadLimit = 5;
const PRUint32 kIdleThreadTimeoutMs = 30000;

TransactionThreadPool* gInstance = nsnull;
bool gShutdown = false;

inline
nsresult
CheckOverlapAndMergeObjectStores(nsTArray<nsString>& aLockedStores,
                                 const nsTArray<nsString>& aObjectStores,
                                 bool aShouldMerge,
                                 bool* aStoresOverlap)
{
  PRUint32 length = aObjectStores.Length();

  bool overlap = false;

  for (PRUint32 index = 0; index < length; index++) {
    const nsString& storeName = aObjectStores[index];
    if (aLockedStores.Contains(storeName)) {
      overlap = true;
    }
    else if (aShouldMerge && !aLockedStores.AppendElement(storeName)) {
      NS_WARNING("Out of memory!");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aStoresOverlap = overlap;
  return NS_OK;
}

} // anonymous namespace

BEGIN_INDEXEDDB_NAMESPACE

class FinishTransactionRunnable : public nsIRunnable
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
  gInstance = nsnull;
}

// static
TransactionThreadPool*
TransactionThreadPool::GetOrCreate()
{
  if (!gInstance && !gShutdown) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    nsAutoPtr<TransactionThreadPool> pool(new TransactionThreadPool());

    nsresult rv = pool->Init();
    NS_ENSURE_SUCCESS(rv, nsnull);

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
    gInstance = nsnull;
  }
}

nsresult
TransactionThreadPool::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransactionsInProgress.Init()) {
    NS_WARNING("Failed to init hash!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  mThreadPool = do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetThreadLimit(kThreadLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetIdleThreadLimit(kIdleThreadLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetIdleThreadTimeout(kIdleThreadTimeoutMs);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
TransactionThreadPool::Cleanup()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = mThreadPool->Shutdown();
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure the pool is still accessible while any callbacks generated from
  // the other threads are processed.
  rv = NS_ProcessPendingEvents(nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mCompleteCallbacks.IsEmpty()) {
    // Run all callbacks manually now.
    for (PRUint32 index = 0; index < mCompleteCallbacks.Length(); index++) {
      mCompleteCallbacks[index].mCallback->Run();
    }
    mCompleteCallbacks.Clear();

    // And make sure they get processed.
    rv = NS_ProcessPendingEvents(nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
TransactionThreadPool::FinishTransaction(IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");

  // AddRef here because removing from the hash will call Release.
  nsRefPtr<IDBTransaction> transaction(aTransaction);

  nsIAtom* databaseId = aTransaction->mDatabase->Id();

  DatabaseTransactionInfo* dbTransactionInfo;
  if (!mTransactionsInProgress.Get(databaseId, &dbTransactionInfo)) {
    NS_ERROR("We don't know anyting about this database?!");
    return;
  }

  nsTArray<TransactionInfo>& transactionsInProgress =
    dbTransactionInfo->transactions;

  PRUint32 transactionCount = transactionsInProgress.Length();

#ifdef DEBUG
  if (aTransaction->mMode == IDBTransaction::VERSION_CHANGE) {
    NS_ASSERTION(transactionCount == 1,
                 "More transactions running than should be!");
  }
#endif

  if (transactionCount == 1) {
#ifdef DEBUG
    {
      TransactionInfo& info = transactionsInProgress[0];
      NS_ASSERTION(info.transaction == aTransaction, "Transaction mismatch!");
    }
#endif
    mTransactionsInProgress.Remove(databaseId);

    // See if we need to fire any complete callbacks.
    for (PRUint32 index = 0; index < mCompleteCallbacks.Length(); index++) {
      MaybeFireCallback(index);
    }
  }
  else {
    // We need to rebuild the locked object store list.
    nsTArray<nsString> storesWriting, storesReading;

    for (PRUint32 index = 0, count = transactionCount; index < count; index++) {
      IDBTransaction* transaction = transactionsInProgress[index].transaction;
      if (transaction == aTransaction) {
        NS_ASSERTION(count == transactionCount, "More than one match?!");

        transactionsInProgress.RemoveElementAt(index);
        index--;
        count--;

        continue;
      }

      const nsTArray<nsString>& objectStores = transaction->mObjectStoreNames;

      bool dummy;
      if (transaction->mMode == IDBTransaction::READ_WRITE) {
        if (NS_FAILED(CheckOverlapAndMergeObjectStores(storesWriting,
                                                       objectStores,
                                                       true, &dummy))) {
          NS_WARNING("Out of memory!");
        }
      }
      else if (transaction->mMode == IDBTransaction::READ_ONLY) {
        if (NS_FAILED(CheckOverlapAndMergeObjectStores(storesReading,
                                                       objectStores,
                                                       true, &dummy))) {
          NS_WARNING("Out of memory!");
        }
      }
      else {
        NS_NOTREACHED("Unknown mode!");
      }
    }

    NS_ASSERTION(transactionsInProgress.Length() == transactionCount - 1,
                 "Didn't find the transaction we were looking for!");

    dbTransactionInfo->storesWriting.SwapElements(storesWriting);
    dbTransactionInfo->storesReading.SwapElements(storesReading);
  }

  // Try to dispatch all the queued transactions again.
  nsTArray<QueuedDispatchInfo> queuedDispatch;
  queuedDispatch.SwapElements(mDelayedDispatchQueue);

  transactionCount = queuedDispatch.Length();
  for (PRUint32 index = 0; index < transactionCount; index++) {
    if (NS_FAILED(Dispatch(queuedDispatch[index]))) {
      NS_WARNING("Dispatch failed!");
    }
  }
}

nsresult
TransactionThreadPool::TransactionCanRun(IDBTransaction* aTransaction,
                                         bool* aCanRun,
                                         TransactionQueue** aExistingQueue)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aCanRun, "Null pointer!");
  NS_ASSERTION(aExistingQueue, "Null pointer!");

  nsIAtom* databaseId = aTransaction->mDatabase->Id();
  const nsTArray<nsString>& objectStoreNames = aTransaction->mObjectStoreNames;
  const PRUint16 mode = aTransaction->mMode;

  // See if we can run this transaction now.
  DatabaseTransactionInfo* dbTransactionInfo;
  if (!mTransactionsInProgress.Get(databaseId, &dbTransactionInfo)) {
    // First transaction for this database, fine to run.
    *aCanRun = true;
    *aExistingQueue = nsnull;
    return NS_OK;
  }

  nsTArray<TransactionInfo>& transactionsInProgress =
    dbTransactionInfo->transactions;

  PRUint32 transactionCount = transactionsInProgress.Length();
  NS_ASSERTION(transactionCount, "Should never be 0!");

  for (PRUint32 index = 0; index < transactionCount; index++) {
    // See if this transaction is in out list of current transactions.
    const TransactionInfo& info = transactionsInProgress[index];
    if (info.transaction == aTransaction) {
      *aCanRun = true;
      *aExistingQueue = info.queue;
      return NS_OK;
    }
  }

  NS_ASSERTION(mode != IDBTransaction::VERSION_CHANGE, "How did we get here?");

  bool writeOverlap;
  nsresult rv =
    CheckOverlapAndMergeObjectStores(dbTransactionInfo->storesWriting,
                                     objectStoreNames,
                                     mode == IDBTransaction::READ_WRITE,
                                     &writeOverlap);
  NS_ENSURE_SUCCESS(rv, rv);

  bool readOverlap;
  rv = CheckOverlapAndMergeObjectStores(dbTransactionInfo->storesReading,
                                        objectStoreNames,
                                        mode == IDBTransaction::READ_ONLY,
                                        &readOverlap);
  NS_ENSURE_SUCCESS(rv, rv);

  if (writeOverlap ||
      (readOverlap && mode == IDBTransaction::READ_WRITE)) {
    *aCanRun = false;
    *aExistingQueue = nsnull;
    return NS_OK;
  }

  *aCanRun = true;
  *aExistingQueue = nsnull;
  return NS_OK;
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

  bool canRun;
  TransactionQueue* existingQueue;
  nsresult rv = TransactionCanRun(aTransaction, &canRun, &existingQueue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!canRun) {
    QueuedDispatchInfo* info = mDelayedDispatchQueue.AppendElement();
    NS_ENSURE_TRUE(info, NS_ERROR_OUT_OF_MEMORY);

    info->transaction = aTransaction;
    info->runnable = aRunnable;
    info->finish = aFinish;
    info->finishRunnable = aFinishRunnable;

    return NS_OK;
  }

  if (existingQueue) {
    existingQueue->Dispatch(aRunnable);
    if (aFinish) {
      existingQueue->Finish(aFinishRunnable);
    }
    return NS_OK;
  }

  nsIAtom* databaseId = aTransaction->mDatabase->Id();

#ifdef DEBUG
  if (aTransaction->mMode == IDBTransaction::VERSION_CHANGE) {
    NS_ASSERTION(!mTransactionsInProgress.Get(databaseId, nsnull),
                 "Shouldn't have anything in progress!");
  }
#endif

  DatabaseTransactionInfo* dbTransactionInfo;
  nsAutoPtr<DatabaseTransactionInfo> autoDBTransactionInfo;

  if (!mTransactionsInProgress.Get(databaseId, &dbTransactionInfo)) {
    // Make a new struct for this transaction.
    autoDBTransactionInfo = new DatabaseTransactionInfo();
    dbTransactionInfo = autoDBTransactionInfo;
  }

  const nsTArray<nsString>& objectStoreNames = aTransaction->mObjectStoreNames;

  nsTArray<nsString>& storesInUse =
    aTransaction->mMode == IDBTransaction::READ_WRITE ?
    dbTransactionInfo->storesWriting :
    dbTransactionInfo->storesReading;

  if (!storesInUse.AppendElements(objectStoreNames)) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsTArray<TransactionInfo>& transactionInfoArray =
    dbTransactionInfo->transactions;

  TransactionInfo* transactionInfo = transactionInfoArray.AppendElement();
  NS_ENSURE_TRUE(transactionInfo, NS_ERROR_OUT_OF_MEMORY);

  transactionInfo->transaction = aTransaction;
  transactionInfo->queue = new TransactionQueue(aTransaction, aRunnable);
  if (aFinish) {
    transactionInfo->queue->Finish(aFinishRunnable);
  }

  if (!transactionInfo->objectStoreNames.AppendElements(objectStoreNames)) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (autoDBTransactionInfo) {
    if (!mTransactionsInProgress.Put(databaseId, autoDBTransactionInfo)) {
      NS_WARNING("Failed to put!");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    autoDBTransactionInfo.forget();
  }

  return mThreadPool->Dispatch(transactionInfo->queue, NS_DISPATCH_NORMAL);
}

bool
TransactionThreadPool::WaitForAllDatabasesToComplete(
                                   nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
                                   nsIRunnable* aCallback)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aDatabases.IsEmpty(), "No databases to wait on!");
  NS_ASSERTION(aCallback, "Null pointer!");

  DatabasesCompleteCallback* callback = mCompleteCallbacks.AppendElement();
  if (!callback) {
    NS_WARNING("Out of memory!");
    return false;
  }

  callback->mCallback = aCallback;
  if (!callback->mDatabases.SwapElements(aDatabases)) {
    NS_ERROR("This should never fail!");
  }

  MaybeFireCallback(mCompleteCallbacks.Length() - 1);
  return true;
}

void
TransactionThreadPool::AbortTransactionsForDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Get list of transactions for this database id
  DatabaseTransactionInfo* dbTransactionInfo;
  if (!mTransactionsInProgress.Get(aDatabase->Id(), &dbTransactionInfo)) {
    // If there are no running transactions, there can't be any pending ones
    return;
  }

  nsAutoTArray<nsRefPtr<IDBTransaction>, 50> transactions;

  // Collect any running transactions
  nsTArray<TransactionInfo>& transactionsInProgress =
    dbTransactionInfo->transactions;

  PRUint32 transactionCount = transactionsInProgress.Length();
  NS_ASSERTION(transactionCount, "Should never be 0!");

  for (PRUint32 index = 0; index < transactionCount; index++) {
    // See if any transaction belongs to this IDBDatabase instance
    IDBTransaction* transaction = transactionsInProgress[index].transaction;
    if (transaction->Database() == aDatabase) {
      transactions.AppendElement(transaction);
    }
  }

  // Collect any pending transactions.
  for (PRUint32 index = 0; index < mDelayedDispatchQueue.Length(); index++) {
    // See if any transaction belongs to this IDBDatabase instance
    IDBTransaction* transaction = mDelayedDispatchQueue[index].transaction;
    if (transaction->Database() == aDatabase) {
      transactions.AppendElement(transaction);
    }
  }

  // Abort transactions. Do this after collecting the transactions in case
  // calling Abort() modifies the data structures we're iterating above.
  for (PRUint32 index = 0; index < transactions.Length(); index++) {
    // This can fail, for example if the transaction is in the process of
    // being comitted. That is expected and fine, so we ignore any returned
    // errors.
    transactions[index]->Abort();
  }
}

bool
TransactionThreadPool::HasTransactionsForDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Get list of transactions for this database id
  DatabaseTransactionInfo* dbTransactionInfo;
  if (!mTransactionsInProgress.Get(aDatabase->Id(), &dbTransactionInfo)) {
    return false;
  }

  nsTArray<TransactionInfo>& transactionsInProgress =
    dbTransactionInfo->transactions;

  PRUint32 transactionCount = transactionsInProgress.Length();
  NS_ASSERTION(transactionCount, "Should never be 0!");

  for (PRUint32 index = 0; index < transactionCount; index++) {
    // See if any transaction belongs to this IDBDatabase instance
    if (transactionsInProgress[index].transaction->Database() == aDatabase) {
      return true;
    }
  }

  return false;
}

void
TransactionThreadPool::MaybeFireCallback(PRUint32 aCallbackIndex)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  DatabasesCompleteCallback& callback = mCompleteCallbacks[aCallbackIndex];

  bool freeToRun = true;
  for (PRUint32 index = 0; index < callback.mDatabases.Length(); index++) {
    if (mTransactionsInProgress.Get(callback.mDatabases[index]->Id(), nsnull)) {
      freeToRun = false;
      break;
    }
  }

  if (freeToRun) {
    callback.mCallback->Run();
    mCompleteCallbacks.RemoveElementAt(aCallbackIndex);
  }
}

TransactionThreadPool::
TransactionQueue::TransactionQueue(IDBTransaction* aTransaction,
                                   nsIRunnable* aRunnable)
: mMonitor("TransactionQueue::mMonitor"),
  mTransaction(aTransaction),
  mShouldFinish(false)
{
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aRunnable, "Null pointer!");
  mQueue.AppendElement(aRunnable);
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
  nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> queue;
  nsCOMPtr<nsIRunnable> finishRunnable;
  bool shouldFinish = false;

  while(!shouldFinish) {
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

    PRUint32 count = queue.Length();
    for (PRUint32 index = 0; index < count; index++) {
      nsCOMPtr<nsIRunnable>& runnable = queue[index];
      runnable->Run();
      runnable = nsnull;
    }

    if (count) {
      queue.Clear();
    }
  }

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
  if (!gInstance) {
    NS_ERROR("Running after shutdown!");
    return NS_ERROR_FAILURE;
  }

  gInstance->FinishTransaction(mTransaction);

  if (mFinishRunnable) {
    mFinishRunnable->Run();
    mFinishRunnable = nsnull;
  }

  return NS_OK;
}
