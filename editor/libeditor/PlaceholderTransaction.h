/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PlaceholderTransaction_h
#define PlaceholderTransaction_h

#include "EditAggregateTransaction.h"
#include "mozilla/Maybe.h"
#include "mozilla/SelectionState.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {

class EditorBase;

/**
 * An aggregate transaction that knows how to absorb all subsequent
 * transactions with the same name.  This transaction does not "Do" anything.
 * But it absorbs other transactions via merge, and can undo/redo the
 * transactions it has absorbed.
 */

class PlaceholderTransaction final
    : public EditAggregateTransaction,
      public SupportsWeakPtr<PlaceholderTransaction> {
 protected:
  PlaceholderTransaction(EditorBase& aEditorBase, nsStaticAtom& aName,
                         Maybe<SelectionState>&& aSelState);

 public:
  /**
   * Creates a placeholder transaction.  This never returns nullptr.
   *
   * @param aEditorBase     The editor.
   * @param aName           The name of creating transaction.
   * @param aSelState       The selection state of aEditorBase.
   */
  static already_AddRefed<PlaceholderTransaction> Create(
      EditorBase& aEditorBase, nsStaticAtom& aName,
      Maybe<SelectionState>&& aSelState) {
    // Make sure to move aSelState into a local variable to null out the
    // original Maybe<SelectionState> variable.
    Maybe<SelectionState> selState(std::move(aSelState));
    RefPtr<PlaceholderTransaction> transaction =
        new PlaceholderTransaction(aEditorBase, aName, std::move(selState));
    return transaction.forget();
  }

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(PlaceholderTransaction)

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PlaceholderTransaction,
                                           EditAggregateTransaction)
  // ------------ EditAggregateTransaction -----------------------

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(PlaceholderTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;
  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;

  bool StartSelectionEquals(SelectionState& aSelectionState);

  nsresult EndPlaceHolderBatch();

  void ForwardEndBatchTo(PlaceholderTransaction& aForwardingTransaction) {
    mForwardingTransaction = &aForwardingTransaction;
  }

  void Commit() { mCommitted = true; }

  nsresult RememberEndingSelection();

 protected:
  virtual ~PlaceholderTransaction() = default;

  // The editor for this transaction.
  RefPtr<EditorBase> mEditorBase;

  WeakPtr<PlaceholderTransaction> mForwardingTransaction;

  // First IME txn in this placeholder - used for IME merging.
  WeakPtr<CompositionTransaction> mCompositionTransaction;

  // These next two members store the state of the selection in a safe way.
  // Selection at the start of the transaction is stored, as is the selection
  // at the end.  This is so that UndoTransaction() and RedoTransaction() can
  // restore the selection properly.

  SelectionState mStartSel;
  SelectionState mEndSel;

  // Do we auto absorb any and all transaction?
  bool mAbsorb;
  // Do we stop auto absorbing any matching placeholder transactions?
  bool mCommitted;
};

}  // namespace mozilla

#endif  // #ifndef PlaceholderTransaction_h
