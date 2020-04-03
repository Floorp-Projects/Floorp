/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TransactionManager.h"

#include "mozilla/Assertions.h"
#include "mozilla/mozalloc.h"
#include "mozilla/TransactionStack.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsITransaction.h"
#include "nsITransactionListener.h"
#include "nsIWeakReference.h"
#include "TransactionItem.h"

namespace mozilla {

TransactionManager::TransactionManager(int32_t aMaxTransactionCount)
    : mMaxTransactionCount(aMaxTransactionCount),
      mDoStack(TransactionStack::FOR_UNDO),
      mUndoStack(TransactionStack::FOR_UNDO),
      mRedoStack(TransactionStack::FOR_REDO) {}

NS_IMPL_CYCLE_COLLECTION_CLASS(TransactionManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TransactionManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListeners)
  tmp->mDoStack.DoUnlink();
  tmp->mUndoStack.DoUnlink();
  tmp->mRedoStack.DoUnlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TransactionManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
  tmp->mDoStack.DoTraverse(cb);
  tmp->mUndoStack.DoTraverse(cb);
  tmp->mRedoStack.DoTraverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TransactionManager)
  NS_INTERFACE_MAP_ENTRY(nsITransactionManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITransactionManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TransactionManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TransactionManager)

NS_IMETHODIMP TransactionManager::DoTransaction(nsITransaction* aTransaction) {
  if (NS_WARN_IF(!aTransaction)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool doInterrupt = false;

  nsresult rv = WillDoNotify(aTransaction, &doInterrupt);
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionManager::WillDoNotify() failed");
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = BeginTransaction(aTransaction, nullptr);
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionManager::BeginTransaction() failed");
    DebugOnly<nsresult> rvIgnored = DidDoNotify(aTransaction, rv);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "TransactionManager::DidDoNotify() failed, but ignored");
    return rv;
  }

  rv = EndTransaction(false);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TransactionManager::EndTransaction() failed");

  nsresult rv2 = DidDoNotify(aTransaction, rv);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2),
                       "TransactionManager::DidDoNotify() failed");

  // XXX The result of EndTransaction() or DidDoNotify() if EndTransaction()
  //     succeeded.
  return NS_SUCCEEDED(rv) ? rv2 : rv;
}

NS_IMETHODIMP TransactionManager::UndoTransaction() { return Undo(); }

nsresult TransactionManager::Undo() {
  // It's possible to be called Undo() again while the transaction manager is
  // executing a transaction's DoTransaction() method.  If this happens,
  // the Undo() request is ignored, and we return NS_ERROR_FAILURE.  This
  // may occur if a mutation event listener calls document.execCommand("undo").
  if (NS_WARN_IF(!mDoStack.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  // Peek at the top of the undo stack. Don't remove the transaction
  // until it has successfully completed.
  RefPtr<TransactionItem> transactionItem = mUndoStack.Peek();
  if (!transactionItem) {
    // Bail if there's nothing on the stack.
    return NS_OK;
  }

  nsCOMPtr<nsITransaction> transaction = transactionItem->GetTransaction();
  bool doInterrupt = false;
  nsresult rv = WillUndoNotify(transaction, &doInterrupt);
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionManager::WillUndoNotify() failed");
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = transactionItem->UndoTransaction(this);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TransactionItem::UndoTransaction() failed");
  if (NS_SUCCEEDED(rv)) {
    transactionItem = mUndoStack.Pop();
    mRedoStack.Push(transactionItem.forget());
  }

  nsresult rv2 = DidUndoNotify(transaction, rv);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2),
                       "TransactionManager::DidUndoNotify() failed");

  // XXX The result of UndoTransaction() or DidUndoNotify() if UndoTransaction()
  //     succeeded.
  return NS_SUCCEEDED(rv) ? rv2 : rv;
}

NS_IMETHODIMP TransactionManager::RedoTransaction() { return Redo(); }

