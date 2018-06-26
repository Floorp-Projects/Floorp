/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlaceholderTransaction.h"

#include "CompositionTransaction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/Move.h"
#include "nsGkAtoms.h"
#include "nsQueryObject.h"

namespace mozilla {

using namespace dom;

PlaceholderTransaction::PlaceholderTransaction(
                          EditorBase& aEditorBase,
                          nsAtom* aName,
                          Maybe<SelectionState>&& aSelState)
  : mEditorBase(&aEditorBase)
  , mForwarding(nullptr)
  , mCompositionTransaction(nullptr)
  , mStartSel(*std::move(aSelState))
  , mAbsorb(true)
  , mCommitted(false)
{
  mName = aName;
}

PlaceholderTransaction::~PlaceholderTransaction()
{
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
  NS_INTERFACE_MAP_ENTRY(nsIAbsorbingTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTransaction)

NS_IMPL_ADDREF_INHERITED(PlaceholderTransaction, EditAggregateTransaction)
NS_IMPL_RELEASE_INHERITED(PlaceholderTransaction, EditAggregateTransaction)

NS_IMETHODIMP
PlaceholderTransaction::DoTransaction()
{
  return NS_OK;
}

NS_IMETHODIMP
PlaceholderTransaction::UndoTransaction()
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Undo transactions.
  nsresult rv = EditAggregateTransaction::UndoTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  // now restore selection
  RefPtr<Selection> selection = mEditorBase->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  return mStartSel.RestoreSelection(selection);
}

NS_IMETHODIMP
PlaceholderTransaction::RedoTransaction()
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Redo transactions.
  nsresult rv = EditAggregateTransaction::RedoTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  // now restore selection
  RefPtr<Selection> selection = mEditorBase->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  return mEndSel.RestoreSelection(selection);
}


NS_IMETHODIMP
PlaceholderTransaction::Merge(nsITransaction* aTransaction,
                              bool* aDidMerge)
{
  NS_ENSURE_TRUE(aDidMerge && aTransaction, NS_ERROR_NULL_POINTER);

  // set out param default value
  *aDidMerge=false;

  if (mForwarding) {
    MOZ_ASSERT_UNREACHABLE("tried to merge into a placeholder that was in "
                           "forwarding mode!");
    return NS_ERROR_FAILURE;
  }

  // XXX: hack, not safe!  need nsIEditTransaction!
  EditTransactionBase* editTransactionBase = (EditTransactionBase*)aTransaction;
  // determine if this incoming txn is a placeholder txn
  nsCOMPtr<nsIAbsorbingTransaction> absorbingTransaction =
    do_QueryObject(editTransactionBase);

  // We are absorbing all transactions if mAbsorb is lit.
  if (mAbsorb) {
    RefPtr<CompositionTransaction> otherTransaction =
      do_QueryObject(aTransaction);
    if (otherTransaction) {
      // special handling for CompositionTransaction's: they need to merge with
      // any previous CompositionTransaction in this placeholder, if possible.
      if (!mCompositionTransaction) {
        // this is the first IME txn in the placeholder
        mCompositionTransaction = otherTransaction;
        AppendChild(editTransactionBase);
      } else {
        bool didMerge;
        mCompositionTransaction->Merge(otherTransaction, &didMerge);
        if (!didMerge) {
          // it wouldn't merge.  Earlier IME txn is already committed and will
          // not absorb further IME txns.  So just stack this one after it
          // and remember it as a candidate for further merges.
          mCompositionTransaction = otherTransaction;
          AppendChild(editTransactionBase);
        }
      }
    } else if (!absorbingTransaction) {
      // See bug 171243: just drop incoming placeholders on the floor.
      // Their children will be swallowed by this preexisting one.
      AppendChild(editTransactionBase);
    }
    *aDidMerge = true;
//  RememberEndingSelection();
//  efficiency hack: no need to remember selection here, as we haven't yet
//  finished the initial batch and we know we will be told when the batch ends.
//  we can remeber the selection then.
  } else {
    // merge typing or IME or deletion transactions if the selection matches
    if ((mName.get() == nsGkAtoms::TypingTxnName ||
         mName.get() == nsGkAtoms::IMETxnName    ||
         mName.get() == nsGkAtoms::DeleteTxnName) && !mCommitted) {
      if (absorbingTransaction) {
        RefPtr<nsAtom> atom;
        absorbingTransaction->GetTxnName(getter_AddRefs(atom));
        if (atom && atom == mName) {
          // check if start selection of next placeholder matches
          // end selection of this placeholder
          bool isSame;
          absorbingTransaction->StartSelectionEquals(&mEndSel, &isSame);
          if (isSame) {
            mAbsorb = true;  // we need to start absorbing again
            absorbingTransaction->ForwardEndBatchTo(this);
            // AppendChild(editTransactionBase);
            // see bug 171243: we don't need to merge placeholders
            // into placeholders.  We just reactivate merging in the pre-existing
            // placeholder and drop the new one on the floor.  The EndPlaceHolderBatch()
            // call on the new placeholder will be forwarded to this older one.
            RememberEndingSelection();
            *aDidMerge = true;
          }
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PlaceholderTransaction::GetTxnName(nsAtom** aName)
{
  return GetName(aName);
}

NS_IMETHODIMP
PlaceholderTransaction::StartSelectionEquals(SelectionState* aSelState,
                                             bool* aResult)
{
  // determine if starting selection matches the given selection state.
  // note that we only care about collapsed selections.
  NS_ENSURE_TRUE(aResult && aSelState, NS_ERROR_NULL_POINTER);
  if (!mStartSel.IsCollapsed() || !aSelState->IsCollapsed()) {
    *aResult = false;
    return NS_OK;
  }
  *aResult = mStartSel.IsEqual(aSelState);
  return NS_OK;
}

NS_IMETHODIMP
PlaceholderTransaction::EndPlaceHolderBatch()
{
  mAbsorb = false;

  if (mForwarding) {
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryReferent(mForwarding);
    if (plcTxn) {
      plcTxn->EndPlaceHolderBatch();
    }
  }
  // remember our selection state.
  return RememberEndingSelection();
}

NS_IMETHODIMP
PlaceholderTransaction::ForwardEndBatchTo(
                          nsIAbsorbingTransaction* aForwardingAddress)
{
  mForwarding = do_GetWeakReference(aForwardingAddress);
  return NS_OK;
}

NS_IMETHODIMP
PlaceholderTransaction::Commit()
{
  mCommitted = true;
  return NS_OK;
}

nsresult
PlaceholderTransaction::RememberEndingSelection()
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Selection> selection = mEditorBase->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  mEndSel.SaveSelection(selection);
  return NS_OK;
}

} // namespace mozilla
