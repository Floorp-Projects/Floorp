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
#include "nsITransaction.h"
#include "nsIOutputStream.h"

/*
Transaction Manager interface to outside world
*/

#define NS_TRANSACTION_MANAGER_IID \
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
   * Execute() tells the implementation of nsITransactionManager to execute a transaction.
   * @param nsITransaction *tx the transaction to execute.
   */
  virtual nsresult Execute(nsITransaction *tx) = 0;

  /**
   * Undo() tells the implementation of nsITransactionManager to undo the specified number
   * of transactions.
   * @param PRInt32 n number of transactions to undo. n <= 0 means undo all transactions.
   */
  virtual nsresult Undo(PRInt32 n) = 0;

  /**
   * Redo() tells the implementation of nsITransactionManager to redo the specified number
   * of transactions.
   * @param PRInt32 n number of transactions to redo. n <= 0 means redo all transactions
   * previously undone.
   */
  virtual nsresult Redo(PRInt32 n) = 0;

  /**
   * Write() tells the implementation of nsITransactionManager to write a representation
   * of itself.
   * @param nsIOutputStream *
   */
  virtual nsresult Write(nsIOutputStream *) = 0;
};

#endif // nsITransactionManager