nsresult TransactionManager::Redo() {
  // It's possible to be called Redo() again while the transaction manager is
  // executing a transaction's DoTransaction() method.  If this happens,
  // the Redo() request is ignored, and we return NS_ERROR_FAILURE.  This
  // may occur if a mutation event listener calls document.execCommand("redo").
  if (NS_WARN_IF(!mDoStack.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  // Peek at the top of the redo stack. Don't remove the transaction
  // until it has successfully completed.
  RefPtr<TransactionItem> transactionItem = mRedoStack.Peek();
  if (!transactionItem) {
    // Bail if there's nothing on the stack.
    return NS_OK;
  }

  nsCOMPtr<nsITransaction> transaction = transactionItem->GetTransaction();
  bool doInterrupt = false;
  nsresult rv = WillRedoNotify(transaction, &doInterrupt);
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionManager::WillRedoNotify() failed");
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = transactionItem->RedoTransaction(this);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TransactionItem::RedoTransaction() failed");
  if (NS_SUCCEEDED(rv)) {
    transactionItem = mRedoStack.Pop();
    mUndoStack.Push(transactionItem.forget());
  }

  nsresult rv2 = DidRedoNotify(transaction, rv);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2),
                       "TransactionManager::DidRedoNotify() failed");

  // XXX The result of RedoTransaction() or DidRedoNotify() if RedoTransaction()
  //     succeeded.
  return NS_SUCCEEDED(rv) ? rv2 : rv;
}

NS_IMETHODIMP TransactionManager::Clear() {
  return NS_WARN_IF(!ClearUndoRedo()) ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP TransactionManager::BeginBatch(nsISupports* aData) {
  nsresult rv = BeginBatchInternal(aData);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TransactionManager::BeginBatchInternal() failed");
  return rv;
}

nsresult TransactionManager::BeginBatchInternal(nsISupports* aData) {
  // We can batch independent transactions together by simply pushing
  // a dummy transaction item on the do stack. This dummy transaction item
  // will be popped off the do stack, and then pushed on the undo stack
  // in EndBatch().
  bool doInterrupt = false;
  nsresult rv = WillBeginBatchNotify(&doInterrupt);
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionManager::WillBeginBatchNotify() failed");
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = BeginTransaction(nullptr, aData);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TransactionManager::BeginTransaction() failed");

  nsresult rv2 = DidBeginBatchNotify(rv);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2),
                       "TransactionManager::DidBeginBatchNotify() failed");

  // XXX The result of BeginTransaction() or DidBeginBatchNotify() if
  //     BeginTransaction() succeeded.
  return NS_SUCCEEDED(rv) ? rv2 : rv;
}

NS_IMETHODIMP TransactionManager::EndBatch(bool aAllowEmpty) {
  nsresult rv = EndBatchInternal(aAllowEmpty);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TransactionManager::EndBatchInternal() failed");
  return rv;
}

nsresult TransactionManager::EndBatchInternal(bool aAllowEmpty) {
  // XXX: Need to add some mechanism to detect the case where the transaction
  //      at the top of the do stack isn't the dummy transaction, so we can
  //      throw an error!! This can happen if someone calls EndBatch() within
  //      the DoTransaction() method of a transaction.
  //
  //      For now, we can detect this case by checking the value of the
  //      dummy transaction's mTransaction field. If it is our dummy
  //      transaction, it should be nullptr. This may not be true in the
  //      future when we allow users to execute a transaction when beginning
  //      a batch!!!!
  RefPtr<TransactionItem> transactionItem = mDoStack.Peek();
  if (NS_WARN_IF(!transactionItem)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsITransaction> transaction = transactionItem->GetTransaction();
  if (NS_WARN_IF(transaction)) {
    return NS_ERROR_FAILURE;
  }

  bool doInterrupt = false;
  nsresult rv = WillEndBatchNotify(&doInterrupt);
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionManager::WillEndBatchNotify() failed");
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = EndTransaction(aAllowEmpty);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TransactionManager::EndTransaction() failed");

  nsresult rv2 = DidEndBatchNotify(rv);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2),
                       "TransactionManager::DidEndBatchNotify() failed");

  // XXX The result of EndTransaction() or DidEndBatchNotify() if
  //     EndTransaction() succeeded.
  return NS_SUCCEEDED(rv) ? rv2 : rv;
}

NS_IMETHODIMP TransactionManager::GetNumberOfUndoItems(int32_t* aNumItems) {
  *aNumItems = static_cast<int32_t>(NumberOfUndoItems());
  MOZ_ASSERT(*aNumItems >= 0);
  return NS_OK;
}

