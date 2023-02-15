/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlaceholderTransaction.h"

#include <utility>

#include "CompositionTransaction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/Logging.h"
#include "mozilla/ToString.h"
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

void PlaceholderTransaction::AppendChild(EditTransactionBase& aTransaction) {
  mChildren.AppendElement(aTransaction);
}

NS_IMETHODIMP PlaceholderTransaction::DoTransaction() {
  MOZ_LOG(
      GetLogModule(), LogLevel::Info,
      ("%p PlaceholderTransaction::%s this={ mName=%s }", this, __FUNCTION__,
       nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p PlaceholderTransaction::%s this={ mName=%s } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));

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

  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p PlaceholderTransaction::%s this={ mName=%s } "
           "End==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return rv;
}

NS_IMETHODIMP PlaceholderTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p PlaceholderTransaction::%s this={ mName=%s } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));

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
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p PlaceholderTransaction::%s this={ mName=%s } "
           "End==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
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
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
             "mName=%s } returned false due to non edit transaction",
             this, __FUNCTION__, aOtherTransaction,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
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
        AppendChild(*otherCompositionTransaction);
      } else {
        bool didMerge;
        mCompositionTransaction->Merge(otherCompositionTransaction, &didMerge);
        if (!didMerge) {
          // it wouldn't merge.  Earlier IME txn is already committed and will
          // not absorb further IME txns.  So just stack this one after it
          // and remember it as a candidate for further merges.
          mCompositionTransaction = otherCompositionTransaction;
          AppendChild(*otherCompositionTransaction);
        }
      }
    } else {
      PlaceholderTransaction* otherPlaceholderTransaction =
          otherTransactionBase->GetAsPlaceholderTransaction();
      if (!otherPlaceholderTransaction) {
        // See bug 171243: just drop incoming placeholders on the floor.
        // Their children will be swallowed by this preexisting one.
        AppendChild(*otherTransactionBase);
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
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
             "mName=%s } returned false due to non mergable transaction",
             this, __FUNCTION__, aOtherTransaction,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
    return NS_OK;
  }

  PlaceholderTransaction* otherPlaceholderTransaction =
      otherTransactionBase->GetAsPlaceholderTransaction();
  if (!otherPlaceholderTransaction) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
             "mName=%s } returned false due to non placeholder transaction",
             this, __FUNCTION__, aOtherTransaction,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
    return NS_OK;
  }

  RefPtr<nsAtom> otherTransactionName = otherPlaceholderTransaction->GetName();
  if (!otherTransactionName || otherTransactionName == nsGkAtoms::_empty ||
      otherTransactionName != mName) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
             "mName=%s } returned false due to non mergable placeholder "
             "transaction",
             this, __FUNCTION__, aOtherTransaction,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
    return NS_OK;
  }

  // check if start selection of next placeholder matches
  // end selection of this placeholder
  // XXX Theese checks seem wrong.  The ending selection is initialized with
  //     actual Selection rather than expected Selection.  Therefore, even when
  //     web apps modifies Selection, we don't merge mergable transactions.

  // If the new transaction's starting Selection is not a caret, we shouldn't be
  // merged with it because it's probably caused deleting the selection.
  if (!otherPlaceholderTransaction->mStartSel.HasOnlyCollapsedRange()) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
             "mName=%s } returned false due to not collapsed selection at "
             "start of new transactions",
             this, __FUNCTION__, aOtherTransaction,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
    return NS_OK;
  }

  // If our ending Selection is not a caret, we should not be merged with it
  // because we probably changed format of a block or style of text.
  if (!mEndSel.HasOnlyCollapsedRange()) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
             "mName=%s } returned false due to not collapsed selection at end "
             "of previous transactions",
             this, __FUNCTION__, aOtherTransaction,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
    return NS_OK;
  }

  // If the caret positions are now in different root nodes, e.g., the previous
  // caret position was removed from the DOM tree, this merge should not be
  // done.
  const bool isPreviousCaretPointInSameRootOfNewCaretPoint = [&]() {
    nsINode* previousRootInCurrentDOMTree = mEndSel.GetCommonRootNode();
    return previousRootInCurrentDOMTree &&
           previousRootInCurrentDOMTree ==
               otherPlaceholderTransaction->mStartSel.GetCommonRootNode();
  }();
  if (!isPreviousCaretPointInSameRootOfNewCaretPoint) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
             "mName=%s } returned false due to the caret points are in "
             "different root nodes",
             this, __FUNCTION__, aOtherTransaction,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
    return NS_OK;
  }

  // If the caret points of end of us and start of new transaction are not same,
  // we shouldn't merge them.
  if (!otherPlaceholderTransaction->mStartSel.Equals(mEndSel)) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
             "mName=%s } returned false due to caret positions were different",
             this, __FUNCTION__, aOtherTransaction,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
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
  DebugOnly<nsresult> rvIgnored = RememberEndingSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "PlaceholderTransaction::RememberEndingSelection() failed, but "
      "ignored");
  *aDidMerge = true;
  MOZ_LOG(GetLogModule(), LogLevel::Debug,
          ("%p PlaceholderTransaction::%s(aOtherTransaction=%p) this={ "
           "mName=%s } returned true",
           this, __FUNCTION__, aOtherTransaction,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return NS_OK;
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
