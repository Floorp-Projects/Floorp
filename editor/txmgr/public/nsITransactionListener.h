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

class nsITransaction;
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

  static const nsIID& GetIID() { static nsIID iid = NS_ITRANSACTIONLISTENER_IID; return iid; }

  /**
   * Called before a transaction manager calls a transaction's
   * Do() method.
   * @param aManager the transaction manager doing the transaction.
   * @param aTransaction the transaction being done.
   * @result error status returned by the listener. NS_OK
   * should be used to indicate no error, proceed with normal control
   * flow. NS_COMFALSE can be returned by the listener to
   * indicate no error, interrupt normal control flow.
   */
  NS_IMETHOD WillDo(nsITransactionManager *aManager,
                    nsITransaction *aTransaction) = 0;

  /**
   * Called after a transaction manager calls the Do() method of
   * a transaction.
   * @param aManager the transaction manager that did the transaction.
   * @param aTransaction the transaction that was done.
   * @param aDoResult the nsresult returned after doing the transaction.
   * @result error status returned by the listener.
   */
  NS_IMETHOD DidDo(nsITransactionManager *aManager,
                   nsITransaction *aTransaction,
                   nsresult aDoResult) = 0;

  /**
   * Called before a transaction manager calls the Undo() method of
   * a transaction.
   * @param aManager the transaction manager undoing the transaction.
   * @param aTransaction the transaction being undone.
   * @result error status returned by the listener. NS_OK
   * should be used to indicate no error, proceed with normal control
   * flow. NS_COMFALSE can be returned by the listener to
   * indicate no error, interrupt normal control flow.
   */
  NS_IMETHOD WillUndo(nsITransactionManager *aManager,
                      nsITransaction *aTransaction) = 0;

  /**
   * Called after a transaction manager calls the Undo() method of
   * a transaction.
   * @param aManager the transaction manager undoing the transaction.
   * @param aTransaction the transaction being undone.
   * @param aUndoResult the nsresult returned after undoing the transaction.
   * @result error status returned by the listener.
   */
  NS_IMETHOD DidUndo(nsITransactionManager *aManager,
                     nsITransaction *aTransaction,
                     nsresult aUndoResult) = 0;

  /**
   * Called before a transaction manager calls the Redo() method of
   * a transaction.
   * @param aManager the transaction manager redoing the transaction.
   * @param aTransaction the transaction being redone.
   * @result error status returned by the listener. NS_OK
   * should be used to indicate no error, proceed with normal control
   * flow. NS_COMFALSE can be returned by the listener to
   * indicate no error, interrupt normal control flow.
   */
  NS_IMETHOD WillRedo(nsITransactionManager *aManager,
                      nsITransaction *aTransaction) = 0;

  /**
   * Called after a transaction manager calls the Redo() method of
   * a transaction.
   * @param aManager the transaction manager redoing the transaction.
   * @param aTransaction the transaction being redone.
   * @param aRedoResult the nsresult returned after redoing the transaction.
   * @result error status returned by the listener.
   */
  NS_IMETHOD DidRedo(nsITransactionManager *aManager,
                     nsITransaction *aTransaction,
                     nsresult aRedoResult) = 0;

  /**
   * Called before a transaction manager begins a batch.
   * @param aManager the transaction manager beginning a batch.
   * @result error status returned by the listener. NS_OK
   * should be used to indicate no error, proceed with normal control
   * flow. NS_COMFALSE can be returned by the listener to
   * indicate no error, interrupt normal control flow.
   */
  NS_IMETHOD WillBeginBatch(nsITransactionManager *aManager) = 0;

  /**
   * Called after a transaction manager begins a batch.
   * @param aManager the transaction manager that began a batch.
   * @param aResult the nsresult returned after beginning a batch.
   * @result error status returned by the listener.
   */
  NS_IMETHOD DidBeginBatch(nsITransactionManager *aManager,
                           nsresult aResult) = 0;

  /**
   * Called before a transaction manager ends a batch.
   * @param aManager the transaction manager ending a batch.
   * @result error status returned by the listener. NS_OK
   * should be used to indicate no error, proceed with normal control
   * flow. NS_COMFALSE can be returned by the listener to
   * indicate no error, interrupt normal control flow.
   */
  NS_IMETHOD WillEndBatch(nsITransactionManager *aManager) = 0;

  /**
   * Called after a transaction manager ends a batch.
   * @param aManager the transaction manager ending a batch.
   * @param aResult the nsresult returned after ending a batch.
   * @result error status returned by the listener.
   */
  NS_IMETHOD DidEndBatch(nsITransactionManager *aManager,
                         nsresult aResult) = 0;

  /**
   * Called before a transaction manager tries to merge
   * a transaction, that was just executed, with the
   * transaction at the top of the undo stack.
   * @param aManager the transaction manager ending a batch.
   * @param aTopTransaction the transaction at the top of the undo stack.
   * @param aTransactionToMerge the transaction to merge.
   * @result error status returned by the listener. NS_OK
   * should be used to indicate no error, proceed with normal control
   * flow. NS_COMFALSE can be returned by the listener to
   * indicate no error, interrupt normal control flow.
   */
  NS_IMETHOD WillMerge(nsITransactionManager *aManager,
                       nsITransaction *aTopTransaction,
                       nsITransaction *aTransactionToMerge) = 0;

  /**
   * Called after a transaction manager tries to merge
   * a transaction, that was just executed, with the
   * transaction at the top of the undo stack.
   * @param aManager the transaction manager ending a batch.
   * @param aTopTransaction the transaction at the top of the undo stack.
   * @param aTransactionToMerge the transaction to merge.
   * @param aDidMerge true if transaction was merged, else false.
   * @param aMergeResult the nsresult returned after the merge attempt.
   * @result error status returned by the listener. NS_OK
   * should be used to indicate no error, proceed with normal control
   * flow. NS_COMFALSE can be returned by the listener to
   * indicate no error, interrupt normal control flow.
   */
  NS_IMETHOD DidMerge(nsITransactionManager *aManager,
                      nsITransaction *aTopTransaction,
                      nsITransaction *aTransactionToMerge,
                      PRBool aDidMerge,
                      nsresult aMergeResult) = 0;


  /* XXX: We should probably add pruning notification methods. */
};

#endif // nsITransactionListener_h__

