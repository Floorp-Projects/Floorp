/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsITransaction.h"
#include "nsITransactionListener.h"

#include "nsTransactionItem.h"
#include "nsTransactionStack.h"
#include "nsVoidArray.h"
#include "nsTransactionManager.h"
#include "nsTransactionList.h"

#include "nsCOMPtr.h"

#define LOCK_TX_MANAGER(mgr)    (mgr)->Lock()
#define UNLOCK_TX_MANAGER(mgr)  (mgr)->Unlock()

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);

nsTransactionManager::nsTransactionManager(PRInt32 aMaxTransactionCount)
  : mMaxTransactionCount(aMaxTransactionCount), mListeners(0)
{
  NS_INIT_REFCNT();

  mMonitor = ::PR_NewMonitor();
}

nsTransactionManager::~nsTransactionManager()
{
  if (mListeners)
  {
    PRInt32 i;
    nsITransactionListener *listener;

    for (i = 0; i < mListeners->Count(); i++)
    {
      listener = (nsITransactionListener *)mListeners->ElementAt(i);
      NS_IF_RELEASE(listener);
    }

    delete mListeners;
    mListeners = 0;
  }

  if (mMonitor)
  {
    ::PR_DestroyMonitor(mMonitor);
    mMonitor = 0;
  }
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

NS_IMPL_QUERY_INTERFACE2(nsTransactionManager, nsITransactionManager, nsISupportsWeakReference)

#else

NS_IMPL_ISUPPORTS2(nsTransactionManager, nsITransactionManager, nsISupportsWeakReference)

#endif

NS_IMETHODIMP
nsTransactionManager::DoTransaction(nsITransaction *aTransaction)
{
  nsresult result;

  if (!aTransaction)
    return NS_ERROR_NULL_POINTER;

  LOCK_TX_MANAGER(this);

  PRBool doInterrupt = PR_FALSE;

  result = WillDoNotify(aTransaction, &doInterrupt);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (doInterrupt) {
    UNLOCK_TX_MANAGER(this);
    return NS_OK;
  }

  result = BeginTransaction(aTransaction);

  if (NS_FAILED(result)) {
    DidDoNotify(aTransaction, result);
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  result = EndTransaction();

  nsresult result2 = DidDoNotify(aTransaction, result);

  if (NS_SUCCEEDED(result))
    result = result2;

  UNLOCK_TX_MANAGER(this);

  return result;
}

NS_IMETHODIMP
nsTransactionManager::UndoTransaction()
{
  nsresult result       = NS_OK;
  nsTransactionItem *tx = 0;

  LOCK_TX_MANAGER(this);

  // It is illegal to call UndoTransaction() while the transaction manager is
  // executing a  transaction's DoTransaction() method! If this happens,
  // the UndoTransaction() request is ignored, and we return NS_ERROR_FAILURE.

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

  nsITransaction *t = 0;

  result = tx->GetTransaction(&t);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  PRBool doInterrupt = PR_FALSE;

  result = WillUndoNotify(t, &doInterrupt);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (doInterrupt) {
    UNLOCK_TX_MANAGER(this);
    return NS_OK;
  }

  result = tx->UndoTransaction(this);

  if (NS_SUCCEEDED(result)) {
    result = mUndoStack.Pop(&tx);

    if (NS_SUCCEEDED(result))
      result = mRedoStack.Push(tx);
  }

  nsresult result2 = DidUndoNotify(t, result);

  if (NS_SUCCEEDED(result))
    result = result2;

  UNLOCK_TX_MANAGER(this);

  return result;
}

NS_IMETHODIMP
nsTransactionManager::RedoTransaction()
{
  nsresult result       = NS_OK;
  nsTransactionItem *tx = 0;

  LOCK_TX_MANAGER(this);

  // It is illegal to call RedoTransaction() while the transaction manager is
  // executing a  transaction's DoTransaction() method! If this happens,
  // the RedoTransaction() request is ignored, and we return NS_ERROR_FAILURE.

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

  nsITransaction *t = 0;

  result = tx->GetTransaction(&t);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  PRBool doInterrupt = PR_FALSE;

  result = WillRedoNotify(t, &doInterrupt);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (doInterrupt) {
    UNLOCK_TX_MANAGER(this);
    return NS_OK;
  }

  result = tx->RedoTransaction(this);

  if (NS_SUCCEEDED(result)) {
    result = mRedoStack.Pop(&tx);

    if (NS_SUCCEEDED(result))
      result = mUndoStack.Push(tx);
  }

  nsresult result2 = DidRedoNotify(t, result);

  if (NS_SUCCEEDED(result))
    result = result2;

  UNLOCK_TX_MANAGER(this);

  return result;
}

NS_IMETHODIMP
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

NS_IMETHODIMP
nsTransactionManager::BeginBatch()
{
  nsresult result;

  // We can batch independent transactions together by simply pushing
  // a dummy transaction item on the do stack. This dummy transaction item
  // will be popped off the do stack, and then pushed on the undo stack
  // in EndBatch().

  LOCK_TX_MANAGER(this);

  PRBool doInterrupt = PR_FALSE;

  result = WillBeginBatchNotify(&doInterrupt);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (doInterrupt) {
    UNLOCK_TX_MANAGER(this);
    return NS_OK;
  }

  result = BeginTransaction(0);
  
  nsresult result2 = DidBeginBatchNotify(result);

  if (NS_SUCCEEDED(result))
    result = result2;

  UNLOCK_TX_MANAGER(this);

  return result;
}

NS_IMETHODIMP
nsTransactionManager::EndBatch()
{
  nsTransactionItem *tx = 0;
  nsITransaction *ti    = 0;
  nsresult result;

  LOCK_TX_MANAGER(this);

  // XXX: Need to add some mechanism to detect the case where the transaction
  //      at the top of the do stack isn't the dummy transaction, so we can
  //      throw an error!! This can happen if someone calls EndBatch() within
  //      the DoTransaction() method of a transaction.
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

  PRBool doInterrupt = PR_FALSE;

  result = WillEndBatchNotify(&doInterrupt);

  if (NS_FAILED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  if (doInterrupt) {
    UNLOCK_TX_MANAGER(this);
    return NS_OK;
  }

  result = EndTransaction();

  nsresult result2 = DidEndBatchNotify(result);

  if (NS_SUCCEEDED(result))
    result = result2;

  UNLOCK_TX_MANAGER(this);

  return result;
}

NS_IMETHODIMP
nsTransactionManager::GetNumberOfUndoItems(PRInt32 *aNumItems)
{
  nsresult result;

  LOCK_TX_MANAGER(this);
  result = mUndoStack.GetSize(aNumItems);
  UNLOCK_TX_MANAGER(this);

  return result;
}

NS_IMETHODIMP
nsTransactionManager::GetNumberOfRedoItems(PRInt32 *aNumItems)
{
  nsresult result;

  LOCK_TX_MANAGER(this);
  result = mRedoStack.GetSize(aNumItems);
  UNLOCK_TX_MANAGER(this);

  return result;
}

NS_IMETHODIMP
nsTransactionManager::GetMaxTransactionCount(PRInt32 *aMaxCount)
{
  if (!aMaxCount)
    return NS_ERROR_NULL_POINTER;

  LOCK_TX_MANAGER(this);
  *aMaxCount = mMaxTransactionCount;
  UNLOCK_TX_MANAGER(this);

  return NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::SetMaxTransactionCount(PRInt32 aMaxCount)
{
  PRInt32 numUndoItems  = 0, numRedoItems = 0, total = 0;
  nsTransactionItem *tx = 0;
  nsresult result;

  LOCK_TX_MANAGER(this);

  // It is illegal to call SetMaxTransactionCount() while the transaction
  // manager is executing a  transaction's DoTransaction() method because
  // the undo and redo stacks might get pruned! If this happens, the
  // SetMaxTransactionCount() request is ignored, and we return
  // NS_ERROR_FAILURE.

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

NS_IMETHODIMP
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

  NS_IF_ADDREF(*aTransaction);

  return result;
}

NS_IMETHODIMP
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

  NS_IF_ADDREF(*aTransaction);

  return result;
}

NS_IMETHODIMP
nsTransactionManager::GetUndoList(nsITransactionList **aTransactionList)
{
  if (!aTransactionList)
    return NS_ERROR_NULL_POINTER;

  *aTransactionList = (nsITransactionList *)new nsTransactionList(this, &mUndoStack);

  NS_IF_ADDREF(*aTransactionList);

  return (! *aTransactionList) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::GetRedoList(nsITransactionList **aTransactionList)
{
  if (!aTransactionList)
    return NS_ERROR_NULL_POINTER;

  *aTransactionList = (nsITransactionList *)new nsTransactionList(this, &mRedoStack);

  NS_IF_ADDREF(*aTransactionList);

  return (! *aTransactionList) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::AddListener(nsITransactionListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  LOCK_TX_MANAGER(this);

  if (!mListeners) {
    mListeners = new nsAutoVoidArray();

    if (!mListeners) {
      UNLOCK_TX_MANAGER(this);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!mListeners->AppendElement((void *)aListener)) {
    UNLOCK_TX_MANAGER(this);
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(aListener);

  UNLOCK_TX_MANAGER(this);

  return NS_OK;
}

NS_IMETHODIMP
nsTransactionManager::RemoveListener(nsITransactionListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  if (!mListeners)
    return NS_ERROR_FAILURE;

  LOCK_TX_MANAGER(this);

  if (!mListeners->RemoveElement((void *)aListener))
  {
    UNLOCK_TX_MANAGER(this);
    return NS_ERROR_FAILURE;
  }

  NS_IF_RELEASE(aListener);

  if (mListeners->Count() < 1)
  {
    delete mListeners;
    mListeners = 0;
  }

  UNLOCK_TX_MANAGER(this);

  return NS_OK;
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
nsTransactionManager::WillDoNotify(nsITransaction *aTransaction, PRBool *aInterrupt)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->WillDo(this, aTransaction, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidDoNotify(nsITransaction *aTransaction, nsresult aDoResult)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->DidDo(this, aTransaction, aDoResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillUndoNotify(nsITransaction *aTransaction, PRBool *aInterrupt)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->WillUndo(this, aTransaction, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidUndoNotify(nsITransaction *aTransaction, nsresult aUndoResult)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->DidUndo(this, aTransaction, aUndoResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillRedoNotify(nsITransaction *aTransaction, PRBool *aInterrupt)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->WillRedo(this, aTransaction, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidRedoNotify(nsITransaction *aTransaction, nsresult aRedoResult)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->DidRedo(this, aTransaction, aRedoResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillBeginBatchNotify(PRBool *aInterrupt)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->WillBeginBatch(this, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidBeginBatchNotify(nsresult aResult)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->DidBeginBatch(this, aResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillEndBatchNotify(PRBool *aInterrupt)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->WillEndBatch(this, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidEndBatchNotify(nsresult aResult)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->DidEndBatch(this, aResult);
    
    if (NS_FAILED(result))
      break;
  }

  return result;
}

nsresult
nsTransactionManager::WillMergeNotify(nsITransaction *aTop, nsITransaction *aTransaction, PRBool *aInterrupt)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->WillMerge(this, aTop, aTransaction, aInterrupt);
    
    if (NS_FAILED(result) || *aInterrupt)
      break;
  }

  return result;
}

nsresult
nsTransactionManager::DidMergeNotify(nsITransaction *aTop,
                                     nsITransaction *aTransaction,
                                     PRBool aDidMerge,
                                     nsresult aMergeResult)
{
  if (!mListeners)
    return NS_OK;

  nsresult result = NS_OK;
  PRInt32 i, lcount = mListeners->Count();

  for (i = 0; i < lcount; i++)
  {
    nsITransactionListener *listener = (nsITransactionListener *)mListeners->ElementAt(i);

    if (!listener)
      return NS_ERROR_FAILURE;

    result = listener->DidMerge(this, aTop, aTransaction, aDidMerge, aMergeResult);
    
    if (NS_FAILED(result))
      break;
  }

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

  result = tx->DoTransaction();

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

      PRBool doInterrupt = PR_FALSE;

      result = WillMergeNotify(topTransaction, tint, &doInterrupt);

      if (NS_FAILED(result))
        return result;

      if (!doInterrupt) {
        result = topTransaction->Merge(tint, &didMerge);

        nsresult result2 = DidMergeNotify(topTransaction, tint, didMerge, result);

        if (NS_SUCCEEDED(result))
          result = result2;

        if (NS_FAILED(result)) {
          // XXX: What do we do if this fails?
        }

        if (didMerge) {
          delete tx;
          return result;
        }
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

nsresult
nsTransactionManager::Lock()
{
  if (mMonitor)
    PR_EnterMonitor(mMonitor);

  return NS_OK;
}

nsresult
nsTransactionManager::Unlock()
{
  if (mMonitor)
    PR_ExitMonitor(mMonitor);

  return NS_OK;
}

