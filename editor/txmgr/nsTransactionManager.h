/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTransactionManager_h__
#define nsTransactionManager_h__

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTransactionStack.h"
#include "nsISupportsImpl.h"
#include "nsITransactionManager.h"
#include "nsTransactionStack.h"
#include "nsWeakReference.h"
#include "nscore.h"

class nsITransaction;
class nsITransactionListener;
class nsTransactionItem;

/** implementation of a transaction manager object.
 *
 */
class nsTransactionManager MOZ_FINAL : public nsITransactionManager
                                     , public nsSupportsWeakReference
{
private:

  int32_t                mMaxTransactionCount;
  nsTransactionStack     mDoStack;
  nsTransactionStack     mUndoStack;
  nsTransactionStack     mRedoStack;
  nsCOMArray<nsITransactionListener> mListeners;

  /** The default destructor.
   */
  virtual ~nsTransactionManager();

public:

  /** The default constructor.
   */
  nsTransactionManager(int32_t aMaxTransactionCount=-1);

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsTransactionManager,
                                           nsITransactionManager)

  /* nsITransactionManager method implementations. */
  NS_DECL_NSITRANSACTIONMANAGER

  already_AddRefed<nsITransaction> PeekUndoStack();
  already_AddRefed<nsITransaction> PeekRedoStack();

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
  virtual nsresult BeginTransaction(nsITransaction *aTransaction,
                                    nsISupports *aData);
  virtual nsresult EndTransaction(bool aAllowEmpty);
};

#endif // nsTransactionManager_h__
