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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);

nsTransactionManager::nsTransactionManager(PRInt32 aMaxLevelsOfUndo)
  : mMaxLevelsOfUndo(aMaxLevelsOfUndo), mRF(), mUndoStack(mRF), mRedoStack(mRF)
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

  // XXX: LOCK the transaction manager

  result = aTransaction->Do();

  if (! NS_SUCCEEDED(result)) {
    // XXX: UNLOCK the transaction manager
    return result;
  }

  // XXX: Check if we can coalesce this transaction with the one
  //      at the top of the undo stack. If we can, merge this one
  //      into the one currently on the stack.

  // XXX: Check to see if we've hit the max level of undo. If so,
  //      pop the bottom transaction of the undo stack and release it!

  NS_ADDREF(aTransaction);
  mUndoStack.Push(aTransaction);

  result = PruneRedoStack();

  if (! NS_SUCCEEDED(result)) {
    // XXX: What do we do in the case where a prune fails?
    //      Remove the transaction from the stack, and release it?
  }

  // XXX: UNLOCK the transaction manager

  return result;
}

nsresult
nsTransactionManager::Undo()
{
  nsresult result    = NS_OK;
  nsITransaction *tx = 0;

  // XXX: LOCK the transaction manager

  // Peek at the top of the undo stack. Don't remove the transaction
  // until it has successfully completed.
  tx = (nsITransaction *)(--mUndoStack.End());

  // Bail if there's nothing on the stack.
  if (!tx) {
    // XXX: UNLOCK the transaction manager
    return NS_OK;
  }

  result = tx->Undo();

  if (NS_SUCCEEDED(result)) {
    mRedoStack.Push(mUndoStack.Pop());
  }

  // XXX: UNLOCK the transaction manager

  return result;
}

nsresult
nsTransactionManager::Redo()
{
  nsresult result    = NS_OK;
  nsITransaction *tx = 0;

  // XXX: LOCK the transaction manager

  // Peek at the top of the redo stack. Don't remove the transaction
  // until it has successfully completed.
  tx = (nsITransaction *)(--mRedoStack.End());

  // Bail if there's nothing on the stack.
  if (!tx) {
    // XXX: UNLOCK the transaction manager
    return NS_OK;
  }

  result = tx->Redo();

  if (NS_SUCCEEDED(result)) {
    mUndoStack.Push(mRedoStack.Pop());
  }

  // XXX: UNLOCK the transaction manager

  return result;
}

nsresult
nsTransactionManager::GetNumberOfUndoItems(PRInt32 *aNumItems)
{
  if (aNumItems)
    *aNumItems = 0;

  *aNumItems = mUndoStack.GetSize();

  return NS_OK;
}

nsresult
nsTransactionManager::GetNumberOfRedoItems(PRInt32 *aNumItems)
{
  if (aNumItems)
    *aNumItems = 0;

  *aNumItems = mRedoStack.GetSize();

  return NS_OK;
}

nsresult
nsTransactionManager::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult
nsTransactionManager::AddListener(nsITransactionListener *aListener)
{
  return NS_OK;
}

nsresult
nsTransactionManager::RemoveListener(nsITransactionListener *aListener)
{
  return NS_OK;
}

nsresult
nsTransactionManager::PruneRedoStack()
{
  // XXX: The order in which you release things off this stack matters!
  //      We may not be able to use Erase().
  mRedoStack.Erase();
  return NS_OK;
}

