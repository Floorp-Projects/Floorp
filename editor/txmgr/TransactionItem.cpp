/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransactionItem.h"

#include "mozilla/mozalloc.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/TransactionManager.h"
#include "mozilla/TransactionStack.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsImpl.h"
#include "nsITransaction.h"

namespace mozilla {

TransactionItem::TransactionItem(nsITransaction* aTransaction)
    : mTransaction(aTransaction), mUndoStack(0), mRedoStack(0) {}

TransactionItem::~TransactionItem() {
  delete mRedoStack;
  delete mUndoStack;
}

void TransactionItem::CleanUp() {
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

nsresult TransactionItem::AddChild(TransactionItem& aTransactionItem) {
  if (!mUndoStack) {
    mUndoStack = new TransactionStack(TransactionStack::FOR_UNDO);
  }

  mUndoStack->Push(&aTransactionItem);
  return NS_OK;
}

already_AddRefed<nsITransaction> TransactionItem::GetTransaction() {
  return do_AddRef(mTransaction);
}

nsresult TransactionItem::DoTransaction() {
  if (!mTransaction) {
    return NS_OK;
  }
  OwningNonNull<nsITransaction> transaction = *mTransaction;
  nsresult rv = transaction->DoTransaction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsITransaction::DoTransaction() failed");
  return rv;
}

nsresult TransactionItem::UndoTransaction(
    TransactionManager* aTransactionManager) {
  nsresult rv = UndoChildren(aTransactionManager);
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionItem::UndoChildren() failed");
    DebugOnly<nsresult> rvIgnored = RecoverFromUndoError(aTransactionManager);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "TransactionItem::RecoverFromUndoError() failed");
    return rv;
  }

  if (!mTransaction) {
    return NS_OK;
  }

  OwningNonNull<nsITransaction> transaction = *mTransaction;
  rv = transaction->UndoTransaction();
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }

  NS_WARNING("TransactionItem::UndoTransaction() failed");
  DebugOnly<nsresult> rvIgnored = RecoverFromUndoError(aTransactionManager);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "TransactionItem::RecoverFromUndoError() failed");
  return rv;
}

nsresult TransactionItem::UndoChildren(
    TransactionManager* aTransactionManager) {
  if (!mUndoStack) {
    return NS_OK;
  }

  if (!mRedoStack) {
    mRedoStack = new TransactionStack(TransactionStack::FOR_REDO);
  }

  size_t undoStackSize = mUndoStack->GetSize();

  nsresult rv = NS_OK;
  while (undoStackSize-- > 0) {
    RefPtr<TransactionItem> transactionItem = mUndoStack->Peek();
    if (!transactionItem) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsITransaction> transaction = transactionItem->GetTransaction();
    bool doInterrupt = false;
    rv = aTransactionManager->WillUndoNotify(transaction, &doInterrupt);
    if (NS_FAILED(rv)) {
      NS_WARNING("TransactionManager::WillUndoNotify() failed");
      return rv;
    }
    if (doInterrupt) {
      return NS_OK;
    }

    rv = transactionItem->UndoTransaction(aTransactionManager);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TransactionItem::UndoTransaction() failed");
    if (NS_SUCCEEDED(rv)) {
      transactionItem = mUndoStack->Pop();
      mRedoStack->Push(transactionItem.forget());
    }

    nsresult rv2 = aTransactionManager->DidUndoNotify(transaction, rv);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2),
                         "TransactionManager::DidUndoNotify() failed");
    if (NS_SUCCEEDED(rv)) {
      rv = rv2;
    }
  }
  // XXX NS_OK if there is no Undo items or all methods work fine, otherwise,
  //     the result of the last item's UndoTransaction() or
  //     DidUndoNotify() if UndoTransaction() succeeded.
  return rv;
}

