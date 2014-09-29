/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_transactionthreadpool_h__
#define mozilla_dom_indexeddb_transactionthreadpool_h__

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

class nsIEventTarget;
class nsIRunnable;
class nsIThreadPool;

namespace mozilla {
namespace dom {
namespace indexedDB {

class TransactionThreadPool MOZ_FINAL
{
  class FinishTransactionRunnable;
  friend class FinishTransactionRunnable;

  class TransactionQueue;
  friend class TransactionQueue;

  struct DatabaseTransactionInfo;
  struct DatabasesCompleteCallback;
  struct TransactionInfo;
  struct TransactionInfoPair;

  nsCOMPtr<nsIThreadPool> mThreadPool;
  nsCOMPtr<nsIEventTarget> mOwningThread;

  nsClassHashtable<nsCStringHashKey, DatabaseTransactionInfo>
    mTransactionsInProgress;

  nsTArray<nsAutoPtr<DatabasesCompleteCallback>> mCompleteCallbacks;

  uint64_t mNextTransactionId;
  bool mShutdownRequested;
  bool mShutdownComplete;

public:
  class FinishCallback;

  static already_AddRefed<TransactionThreadPool> Create();

  uint64_t NextTransactionId();

  void Dispatch(uint64_t aTransactionId,
                const nsACString& aDatabaseId,
                const nsTArray<nsString>& aObjectStoreNames,
                uint16_t aMode,
                nsIRunnable* aRunnable,
                bool aFinish,
                FinishCallback* aFinishCallback);

  void Dispatch(uint64_t aTransactionId,
                const nsACString& aDatabaseId,
                nsIRunnable* aRunnable,
                bool aFinish,
                FinishCallback* aFinishCallback);

  void WaitForDatabasesToComplete(nsTArray<nsCString>& aDatabaseIds,
                                  nsIRunnable* aCallback);

  // Returns true if there are running or pending transactions for aDatabase.
  bool HasTransactionsForDatabase(const nsACString& aDatabaseId);

  NS_INLINE_DECL_REFCOUNTING(TransactionThreadPool)

  void Shutdown();

  void AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

private:
  static PLDHashOperator
  CollectTransactions(const uint64_t& aTransactionId,
                      TransactionInfo* aValue,
                      void* aUserArg);

  static PLDHashOperator
  FindTransaction(const uint64_t& aTransactionId,
                  TransactionInfo* aValue,
                  void* aUserArg);

  static PLDHashOperator
  MaybeUnblockTransaction(nsPtrHashKey<TransactionInfo>* aKey,
                          void* aUserArg);

  TransactionThreadPool();

  // Reference counted.
  ~TransactionThreadPool();

  nsresult Init();
  void Cleanup();

  void FinishTransaction(uint64_t aTransactionId,
                         const nsACString& aDatabaseId,
                         const nsTArray<nsString>& aObjectStoreNames,
                         uint16_t aMode);

  TransactionQueue* GetQueueForTransaction(uint64_t aTransactionId,
                                           const nsACString& aDatabaseId);

  TransactionQueue& GetQueueForTransaction(
                                    uint64_t aTransactionId,
                                    const nsACString& aDatabaseId,
                                    const nsTArray<nsString>& aObjectStoreNames,
                                    uint16_t aMode);

  bool MaybeFireCallback(DatabasesCompleteCallback* aCallback);

  void CleanupAsync();
};

class NS_NO_VTABLE TransactionThreadPool::FinishCallback
{
public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() = 0;

  NS_IMETHOD_(MozExternalRefCountType)
  Release() = 0;

  // Called on the owning thread before any additional transactions are
  // unblocked.
  virtual void
  TransactionFinishedBeforeUnblock() = 0;

  // Called on the owning thread after additional transactions may have been
  // unblocked.
  virtual void
  TransactionFinishedAfterUnblock() = 0;

protected:
  FinishCallback()
  { }
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_transactionthreadpool_h__
