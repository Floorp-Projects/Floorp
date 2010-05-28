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

#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCIDInternal.h"

using mozilla::Mutex;
using mozilla::MutexAutoLock;
using mozilla::MutexAutoUnlock;
using mozilla::CondVar;

USING_INDEXEDDB_NAMESPACE

namespace {

const PRUint32 kThreadLimit = 20;
const PRUint32 kIdleThreadLimit = 5;
const PRUint32 kIdleThreadTimeoutMs = 30000;

TransactionThreadPool* gInstance = nsnull;
bool gShutdown = false;

struct TransactionObjectStoreInfo
{
  TransactionObjectStoreInfo() : writing(false) { }

  nsString objectStoreName;
  bool writing;
};

} // anonymous namespace

BEGIN_INDEXEDDB_NAMESPACE

struct TransactionInfo
{
  TransactionInfo()
  : mode(nsIIDBTransaction::READ_ONLY)
  { }

  nsRefPtr<IDBTransactionRequest> transaction;
  nsRefPtr<TransactionQueue> queue;
  nsTArray<TransactionObjectStoreInfo> objectStoreInfo;
  PRUint16 mode;
};

struct QueuedDispatchInfo
{
  QueuedDispatchInfo()
  : finish(false)
  { }

  nsRefPtr<IDBTransactionRequest> transaction;
  nsCOMPtr<nsIRunnable> runnable;
  bool finish;
};

class TransactionQueue : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  inline TransactionQueue(IDBTransactionRequest* aTransaction,
                          nsIRunnable* aRunnable);

  inline void Dispatch(nsIRunnable* aRunnable);

  inline void Finish();

private:
  Mutex mMutex;
  CondVar mCondVar;
  IDBTransactionRequest* mTransaction;
  nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> mQueue;
  bool mShouldFinish;
};

class FinishTransactionRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  inline FinishTransactionRunnable(IDBTransactionRequest* aTransaction);

private:
  IDBTransactionRequest* mTransaction;
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
  NS_ASSERTION(!gInstance, "More than one instance!");
}

// static
TransactionThreadPool*
TransactionThreadPool::GetOrCreate()
{
  if (!gInstance && !gShutdown) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    nsRefPtr<TransactionThreadPool> pool(new TransactionThreadPool());

    nsresult rv = pool->Init();
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsCOMPtr<nsIObserverService> obs =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, nsnull);

    rv = obs->AddObserver(pool, "xpcom-shutdown-threads", PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    // The observer service now owns us.
    gInstance = pool;
  }
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

  if (mTransactionsInProgress.Count()) {
    // This is actually really bad, but if we don't force everything awake then
    // we will deadlock on shutdown...
    NS_ERROR("Transactions still in progress!");
  }

  nsresult rv = mThreadPool->Shutdown();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
TransactionThreadPool::FinishTransaction(IDBTransactionRequest* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");

  // AddRef here because removing from the hash will call Release.
  nsRefPtr<IDBTransactionRequest> transaction(aTransaction);

  const PRUint32 databaseId = aTransaction->mDatabase->Id();

  nsTArray<TransactionInfo>* transactionsInProgress;
  if (!mTransactionsInProgress.Get(databaseId, &transactionsInProgress)) {
    NS_ERROR("We don't know anyting about this database?!");
    return;
  }

  PRUint32 count = transactionsInProgress->Length();
  if (count == 1) {
#ifdef DEBUG
    {
      TransactionInfo& info = transactionsInProgress->ElementAt(0);
      NS_ASSERTION(info.transaction == aTransaction, "Transaction mismatch!");
      NS_ASSERTION(info.mode == aTransaction->mMode, "Mode mismatch!");
    }
#endif
    mTransactionsInProgress.Remove(databaseId);
  }
  else {
    for (PRUint32 index = 0; index < count; index++) {
      TransactionInfo& info = transactionsInProgress->ElementAt(index);
      if (info.transaction == aTransaction) {
        transactionsInProgress->RemoveElementAt(index);
        break;
      }
    }

    NS_ASSERTION(transactionsInProgress->Length() == count - 1,
                 "Didn't find the transaction we were looking for!");
  }


  // Try to dispatch all the queued 
  nsTArray<QueuedDispatchInfo> queuedDispatch;
  queuedDispatch.SwapElements(mDelayedDispatchQueue);

  count = queuedDispatch.Length();
  for (PRUint32 index = 0; index < count; index++) {
    QueuedDispatchInfo& info = queuedDispatch[index];
    if (NS_FAILED(Dispatch(info.transaction, info.runnable, info.finish))) {
      NS_WARNING("Dispatch failed!");
    }
  }
}

