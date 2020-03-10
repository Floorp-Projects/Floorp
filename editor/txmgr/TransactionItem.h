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

namespace mozilla {

class TransactionManager;
class TransactionStack;

class TransactionItem final {
 public:
  explicit TransactionItem(nsITransaction* aTransaction);
  NS_METHOD_(MozExternalRefCountType) AddRef();
  NS_METHOD_(MozExternalRefCountType) Release();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(TransactionItem)

  nsresult AddChild(TransactionItem& aTransactionItem);
  already_AddRefed<nsITransaction> GetTransaction();
  size_t NumberOfChildren() const {
    return NumberOfUndoItems() + NumberOfRedoItems();
  }
  nsresult GetChild(size_t aIndex, TransactionItem** aChild);

  nsresult DoTransaction();
  nsresult UndoTransaction(TransactionManager* aTransactionManager);
  nsresult RedoTransaction(TransactionManager* aTransactionManager);

  nsCOMArray<nsISupports>& GetData() { return mData; }

 private:
  nsresult UndoChildren(TransactionManager* aTransactionManager);
  nsresult RedoChildren(TransactionManager* aTransactionManager);

  nsresult RecoverFromUndoError(TransactionManager* aTransactionManager);
  nsresult RecoverFromRedoError(TransactionManager* aTransactionManager);

  size_t NumberOfUndoItems() const;
  size_t NumberOfRedoItems() const;

  void CleanUp();

  ~TransactionItem();

  nsCycleCollectingAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  nsCOMArray<nsISupports> mData;
  nsCOMPtr<nsITransaction> mTransaction;
  TransactionStack* mUndoStack;
  TransactionStack* mRedoStack;
};

}  // namespace mozilla

#endif  // #ifndef TransactionItem_h
