/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PlaceholderTransaction_h
#define PlaceholderTransaction_h

#include "EditAggregateTransaction.h"
#include "mozilla/EditorUtils.h"
#include "nsIAbsorbingTransaction.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"
#include "nsAutoPtr.h"

namespace mozilla {

class CompositionTransaction;

/**
 * An aggregate transaction that knows how to absorb all subsequent
 * transactions with the same name.  This transaction does not "Do" anything.
 * But it absorbs other transactions via merge, and can undo/redo the
 * transactions it has absorbed.
 */

class PlaceholderTransaction final : public EditAggregateTransaction,
                                     public nsIAbsorbingTransaction,
                                     public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  PlaceholderTransaction();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PlaceholderTransaction,
                                           EditAggregateTransaction)
// ------------ EditAggregateTransaction -----------------------

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;
  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;

// ------------ nsIAbsorbingTransaction -----------------------

  NS_IMETHOD Init(nsIAtom* aName, SelectionState* aSelState,
                  EditorBase* aEditorBase) override;

  NS_IMETHOD GetTxnName(nsIAtom** aName) override;

  NS_IMETHOD StartSelectionEquals(SelectionState* aSelState,
                                  bool* aResult) override;

  NS_IMETHOD EndPlaceHolderBatch() override;

  NS_IMETHOD ForwardEndBatchTo(
               nsIAbsorbingTransaction* aForwardingAddress) override;

  NS_IMETHOD Commit() override;

  nsresult RememberEndingSelection();

protected:
  virtual ~PlaceholderTransaction();

  // Do we auto absorb any and all transaction?
  bool mAbsorb;
  nsWeakPtr mForwarding;
  // First IME txn in this placeholder - used for IME merging.
  mozilla::CompositionTransaction* mCompositionTransaction;
  // Do we stop auto absorbing any matching placeholder transactions?
  bool mCommitted;

  // These next two members store the state of the selection in a safe way.
  // Selection at the start of the transaction is stored, as is the selection
  // at the end.  This is so that UndoTransaction() and RedoTransaction() can
  // restore the selection properly.

  // Use a pointer because this is constructed before we exist.
  nsAutoPtr<SelectionState> mStartSel;
  SelectionState mEndSel;

  // The editor for this transaction.
  EditorBase* mEditorBase;
};

} // namespace mozilla

#endif // #ifndef PlaceholderTransaction_h