nsresult TransactionItem::RedoTransaction(
    TransactionManager* aTransactionManager) {
  nsCOMPtr<nsITransaction> transaction(mTransaction);
  if (transaction) {
    nsresult rv = transaction->RedoTransaction();
    if (NS_FAILED(rv)) {
      NS_WARNING("nsITransaction::RedoTransaction() failed");
      return rv;
    }
  }

  nsresult rv = RedoChildren(aTransactionManager);
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }

  NS_WARNING("TransactionItem::RedoChildren() failed");
  DebugOnly<nsresult> rvIgnored = RecoverFromRedoError(aTransactionManager);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "TransactionItem::RecoverFromRedoError() failed");
  return rv;
}

nsresult TransactionItem::RedoChildren(
    TransactionManager* aTransactionManager) {
  if (!mRedoStack) {
    return NS_OK;
  }

  /* Redo all of the transaction items children! */
  size_t redoStackSize = mRedoStack->GetSize();

  nsresult rv = NS_OK;
  while (redoStackSize-- > 0) {
    RefPtr<TransactionItem> transactionItem = mRedoStack->Peek();
    if (NS_WARN_IF(!transactionItem)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsITransaction> transaction = transactionItem->GetTransaction();
    bool doInterrupt = false;
    rv = aTransactionManager->WillRedoNotify(transaction, &doInterrupt);
    if (NS_FAILED(rv)) {
      NS_WARNING("TransactionManager::WillRedoNotify() failed");
      return rv;
    }
    if (doInterrupt) {
      return NS_OK;
    }

    rv = transactionItem->RedoTransaction(aTransactionManager);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TransactionItem::RedoTransaction() failed");
    if (NS_SUCCEEDED(rv)) {
      transactionItem = mRedoStack->Pop();
      mUndoStack->Push(transactionItem.forget());
    }

    // XXX Shouldn't this DidRedoNotify()? (bug 1311626)
    nsresult rv2 = aTransactionManager->DidUndoNotify(transaction, rv);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2),
                         "TransactionManager::DidUndoNotify() failed");
    if (NS_SUCCEEDED(rv)) {
      rv = rv2;
    }
  }
  // XXX NS_OK if there is no Redo items or all methods work fine, otherwise,
  //     the result of the last item's RedoTransaction() or
  //     DidUndoNotify() if UndoTransaction() succeeded.
  return rv;
}

size_t TransactionItem::NumberOfUndoItems() const {
  NS_WARNING_ASSERTION(!mUndoStack || mUndoStack->GetSize() > 0,
                       "UndoStack cannot have no children");
  return mUndoStack ? mUndoStack->GetSize() : 0;
}

size_t TransactionItem::NumberOfRedoItems() const {
  NS_WARNING_ASSERTION(!mRedoStack || mRedoStack->GetSize() > 0,
                       "UndoStack cannot have no children");
  return mRedoStack ? mRedoStack->GetSize() : 0;
}

nsresult TransactionItem::RecoverFromUndoError(
    TransactionManager* aTransactionManager) {
  // If this method gets called, we never got to the point where we
  // successfully called UndoTransaction() for the transaction item itself.
  // Just redo any children that successfully called undo!
  nsresult rv = RedoChildren(aTransactionManager);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TransactionItem::RedoChildren() failed");
  return rv;
}

nsresult TransactionItem::RecoverFromRedoError(
    TransactionManager* aTransactionManager) {
  // If this method gets called, we already successfully called
  // RedoTransaction() for the transaction item itself. Undo all
  // the children that successfully called RedoTransaction(),
  // then undo the transaction item itself.
  nsresult rv = UndoChildren(aTransactionManager);
  if (NS_FAILED(rv)) {
    NS_WARNING("TransactionItem::UndoChildren() failed");
    return rv;
  }

  if (!mTransaction) {
    return NS_OK;
  }

  OwningNonNull<nsITransaction> transaction = *mTransaction;
  rv = transaction->UndoTransaction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsITransaction::UndoTransaction() failed");
  return rv;
}

}  // namespace mozilla
