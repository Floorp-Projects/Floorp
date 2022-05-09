/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TransactionManager_h
#define mozilla_TransactionManager_h

#include "mozilla/EditorForwards.h"
#include "mozilla/TransactionStack.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsITransactionManager.h"
#include "nsWeakReference.h"
#include "nscore.h"

class nsITransaction;

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

  void Attach(HTMLEditor& aHTMLEditor);
  void Detach(const HTMLEditor& aHTMLEditor);

  MOZ_CAN_RUN_SCRIPT void DidDoNotify(nsITransaction& aTransaction,
                                      nsresult aDoResult);
  MOZ_CAN_RUN_SCRIPT void DidUndoNotify(nsITransaction& aTransaction,
                                        nsresult aUndoResult);
  MOZ_CAN_RUN_SCRIPT void DidRedoNotify(nsITransaction& aTransaction,
                                        nsresult aRedoResult);

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
  RefPtr<HTMLEditor> mHTMLEditor;
};

}  // namespace mozilla

mozilla::TransactionManager* nsITransactionManager::AsTransactionManager() {
  return static_cast<mozilla::TransactionManager*>(this);
}

#endif  // #ifndef mozilla_TransactionManager_h
