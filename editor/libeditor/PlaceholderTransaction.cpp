/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlaceholderTransaction.h"

#include <utility>

#include "CompositionTransaction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/dom/Selection.h"
#include "nsGkAtoms.h"
#include "nsQueryObject.h"

namespace mozilla {

using namespace dom;

PlaceholderTransaction::PlaceholderTransaction(
    EditorBase& aEditorBase, nsStaticAtom& aName,
    Maybe<SelectionState>&& aSelState)
    : mEditorBase(&aEditorBase),
      mCompositionTransaction(nullptr),
      mStartSel(*std::move(aSelState)),
      mAbsorb(true),
      mCommitted(false) {
  mName = &aName;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PlaceholderTransaction)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PlaceholderTransaction,
                                                EditAggregateTransaction)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditorBase);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStartSel);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEndSel);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PlaceholderTransaction,
                                                  EditAggregateTransaction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditorBase);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStartSel);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEndSel);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PlaceholderTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTransaction)

NS_IMPL_ADDREF_INHERITED(PlaceholderTransaction, EditAggregateTransaction)
NS_IMPL_RELEASE_INHERITED(PlaceholderTransaction, EditAggregateTransaction)

NS_IMETHODIMP PlaceholderTransaction::DoTransaction() { return NS_OK; }

NS_IMETHODIMP PlaceholderTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Undo transactions.
  nsresult rv = EditAggregateTransaction::UndoTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditAggregateTransaction::UndoTransaction() failed");
    return rv;
  }

  // now restore selection
  RefPtr<Selection> selection = mEditorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  rv = mStartSel.RestoreSelection(*selection);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "SelectionState::RestoreSelection() failed");
  return rv;
}

NS_IMETHODIMP PlaceholderTransaction::RedoTransaction() {
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Redo transactions.
  nsresult rv = EditAggregateTransaction::RedoTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditAggregateTransaction::RedoTransaction() failed");
    return rv;
  }

  // now restore selection
  RefPtr<Selection> selection = mEditorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  rv = mEndSel.RestoreSelection(*selection);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "SelectionState::RestoreSelection() failed");
  return rv;
}

NS_IMETHODIMP PlaceholderTransaction::Merge(nsITransaction* aOtherTransaction,
                                            bool* aDidMerge) {
  if (NS_WARN_IF(!aDidMerge) || NS_WARN_IF(!aOtherTransaction)) {
    return NS_ERROR_INVALID_ARG;
  }

  // set out param default value
  *aDidMerge = false;

  if (mForwardingTransaction) {
    MOZ_ASSERT_UNREACHABLE(
        "tried to merge into a placeholder that was in "
        "forwarding mode!");
    return NS_ERROR_FAILURE;
  }

  RefPtr<EditTransactionBase> otherTransactionBase =
      aOtherTransaction->GetAsEditTransactionBase();
  if (!otherTransactionBase) {
    return NS_OK;
  }

  // We are absorbing all transactions if mAbsorb is lit.
  if (mAbsorb) {
    if (CompositionTransaction* otherCompositionTransaction =
            otherTransactionBase->GetAsCompositionTransaction()) {
      // special handling for CompositionTransaction's: they need to merge with
      // any previous CompositionTransaction in this placeholder, if possible.
      if (!mCompositionTransaction) {
        // this is the first IME txn in the placeholder
        mCompositionTransaction = otherCompositionTransaction;
        DebugOnly<nsresult> rvIgnored =
            AppendChild(otherCompositionTransaction);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            "EditAggregateTransaction::AppendChild() failed, but ignored");
      } else {
        bool didMerge;
        mCompositionTransaction->Merge(otherCompositionTransaction, &didMerge);
        if (!didMerge) {
          // it wouldn't merge.  Earlier IME txn is already committed and will
          // not absorb further IME txns.  So just stack this one after it
          // and remember it as a candidate for further merges.
          mCompositionTransaction = otherCompositionTransaction;
          DebugOnly<nsresult> rvIgnored =
              AppendChild(otherCompositionTransaction);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rvIgnored),
              "EditAggregateTransaction::AppendChild() failed, but ignored");
        }
      }
    } else {
      PlaceholderTransaction* otherPlaceholderTransaction =
          otherTransactionBase->GetAsPlaceholderTransaction();
      if (!otherPlaceholderTransaction) {
        // See bug 171243: just drop incoming placeholders on the floor.
        // Their children will be swallowed by this preexisting one.
        DebugOnly<nsresult> rvIgnored = AppendChild(otherTransactionBase);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            "EditAggregateTransaction::AppendChild() failed, but ignored");
      }
    }
    *aDidMerge = true;
    //  RememberEndingSelection();
    //  efficiency hack: no need to remember selection here, as we haven't yet
    //  finished the initial batch and we know we will be told when the batch
    //  ends. we can remeber the selection then.
    return NS_OK;
  }

  // merge typing or IME or deletion transactions if the selection matches
  if (mCommitted ||
      (mName != nsGkAtoms::TypingTxnName && mName != nsGkAtoms::IMETxnName &&
       mName != nsGkAtoms::DeleteTxnName)) {
    return NS_OK;
  }

  PlaceholderTransaction* otherPlaceholderTransaction =
      otherTransactionBase->GetAsPlaceholderTransaction();
  if (!otherPlaceholderTransaction) {
    return NS_OK;
  }

  RefPtr<nsAtom> otherTransactionName;
  DebugOnly<nsresult> rvIgnored = otherPlaceholderTransaction->GetName(
      getter_AddRefs(otherTransactionName));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "PlaceholderTransaction::GetName() failed, but ignored");
  if (!otherTransactionName || otherTransactionName == nsGkAtoms::_empty ||
      otherTransactionName != mName) {
    return NS_OK;
  }
  // check if start selection of next placeholder matches
  // end selection of this placeholder
  if (!otherPlaceholderTransaction->StartSelectionEquals(mEndSel)) {
    return NS_OK;
  }
  mAbsorb = true;  // we need to start absorbing again
  otherPlaceholderTransaction->ForwardEndBatchTo(*this);
  // AppendChild(editTransactionBase);
  // see bug 171243: we don't need to merge placeholders
  // into placeholders.  We just reactivate merging in the
  // pre-existing placeholder and drop the new one on the floor.  The
  // EndPlaceHolderBatch() call on the new placeholder will be
  // forwarded to this older one.
  rvIgnored = RememberEndingSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "PlaceholderTransaction::RememberEndingSelection() failed, but "
      "ignored");
  *aDidMerge = true;
  return NS_OK;
}

bool PlaceholderTransaction::StartSelectionEquals(
    SelectionState& aSelectionState) {
  // determine if starting selection matches the given selection state.
  // note that we only care about collapsed selections.
  return mStartSel.IsCollapsed() && aSelectionState.IsCollapsed() &&
         mStartSel.Equals(aSelectionState);
}

nsresult PlaceholderTransaction::EndPlaceHolderBatch() {
  mAbsorb = false;

  if (mForwardingTransaction) {
    if (mForwardingTransaction) {
      DebugOnly<nsresult> rvIgnored =
          mForwardingTransaction->EndPlaceHolderBatch();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "PlaceholderTransaction::EndPlaceHolderBatch() failed, but ignored");
    }
  }
  // remember our selection state.
  nsresult rv = RememberEndingSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "PlaceholderTransaction::RememberEndingSelection() failed");
  return rv;
}

nsresult PlaceholderTransaction::RememberEndingSelection() {
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Selection> selection = mEditorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  mEndSel.SaveSelection(*selection);
  return NS_OK;
}

}  // namespace mozilla
