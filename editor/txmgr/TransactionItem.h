/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TransactionItem_h
#define TransactionItem_h

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

class nsITransaction;
class nsTransactionStack;

namespace mozilla {

class TransactionManager;

class TransactionItem final
{
public:
  explicit TransactionItem(nsITransaction* aTransaction);
  NS_METHOD_(MozExternalRefCountType) AddRef();
  NS_METHOD_(MozExternalRefCountType) Release();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(TransactionItem)

  nsresult AddChild(TransactionItem* aTransactionItem);
  already_AddRefed<nsITransaction> GetTransaction();
  nsresult GetIsBatch(bool *aIsBatch);
  nsresult GetNumberOfChildren(int32_t *aNumChildren);
  nsresult GetChild(int32_t aIndex, TransactionItem** aChild);

  nsresult DoTransaction();
  nsresult UndoTransaction(TransactionManager* aTransactionManager);
  nsresult RedoTransaction(TransactionManager* aTransactionManager);

  nsCOMArray<nsISupports>& GetData()
  {
    return mData;
  }

private:
  nsresult UndoChildren(TransactionManager* aTransactionManager);
  nsresult RedoChildren(TransactionManager* aTransactionManager);

  nsresult RecoverFromUndoError(TransactionManager* aTransactionManager);
  nsresult RecoverFromRedoError(TransactionManager* aTransactionManager);

  nsresult GetNumberOfUndoItems(int32_t* aNumItems);
  nsresult GetNumberOfRedoItems(int32_t* aNumItems);

  void CleanUp();

  ~TransactionItem();

  nsCycleCollectingAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  nsCOMArray<nsISupports> mData;
  nsCOMPtr<nsITransaction> mTransaction;
  nsTransactionStack* mUndoStack;
  nsTransactionStack* mRedoStack;
};

} // namespace mozilla

#endif // #ifndef TransactionItem_h
