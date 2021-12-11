/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TransactionManager_h
#define mozilla_TransactionManager_h

#include "mozilla/TransactionStack.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsITransactionManager.h"
#include "nsWeakReference.h"
#include "nscore.h"

class nsITransaction;
class nsITransactionListener;

namespace mozilla {

class TransactionManager final : public nsITransactionManager,
                                 public nsSupportsWeakReference {
 public:
  explicit TransactionManager(int32_t aMaxTransactionCount = -1);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(TransactionManager,
                                           nsITransactionManager)

  NS_DECL_NSITRANSACTIONMANAGER

  already_AddRefed<nsITransaction> PeekUndoStack();
  already_AddRefed<nsITransaction> PeekRedoStack();

  MOZ_CAN_RUN_SCRIPT nsresult Undo();
  MOZ_CAN_RUN_SCRIPT nsresult Redo();

  size_t NumberOfUndoItems() const { return mUndoStack.GetSize(); }
  size_t NumberOfRedoItems() const { return mRedoStack.GetSize(); }

  int32_t NumberOfMaximumTransactions() const { return mMaxTransactionCount; }

  bool EnableUndoRedo(int32_t aMaxTransactionCount = -1);
  bool DisableUndoRedo() { return EnableUndoRedo(0); }
  bool ClearUndoRedo() {
    if (NS_WARN_IF(!mDoStack.IsEmpty())) {
      return false;
    }
    mUndoStack.Clear();
    mRedoStack.Clear();
    return true;
  }

  bool AddTransactionListener(nsITransactionListener& aListener) {
    // XXX Shouldn't we check if aListener has already been in mListeners?
    return mListeners.AppendObject(&aListener);
  }
  bool RemoveTransactionListener(nsITransactionListener& aListener) {
    return mListeners.RemoveObject(&aListener);
  }

  // FYI: We don't need to treat the following methods as `MOZ_CAN_RUN_SCRIPT`
  //      for now because only ComposerCommandUpdater is the listener and it
  //      does not do something dangerous synchronously.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  WillDoNotify(nsITransaction* aTransaction, bool* aInterrupt);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult DidDoNotify(nsITransaction* aTransaction,
                                                   nsresult aExecuteResult);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  WillUndoNotify(nsITransaction* aTransaction, bool* aInterrupt);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  DidUndoNotify(nsITransaction* aTransaction, nsresult aUndoResult);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  WillRedoNotify(nsITransaction* aTransaction, bool* aInterrupt);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  DidRedoNotify(nsITransaction* aTransaction, nsresult aRedoResult);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult WillBeginBatchNotify(bool* aInterrupt);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult DidBeginBatchNotify(nsresult aResult);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult WillEndBatchNotify(bool* aInterrupt);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult DidEndBatchNotify(nsresult aResult);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult WillMergeNotify(
      nsITransaction* aTop, nsITransaction* aTransaction, bool* aInterrupt);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  DidMergeNotify(nsITransaction* aTop, nsITransaction* aTransaction,
                 bool aDidMerge, nsresult aMergeResult);

  /**
   * Exposing non-virtual methods of nsITransactionManager methods.
   */
  MOZ_CAN_RUN_SCRIPT nsresult BeginBatchInternal(nsISupports* aData);
  nsresult EndBatchInternal(bool aAllowEmpty);

 private:
  virtual ~TransactionManager() = default;

  MOZ_CAN_RUN_SCRIPT nsresult BeginTransaction(nsITransaction* aTransaction,
                                               nsISupports* aData);
  nsresult EndTransaction(bool aAllowEmpty);

  int32_t mMaxTransactionCount;
  TransactionStack mDoStack;
  TransactionStack mUndoStack;
  TransactionStack mRedoStack;
  nsCOMArray<nsITransactionListener> mListeners;
};

}  // namespace mozilla

mozilla::TransactionManager* nsITransactionManager::AsTransactionManager() {
  return static_cast<mozilla::TransactionManager*>(this);
}

#endif  // #ifndef mozilla_TransactionManager_h
