/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_transactionthreadpool_h__
#define mozilla_dom_indexeddb_transactionthreadpool_h__

// Only meant to be included in IndexedDB source files, not exported.
#include "IndexedDatabase.h"

#include "nsIObserver.h"
#include "nsIRunnable.h"

#include "mozilla/Monitor.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"

#include "IDBTransaction.h"

class nsIThreadPool;

BEGIN_INDEXEDDB_NAMESPACE

class FinishTransactionRunnable;
class QueuedDispatchInfo;

class TransactionThreadPool
{
  friend class nsAutoPtr<TransactionThreadPool>;
  friend class FinishTransactionRunnable;

public:
  // returns a non-owning ref!
  static TransactionThreadPool* GetOrCreate();

  // returns a non-owning ref!
  static TransactionThreadPool* Get();

  static void Shutdown();

  nsresult Dispatch(IDBTransaction* aTransaction,
                    nsIRunnable* aRunnable,
                    bool aFinish,
                    nsIRunnable* aFinishRunnable);

  bool WaitForAllDatabasesToComplete(
                                   nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
                                   nsIRunnable* aCallback);

  // Abort all transactions, unless they are already in the process of being
  // committed, for aDatabase.
  void AbortTransactionsForDatabase(IDBDatabase* aDatabase);

  // Returns true if there are running or pending transactions for aDatabase.
  bool HasTransactionsForDatabase(IDBDatabase* aDatabase);

protected:
  class TransactionQueue MOZ_FINAL : public nsIRunnable
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    inline TransactionQueue(IDBTransaction* aTransaction,
                            nsIRunnable* aRunnable);

    inline void Dispatch(nsIRunnable* aRunnable);

    inline void Finish(nsIRunnable* aFinishRunnable);

  private:
    mozilla::Monitor mMonitor;
    IDBTransaction* mTransaction;
    nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> mQueue;
    nsCOMPtr<nsIRunnable> mFinishRunnable;
    bool mShouldFinish;
  };

  struct TransactionInfo
  {
    nsRefPtr<IDBTransaction> transaction;
    nsRefPtr<TransactionQueue> queue;
    nsTArray<nsString> objectStoreNames;
  };

  struct DatabaseTransactionInfo
  {
    nsTArray<TransactionInfo> transactions;
    nsTArray<nsString> storesReading;
    nsTArray<nsString> storesWriting;
  };

  struct QueuedDispatchInfo
  {
    QueuedDispatchInfo()
    : finish(false)
    { }

    nsRefPtr<IDBTransaction> transaction;
    nsCOMPtr<nsIRunnable> runnable;
    nsCOMPtr<nsIRunnable> finishRunnable;
    bool finish;
  };

  struct DatabasesCompleteCallback
  {
    nsTArray<nsRefPtr<IDBDatabase> > mDatabases;
    nsCOMPtr<nsIRunnable> mCallback;
  };

  TransactionThreadPool();
  ~TransactionThreadPool();

  nsresult Init();
  nsresult Cleanup();

  void FinishTransaction(IDBTransaction* aTransaction);

  nsresult TransactionCanRun(IDBTransaction* aTransaction,
                             bool* aCanRun,
                             TransactionQueue** aExistingQueue);

  nsresult Dispatch(const QueuedDispatchInfo& aInfo)
  {
    return Dispatch(aInfo.transaction, aInfo.runnable, aInfo.finish,
                    aInfo.finishRunnable);
  }

  bool MaybeFireCallback(DatabasesCompleteCallback& aCallback);

  nsCOMPtr<nsIThreadPool> mThreadPool;

  nsClassHashtable<nsISupportsHashKey, DatabaseTransactionInfo>
    mTransactionsInProgress;

  nsTArray<QueuedDispatchInfo> mDelayedDispatchQueue;

  nsTArray<DatabasesCompleteCallback> mCompleteCallbacks;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_transactionthreadpool_h__
