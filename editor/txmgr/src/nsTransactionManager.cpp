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
#include "COM_auto_ptr.h"

#define LOCK_TX_MANAGER(mgr)
#define UNLOCK_TX_MANAGER(mgr)

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);

nsTransactionManager::nsTransactionManager(PRInt32 aMaxLevelsOfUndo)
  : mMaxLevelsOfUndo(aMaxLevelsOfUndo)
{
}

nsTransactionManager::~nsTransactionManager()
{
}

NS_IMPL_ADDREF(nsTransactionManager)
NS_IMPL_RELEASE(nsTransactionManager)

nsresult
nsTransactionManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
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
  nsresult result = NS_OK;

  if (!aTransaction)
    return NS_ERROR_NULL_POINTER;

  LOCK_TX_MANAGER(this);

  result = aTransaction->Do();

  if (! NS_SUCCEEDED(result)) {
    UNLOCK_TX_MANAGER(this);
    return result;
  }

  // XXX: Check if we can coalesce this transaction with the one
  //      at the top of the undo stack. If we can, merge this one
  //      into the one currently on the stack.

  // XXX: Check to see if we've hit the max level of undo. If so,
  //      pop the bottom transaction of the undo stack and release it!

  NS_ADDREF(aTransaction);
  mUndoStack.Push(aTransaction);

  result = ClearRedoStack();

  if (! NS_SUCCEEDED(result)) {
    // XXX: What do we do in the case where a clear fails?
    //      Remove the transaction from the stack, and release it?
  }

  UNLOCK_TX_MANAGER(this);

  return result;
}

nsresult
nsTransactionManager::Undo()
{
  nsresult result    = NS_OK;
  nsITransaction *tx = 0;

  LOCK_TX_MANAGER(this);

  // Peek at the top of the undo stack. Don't remove the transaction
  // until it has successfully completed.
  result = mUndoStack.Peek(&tx);

  // Bail if there's nothing on the stack.
  if (!NS_SUCCEEDED(result) || !tx) {
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
  nsresult result    = NS_OK;
  nsITransaction *tx = 0;

  LOCK_TX_MANAGER(this);

  // Peek at the top of the redo stack. Don't remove the transaction
  // until it has successfully completed.
  result = mRedoStack.Peek(&tx);

  // Bail if there's nothing on the stack.
  if (!NS_SUCCEEDED(result) || !tx) {
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
nsTransactionManager::GetNumberOfUndoItems(PRInt32 *aNumItems)
{
  return mUndoStack.GetSize(aNumItems);
}

nsresult
nsTransactionManager::GetNumberOfRedoItems(PRInt32 *aNumItems)
{
  return mRedoStack.GetSize(aNumItems);
}

nsresult
nsTransactionManager::Write(nsIOutputStream *aOutputStream)
{
  PRInt32 len;

  if (!aOutputStream)
    return NS_ERROR_NULL_POINTER;

  aOutputStream->Write("UndoStack:\n\n", 0, 12, &len);
  mUndoStack.Write(aOutputStream);

  aOutputStream->Write("\nRedoStack:\n\n", 0, 13, &len);
  mRedoStack.Write(aOutputStream);

  return NS_OK;
}

nsresult
nsTransactionManager::AddListener(nsITransactionListener *aListener)
{
  // XXX: Need to add listener support.
  return NS_OK;
}

nsresult
nsTransactionManager::RemoveListener(nsITransactionListener *aListener)
{
  // XXX: Need to add listener support.
  return NS_OK;
}

nsresult
nsTransactionManager::ClearUndoStack()
{
  mUndoStack.Clear();
  return NS_OK;
}

nsresult
nsTransactionManager::ClearRedoStack()
{
  mRedoStack.Clear();
  return NS_OK;
}

