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

#include "nsTransactionItem.h"
#include "COM_auto_ptr.h"

nsTransactionItem::nsTransactionItem(nsITransaction *aTransaction)
    : mTransaction(aTransaction), mUndoStack(0), mRedoStack(0)
{
}

nsTransactionItem::~nsTransactionItem()
{
  if (mRedoStack)
    delete mRedoStack;

  if (mUndoStack)
    delete mUndoStack;

  NS_IF_RELEASE(mTransaction);
}

nsresult
nsTransactionItem::AddChild(nsTransactionItem *aTransactionItem)
{
  if (!aTransactionItem)
    return NS_ERROR_NULL_POINTER;

  if (!mUndoStack) {
    mUndoStack = new nsTransactionStack();
    if (!mUndoStack)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  mUndoStack->Push(aTransactionItem);

  return NS_OK;
}

nsresult
nsTransactionItem::GetTransaction(nsITransaction **aTransaction)
{
  if (!aTransaction)
    return NS_ERROR_NULL_POINTER;

  *aTransaction = mTransaction;

  return NS_OK;
}

nsresult
nsTransactionItem::Do()
{
  if (mTransaction)
    return mTransaction->Do();
  return NS_OK;
}

nsresult
nsTransactionItem::Undo()
{
  nsTransactionItem *item;
  nsresult result;
  PRInt32 sz = 0;

  if (mUndoStack) {
    if (!mRedoStack && mUndoStack) {
      mRedoStack = new nsTransactionRedoStack();
      if (!mRedoStack)
        return NS_ERROR_OUT_OF_MEMORY;
    }

    /* Undo all of the transaction items children! */
    result = mUndoStack->GetSize(&sz);

    if (!NS_SUCCEEDED(result))
      return result;

    while (sz-- > 0) {
      result = mUndoStack->Peek(&item);

      if (!NS_SUCCEEDED(result)) {
        RecoverFromUndo();
        return result;
      }

      result = item->Undo();

      if (!NS_SUCCEEDED(result)) {
        RecoverFromUndo();
        return result;
      }

      result = mUndoStack->Pop(&item);

      if (!NS_SUCCEEDED(result)) {
        RecoverFromUndo();
        return result;
      }

      result = mRedoStack->Push(item);

      if (!NS_SUCCEEDED(result)) {
        /* If we got an error here, I doubt we can recover! */
        RecoverFromUndo();
        return result;
      }
    }
  }

  if (!mTransaction)
    return NS_OK;

  result = mTransaction->Undo();

  if (!NS_SUCCEEDED(result)) {
    RecoverFromUndo();
    return result;
  }

  return NS_OK;
}

nsresult
nsTransactionItem::Redo()
{
  nsTransactionItem *item;
  nsresult result;
  PRInt32 sz = 0;

  if (!mTransaction)
    return NS_OK;

  result = mTransaction->Redo();

  if (!NS_SUCCEEDED(result))
    return result;

  if (!mRedoStack)
    return NS_OK;

  /* Redo all of the transaction items children! */
  result = mRedoStack->GetSize(&sz);

  if (!NS_SUCCEEDED(result)) {
    RecoverFromRedo();
    return result;
  }

  while (sz-- > 0) {
    result = mRedoStack->Peek(&item);

    if (!NS_SUCCEEDED(result)) {
      RecoverFromRedo();
      return result;
    }

    result = item->Redo();

    if (!NS_SUCCEEDED(result)) {
      RecoverFromRedo();
      return result;
    }

    result = mRedoStack->Pop(&item);

    if (!NS_SUCCEEDED(result)) {
      RecoverFromRedo();
      return result;
    }

    result = mUndoStack->Push(item);

    if (!NS_SUCCEEDED(result)) {
      /* If we got an error here, I doubt we can recover! */
      RecoverFromRedo();
      return result;
    }
  }

  return NS_OK;
}

nsresult
nsTransactionItem::GetNumberOfUndoItems(PRInt32 *aNumItems)
{
  return mUndoStack->GetSize(aNumItems);
}

nsresult
nsTransactionItem::GetNumberOfRedoItems(PRInt32 *aNumItems)
{
  return mRedoStack->GetSize(aNumItems);
}

nsresult
nsTransactionItem::Write(nsIOutputStream *aOutputStream)
{
  PRInt32 len;

  if (mTransaction)
    mTransaction->Write(aOutputStream);

  aOutputStream->Write("    ItemUndoStack:\n", 0, 19, &len);
  if (mUndoStack) {
    mUndoStack->Write(aOutputStream);
  }

  aOutputStream->Write("\n    ItemRedoStack:\n", 0, 20, &len);
  if (mRedoStack) {
    mRedoStack->Write(aOutputStream);
  }
  aOutputStream->Write("\n", 0, 1, &len);

  return NS_OK;
}

nsresult
nsTransactionItem::RecoverFromUndo(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsTransactionItem::RecoverFromRedo(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


