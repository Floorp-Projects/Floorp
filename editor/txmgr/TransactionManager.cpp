/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TransactionManager.h"

#include "mozilla/Assertions.h"
#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsITransaction.h"
#include "nsITransactionListener.h"
#include "nsIWeakReference.h"
#include "nsTransactionItem.h"
#include "nsTransactionStack.h"

namespace mozilla {

TransactionManager::TransactionManager(int32_t aMaxTransactionCount)
  : mMaxTransactionCount(aMaxTransactionCount)
  , mDoStack(nsTransactionStack::FOR_UNDO)
  , mUndoStack(nsTransactionStack::FOR_UNDO)
  , mRedoStack(nsTransactionStack::FOR_REDO)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TransactionManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TransactionManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListeners)
  tmp->mDoStack.DoUnlink();
  tmp->mUndoStack.DoUnlink();
  tmp->mRedoStack.DoUnlink();
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

NS_IMETHODIMP
TransactionManager::DoTransaction(nsITransaction* aTransaction)
{
  NS_ENSURE_TRUE(aTransaction, NS_ERROR_NULL_POINTER);

  bool doInterrupt = false;

  nsresult rv = WillDoNotify(aTransaction, &doInterrupt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = BeginTransaction(aTransaction, nullptr);
  if (NS_FAILED(rv)) {
    DidDoNotify(aTransaction, rv);
    return rv;
  }

  rv = EndTransaction(false);

  nsresult rv2 = DidDoNotify(aTransaction, rv);
  if (NS_SUCCEEDED(rv)) {
    rv = rv2;
  }

  // XXX The result of EndTransaction() or DidDoNotify() if EndTransaction()
  //     succeeded.
  return rv;
}

NS_IMETHODIMP
TransactionManager::UndoTransaction()
{
  // It is illegal to call UndoTransaction() while the transaction manager is
  // executing a  transaction's DoTransaction() method! If this happens,
  // the UndoTransaction() request is ignored, and we return NS_ERROR_FAILURE.
  if (!mDoStack.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  // Peek at the top of the undo stack. Don't remove the transaction
  // until it has successfully completed.
  RefPtr<nsTransactionItem> tx = mUndoStack.Peek();
  if (!tx) {
    // Bail if there's nothing on the stack.
    return NS_OK;
  }

  nsCOMPtr<nsITransaction> t = tx->GetTransaction();
  bool doInterrupt = false;
  nsresult rv = WillUndoNotify(t, &doInterrupt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = tx->UndoTransaction(this);
  if (NS_SUCCEEDED(rv)) {
    tx = mUndoStack.Pop();
    mRedoStack.Push(tx.forget());
  }

  nsresult rv2 = DidUndoNotify(t, rv);
  if (NS_SUCCEEDED(rv)) {
    rv = rv2;
  }

  // XXX The result of UndoTransaction() or DidUndoNotify() if UndoTransaction()
  //     succeeded.
  return rv;
}

NS_IMETHODIMP
TransactionManager::RedoTransaction()
{
  // It is illegal to call RedoTransaction() while the transaction manager is
  // executing a  transaction's DoTransaction() method! If this happens,
  // the RedoTransaction() request is ignored, and we return NS_ERROR_FAILURE.
  if (!mDoStack.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  // Peek at the top of the redo stack. Don't remove the transaction
  // until it has successfully completed.
  RefPtr<nsTransactionItem> tx = mRedoStack.Peek();
  if (!tx) {
    // Bail if there's nothing on the stack.
    return NS_OK;
  }

  nsCOMPtr<nsITransaction> t = tx->GetTransaction();
  bool doInterrupt = false;
  nsresult rv = WillRedoNotify(t, &doInterrupt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = tx->RedoTransaction(this);
  if (NS_SUCCEEDED(rv)) {
    tx = mRedoStack.Pop();
    mUndoStack.Push(tx.forget());
  }

  nsresult rv2 = DidRedoNotify(t, rv);
  if (NS_SUCCEEDED(rv)) {
    rv = rv2;
  }

  // XXX The result of RedoTransaction() or DidRedoNotify() if RedoTransaction()
  //     succeeded.
  return rv;
}

NS_IMETHODIMP
TransactionManager::Clear()
{
  nsresult rv = ClearRedoStack();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return ClearUndoStack();
}

NS_IMETHODIMP
TransactionManager::BeginBatch(nsISupports* aData)
{
  // We can batch independent transactions together by simply pushing
  // a dummy transaction item on the do stack. This dummy transaction item
  // will be popped off the do stack, and then pushed on the undo stack
  // in EndBatch().
  bool doInterrupt = false;
  nsresult rv = WillBeginBatchNotify(&doInterrupt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = BeginTransaction(0, aData);

  nsresult rv2 = DidBeginBatchNotify(rv);
  if (NS_SUCCEEDED(rv)) {
    rv = rv2;
  }

  // XXX The result of BeginTransaction() or DidBeginBatchNotify() if
  //     BeginTransaction() succeeded.
  return rv;
}

NS_IMETHODIMP
TransactionManager::EndBatch(bool aAllowEmpty)
{
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
  RefPtr<nsTransactionItem> tx = mDoStack.Peek();
  nsCOMPtr<nsITransaction> ti;
  if (tx) {
    ti = tx->GetTransaction();
  }
  if (!tx || ti) {
    return NS_ERROR_FAILURE;
  }

  bool doInterrupt = false;
  nsresult rv = WillEndBatchNotify(&doInterrupt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (doInterrupt) {
    return NS_OK;
  }

  rv = EndTransaction(aAllowEmpty);
  nsresult rv2 = DidEndBatchNotify(rv);
  if (NS_SUCCEEDED(rv)) {
    rv = rv2;
  }

  // XXX The result of EndTransaction() or DidEndBatchNotify() if
  //     EndTransaction() succeeded.
  return rv;
}

NS_IMETHODIMP
TransactionManager::GetNumberOfUndoItems(int32_t* aNumItems)
{
  *aNumItems = mUndoStack.GetSize();
  return NS_OK;
}

NS_IMETHODIMP
TransactionManager::GetNumberOfRedoItems(int32_t* aNumItems)
{
  *aNumItems = mRedoStack.GetSize();
  return NS_OK;
}

NS_IMETHODIMP
TransactionManager::GetMaxTransactionCount(int32_t* aMaxCount)
{
  NS_ENSURE_TRUE(aMaxCount, NS_ERROR_NULL_POINTER);
  *aMaxCount = mMaxTransactionCount;
  return NS_OK;
}

NS_IMETHODIMP
TransactionManager::SetMaxTransactionCount(int32_t aMaxCount)
{
  // It is illegal to call SetMaxTransactionCount() while the transaction
  // manager is executing a  transaction's DoTransaction() method because
  // the undo and redo stacks might get pruned! If this happens, the
  // SetMaxTransactionCount() request is ignored, and we return
  // NS_ERROR_FAILURE.
  if (!mDoStack.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  // If aMaxCount is less than zero, the user wants unlimited
  // levels of undo! No need to prune the undo or redo stacks!
  if (aMaxCount < 0) {
    mMaxTransactionCount = -1;
    return NS_OK;
  }

  // If aMaxCount is greater than the number of transactions that currently
  // exist on the undo and redo stack, there is no need to prune the
  // undo or redo stacks!
  int32_t numUndoItems = mUndoStack.GetSize();
  int32_t numRedoItems = mRedoStack.GetSize();
  int32_t total = numUndoItems + numRedoItems;
  if (aMaxCount > total) {
    mMaxTransactionCount = aMaxCount;
    return NS_OK;
  }

  // Try getting rid of some transactions on the undo stack! Start at
  // the bottom of the stack and pop towards the top.
  while (numUndoItems > 0 && (numRedoItems + numUndoItems) > aMaxCount) {
    RefPtr<nsTransactionItem> tx = mUndoStack.PopBottom();
    if (!tx) {
      return NS_ERROR_FAILURE;
    }
    --numUndoItems;
  }

  // If necessary, get rid of some transactions on the redo stack! Start at
  // the bottom of the stack and pop towards the top.
  while (numRedoItems > 0 && (numRedoItems + numUndoItems) > aMaxCount) {
    RefPtr<nsTransactionItem> tx = mRedoStack.PopBottom();
    if (!tx) {
      return NS_ERROR_FAILURE;
    }
    --numRedoItems;
  }

  mMaxTransactionCount = aMaxCount;
  return NS_OK;
}

NS_IMETHODIMP
TransactionManager::PeekUndoStack(nsITransaction** aTransaction)
{
  MOZ_ASSERT(aTransaction);
  *aTransaction = PeekUndoStack().take();
  return NS_OK;
}

already_AddRefed<nsITransaction>
TransactionManager::PeekUndoStack()
{
  RefPtr<nsTransactionItem> tx = mUndoStack.Peek();
  if (!tx) {
    return nullptr;
  }
  return tx->GetTransaction();
}

NS_IMETHODIMP
TransactionManager::PeekRedoStack(nsITransaction** aTransaction)
{
  MOZ_ASSERT(aTransaction);
  *aTransaction = PeekRedoStack().take();
  return NS_OK;
}

already_AddRefed<nsITransaction>
TransactionManager::PeekRedoStack()
{
  RefPtr<nsTransactionItem> tx = mRedoStack.Peek();
  if (!tx) {
    return nullptr;
  }
  return tx->GetTransaction();
}

nsresult
TransactionManager::BatchTopUndo()
{
  if (mUndoStack.GetSize() < 2) {
    // Not enough transactions to merge into one batch.
    return NS_OK;
  }

  RefPtr<nsTransactionItem> lastUndo;
  RefPtr<nsTransactionItem> previousUndo;

  lastUndo = mUndoStack.Pop();
  MOZ_ASSERT(lastUndo, "There should be at least two transactions.");

  previousUndo = mUndoStack.Peek();
  MOZ_ASSERT(previousUndo, "There should be at least two transactions.");

  nsresult rv = previousUndo->AddChild(lastUndo);

  // Transfer data from the transactions that is going to be
  // merged to the transaction that it is being merged with.
  nsCOMArray<nsISupports>& lastData = lastUndo->GetData();
  nsCOMArray<nsISupports>& previousData = previousUndo->GetData();
  NS_ENSURE_TRUE(previousData.AppendObjects(lastData), NS_ERROR_UNEXPECTED);
  lastData.Clear();
  return rv;
}

nsresult
TransactionManager::RemoveTopUndo()
{
  if (mUndoStack.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<nsTransactionItem> lastUndo = mUndoStack.Pop();
  return NS_OK;
}

NS_IMETHODIMP
TransactionManager::AddListener(nsITransactionListener* aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);
  return mListeners.AppendObject(aListener) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TransactionManager::RemoveListener(nsITransactionListener* aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);
  return mListeners.RemoveObject(aListener) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TransactionManager::ClearUndoStack()
{
  mUndoStack.Clear();
  return NS_OK;
}

NS_IMETHODIMP
TransactionManager::ClearRedoStack()
{
  mRedoStack.Clear();
  return NS_OK;
}

nsresult
TransactionManager::WillDoNotify(nsITransaction* aTransaction,
                                 bool* aInterrupt)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->WillDo(this, aTransaction, aInterrupt);
    if (NS_FAILED(rv) || *aInterrupt) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::DidDoNotify(nsITransaction* aTransaction,
                                nsresult aDoResult)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->DidDo(this, aTransaction, aDoResult);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::WillUndoNotify(nsITransaction* aTransaction,
                                   bool* aInterrupt)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->WillUndo(this, aTransaction, aInterrupt);
    if (NS_FAILED(rv) || *aInterrupt) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::DidUndoNotify(nsITransaction* aTransaction,
                                  nsresult aUndoResult)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->DidUndo(this, aTransaction, aUndoResult);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::WillRedoNotify(nsITransaction* aTransaction,
                                   bool* aInterrupt)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->WillRedo(this, aTransaction, aInterrupt);
    if (NS_FAILED(rv) || *aInterrupt) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::DidRedoNotify(nsITransaction* aTransaction,
                                  nsresult aRedoResult)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->DidRedo(this, aTransaction, aRedoResult);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::WillBeginBatchNotify(bool* aInterrupt)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->WillBeginBatch(this, aInterrupt);
    if (NS_FAILED(rv) || *aInterrupt) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::DidBeginBatchNotify(nsresult aResult)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->DidBeginBatch(this, aResult);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::WillEndBatchNotify(bool* aInterrupt)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->WillEndBatch(this, aInterrupt);
    if (NS_FAILED(rv) || *aInterrupt) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::DidEndBatchNotify(nsresult aResult)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->DidEndBatch(this, aResult);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::WillMergeNotify(nsITransaction* aTop,
                                    nsITransaction* aTransaction,
                                    bool* aInterrupt)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv = listener->WillMerge(this, aTop, aTransaction, aInterrupt);
    if (NS_FAILED(rv) || *aInterrupt) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::DidMergeNotify(nsITransaction* aTop,
                                   nsITransaction* aTransaction,
                                   bool aDidMerge,
                                   nsresult aMergeResult)
{
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++) {
    nsITransactionListener* listener = mListeners[i];
    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    nsresult rv =
      listener->DidMerge(this, aTop, aTransaction, aDidMerge, aMergeResult);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TransactionManager::BeginTransaction(nsITransaction* aTransaction,
                                     nsISupports* aData)
{
  // XXX: POSSIBLE OPTIMIZATION
  //      We could use a factory that pre-allocates/recycles transaction items.
  RefPtr<nsTransactionItem> tx = new nsTransactionItem(aTransaction);
  if (!tx) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (aData) {
    nsCOMArray<nsISupports>& data = tx->GetData();
    data.AppendObject(aData);
  }

  mDoStack.Push(tx);

  nsresult rv = tx->DoTransaction();
  if (NS_FAILED(rv)) {
    tx = mDoStack.Pop();
    return rv;
  }
  return NS_OK;
}

nsresult
TransactionManager::EndTransaction(bool aAllowEmpty)
{
  RefPtr<nsTransactionItem> tx = mDoStack.Pop();
  if (!tx) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsITransaction> tint = tx->GetTransaction();
  if (!tint && !aAllowEmpty) {
    // If we get here, the transaction must be a dummy batch transaction
    // created by BeginBatch(). If it contains no children, get rid of it!
    int32_t nc = 0;
    tx->GetNumberOfChildren(&nc);
    if (!nc) {
      return NS_OK;
    }
  }

  // Check if the transaction is transient. If it is, there's nothing
  // more to do, just return.
  bool isTransient = false;
  nsresult rv = NS_OK;
  if (tint) {
    rv = tint->GetIsTransient(&isTransient);
  }
  if (NS_FAILED(rv) || isTransient || !mMaxTransactionCount) {
    // XXX: Should we be clearing the redo stack if the transaction
    //      is transient and there is nothing on the do stack?
    return rv;
  }

  // Check if there is a transaction on the do stack. If there is,
  // the current transaction is a "sub" transaction, and should
  // be added to the transaction at the top of the do stack.
  RefPtr<nsTransactionItem> top = mDoStack.Peek();
  if (top) {
    return top->AddChild(tx); // XXX: What do we do if this fails?
  }

  // The transaction succeeded, so clear the redo stack.
  rv = ClearRedoStack();
  if (NS_FAILED(rv)) {
    // XXX: What do we do if this fails?
  }

  // Check if we can coalesce this transaction with the one at the top
  // of the undo stack.
  top = mUndoStack.Peek();
  if (tint && top) {
    bool didMerge = false;
    nsCOMPtr<nsITransaction> topTransaction = top->GetTransaction();
    if (topTransaction) {
      bool doInterrupt = false;
      rv = WillMergeNotify(topTransaction, tint, &doInterrupt);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!doInterrupt) {
        rv = topTransaction->Merge(tint, &didMerge);
        nsresult rv2 = DidMergeNotify(topTransaction, tint, didMerge, rv);
        if (NS_SUCCEEDED(rv)) {
          rv = rv2;
        }
        if (NS_FAILED(rv)) {
          // XXX: What do we do if this fails?
        }
        if (didMerge) {
          return rv;
        }
      }
    }
  }

  // Check to see if we've hit the max level of undo. If so,
  // pop the bottom transaction off the undo stack and release it!
  int32_t sz = mUndoStack.GetSize();
  if (mMaxTransactionCount > 0 && sz >= mMaxTransactionCount) {
    RefPtr<nsTransactionItem> overflow = mUndoStack.PopBottom();
  }

  // Push the transaction on the undo stack:
  mUndoStack.Push(tx.forget());
  return NS_OK;
}

} // namespace mozilla
