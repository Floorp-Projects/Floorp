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

#ifndef nsITransactionManager_h__
#define nsITransactionManager_h__

#include "nsISupports.h"
#include "nsIOutputStream.h"
#include "nsITransaction.h"
#include "nsITransactionListener.h"

/*
Transaction Manager interface to outside world
*/

#define NS_ITRANSACTIONMANAGER_IID \
{ /* 58E330C2-7B48-11d2-98B9-00805F297D89 */ \
0x58e330c2, 0x7b48, 0x11d2, \
{ 0x98, 0xb9, 0x0, 0x80, 0x5f, 0x29, 0x7d, 0x89 } }; 


/**
 * A transaction manager specific interface. 
 * <P>
 * It's implemented by an object that tracks transactions.
 */
class nsITransactionManager  : public nsISupports{
public:

  /**
   * Calls a transaction's Do() method, then pushes it on the undo stack.
   * <P>
   * This method calls the transaction's AddRef() method.
   * The transaction's Release() method will be called when the undo or redo
   * stack is pruned or when the transaction manager is destroyed.
   * @param aTransaction the transaction to do.
   */
  virtual nsresult Do(nsITransaction *aTransaction) = 0;

  /**
   * Pops the topmost transaction on the undo stack, calls it's Undo() method,
   * then pushes it on the redo stack.
   */
  virtual nsresult Undo(void) = 0;

  /**
   * Pops the topmost transaction on the redo stack, calls it's Redo() method,
   * then pushes it on the undo stack.
   */
  virtual nsresult Redo(void) = 0;

  /**
   * Clears the undo and redo stacks.
   */
  virtual nsresult Clear(void) = 0;

  /**
   * Returns the number of items on the undo stack.
   * @param aNumItems will contain number of items.
   */
  virtual nsresult GetNumberOfUndoItems(PRInt32 *aNumItems) = 0;

  /**
   * Returns the number of items on the redo stack.
   * @param aNumItems will contain number of items.
   */
  virtual nsresult GetNumberOfRedoItems(PRInt32 *aNumItems) = 0;

  /**
   * Returns a pointer to the transaction at the top of the undo stack.
   * @param aTransaction will contain pointer to the transaction.
   */
  virtual nsresult PeekUndoStack(nsITransaction **aTransaction) = 0;

  /**
   * Returns a pointer to the transaction at the top of the redo stack.
   * @param aTransaction will contain pointer to the transaction.
   */
  virtual nsresult PeekRedoStack(nsITransaction **aTransaction) = 0;

  /**
   * Writes a stream representation of the transaction manager and it's
   * execution stacks. Calls the Write() method of each transaction on the
   * execution stacks.
   * @param aOutputStream the stream to write to.
   */
  virtual nsresult Write(nsIOutputStream *aOutputStream) = 0;

  /**
   * Adds a listener to the transaction manager's notification list. Listeners
   * are notified whenever a transaction is done, undone, or redone.
   * <P>
   * The listener's AddRef() method is called.
   * @param aListener the lister to add.
   */
  virtual nsresult AddListener(nsITransactionListener *aListener) = 0;

  /**
   * Removes a listener from the transaction manager's notification list.
   * <P>
   * The listener's Release() method is called.
   * @param aListener the lister to remove.
   */
  virtual nsresult RemoveListener(nsITransactionListener *aListener) = 0;
};

#endif // nsITransactionManager_h__

