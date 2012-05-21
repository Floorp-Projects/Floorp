/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTransactionManager_h__
#define nsTransactionManager_h__

#include "nsWeakReference.h"
#include "nsITransactionManager.h"
#include "nsCOMArray.h"
#include "nsITransactionListener.h"
#include "nsCycleCollectionParticipant.h"

class nsITransaction;
class nsITransactionListener;
class nsTransactionItem;
class nsTransactionStack;

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
  nsTransactionStack     mRedoStack;
  nsCOMArray<nsITransactionListener> mListeners;

public:

  /** The default constructor.
   */
  nsTransactionManager(PRInt32 aMaxTransactionCount=-1);

  /** The default destructor.
   */
  virtual ~nsTransactionManager();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsTransactionManager,
                                           nsITransactionManager)

  /* nsITransactionManager method implementations. */
  NS_DECL_NSITRANSACTIONMANAGER

  /* nsTransactionManager specific methods. */
  virtual nsresult ClearUndoStack(void);
  virtual nsresult ClearRedoStack(void);

  virtual nsresult WillDoNotify(nsITransaction *aTransaction, bool *aInterrupt);
  virtual nsresult DidDoNotify(nsITransaction *aTransaction, nsresult aExecuteResult);
  virtual nsresult WillUndoNotify(nsITransaction *aTransaction, bool *aInterrupt);
  virtual nsresult DidUndoNotify(nsITransaction *aTransaction, nsresult aUndoResult);
  virtual nsresult WillRedoNotify(nsITransaction *aTransaction, bool *aInterrupt);
  virtual nsresult DidRedoNotify(nsITransaction *aTransaction, nsresult aRedoResult);
  virtual nsresult WillBeginBatchNotify(bool *aInterrupt);
  virtual nsresult DidBeginBatchNotify(nsresult aResult);
  virtual nsresult WillEndBatchNotify(bool *aInterrupt);
  virtual nsresult DidEndBatchNotify(nsresult aResult);
  virtual nsresult WillMergeNotify(nsITransaction *aTop,
                                   nsITransaction *aTransaction,
                                   bool *aInterrupt);
  virtual nsresult DidMergeNotify(nsITransaction *aTop,
                                  nsITransaction *aTransaction,
                                  bool aDidMerge,
                                  nsresult aMergeResult);

private:

  /* nsTransactionManager specific private methods. */
  virtual nsresult BeginTransaction(nsITransaction *aTransaction);
  virtual nsresult EndTransaction(void);
};

#endif // nsTransactionManager_h__
