/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/mozalloc.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsITransaction.h"
#include "nsITransactionList.h"
#include "nsITransactionListener.h"
#include "nsIWeakReference.h"
#include "nsTransactionItem.h"
#include "nsTransactionList.h"
#include "nsTransactionManager.h"
#include "nsTransactionStack.h"

nsTransactionManager::nsTransactionManager(int32_t aMaxTransactionCount)
  : mMaxTransactionCount(aMaxTransactionCount)
  , mDoStack(nsTransactionStack::FOR_UNDO)
  , mUndoStack(nsTransactionStack::FOR_UNDO)
  , mRedoStack(nsTransactionStack::FOR_REDO)
{
}

nsTransactionManager::~nsTransactionManager()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsTransactionManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsTransactionManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListeners)
  tmp->mDoStack.DoUnlink();
  tmp->mUndoStack.DoUnlink();
  tmp->mRedoStack.DoUnlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsTransactionManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
  tmp->mDoStack.DoTraverse(cb);
  tmp->mUndoStack.DoTraverse(cb);
  tmp->mRedoStack.DoTraverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTransactionManager)
  NS_INTERFACE_MAP_ENTRY(nsITransactionManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITransactionManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTransactionManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTransactionManager)

NS_IMETHODIMP
nsTransactionManager::DoTransaction(nsITransaction *aTransaction)
{
  nsresult result;

  NS_ENSURE_TRUE(aTransaction, NS_ERROR_NULL_POINTER);

  bool doInterrupt = false;

  result = WillDoNotify(aTransaction, &doInterrupt);

  if (NS_FAILED(result)) {
    return result;
  }

  if (doInterrupt) {
    return NS_OK;
  }

  result = BeginTransaction(aTransaction, nullptr);

  if (NS_FAILED(result)) {
    DidDoNotify(aTransaction, result);
    return result;
  }

  result = EndTransaction(false);

  nsresult result2 = DidDoNotify(aTransaction, result);

  if (NS_SUCCEEDED(result))
    result = result2;

  return result;
}

NS_IMETHODIMP
nsTransactionManager::UndoTransaction()
{
  nsresult result       = NS_OK;

  // It is illegal to call UndoTransaction() while the transaction manager is
  // executing a  transaction's DoTransaction() method! If this happens,
  // the UndoTransaction() request is ignored, and we return NS_ERROR_FAILURE.

  nsRefPtr<nsTransactionItem> tx = mDoStack.Peek();

  if (tx) {
    return NS_ERROR_FAILURE;
  }

  // Peek at the top of the undo stack. Don't remove the transaction
  // until it has successfully completed.
  tx = mUndoStack.Peek();

  // Bail if there's nothing on the stack.
  if (!tx) {
    return NS_OK;
  }

  nsCOMPtr<nsITransaction> t = tx->GetTransaction();

  bool doInterrupt = false;

  result = WillUndoNotify(t, &doInterrupt);

  if (NS_FAILED(result)) {
    return result;
  }

  if (doInterrupt) {
    return NS_OK;
  }

  result = tx->UndoTransaction(this);

  if (NS_SUCCEEDED(result)) {
    tx = mUndoStack.Pop();
    mRedoStack.Push(tx);
  }

  nsresult result2 = DidUndoNotify(t, result);

  if (NS_SUCCEEDED(result))
    result = result2;

  return result;
}

NS_IMETHODIMP
nsTransactionManager::RedoTransaction()
{
  nsresult result       = NS_OK;

  // It is illegal to call RedoTransaction() while the transaction manager is
  // executing a  transaction's DoTransaction() method! If this happens,
  // the RedoTransaction() request is ignored, and we return NS_ERROR_FAILURE.

  nsRefPtr<nsTransactionItem> tx = mDoStack.Peek();

  if (tx) {
    return NS_ERROR_FAILURE;
  }

  // Peek at the top of the redo stack. Don't remove the transaction
  // until it has successfully completed.
  tx = mRedoStack.Peek();

  // Bail if there's nothing on the stack.
  if (!tx) {
    return NS_OK;
  }

  nsCOMPtr<nsITransaction> t = tx->GetTransaction();

  bool doInterrupt = false;

  result = WillRedoNotify(t, &doInterrupt);

  if (NS_FAILED(result)) {
    return result;
  }

  if (doInterrupt) {
    return NS_OK;
  }

  result = tx->RedoTransaction(this);

  if (NS_SUCCEEDED(result)) {
    tx = mRedoStack.Pop();
    mUndoStack.Push(tx);
  }

  nsresult result2 = DidRedoNotify(t, result);

  if (NS_SUCCEEDED(result))
    result = result2;

  return result;
}

