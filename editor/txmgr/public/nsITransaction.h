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

#ifndef nsITransaction_h__
#define nsITransaction_h__

#include "nsISupports.h"
#include "nsIOutputStream.h"
#include "nsString.h"

/*
Transaction interface to outside world
*/

#define NS_ITRANSACTION_IID \
{ /* 58E330C1-7B48-11d2-98B9-00805F297D89 */ \
0x58e330c1, 0x7b48, 0x11d2, \
{ 0x98, 0xb9, 0x0, 0x80, 0x5f, 0x29, 0x7d, 0x89 } }


/**
 * A transaction specific interface. 
 * <P>
 * It's implemented by an object that executes some behavior that must be
 * tracked by the transaction manager.
 */
class nsITransaction  : public nsISupports{
public:

  /**
   * Executes the transaction.
   */
  virtual nsresult Do(void) = 0;

  /**
   * Restores the state to what it was before the transaction was executed.
   */
  virtual nsresult Undo(void) = 0;

  /**
   * Executes the transaction again. Can only be called on a transaction that
   * was previously undone.
   * <P>
   * In most cases, the Redo() method will actually call the Do() method to
   * execute the transaction again.
   */
  virtual nsresult Redo(void) = 0;

  /**
   * Attempts to merge a transaction into "this" transaction. Both transactions
   * must be in their undo state, Do() methods already executed. The transaction
   * manager calls this method to coalesce a new transaction with the
   * transaction on the top of the undo stack.
   * @param aDidMerge will contain merge result. True if transactions were
   * merged successfully. False if merge is not possible or failed. If true,
   * the transaction manager will Release() the new transacton instead of
   * pushing it on the undo stack.
   * @param aTransaction the previously executed transaction to merge.
   */
  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction) = 0;

  /**
   * Write a stream representation of the current state of the transaction.
   * @param aOutputStream the stream to write to.
   */
  virtual nsresult Write(nsIOutputStream *aOutputStream) = 0;

  /**
   * Returns the string to display for the undo menu item.
   * @param aString will point to string to display.
   */
  virtual nsresult GetUndoString(nsString **aString) = 0;

  /**
   * Returns the string to display for the redo menu item.
   * @param aString will point to string to display.
   */
  virtual nsresult GetRedoString(nsString **aString) = 0;
};

#endif // nsITransaction_h__