NS_IMETHODIMP TransactionManager::GetNumberOfRedoItems(int32_t* aNumItems) {
  *aNumItems = static_cast<int32_t>(NumberOfRedoItems());
  MOZ_ASSERT(*aNumItems >= 0);
  return NS_OK;
}

NS_IMETHODIMP TransactionManager::GetMaxTransactionCount(int32_t* aMaxCount) {
  if (NS_WARN_IF(!aMaxCount)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aMaxCount = mMaxTransactionCount;
  return NS_OK;
}

NS_IMETHODIMP TransactionManager::SetMaxTransactionCount(int32_t aMaxCount) {
  return NS_WARN_IF(!EnableUndoRedo(aMaxCount)) ? NS_ERROR_FAILURE : NS_OK;
}

bool TransactionManager::EnableUndoRedo(int32_t aMaxTransactionCount) {
  // It is illegal to call EnableUndoRedo() while the transaction manager is
  // executing a transaction's DoTransaction() method because the undo and redo
  // stacks might get pruned.  If this happens, the EnableUndoRedo() request is
  // ignored, and we return false.
  if (NS_WARN_IF(!mDoStack.IsEmpty())) {
    return false;
  }

  // If aMaxTransactionCount is 0, it means to disable undo/redo.
  if (!aMaxTransactionCount) {
    mUndoStack.Clear();
    mRedoStack.Clear();
    mMaxTransactionCount = 0;
    return true;
  }

  // If aMaxTransactionCount is less than zero, the user wants unlimited
  // levels of undo! No need to prune the undo or redo stacks.
  if (aMaxTransactionCount < 0) {
    mMaxTransactionCount = -1;
    return true;
  }

  // If new max transaction count is greater than or equal to current max
  // transaction count, we don't need to remove any transactions.
  if (mMaxTransactionCount >= 0 &&
      mMaxTransactionCount <= aMaxTransactionCount) {
    mMaxTransactionCount = aMaxTransactionCount;
    return true;
  }

  // If aMaxTransactionCount is greater than the number of transactions that
  // currently exist on the undo and redo stack, there is no need to prune the
  // undo or redo stacks.
  size_t numUndoItems = NumberOfUndoItems();
  size_t numRedoItems = NumberOfRedoItems();
  size_t total = numUndoItems + numRedoItems;
  size_t newMaxTransactionCount = static_cast<size_t>(aMaxTransactionCount);
  if (newMaxTransactionCount > total) {
    mMaxTransactionCount = aMaxTransactionCount;
    return true;
  }

  // Try getting rid of some transactions on the undo stack! Start at
  // the bottom of the stack and pop towards the top.
  for (; numUndoItems && (numRedoItems + numUndoItems) > newMaxTransactionCount;
       numUndoItems--) {
    RefPtr<TransactionItem> transactionItem = mUndoStack.PopBottom();
    MOZ_ASSERT(transactionItem);
  }

  // If necessary, get rid of some transactions on the redo stack! Start at
  // the bottom of the stack and pop towards the top.
  for (; numRedoItems && (numRedoItems + numUndoItems) > newMaxTransactionCount;
       numRedoItems--) {
    RefPtr<TransactionItem> transactionItem = mRedoStack.PopBottom();
    MOZ_ASSERT(transactionItem);
  }

  mMaxTransactionCount = aMaxTransactionCount;
  return true;
}

NS_IMETHODIMP TransactionManager::PeekUndoStack(nsITransaction** aTransaction) {
  MOZ_ASSERT(aTransaction);
  *aTransaction = PeekUndoStack().take();
  return NS_OK;
}

already_AddRefed<nsITransaction> TransactionManager::PeekUndoStack() {
  RefPtr<TransactionItem> transactionItem = mUndoStack.Peek();
  if (!transactionItem) {
    return nullptr;
  }
  return transactionItem->GetTransaction();
}

NS_IMETHODIMP TransactionManager::PeekRedoStack(nsITransaction** aTransaction) {
  MOZ_ASSERT(aTransaction);
  *aTransaction = PeekRedoStack().take();
  return NS_OK;
}

already_AddRefed<nsITransaction> TransactionManager::PeekRedoStack() {
  RefPtr<TransactionItem> transactionItem = mRedoStack.Peek();
  if (!transactionItem) {
    return nullptr;
  }
  return transactionItem->GetTransaction();
}

nsresult TransactionManager::BatchTopUndo() {
  if (mUndoStack.GetSize() < 2) {
    // Not enough transactions to merge into one batch.
    return NS_OK;
  }

  RefPtr<TransactionItem> lastUndo = mUndoStack.Pop();
  MOZ_ASSERT(lastUndo, "There should be at least two transactions.");

  RefPtr<TransactionItem> previousUndo = mUndoStack.Peek();
  MOZ_ASSERT(previousUndo, "There should be at least two transactions.");

  nsresult rv = previousUndo->AddChild(*lastUndo);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "TransactionItem::AddChild() failed");

  // Transfer data from the transactions that is going to be
  // merged to the transaction that it is being merged with.
  nsCOMArray<nsISupports>& lastData = lastUndo->GetData();
  nsCOMArray<nsISupports>& previousData = previousUndo->GetData();
  if (!previousData.AppendObjects(lastData)) {
    NS_WARNING("nsISupports::AppendObjects() failed");
    return NS_ERROR_FAILURE;
  }
  lastData.Clear();
  return rv;
}

