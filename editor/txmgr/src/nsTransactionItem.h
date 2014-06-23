/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTransactionItem_h__
#define nsTransactionItem_h__

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

class nsITransaction;
class nsTransactionManager;
class nsTransactionStack;

class nsTransactionItem
{
  nsCOMArray<nsISupports>  mData;
  nsCOMPtr<nsITransaction> mTransaction;
  nsTransactionStack      *mUndoStack;
  nsTransactionStack      *mRedoStack;

public:

  nsTransactionItem(nsITransaction *aTransaction);
  NS_METHOD_(MozExternalRefCountType) AddRef();
  NS_METHOD_(MozExternalRefCountType) Release();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsTransactionItem)

  virtual nsresult AddChild(nsTransactionItem *aTransactionItem);
  already_AddRefed<nsITransaction> GetTransaction();
  virtual nsresult GetIsBatch(bool *aIsBatch);
  virtual nsresult GetNumberOfChildren(int32_t *aNumChildren);
  virtual nsresult GetChild(int32_t aIndex, nsTransactionItem **aChild);

  virtual nsresult DoTransaction(void);
  virtual nsresult UndoTransaction(nsTransactionManager *aTxMgr);
  virtual nsresult RedoTransaction(nsTransactionManager *aTxMgr);

  nsCOMArray<nsISupports>& GetData()
  {
    return mData;
  }

private:

  virtual nsresult UndoChildren(nsTransactionManager *aTxMgr);
  virtual nsresult RedoChildren(nsTransactionManager *aTxMgr);

  virtual nsresult RecoverFromUndoError(nsTransactionManager *aTxMgr);
  virtual nsresult RecoverFromRedoError(nsTransactionManager *aTxMgr);

  virtual nsresult GetNumberOfUndoItems(int32_t *aNumItems);
  virtual nsresult GetNumberOfRedoItems(int32_t *aNumItems);

  void CleanUp();
protected:
  virtual ~nsTransactionItem();

  nsCycleCollectingAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

#endif // nsTransactionItem_h__