bool
TransactionThreadPool::TransactionCanRun(IDBTransactionRequest* aTransaction,
                                         TransactionQueue** aQueue)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aQueue, "Null pointer!");

  const PRUint32 databaseId = aTransaction->mDatabase->Id();
  const nsTArray<nsString>& objectStoreNames = aTransaction->mObjectStoreNames;
  const PRUint16 mode = aTransaction->mMode;

  // See if we can run this transaction now.
  nsTArray<TransactionInfo>* transactionsInProgress;
  if (!mTransactionsInProgress.Get(databaseId, &transactionsInProgress)) {
    // First transaction for this database, fine to run.
    *aQueue = nsnull;
    return true;
  }

  // Another transaction for this database is already running. See if we can
  // run at the same time.
  PRUint32 transactionCount = transactionsInProgress->Length();
  for (PRUint32 transactionIndex = 0;
       transactionIndex < transactionCount;
       transactionIndex++) {
    const TransactionInfo& transactionInfo =
      transactionsInProgress->ElementAt(transactionIndex);

    if (transactionInfo.transaction == aTransaction) {
      // Same transaction, already running.
      *aQueue = transactionInfo.queue;
      return true;
    }

    // Not our transaction, see if the objectStores overlap.
    const nsTArray<TransactionObjectStoreInfo>& objectStoreInfoArray =
      transactionInfo.objectStoreInfo;

    PRUint32 objectStoreCount = objectStoreInfoArray.Length();
    for (PRUint32 objectStoreIndex = 0;
         objectStoreIndex < objectStoreCount;
         objectStoreIndex++) {
      const TransactionObjectStoreInfo& objectStoreInfo =
        objectStoreInfoArray[objectStoreIndex];

      if (objectStoreNames.Contains(objectStoreInfo.objectStoreName)) {
        // Overlapping name, see if the modes are compatible.
        switch (mode) {
          case nsIIDBTransaction::READ_WRITE: {
            // Someone else is reading or writing to this table, we can't
            // run now.
            return false;
          }

          case nsIIDBTransaction::READ_ONLY: {
            if (objectStoreInfo.writing) {
              // Someone else is writing to this table, we can't run now.
              return false;
            }
          } break;

          case nsIIDBTransaction::SNAPSHOT_READ: {
            NS_NOTYETIMPLEMENTED("Not implemented!");
          } break;

          default:
            NS_NOTREACHED("Should never get here!");
        }
      }

      // Continue on to the next TransactionObjectStoreInfo.
    }

    // Continue on to the next TransactionInfo.
  }

  // If we got here then there are no conflicting transactions and we should
  // be fine to run.
  *aQueue = nsnull;
  return true;
}

