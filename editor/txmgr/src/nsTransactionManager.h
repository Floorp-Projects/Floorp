/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsTransactionManager_h__
#define nsTransactionManager_h__

#include "prmon.h"
#include "nsWeakReference.h"
#include "nsITransactionManager.h"

class nsITransaction;
class nsITransactionListener;
class nsTransactionItem;
class nsTransactionStack;
class nsTransactionRedoStack;
class nsVoidArray;

/** implementation of a transaction manager object.
 *
 */
class nsTransactionManager : public nsITransactionManager
                           , public nsSupportsWeakReference
{
private:

  PRInt32                mMaxTransactionCount;
  nsTransactionStack     mDoStack;
  nsTransactionStack     mUndoStack;
  nsTransactionRedoStack mRedoStack;
  nsVoidArray            *mListeners;

  PRMonitor              *mMonitor;

public:

  /** The default constructor.
   */
  nsTransactionManager(PRInt32 aMaxTransactionCount=-1);

  /** The default destructor.
   */
  virtual ~nsTransactionManager();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsITransactionManager method implementations. */
  NS_DECL_NSITRANSACTIONMANAGER

  /* nsTransactionManager specific methods. */
  virtual nsresult ClearUndoStack(void);
  virtual nsresult ClearRedoStack(void);

  virtual nsresult WillDoNotify(nsITransaction *aTransaction, PRBool *aInterrupt);
  virtual nsresult DidDoNotify(nsITransaction *aTransaction, nsresult aExecuteResult);
  virtual nsresult WillUndoNotify(nsITransaction *aTransaction, PRBool *aInterrupt);
  virtual nsresult DidUndoNotify(nsITransaction *aTransaction, nsresult aUndoResult);
  virtual nsresult WillRedoNotify(nsITransaction *aTransaction, PRBool *aInterrupt);
  virtual nsresult DidRedoNotify(nsITransaction *aTransaction, nsresult aRedoResult);
  virtual nsresult WillBeginBatchNotify(PRBool *aInterrupt);
  virtual nsresult DidBeginBatchNotify(nsresult aResult);
  virtual nsresult WillEndBatchNotify(PRBool *aInterrupt);
  virtual nsresult DidEndBatchNotify(nsresult aResult);
  virtual nsresult WillMergeNotify(nsITransaction *aTop,
                                   nsITransaction *aTransaction,
                                   PRBool *aInterrupt);
  virtual nsresult DidMergeNotify(nsITransaction *aTop,
                                  nsITransaction *aTransaction,
                                  PRBool aDidMerge,
                                  nsresult aMergeResult);

private:

  /* nsTransactionManager specific private methods. */
  virtual nsresult BeginTransaction(nsITransaction *aTransaction);
  virtual nsresult EndTransaction(void);
  virtual nsresult Lock(void);
  virtual nsresult Unlock(void);
};

#endif // nsTransactionManager_h__