NS_IMETHODIMP
nsTransactionManager::Clear()
{
  nsresult result;

  result = ClearRedoStack();

  if (NS_FAILED(result)) {
    return result;
  }

  result = ClearUndoStack();

  return result;
}

NS_IMETHODIMP
nsTransactionManager::BeginBatch(nsISupports* aData)
{
  nsresult result;

  // We can batch independent transactions together by simply pushing
  // a dummy transaction item on the do stack. This dummy transaction item
  // will be popped off the do stack, and then pushed on the undo stack
  // in EndBatch().

  bool doInterrupt = false;

  result = WillBeginBatchNotify(&doInterrupt);

  if (NS_FAILED(result)) {
    return result;
  }

  if (doInterrupt) {
    return NS_OK;
  }

  result = BeginTransaction(0, aData);
  
  nsresult result2 = DidBeginBatchNotify(result);

  if (NS_SUCCEEDED(result))
    result = result2;

  return result;
}

NS_IMETHODIMP
nsTransactionManager::EndBatch(bool aAllowEmpty)
{
  nsCOMPtr<nsITransaction> ti;
  nsresult result;

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

  nsRefPtr<nsTransactionItem> tx = mDoStack.Peek();

  if (tx) {
    ti = tx->GetTransaction();
  }

  if (!tx || ti) {
    return NS_ERROR_FAILURE;
  }

  bool doInterrupt = false;

  result = WillEndBatchNotify(&doInterrupt);

  if (NS_FAILED(result)) {
    return result;
  }

  if (doInterrupt) {
    return NS_OK;
  }

  result = EndTransaction(aAllowEmpty);

  nsresult result2 = DidEndBatchNotify(result);

  if (NS_SUCCEEDED(result))
    result = result2;

  return result;
}