nsresult TransactionManager::RemoveTopUndo() {
  if (mUndoStack.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<TransactionItem> lastUndo = mUndoStack.Pop();
  return NS_OK;
}

NS_IMETHODIMP TransactionManager::AddListener(
    nsITransactionListener* aListener) {
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_ARG;
  }
  return NS_WARN_IF(!AddTransactionListener(*aListener)) ? NS_ERROR_FAILURE
                                                         : NS_OK;
}

NS_IMETHODIMP TransactionManager::RemoveListener(
    nsITransactionListener* aListener) {
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_ARG;
  }
  return NS_WARN_IF(!RemoveTransactionListener(*aListener)) ? NS_ERROR_FAILURE
                                                            : NS_OK;
}

NS_IMETHODIMP TransactionManager::ClearUndoStack() {
  if (NS_WARN_IF(!mDoStack.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }
  mUndoStack.Clear();
  return NS_OK;
}

NS_IMETHODIMP TransactionManager::ClearRedoStack() {
  if (NS_WARN_IF(!mDoStack.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }
  mRedoStack.Clear();
  return NS_OK;
}

nsresult TransactionManager::WillDoNotify(nsITransaction* aTransaction,
                                          bool* aInterrupt) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->WillDo(this, aTransaction, aInterrupt);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::WillDo() failed");
      return rv;
    }
    if (*aInterrupt) {
      return NS_OK;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::DidDoNotify(nsITransaction* aTransaction,
                                         nsresult aDoResult) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->DidDo(this, aTransaction, aDoResult);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::DidDo() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::WillUndoNotify(nsITransaction* aTransaction,
                                            bool* aInterrupt) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->WillUndo(this, aTransaction, aInterrupt);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::WillUndo() failed");
      return rv;
    }
    if (*aInterrupt) {
      return NS_OK;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::DidUndoNotify(nsITransaction* aTransaction,
                                           nsresult aUndoResult) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->DidUndo(this, aTransaction, aUndoResult);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::DidUndo() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::WillRedoNotify(nsITransaction* aTransaction,
                                            bool* aInterrupt) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->WillRedo(this, aTransaction, aInterrupt);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::WillRedo() failed");
      return rv;
    }
    if (*aInterrupt) {
      return NS_OK;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::DidRedoNotify(nsITransaction* aTransaction,
                                           nsresult aRedoResult) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->DidRedo(this, aTransaction, aRedoResult);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::DidRedo() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::WillBeginBatchNotify(bool* aInterrupt) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->WillBeginBatch(this, aInterrupt);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::WillBeginBatch() failed");
      return rv;
    }
    if (*aInterrupt) {
      return NS_OK;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::DidBeginBatchNotify(nsresult aResult) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->DidBeginBatch(this, aResult);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::DidBeginBatch() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::WillEndBatchNotify(bool* aInterrupt) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->WillEndBatch(this, aInterrupt);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::WillEndBatch() failed");
      return rv;
    }
    if (*aInterrupt) {
      return NS_OK;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::DidEndBatchNotify(nsresult aResult) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->DidEndBatch(this, aResult);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::DidEndBatch() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::WillMergeNotify(nsITransaction* aTop,
                                             nsITransaction* aTransaction,
                                             bool* aInterrupt) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = listener->WillMerge(this, aTop, aTransaction, aInterrupt);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::WillMerge() failed");
      return rv;
    }
    if (*aInterrupt) {
      return NS_OK;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::DidMergeNotify(nsITransaction* aTop,
                                            nsITransaction* aTransaction,
                                            bool aDidMerge,
                                            nsresult aMergeResult) {
  nsCOMArray<nsITransactionListener> listeners(mListeners);
  for (nsITransactionListener* listener : listeners) {
    if (NS_WARN_IF(!listener)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv =
        listener->DidMerge(this, aTop, aTransaction, aDidMerge, aMergeResult);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransactionListener::DidMerge() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult TransactionManager::BeginTransaction(nsITransaction* aTransaction,
                                              nsISupports* aData) {
  // XXX: POSSIBLE OPTIMIZATION
  //      We could use a factory that pre-allocates/recycles transaction items.
  RefPtr<TransactionItem> transactionItem = new TransactionItem(aTransaction);

  if (aData) {
    nsCOMArray<nsISupports>& data = transactionItem->GetData();
    data.AppendObject(aData);
  }

  mDoStack.Push(transactionItem);

  nsresult rv = transactionItem->DoTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionItem::DoTransaction() failed");
    transactionItem = mDoStack.Pop();
  }
  return rv;
}

nsresult TransactionManager::EndTransaction(bool aAllowEmpty) {
  RefPtr<TransactionItem> transactionItem = mDoStack.Pop();
  if (NS_WARN_IF(!transactionItem)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsITransaction> transaction = transactionItem->GetTransaction();
  if (!transaction && !aAllowEmpty) {
    // If we get here, the transaction must be a dummy batch transaction
    // created by BeginBatch(). If it contains no children, get rid of it!
    if (!transactionItem->NumberOfChildren()) {
      return NS_OK;
    }
  }

  // Check if the transaction is transient. If it is, there's nothing
  // more to do, just return.
  if (transaction) {
    bool isTransient = false;
    nsresult rv = transaction->GetIsTransient(&isTransient);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransaction::GetIsTransient() failed");
      return rv;
    }
    // XXX: Should we be clearing the redo stack if the transaction
    //      is transient and there is nothing on the do stack?
    if (isTransient) {
      return NS_OK;
    }
  }

  if (!mMaxTransactionCount) {
    return NS_OK;
  }

  // Check if there is a transaction on the do stack. If there is,
  // the current transaction is a "sub" transaction, and should
  // be added to the transaction at the top of the do stack.
  RefPtr<TransactionItem> topTransactionItem = mDoStack.Peek();
  if (topTransactionItem) {
    // XXX: What do we do if this fails?
    nsresult rv = topTransactionItem->AddChild(*transactionItem);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TransactionItem::AddChild() failed");
    return rv;
  }

  // The transaction succeeded, so clear the redo stack.
  mRedoStack.Clear();

  // Check if we can coalesce this transaction with the one at the top
  // of the undo stack.
  topTransactionItem = mUndoStack.Peek();
  if (transaction && topTransactionItem) {
    bool didMerge = false;
    nsCOMPtr<nsITransaction> topTransaction =
        topTransactionItem->GetTransaction();
    if (topTransaction) {
      bool doInterrupt = false;
      nsresult rv = WillMergeNotify(topTransaction, transaction, &doInterrupt);
      if (NS_FAILED(rv)) {
        NS_WARNING("TransactionManager::WillMergeNotify() failed");
        return rv;
      }

      if (!doInterrupt) {
        nsresult rv = topTransaction->Merge(transaction, &didMerge);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransaction::Merge() failed");
        nsresult rv2 =
            DidMergeNotify(topTransaction, transaction, didMerge, rv);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2),
                             "TransactionManager::DidMergeNotify() failed");
        if (didMerge) {
          return NS_SUCCEEDED(rv) ? rv2 : rv;
        }
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "The previous error was ignored");
      }
    }
  }

  // Check to see if we've hit the max level of undo. If so,
  // pop the bottom transaction off the undo stack and release it!
  int32_t sz = mUndoStack.GetSize();
  if (mMaxTransactionCount > 0 && sz >= mMaxTransactionCount) {
    RefPtr<TransactionItem> overflow = mUndoStack.PopBottom();
  }

  // Push the transaction on the undo stack:
  mUndoStack.Push(transactionItem.forget());
  return NS_OK;
}

}  // namespace mozilla
