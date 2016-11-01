/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsImpl.h"
#include "nsITransaction.h"
#include "nsTransactionItem.h"
#include "nsTransactionManager.h"
#include "nsTransactionStack.h"

nsTransactionItem::nsTransactionItem(nsITransaction *aTransaction)
    : mTransaction(aTransaction), mUndoStack(0), mRedoStack(0)
{
}

nsTransactionItem::~nsTransactionItem()
{
  delete mRedoStack;
  delete mUndoStack;
}

void
nsTransactionItem::CleanUp()
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

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(nsTransactionItem)
NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE_WITH_LAST_RELEASE(nsTransactionItem,
                                                          CleanUp())

NS_IMPL_CYCLE_COLLECTION_CLASS(nsTransactionItem)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsTransactionItem)
  tmp->CleanUp();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsTransactionItem)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mData)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
  if (tmp->mRedoStack) {
    tmp->mRedoStack->DoTraverse(cb);
  }
  if (tmp->mUndoStack) {
    tmp->mUndoStack->DoTraverse(cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsTransactionItem, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsTransactionItem, Release)

nsresult
nsTransactionItem::AddChild(nsTransactionItem *aTransactionItem)
{
  NS_ENSURE_TRUE(aTransactionItem, NS_ERROR_NULL_POINTER);

  if (!mUndoStack) {
    mUndoStack = new nsTransactionStack(nsTransactionStack::FOR_UNDO);
  }

  mUndoStack->Push(aTransactionItem);
  return NS_OK;
}

already_AddRefed<nsITransaction>
nsTransactionItem::GetTransaction()
{
  nsCOMPtr<nsITransaction> txn = mTransaction;
  return txn.forget();
}

nsresult
nsTransactionItem::GetIsBatch(bool *aIsBatch)
{
  NS_ENSURE_TRUE(aIsBatch, NS_ERROR_NULL_POINTER);
  *aIsBatch = !mTransaction;
  return NS_OK;
}

nsresult
nsTransactionItem::GetNumberOfChildren(int32_t *aNumChildren)
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
nsTransactionItem::GetChild(int32_t aIndex, nsTransactionItem **aChild)
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

    RefPtr<nsTransactionItem> child = mUndoStack->GetItem(aIndex);
    child.forget(aChild);
    return *aChild ? NS_OK : NS_ERROR_FAILURE;
  }

  // Adjust the index for the redo stack:
  aIndex -=  numItems;

  rv = GetNumberOfRedoItems(&numItems);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(mRedoStack && numItems != 0 && aIndex < numItems, NS_ERROR_FAILURE);

  RefPtr<nsTransactionItem> child = mRedoStack->GetItem(aIndex);
  child.forget(aChild);
  return *aChild ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsTransactionItem::DoTransaction()
{
  if (mTransaction) {
    return mTransaction->DoTransaction();
  }
  return NS_OK;
}

nsresult
nsTransactionItem::UndoTransaction(nsTransactionManager *aTxMgr)
{
  nsresult rv = UndoChildren(aTxMgr);
  if (NS_FAILED(rv)) {
    RecoverFromUndoError(aTxMgr);
    return rv;
  }

  if (!mTransaction) {
    return NS_OK;
  }

  rv = mTransaction->UndoTransaction();
  if (NS_FAILED(rv)) {
    RecoverFromUndoError(aTxMgr);
    return rv;
  }

  return NS_OK;
}

nsresult
nsTransactionItem::UndoChildren(nsTransactionManager *aTxMgr)
{
  if (mUndoStack) {
    if (!mRedoStack && mUndoStack) {
      mRedoStack = new nsTransactionStack(nsTransactionStack::FOR_REDO);
    }

    /* Undo all of the transaction items children! */
    int32_t sz = mUndoStack->GetSize();

    nsresult rv = NS_OK;
    while (sz-- > 0) {
      RefPtr<nsTransactionItem> item = mUndoStack->Peek();
      if (!item) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsITransaction> t = item->GetTransaction();
      bool doInterrupt = false;
      rv = aTxMgr->WillUndoNotify(t, &doInterrupt);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (doInterrupt) {
        return NS_OK;
      }

      rv = item->UndoTransaction(aTxMgr);
      if (NS_SUCCEEDED(rv)) {
        item = mUndoStack->Pop();
        mRedoStack->Push(item.forget());
      }

      nsresult rv2 = aTxMgr->DidUndoNotify(t, rv);
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
nsTransactionItem::RedoTransaction(nsTransactionManager *aTxMgr)
{
  nsCOMPtr<nsITransaction> transaction(mTransaction);
  if (transaction) {
    nsresult rv = transaction->RedoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsresult rv = RedoChildren(aTxMgr);
  if (NS_FAILED(rv)) {
    RecoverFromRedoError(aTxMgr);
    return rv;
  }

  return NS_OK;
}

nsresult
nsTransactionItem::RedoChildren(nsTransactionManager *aTxMgr)
{
  if (!mRedoStack) {
    return NS_OK;
  }

  /* Redo all of the transaction items children! */
  int32_t sz = mRedoStack->GetSize();

  nsresult rv = NS_OK;
  while (sz-- > 0) {
    RefPtr<nsTransactionItem> item = mRedoStack->Peek();
    if (!item) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsITransaction> t = item->GetTransaction();
    bool doInterrupt = false;
    rv = aTxMgr->WillRedoNotify(t, &doInterrupt);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (doInterrupt) {
      return NS_OK;
    }

    rv = item->RedoTransaction(aTxMgr);
    if (NS_SUCCEEDED(rv)) {
      item = mRedoStack->Pop();
      mUndoStack->Push(item.forget());
    }

    // XXX Shouldn't this DidRedoNotify()? (bug 1311626)
    nsresult rv2 = aTxMgr->DidUndoNotify(t, rv);
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
nsTransactionItem::GetNumberOfUndoItems(int32_t *aNumItems)
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
nsTransactionItem::GetNumberOfRedoItems(int32_t *aNumItems)
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
nsTransactionItem::RecoverFromUndoError(nsTransactionManager *aTxMgr)
{
  // If this method gets called, we never got to the point where we
  // successfully called UndoTransaction() for the transaction item itself.
  // Just redo any children that successfully called undo!
  return RedoChildren(aTxMgr);
}

nsresult
nsTransactionItem::RecoverFromRedoError(nsTransactionManager *aTxMgr)
{
  // If this method gets called, we already successfully called
  // RedoTransaction() for the transaction item itself. Undo all
  // the children that successfully called RedoTransaction(),
  // then undo the transaction item itself.
  nsresult rv = UndoChildren(aTxMgr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mTransaction) {
    return NS_OK;
  }

  return mTransaction->UndoTransaction();
}