NS_IMETHODIMP
nsTransactionManager::GetNumberOfUndoItems(int32_t *aNumItems)
{
  *aNumItems = mUndoStack.GetSize();
  return NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::GetNumberOfRedoItems(int32_t *aNumItems)
{
  *aNumItems = mRedoStack.GetSize();
  return NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::GetMaxTransactionCount(int32_t *aMaxCount)
{
  NS_ENSURE_TRUE(aMaxCount, NS_ERROR_NULL_POINTER);

  *aMaxCount = mMaxTransactionCount;

  return NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::SetMaxTransactionCount(int32_t aMaxCount)
{
  int32_t numUndoItems  = 0, numRedoItems = 0, total = 0;

  // It is illegal to call SetMaxTransactionCount() while the transaction
  // manager is executing a  transaction's DoTransaction() method because
  // the undo and redo stacks might get pruned! If this happens, the
  // SetMaxTransactionCount() request is ignored, and we return
  // NS_ERROR_FAILURE.

  nsRefPtr<nsTransactionItem> tx = mDoStack.Peek();

  if (tx) {
    return NS_ERROR_FAILURE;
  }

  // If aMaxCount is less than zero, the user wants unlimited
  // levels of undo! No need to prune the undo or redo stacks!

  if (aMaxCount < 0) {
    mMaxTransactionCount = -1;
    return NS_OK;
  }

  numUndoItems = mUndoStack.GetSize();

  numRedoItems = mRedoStack.GetSize();

  total = numUndoItems + numRedoItems;

  // If aMaxCount is greater than the number of transactions that currently
  // exist on the undo and redo stack, there is no need to prune the
  // undo or redo stacks!

  if (aMaxCount > total ) {
    mMaxTransactionCount = aMaxCount;
    return NS_OK;
  }

  // Try getting rid of some transactions on the undo stack! Start at
  // the bottom of the stack and pop towards the top.

  while (numUndoItems > 0 && (numRedoItems + numUndoItems) > aMaxCount) {
    tx = mUndoStack.PopBottom();

    if (!tx) {
      return NS_ERROR_FAILURE;
    }

    --numUndoItems;
  }

  // If necessary, get rid of some transactions on the redo stack! Start at
  // the bottom of the stack and pop towards the top.

  while (numRedoItems > 0 && (numRedoItems + numUndoItems) > aMaxCount) {
    tx = mRedoStack.PopBottom();

    if (!tx) {
      return NS_ERROR_FAILURE;
    }

    --numRedoItems;
  }

  mMaxTransactionCount = aMaxCount;

  return NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::PeekUndoStack(nsITransaction **aTransaction)
{
  MOZ_ASSERT(aTransaction);
  *aTransaction = PeekUndoStack().get();
  return NS_OK;
}

already_AddRefed<nsITransaction>
nsTransactionManager::PeekUndoStack()
{
  nsRefPtr<nsTransactionItem> tx = mUndoStack.Peek();

  if (!tx) {
    return nullptr;
  }

  return tx->GetTransaction();
}

NS_IMETHODIMP
nsTransactionManager::PeekRedoStack(nsITransaction** aTransaction)
{
  MOZ_ASSERT(aTransaction);
  *aTransaction = PeekRedoStack().get();
  return NS_OK;
}

already_AddRefed<nsITransaction>
nsTransactionManager::PeekRedoStack()
{
  nsRefPtr<nsTransactionItem> tx = mRedoStack.Peek();

  if (!tx) {
    return nullptr;
  }

  return tx->GetTransaction();
}

NS_IMETHODIMP
nsTransactionManager::GetUndoList(nsITransactionList **aTransactionList)
{
  NS_ENSURE_TRUE(aTransactionList, NS_ERROR_NULL_POINTER);

  *aTransactionList = (nsITransactionList *)new nsTransactionList(this, &mUndoStack);

  NS_IF_ADDREF(*aTransactionList);

  return (! *aTransactionList) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::GetRedoList(nsITransactionList **aTransactionList)
{
  NS_ENSURE_TRUE(aTransactionList, NS_ERROR_NULL_POINTER);

  *aTransactionList = (nsITransactionList *)new nsTransactionList(this, &mRedoStack);

  NS_IF_ADDREF(*aTransactionList);

  return (! *aTransactionList) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

nsresult
nsTransactionManager::BatchTopUndo()
{
  if (mUndoStack.GetSize() < 2) {
    // Not enough transactions to merge into one batch.
    return NS_OK;
  }

  nsRefPtr<nsTransactionItem> lastUndo;
  nsRefPtr<nsTransactionItem> previousUndo;

  lastUndo = mUndoStack.Pop();
  MOZ_ASSERT(lastUndo, "There should be at least two transactions.");

  previousUndo = mUndoStack.Peek();
  MOZ_ASSERT(previousUndo, "There should be at least two transactions.");

  nsresult result = previousUndo->AddChild(lastUndo);

  // Transfer data from the transactions that is going to be
  // merged to the transaction that it is being merged with.
  nsCOMArray<nsISupports>& lastData = lastUndo->GetData();
  nsCOMArray<nsISupports>& previousData = previousUndo->GetData();
  NS_ENSURE_TRUE(previousData.AppendObjects(lastData), NS_ERROR_UNEXPECTED);
  lastData.Clear();

  return result;
}

nsresult
nsTransactionManager::RemoveTopUndo()
{
  nsRefPtr<nsTransactionItem> lastUndo;

  lastUndo = mUndoStack.Peek();
  if (!lastUndo) {
    return NS_OK;
  }

  lastUndo = mUndoStack.Pop();

  return NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::AddListener(nsITransactionListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  return mListeners.AppendObject(aListener) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTransactionManager::RemoveListener(nsITransactionListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  return mListeners.RemoveObject(aListener) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTransactionManager::ClearUndoStack()
{
  mUndoStack.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::ClearRedoStack()
{
  mRedoStack.Clear();
  return NS_OK;
}

nsresult
nsTransactionManager::WillDoNotify(nsITransaction *aTransaction, bool *aInterrupt)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->WillDo(this, aTransaction, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidDoNotify(nsITransaction *aTransaction, nsresult aDoResult)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->DidDo(this, aTransaction, aDoResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillUndoNotify(nsITransaction *aTransaction, bool *aInterrupt)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->WillUndo(this, aTransaction, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidUndoNotify(nsITransaction *aTransaction, nsresult aUndoResult)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->DidUndo(this, aTransaction, aUndoResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillRedoNotify(nsITransaction *aTransaction, bool *aInterrupt)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->WillRedo(this, aTransaction, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidRedoNotify(nsITransaction *aTransaction, nsresult aRedoResult)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->DidRedo(this, aTransaction, aRedoResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillBeginBatchNotify(bool *aInterrupt)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->WillBeginBatch(this, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidBeginBatchNotify(nsresult aResult)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->DidBeginBatch(this, aResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillEndBatchNotify(bool *aInterrupt)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->WillEndBatch(this, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidEndBatchNotify(nsresult aResult)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->DidEndBatch(this, aResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillMergeNotify(nsITransaction *aTop, nsITransaction *aTransaction, bool *aInterrupt)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->WillMerge(this, aTop, aTransaction, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidMergeNotify(nsITransaction *aTop,
                                     nsITransaction *aTransaction,
                                     bool aDidMerge,
                                     nsresult aMergeResult)
{
  nsresult result = NS_OK;
  for (int32_t i = 0, lcount = mListeners.Count(); i < lcount; i++)
  {
    nsITransactionListener *listener = mListeners[i];

    NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

    result = listener->DidMerge(this, aTop, aTransaction, aDidMerge, aMergeResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::BeginTransaction(nsITransaction *aTransaction,
                                       nsISupports *aData)
{
  nsresult result = NS_OK;

  // XXX: POSSIBLE OPTIMIZATION
  //      We could use a factory that pre-allocates/recycles transaction items.
  nsRefPtr<nsTransactionItem> tx = new nsTransactionItem(aTransaction);

  if (aData) {
    nsCOMArray<nsISupports>& data = tx->GetData();
    data.AppendObject(aData);
  }

  if (!tx) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mDoStack.Push(tx);

  result = tx->DoTransaction();

  if (NS_FAILED(result)) {
    tx = mDoStack.Pop();
    return result;
  }

  return NS_OK;
}

nsresult
nsTransactionManager::EndTransaction(bool aAllowEmpty)
{
  nsresult result              = NS_OK;

  nsRefPtr<nsTransactionItem> tx = mDoStack.Pop();

  if (!tx)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITransaction> tint = tx->GetTransaction();

  if (!tint && !aAllowEmpty) {
    int32_t nc = 0;

    // If we get here, the transaction must be a dummy batch transaction
    // created by BeginBatch(). If it contains no children, get rid of it!

    tx->GetNumberOfChildren(&nc);

    if (!nc) {
      return result;
    }
  }

  // Check if the transaction is transient. If it is, there's nothing
  // more to do, just return.

  bool isTransient = false;

  if (tint)
    result = tint->GetIsTransient(&isTransient);

  if (NS_FAILED(result) || isTransient || !mMaxTransactionCount) {
    // XXX: Should we be clearing the redo stack if the transaction
    //      is transient and there is nothing on the do stack?
    return result;
  }

  // Check if there is a transaction on the do stack. If there is,
  // the current transaction is a "sub" transaction, and should
  // be added to the transaction at the top of the do stack.

  nsRefPtr<nsTransactionItem> top = mDoStack.Peek();
  if (top) {
    result = top->AddChild(tx);

    // XXX: What do we do if this fails?

    return result;
  }

  // The transaction succeeded, so clear the redo stack.

  result = ClearRedoStack();

  if (NS_FAILED(result)) {
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

      result = WillMergeNotify(topTransaction, tint, &doInterrupt);

      NS_ENSURE_SUCCESS(result, result);

      if (!doInterrupt) {
        result = topTransaction->Merge(tint, &didMerge);

        nsresult result2 = DidMergeNotify(topTransaction, tint, didMerge, result);

        if (NS_SUCCEEDED(result))
          result = result2;

        if (NS_FAILED(result)) {
          // XXX: What do we do if this fails?
        }

        if (didMerge) {
          return result;
        }
      }
    }
  }

  // Check to see if we've hit the max level of undo. If so,
  // pop the bottom transaction off the undo stack and release it!

  int32_t sz = mUndoStack.GetSize();

  if (mMaxTransactionCount > 0 && sz >= mMaxTransactionCount) {
    nsRefPtr<nsTransactionItem> overflow = mUndoStack.PopBottom();
  }

  // Push the transaction on the undo stack:

  mUndoStack.Push(tx);

  return NS_OK;
}

