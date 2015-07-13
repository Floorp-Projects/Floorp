/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AggregatePlaceholderTxn_h__
#define AggregatePlaceholderTxn_h__

#include "EditAggregateTxn.h"
#include "nsEditorUtils.h"
#include "nsIAbsorbingTransaction.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {
class IMETextTxn;
} // namespace dom
} // namespace mozilla

/**
 * An aggregate transaction that knows how to absorb all subsequent
 * transactions with the same name.  This transaction does not "Do" anything.
 * But it absorbs other transactions via merge, and can undo/redo the
 * transactions it has absorbed.
 */

class PlaceholderTxn : public EditAggregateTxn,
                       public nsIAbsorbingTransaction,
                       public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  PlaceholderTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PlaceholderTxn, EditAggregateTxn)
// ------------ EditAggregateTxn -----------------------

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction() override;
  NS_IMETHOD Merge(nsITransaction *aTransaction, bool *aDidMerge) override;

// ------------ nsIAbsorbingTransaction -----------------------

  NS_IMETHOD Init(nsIAtom* aName, nsSelectionState* aSelState,
                  nsEditor* aEditor) override;

  NS_IMETHOD GetTxnName(nsIAtom **aName) override;

  NS_IMETHOD StartSelectionEquals(nsSelectionState *aSelState, bool *aResult) override;

  NS_IMETHOD EndPlaceHolderBatch() override;

  NS_IMETHOD ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress) override;

  NS_IMETHOD Commit() override;

  nsresult RememberEndingSelection();

protected:
  virtual ~PlaceholderTxn();

  /** the presentation shell, which we'll need to get the selection */
  bool        mAbsorb;          // do we auto absorb any and all transaction?
  nsWeakPtr   mForwarding;
  mozilla::dom::IMETextTxn *mIMETextTxn;      // first IME txn in this placeholder - used for IME merging
                                // non-owning for now - can't nsCOMPtr it due to broken transaction interfaces
  bool        mCommitted;       // do we stop auto absorbing any matching placeholder txns?
  // these next two members store the state of the selection in a safe way.
  // selection at the start of the txn is stored, as is the selection at the end.
  // This is so that UndoTransaction() and RedoTransaction() can restore the
  // selection properly.
  nsAutoPtr<nsSelectionState> mStartSel; // use a pointer because this is constructed before we exist
  nsSelectionState  mEndSel;
  nsEditor*         mEditor;   /** the editor for this transaction */
};


#endif
