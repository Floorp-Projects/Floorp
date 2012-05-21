/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTransactionItem_h__
#define nsTransactionItem_h__

#include "nsITransaction.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

class nsTransactionStack;
class nsTransactionManager;

class nsTransactionItem
{
  nsCOMPtr<nsITransaction> mTransaction;
  nsTransactionStack      *mUndoStack;
  nsTransactionStack      *mRedoStack;
  nsAutoRefCnt             mRefCnt;

public:

  nsTransactionItem(nsITransaction *aTransaction);
  virtual ~nsTransactionItem();
  nsrefcnt AddRef();
  nsrefcnt Release();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsTransactionItem)

  virtual nsresult AddChild(nsTransactionItem *aTransactionItem);
  virtual nsresult GetTransaction(nsITransaction **aTransaction);
  virtual nsresult GetIsBatch(bool *aIsBatch);
  virtual nsresult GetNumberOfChildren(PRInt32 *aNumChildren);
  virtual nsresult GetChild(PRInt32 aIndex, nsTransactionItem **aChild);

  virtual nsresult DoTransaction(void);
  virtual nsresult UndoTransaction(nsTransactionManager *aTxMgr);
  virtual nsresult RedoTransaction(nsTransactionManager *aTxMgr);

private:

  virtual nsresult UndoChildren(nsTransactionManager *aTxMgr);
  virtual nsresult RedoChildren(nsTransactionManager *aTxMgr);

  virtual nsresult RecoverFromUndoError(nsTransactionManager *aTxMgr);
  virtual nsresult RecoverFromRedoError(nsTransactionManager *aTxMgr);

  virtual nsresult GetNumberOfUndoItems(PRInt32 *aNumItems);
  virtual nsresult GetNumberOfRedoItems(PRInt32 *aNumItems);
};

#endif // nsTransactionItem_h__
