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

#ifndef nsITransactionListener_h__
#define nsITransactionListener_h__

#include "nsISupports.h"
#include "nsITransaction.h"
#include "nsITransactionManager.h"

class nsITransactionManager;

/*
Transaction Listener interface to outside world
*/

#define NS_ITRANSACTIONLISTENER_IID \
{ /* 58E330C4-7B48-11d2-98B9-00805F297D89 */ \
0x58e330c4, 0x7b48, 0x11d2, \
{ 0x98, 0xb9, 0x0, 0x80, 0x5f, 0x29, 0x7d, 0x89 } }

/**
 * A transaction listener specific interface. 
 * <P>
 * It's implemented by an object that tracks transactions.
 */
class nsITransactionListener : public nsISupports {
public:

  /**
   * Called when a transaction manager is doing a transaction.
   * @param aContinue if true, transaction manager continues normal processing.
   *                  if false, transaction manager discontinues processing.
   * @param aManager the transaction manager doing the transaction.
   * @param aTransaction the transaction being done.
   */
  virtual nsresult Do(PRBool *aContinue,
                      nsITransactionManager *aManager,
                      nsITransaction *aTransaction) = 0;

  /**
   * Called when a transaction manager is undoing a transaction.
   * @param aContinue if true, transaction manager continues normal processing.
   *                  if false, transaction manager discontinues processing.
   * @param aManager the transaction manager undoing the transaction.
   * @param aTransaction the transaction being undone.
   */
  virtual nsresult Undo(PRBool *aContinue,
                        nsITransactionManager *aManager,
                        nsITransaction *aTransaction) = 0;

  /**
   * Called when a transaction manager is redoing a transaction.
   * @param aContinue if true, transaction manager continues normal processing.
   *                  if false, transaction manager discontinues processing.
   * @param aManager the transaction manager redoing the transaction.
   * @param aTransaction the transaction being redone.
   */
  virtual nsresult Redo(PRBool *aContinue,
                        nsITransactionManager *aManager,
                        nsITransaction *aTransaction) = 0;

  /* XXX: We should probably add pruning notification methods. */
};

#endif // nsITransactionListener_h__

