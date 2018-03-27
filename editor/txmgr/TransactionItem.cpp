/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransactionItem.h"

#include "mozilla/mozalloc.h"
#include "mozilla/TransactionManager.h"
#include "mozilla/TransactionStack.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsImpl.h"
#include "nsITransaction.h"

namespace mozilla {

TransactionItem::TransactionItem(nsITransaction* aTransaction)
  : mTransaction(aTransaction)
  , mUndoStack(0)
  , mRedoStack(0)
{
}

TransactionItem::~TransactionItem()
{
  delete mRedoStack;
  delete mUndoStack;
}

void
TransactionItem::CleanUp()
{
  mData.Clear();
  mTransaction = nullptr;
  if (mRedoStack) {
    mRedoStack->DoUnlink();
  }
  if (mUndoStack) {
    mUndoStack->DoUnlink();
  }
}

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(TransactionItem)
NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE_WITH_LAST_RELEASE(TransactionItem,
                                                          CleanUp())

NS_IMPL_CYCLE_COLLECTION_CLASS(TransactionItem)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TransactionItem)
  tmp->CleanUp();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TransactionItem)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mData)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
  if (tmp->mRedoStack) {
    tmp->mRedoStack->DoTraverse(cb);
  }
  if (tmp->mUndoStack) {
    tmp->mUndoStack->DoTraverse(cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(TransactionItem, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(TransactionItem, Release)

nsresult
TransactionItem::AddChild(TransactionItem* aTransactionItem)
{
  NS_ENSURE_TRUE(aTransactionItem, NS_ERROR_NULL_POINTER);

  if (!mUndoStack) {
    mUndoStack = new TransactionStack(TransactionStack::FOR_UNDO);
  }

  mUndoStack->Push(aTransactionItem);
  return NS_OK;
}

already_AddRefed<nsITransaction>
TransactionItem::GetTransaction()
{
  nsCOMPtr<nsITransaction> txn = mTransaction;
  return txn.forget();
}

nsresult
TransactionItem::GetIsBatch(bool* aIsBatch)
{
  NS_ENSURE_TRUE(aIsBatch, NS_ERROR_NULL_POINTER);
  *aIsBatch = !mTransaction;
  return NS_OK;
}

nsresult
TransactionItem::GetNumberOfChildren(int32_t* aNumChildren)
{
  NS_ENSURE_TRUE(aNumChildren, NS_ERROR_NULL_POINTER);

  *aNumChildren = 0;

  int32_t ui = 0;
  nsresult rv = GetNumberOfUndoItems(&ui);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t ri = 0;
  rv = GetNumberOfRedoItems(&ri);
  NS_ENSURE_SUCCESS(rv, rv);

  *aNumChildren = ui + ri;
  return NS_OK;
}

nsresult
TransactionItem::GetChild(int32_t aIndex,
                          TransactionItem** aChild)
{
  NS_ENSURE_TRUE(aChild, NS_ERROR_NULL_POINTER);

  *aChild = 0;

  int32_t numItems = 0;
  nsresult rv = GetNumberOfChildren(&numItems);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex < 0 || aIndex >= numItems) {
    return NS_ERROR_FAILURE;
  }

  // Children are expected to be in the order they were added,
  // so the child first added would be at the bottom of the undo
  // stack, or if there are no items on the undo stack, it would
  // be at the top of the redo stack.
  rv = GetNumberOfUndoItems(&numItems);
  NS_ENSURE_SUCCESS(rv, rv);

  if (numItems > 0 && aIndex < numItems) {
    NS_ENSURE_TRUE(mUndoStack, NS_ERROR_FAILURE);

    RefPtr<TransactionItem> child = mUndoStack->GetItem(aIndex);
    child.forget(aChild);
    return *aChild ? NS_OK : NS_ERROR_FAILURE;
  }

  // Adjust the index for the redo stack:
  aIndex -=  numItems;

  rv = GetNumberOfRedoItems(&numItems);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(mRedoStack && numItems != 0 && aIndex < numItems, NS_ERROR_FAILURE);

  RefPtr<TransactionItem> child = mRedoStack->GetItem(aIndex);
  child.forget(aChild);
  return *aChild ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
TransactionItem::DoTransaction()
{
  if (mTransaction) {
    return mTransaction->DoTransaction();
  }
  return NS_OK;
}

nsresult
TransactionItem::UndoTransaction(TransactionManager* aTransactionManager)
{
  nsresult rv = UndoChildren(aTransactionManager);
  if (NS_FAILED(rv)) {
    RecoverFromUndoError(aTransactionManager);
    return rv;
  }

  if (!mTransaction) {
    return NS_OK;
  }

  rv = mTransaction->UndoTransaction();
  if (NS_FAILED(rv)) {
    RecoverFromUndoError(aTransactionManager);
    return rv;
  }

  return NS_OK;
}

nsresult
TransactionItem::UndoChildren(TransactionManager* aTransactionManager)
{
  if (mUndoStack) {
    if (!mRedoStack && mUndoStack) {
      mRedoStack = new TransactionStack(TransactionStack::FOR_REDO);
    }

    /* Undo all of the transaction items children! */
    int32_t sz = mUndoStack->GetSize();

    nsresult rv = NS_OK;
    while (sz-- > 0) {
      RefPtr<TransactionItem> transactionItem = mUndoStack->Peek();
      if (!transactionItem) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsITransaction> transaction = transactionItem->GetTransaction();
      bool doInterrupt = false;
      rv = aTransactionManager->WillUndoNotify(transaction, &doInterrupt);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (doInterrupt) {
        return NS_OK;
      }

      rv = transactionItem->UndoTransaction(aTransactionManager);
      if (NS_SUCCEEDED(rv)) {
        transactionItem = mUndoStack->Pop();
        mRedoStack->Push(transactionItem.forget());
      }

      nsresult rv2 = aTransactionManager->DidUndoNotify(transaction, rv);
      if (NS_SUCCEEDED(rv)) {
        rv = rv2;
      }
    }
    // XXX NS_OK if there is no Undo items or all methods work fine, otherwise,
    //     the result of the last item's UndoTransaction() or
    //     DidUndoNotify() if UndoTransaction() succeeded.
    return rv;
  }

  return NS_OK;
}

nsresult
TransactionItem::RedoTransaction(TransactionManager* aTransactionManager)
{
  nsCOMPtr<nsITransaction> transaction(mTransaction);
  if (transaction) {
    nsresult rv = transaction->RedoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsresult rv = RedoChildren(aTransactionManager);
  if (NS_FAILED(rv)) {
    RecoverFromRedoError(aTransactionManager);
    return rv;
  }

  return NS_OK;
}

nsresult
TransactionItem::RedoChildren(TransactionManager* aTransactionManager)
{
  if (!mRedoStack) {
    return NS_OK;
  }

  /* Redo all of the transaction items children! */
  int32_t sz = mRedoStack->GetSize();

  nsresult rv = NS_OK;
  while (sz-- > 0) {
    RefPtr<TransactionItem> transactionItem = mRedoStack->Peek();
    if (!transactionItem) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsITransaction> transaction = transactionItem->GetTransaction();
    bool doInterrupt = false;
    rv = aTransactionManager->WillRedoNotify(transaction, &doInterrupt);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (doInterrupt) {
      return NS_OK;
    }

    rv = transactionItem->RedoTransaction(aTransactionManager);
    if (NS_SUCCEEDED(rv)) {
      transactionItem = mRedoStack->Pop();
      mUndoStack->Push(transactionItem.forget());
    }

    // XXX Shouldn't this DidRedoNotify()? (bug 1311626)
    nsresult rv2 = aTransactionManager->DidUndoNotify(transaction, rv);
    if (NS_SUCCEEDED(rv)) {
      rv = rv2;
    }
  }
  // XXX NS_OK if there is no Redo items or all methods work fine, otherwise,
  //     the result of the last item's RedoTransaction() or
  //     DidUndoNotify() if UndoTransaction() succeeded.
  return rv;
}

nsresult
TransactionItem::GetNumberOfUndoItems(int32_t* aNumItems)
{
  NS_ENSURE_TRUE(aNumItems, NS_ERROR_NULL_POINTER);

  if (!mUndoStack) {
    *aNumItems = 0;
    return NS_OK;
  }

  *aNumItems = mUndoStack->GetSize();
  return *aNumItems ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
TransactionItem::GetNumberOfRedoItems(int32_t* aNumItems)
{
  NS_ENSURE_TRUE(aNumItems, NS_ERROR_NULL_POINTER);

  if (!mRedoStack) {
    *aNumItems = 0;
    return NS_OK;
  }

  *aNumItems = mRedoStack->GetSize();
  return *aNumItems ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
TransactionItem::RecoverFromUndoError(TransactionManager* aTransactionManager)
{
  // If this method gets called, we never got to the point where we
  // successfully called UndoTransaction() for the transaction item itself.
  // Just redo any children that successfully called undo!
  return RedoChildren(aTransactionManager);
}

nsresult
TransactionItem::RecoverFromRedoError(TransactionManager* aTransactionManager)
{
  // If this method gets called, we already successfully called
  // RedoTransaction() for the transaction item itself. Undo all
  // the children that successfully called RedoTransaction(),
  // then undo the transaction item itself.
  nsresult rv = UndoChildren(aTransactionManager);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mTransaction) {
    return NS_OK;
  }

  return mTransaction->UndoTransaction();
}

} // namespace mozilla
