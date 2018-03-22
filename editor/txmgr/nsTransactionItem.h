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
class nsTransactionStack;

namespace mozilla {
class TransactionManager;
} // namespace mozilla

class nsTransactionItem final
{
  nsCOMArray<nsISupports>  mData;
  nsCOMPtr<nsITransaction> mTransaction;
  nsTransactionStack      *mUndoStack;
  nsTransactionStack      *mRedoStack;

public:
  explicit nsTransactionItem(nsITransaction* aTransaction);
  NS_METHOD_(MozExternalRefCountType) AddRef();
  NS_METHOD_(MozExternalRefCountType) Release();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsTransactionItem)

  nsresult AddChild(nsTransactionItem* aTransactionItem);
  already_AddRefed<nsITransaction> GetTransaction();
  nsresult GetIsBatch(bool *aIsBatch);
  nsresult GetNumberOfChildren(int32_t *aNumChildren);
  nsresult GetChild(int32_t aIndex, nsTransactionItem** aChild);

  nsresult DoTransaction();
  nsresult UndoTransaction(mozilla::TransactionManager* aTransactionManager);
  nsresult RedoTransaction(mozilla::TransactionManager* aTransactionManager);

  nsCOMArray<nsISupports>& GetData()
  {
    return mData;
  }

private:
  nsresult UndoChildren(mozilla::TransactionManager* aTransactionManager);
  nsresult RedoChildren(mozilla::TransactionManager* aTransactionManager);

  nsresult
  RecoverFromUndoError(mozilla::TransactionManager* aTransactionManager);
  nsresult
  RecoverFromRedoError(mozilla::TransactionManager* aTransactionManager);

  nsresult GetNumberOfUndoItems(int32_t* aNumItems);
  nsresult GetNumberOfRedoItems(int32_t* aNumItems);

  void CleanUp();

protected:
  ~nsTransactionItem();

  nsCycleCollectingAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

#endif // nsTransactionItem_h__
