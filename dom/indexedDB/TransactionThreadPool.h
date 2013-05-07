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

  void WaitForDatabasesToComplete(nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
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

    TransactionQueue(IDBTransaction* aTransaction);

    void Unblock();

    void Dispatch(nsIRunnable* aRunnable);

    void Finish(nsIRunnable* aFinishRunnable);

  private:
    mozilla::Monitor mMonitor;
    IDBTransaction* mTransaction;
    nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> mQueue;
    nsCOMPtr<nsIRunnable> mFinishRunnable;
    bool mShouldFinish;
  };

  friend class TransactionQueue;

  struct TransactionInfo
  {
    TransactionInfo(IDBTransaction* aTransaction)
    {
      MOZ_COUNT_CTOR(TransactionInfo);

      blockedOn.Init();
      blocking.Init();

      transaction = aTransaction;
      queue = new TransactionQueue(aTransaction);
    }

    ~TransactionInfo()
    {
      MOZ_COUNT_DTOR(TransactionInfo);
    }

    nsRefPtr<IDBTransaction> transaction;
    nsRefPtr<TransactionQueue> queue;
    nsTHashtable<nsPtrHashKey<TransactionInfo> > blockedOn;
    nsTHashtable<nsPtrHashKey<TransactionInfo> > blocking;
  };

  struct TransactionInfoPair
  {
    TransactionInfoPair()
      : lastBlockingReads(nullptr)
    {
      MOZ_COUNT_CTOR(TransactionInfoPair);
    }

    ~TransactionInfoPair()
    {
      MOZ_COUNT_DTOR(TransactionInfoPair);
    }
    // Multiple reading transactions can block future writes.
    nsTArray<TransactionInfo*> lastBlockingWrites;
    // But only a single writing transaction can block future reads.
    TransactionInfo* lastBlockingReads;
  };

  struct DatabaseTransactionInfo
  {
    DatabaseTransactionInfo()
    {
      MOZ_COUNT_CTOR(DatabaseTransactionInfo);

      transactions.Init();
      blockingTransactions.Init();
    }

    ~DatabaseTransactionInfo()
    {
      MOZ_COUNT_DTOR(DatabaseTransactionInfo);
    }

    typedef nsClassHashtable<nsPtrHashKey<IDBTransaction>, TransactionInfo >
      TransactionHashtable;
    TransactionHashtable transactions;
    nsClassHashtable<nsStringHashKey, TransactionInfoPair> blockingTransactions;
  };

  static PLDHashOperator
  CollectTransactions(IDBTransaction* aKey,
                      TransactionInfo* aValue,
                      void* aUserArg);

  static PLDHashOperator
  FindTransaction(IDBTransaction* aKey,
                  TransactionInfo* aValue,
                  void* aUserArg);

  static PLDHashOperator
  MaybeUnblockTransaction(nsPtrHashKey<TransactionInfo>* aKey,
                          void* aUserArg);

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

  TransactionQueue& GetQueueForTransaction(IDBTransaction* aTransaction);

  bool MaybeFireCallback(DatabasesCompleteCallback aCallback);

  nsCOMPtr<nsIThreadPool> mThreadPool;

  nsClassHashtable<nsISupportsHashKey, DatabaseTransactionInfo>
    mTransactionsInProgress;

  nsTArray<DatabasesCompleteCallback> mCompleteCallbacks;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_transactionthreadpool_h__