nsresult
TransactionThreadPool::Dispatch(IDBTransactionRequest* aTransaction,
                                nsIRunnable* aRunnable,
                                bool aFinish)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  TransactionQueue* existingQueue;
  if (!TransactionCanRun(aTransaction, &existingQueue)) {
    QueuedDispatchInfo* info = mDelayedDispatchQueue.AppendElement();
    NS_ENSURE_TRUE(info, NS_ERROR_OUT_OF_MEMORY);

    info->transaction = aTransaction;
    info->runnable = aRunnable;
    info->finish = aFinish;

    return NS_OK;
  }

  if (existingQueue) {
    existingQueue->Dispatch(aRunnable);
    if (aFinish) {
      existingQueue->Finish();
    }
    return NS_OK;
  }

  const PRUint32 databaseId = aTransaction->mDatabase->Id();

  // Make a new struct for this transaction.
  nsTArray<TransactionInfo>* transactionInfoArray;
  nsAutoPtr<nsTArray<TransactionInfo> > autoArray;
  if (!mTransactionsInProgress.Get(databaseId, &transactionInfoArray)) {
    autoArray = new nsTArray<TransactionInfo>();
    transactionInfoArray = autoArray;
  }

  TransactionInfo* transactionInfo = transactionInfoArray->AppendElement();
  NS_ENSURE_TRUE(transactionInfo, NS_ERROR_OUT_OF_MEMORY);

  transactionInfo->transaction = aTransaction;
  transactionInfo->queue = new TransactionQueue(aTransaction, aRunnable);
  if (aFinish) {
    transactionInfo->queue->Finish();
  }
  transactionInfo->mode = aTransaction->mMode;

  const nsTArray<nsString>& objectStoreNames = aTransaction->mObjectStoreNames;
  PRUint32 count = objectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    TransactionObjectStoreInfo* info =
      transactionInfo->objectStoreInfo.AppendElement();
    NS_ENSURE_TRUE(info, NS_ERROR_OUT_OF_MEMORY);

    info->objectStoreName = objectStoreNames[index];
    info->writing = transactionInfo->mode == nsIIDBTransaction::READ_WRITE;
  }

  if (autoArray) {
    if (!mTransactionsInProgress.Put(databaseId, autoArray)) {
      NS_ERROR("Failed to put!");
      return NS_ERROR_FAILURE;
    }
    autoArray.forget();
  }

  return mThreadPool->Dispatch(transactionInfo->queue, NS_DISPATCH_NORMAL);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(TransactionThreadPool, nsIObserver)

NS_IMETHODIMP
TransactionThreadPool::Observe(nsISupports* /* aSubject */,
                               const char*  aTopic,
                               const PRUnichar* /* aData */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!strcmp("xpcom-shutdown-threads", aTopic), "Wrong topic!");

  Shutdown();

  return NS_OK;
}

TransactionQueue::TransactionQueue(IDBTransactionRequest* aTransaction,
                                   nsIRunnable* aRunnable)
: mMutex("TransactionQueue::mMutex"),
  mCondVar(mMutex, "TransactionQueue::mCondVar"),
  mTransaction(aTransaction),
  mShouldFinish(false)
{
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aRunnable, "Null pointer!");
  mQueue.AppendElement(aRunnable);
}

void
TransactionQueue::Dispatch(nsIRunnable* aRunnable)
{
  MutexAutoLock lock(mMutex);

  NS_ASSERTION(!mShouldFinish, "Dispatch called after Finish!");

  if (!mQueue.AppendElement(aRunnable)) {
    MutexAutoUnlock unlock(mMutex);
    NS_RUNTIMEABORT("Out of memory!");
  }

  mCondVar.Notify();
}

void
TransactionQueue::Finish()
{
  MutexAutoLock lock(mMutex);

  NS_ASSERTION(!mShouldFinish, "Finish called more than once!");

  mShouldFinish = true;

  mCondVar.Notify();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(TransactionQueue, nsIRunnable)

NS_IMETHODIMP
TransactionQueue::Run()
{
  nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> queue;
  bool shouldFinish = false;

  while(!shouldFinish) {
    NS_ASSERTION(queue.IsEmpty(), "Should have cleared this!");

    {
      MutexAutoLock lock(mMutex);
      while (!mShouldFinish && mQueue.IsEmpty()) {
        if (NS_FAILED(mCondVar.Wait())) {
          NS_ERROR("Failed to wait!");
        }
      }

      mQueue.SwapElements(queue);
      shouldFinish = mShouldFinish;
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

  nsCOMPtr<nsIRunnable> finishRunnable =
    new FinishTransactionRunnable(mTransaction);
  if (NS_FAILED(NS_DispatchToMainThread(finishRunnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Failed to dispatch!");
  }

  return NS_OK;
}

FinishTransactionRunnable::FinishTransactionRunnable(
                                            IDBTransactionRequest* aTransaction)
: mTransaction(aTransaction)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");
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
  return NS_OK;
}
