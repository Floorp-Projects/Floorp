/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsTransactionManager.h"
#include "nsCOMPtr.h"

#define LOCK_TX_MANAGER(mgr)
#define UNLOCK_TX_MANAGER(mgr)

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

nsTransactionManager::nsTransactionManager(PRInt32 aMaxTransactionCount)
  : mMaxTransactionCount(aMaxTransactionCount)
{
  mRefCnt = 0;
}

nsTransactionManager::~nsTransactionManager()
{
}

#ifdef DEBUG_TXMGR_REFCNT

nsrefcnt nsTransactionManager::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsTransactionManager::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(nsTransactionManager)
NS_IMPL_RELEASE(nsTransactionManager)

#endif

nsresult
nsTransactionManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITransactionManagerIID)) {
    *aInstancePtr = (void*)(nsITransactionManager*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

nsresult
nsTransactionManager::Do(nsITransaction *aTransaction)
{
  nsresult result;

  if (!aTransaction)
    return NS_ERROR_NULL_POINTER;

  LOCK_TX_MANAGER(this);

  result = BeginTransaction(aTransaction);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  result = EndTransaction();

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::Undo()
{
  nsresult result       = NS_OK;
  nsTransactionItem *tx = 0;

  LOCK_TX_MANAGER(this);

  // It is illegal to call Undo() while the transaction manager is
  // executing a  transaction's Do() method! If this happens, the Undo()
  // request is ignored, and we return NS_ERROR_FAILURE.

  result = mDoStack.Peek(&tx);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (tx) {
    UNLOCK_TX_MANAGER(this);
    return NS_ERROR_FAILURE;
  }

  // Peek at the top of the undo stack. Don't remove the transaction
  // until it has successfully completed.
  result = mUndoStack.Peek(&tx);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  // Bail if there's nothing on the stack.
  if (!tx) {
    UNLOCK_TX_MANAGER(this);
    return NS_OK;
  }

  result = tx->Undo();

  if (NS_SUCCEEDED(result)) {
    result = mUndoStack.Pop(&tx);
    result = mRedoStack.Push(tx);
  }

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::Redo()
{
  nsresult result       = NS_OK;
  nsTransactionItem *tx = 0;

  LOCK_TX_MANAGER(this);

  // It is illegal to call Redo() while the transaction manager is
  // executing a  transaction's Do() method! If this happens, the Redo()
  // request is ignored, and we return NS_ERROR_FAILURE.

  result = mDoStack.Peek(&tx);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (tx) {
    UNLOCK_TX_MANAGER(this);
    return NS_ERROR_FAILURE;
  }

  // Peek at the top of the redo stack. Don't remove the transaction
  // until it has successfully completed.
  result = mRedoStack.Peek(&tx);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  // Bail if there's nothing on the stack.
  if (!tx) {
    UNLOCK_TX_MANAGER(this);
    return NS_OK;
  }

  result = tx->Redo();

  if (NS_SUCCEEDED(result)) {
    result = mRedoStack.Pop(&tx);
    result = mUndoStack.Push(tx);
  }

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::Clear()
{
  nsresult result;

  LOCK_TX_MANAGER(this);

  result = ClearRedoStack();

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  result = ClearUndoStack();

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::BeginBatch()
{
  if (gNoisy) { printf("Begin Batch\n"); }
  nsresult result;

  // We can batch independent transactions together by simply pushing
  // a dummy transaction item on the do stack. This dummy transaction item
  // will be popped off the do stack, and then pushed on the undo stack
  // in EndBatch().

  LOCK_TX_MANAGER(this);

  result = BeginTransaction(0);
  
  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::EndBatch()
{
  if (gNoisy) { printf("End Batch\n"); }
  nsTransactionItem *tx = 0;
  nsITransaction *ti    = 0;
  nsresult result;

  LOCK_TX_MANAGER(this);

  // XXX: Need to add some mechanism to detect the case where the transaction
  //      at the top of the do stack isn't the dummy transaction, so we can
  //      throw an error!! This can happen if someone calls EndBatch() within
  //      the Do() method of a transaction.
  //
  //      For now, we can detect this case by checking the value of the
  //      dummy transaction's mTransaction field. If it is our dummy
  //      transaction, it should be NULL. This may not be true in the
  //      future when we allow users to execute a transaction when beginning
  //      a batch!!!!

  result = mDoStack.Peek(&tx);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (tx)
    tx->GetTransaction(&ti);

  if (!tx || ti) {
    UNLOCK_TX_MANAGER(this);
    return NS_ERROR_FAILURE;
  }

  result = EndTransaction();

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::GetNumberOfUndoItems(PRInt32 *aNumItems)
{
  nsresult result;

  LOCK_TX_MANAGER(this);
  result = mUndoStack.GetSize(aNumItems);
  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::GetNumberOfRedoItems(PRInt32 *aNumItems)
{
  nsresult result;

  LOCK_TX_MANAGER(this);
  result = mRedoStack.GetSize(aNumItems);
  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::SetMaxTransactionCount(PRInt32 aMaxCount)
{
  PRInt32 numUndoItems  = 0, numRedoItems = 0, total = 0;
  nsTransactionItem *tx = 0;
  nsresult result;

  LOCK_TX_MANAGER(this);

  // It is illegal to call SetMaxTransactionCount() while the transaction
  // manager is executing a  transaction's Do() method because the undo and
  // redo stacks might get pruned! If this happens, the SetMaxTransactionCount()
  // request is ignored, and we return NS_ERROR_FAILURE.

  result = mDoStack.Peek(&tx);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (tx) {
    UNLOCK_TX_MANAGER(this);
    return NS_ERROR_FAILURE;
  }

  // If aMaxCount is less than zero, the user wants unlimited
  // levels of undo! No need to prune the undo or redo stacks!

  if (aMaxCount < 0) {
    mMaxTransactionCount = -1;
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  result = mUndoStack.GetSize(&numUndoItems);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  result = mRedoStack.GetSize(&numRedoItems);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  total = numUndoItems + numRedoItems;

  // If aMaxCount is greater than the number of transactions that currently
  // exist on the undo and redo stack, there is no need to prune the
  // undo or redo stacks!

  if (aMaxCount > total ) {
    mMaxTransactionCount = aMaxCount;
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  // Try getting rid of some transactions on the undo stack! Start at
  // the bottom of the stack and pop towards the top.

  while (numUndoItems > 0 && (numRedoItems + numUndoItems) > aMaxCount) {
    tx = 0;
    result = mUndoStack.PopBottom(&tx);

    if (NS_FAILED(result) || !tx) {
      UNLOCK_TX_MANAGER(this);
      return result;
    }

    delete tx;

    --numUndoItems;
  }

  // If neccessary, get rid of some transactions on the redo stack! Start at
  // the bottom of the stack and pop towards the top.

  while (numRedoItems > 0 && (numRedoItems + numUndoItems) > aMaxCount) {
    tx = 0;
    result = mRedoStack.PopBottom(&tx);

    if (NS_FAILED(result) || !tx) {
      UNLOCK_TX_MANAGER(this);
      return result;
    }

    delete tx;

    --numRedoItems;
  }

  mMaxTransactionCount = aMaxCount;

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::PeekUndoStack(nsITransaction **aTransaction)
{
  nsTransactionItem *tx = 0;
  nsresult result;

  if (!aTransaction)
    return NS_ERROR_NULL_POINTER;

  *aTransaction = 0;

  LOCK_TX_MANAGER(this);

  result = mUndoStack.Peek(&tx);

  if (NS_FAILED(result) || !tx) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  result = tx->GetTransaction(aTransaction);

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::PeekRedoStack(nsITransaction **aTransaction)
{
  nsTransactionItem *tx = 0;
  nsresult result;

  if (!aTransaction)
    return NS_ERROR_NULL_POINTER;

  *aTransaction = 0;

  LOCK_TX_MANAGER(this);

  result = mRedoStack.Peek(&tx);

  if (NS_FAILED(result) || !tx) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  result = tx->GetTransaction(aTransaction);

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::Write(nsIOutputStream *aOutputStream)
{
  PRUint32 len;

  if (!aOutputStream)
    return NS_ERROR_NULL_POINTER;

  aOutputStream->Write("UndoStack:\n\n", 12, &len);
  mUndoStack.Write(aOutputStream);

  aOutputStream->Write("\nRedoStack:\n\n", 13, &len);
  mRedoStack.Write(aOutputStream);

  return NS_OK;
}

nsresult
nsTransactionManager::AddListener(nsITransactionListener *aListener)
{
  // XXX: Need to add listener support.
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsTransactionManager::RemoveListener(nsITransactionListener *aListener)
{
  // XXX: Need to add listener support.
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsTransactionManager::ClearUndoStack()
{
  nsresult result;

  LOCK_TX_MANAGER(this);
  result = mUndoStack.Clear();
  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::ClearRedoStack()
{
  nsresult result;

  LOCK_TX_MANAGER(this);
  result = mRedoStack.Clear();
  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::BeginTransaction(nsITransaction *aTransaction)
{
  nsTransactionItem *tx;
  nsresult result = NS_OK;

  // No need for LOCK/UNLOCK_TX_MANAGER() calls since the calling routine
  // should have done this already!

  NS_IF_ADDREF(aTransaction);

  // XXX: POSSIBLE OPTIMIZATION
  //      We could use a factory that pre-allocates/recycles transaction items.
  tx = new nsTransactionItem(aTransaction);

  if (!tx) {
    NS_IF_RELEASE(aTransaction);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  result = mDoStack.Push(tx);

  if (NS_FAILED(result)) {
    delete tx;
    return result;
  }

  result = tx->Do();

  if (NS_FAILED(result)) {
    mDoStack.Pop(&tx);
    delete tx;
    return result;
  }

  return NS_OK;
}

nsresult
nsTransactionManager::EndTransaction()
{
  nsITransaction *tint = 0;
  nsTransactionItem *tx        = 0;
  nsresult result              = NS_OK;

  // No need for LOCK/UNLOCK_TX_MANAGER() calls since the calling routine
  // should have done this already!

  result = mDoStack.Pop(&tx);

  if (NS_FAILED(result) || !tx)
    return result;

  result = tx->GetTransaction(&tint);

  if (NS_FAILED(result)) {
    // XXX: What do we do with the transaction item at this point?
    return result;
  }

  if (!tint) {
    PRInt32 nc = 0;

    // If we get here, the transaction must be a dummy batch transaction
    // created by BeginBatch(). If it contains no children, get rid of it!

    tx->GetNumberOfChildren(&nc);

    if (!nc) {
      delete tx;
      return result;
    }
  }

  // Check if the transaction is transient. If it is, there's nothing
  // more to do, just return.

  PRBool isTransient = PR_FALSE;

  if (tint)
    result = tint->GetIsTransient(&isTransient);

  if (NS_FAILED(result) || isTransient || !mMaxTransactionCount) {
    // XXX: Should we be clearing the redo stack if the transaction
    //      is transient and there is nothing on the do stack?
    delete tx;
    return result;
  }

  nsTransactionItem *top = 0;

  // Check if there is a transaction on the do stack. If there is,
  // the current transaction is a "sub" transaction, and should
  // be added to the transaction at the top of the do stack.

  result = mDoStack.Peek(&top);
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

  top = 0;
  result = mUndoStack.Peek(&top);

  if (tint && top) {
    PRBool didMerge = PR_FALSE;
    nsITransaction *topTransaction = 0;

    result = top->GetTransaction(&topTransaction);

    if (topTransaction) {
      result = topTransaction->Merge(&didMerge, tint);

      if (NS_FAILED(result)) {
        // XXX: What do we do if this fails?
      }

      if (didMerge) {
        delete tx;
        return result;
      }
    }
  }

  // Check to see if we've hit the max level of undo. If so,
  // pop the bottom transaction off the undo stack and release it!

  PRInt32 sz = 0;

  result = mUndoStack.GetSize(&sz);

  if (mMaxTransactionCount > 0 && sz >= mMaxTransactionCount) {
    nsTransactionItem *overflow = 0;

    result = mUndoStack.PopBottom(&overflow);

    // XXX: What do we do in the case where this fails?

    if (overflow)
      delete overflow;
  }

  // Push the transaction on the undo stack:

  result = mUndoStack.Push(tx);

  if (NS_FAILED(result)) {
    // XXX: What do we do in the case where a clear fails?
    //      Remove the transaction from the stack, and release it?
  }

  return result;
}

