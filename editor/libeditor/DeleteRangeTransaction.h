/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteRangeTransaction_h
#define DeleteRangeTransaction_h

#include "EditAggregateTransaction.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsIEditor.h"
#include "nsISupportsImpl.h"
#include "nsRange.h"
#include "nscore.h"

class nsINode;

namespace mozilla {

class EditorBase;
class RangeUpdater;

/**
 * A transaction that deletes an entire range in the content tree
 */
class DeleteRangeTransaction final : public EditAggregateTransaction
{
public:
  /**
   * Initialize the transaction.
   * @param aEditorBase The object providing basic editing operations.
   * @param aRange      The range to delete.
   */
  nsresult Init(EditorBase* aEditorBase,
                nsRange* aRange,
                RangeUpdater* aRangeUpdater);

  DeleteRangeTransaction();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteRangeTransaction,
                                           EditAggregateTransaction)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;

  virtual void LastRelease() override
  {
    mRange = nullptr;
    EditAggregateTransaction::LastRelease();
  }

protected:
  nsresult CreateTxnsToDeleteBetween(nsINode* aNode,
                                     int32_t aStartOffset,
                                     int32_t aEndOffset);

  nsresult CreateTxnsToDeleteNodesBetween();

  nsresult CreateTxnsToDeleteContent(nsINode* aParent,
                                     int32_t aOffset,
                                     nsIEditor::EDirection aAction);

  // P1 in the range.
  RefPtr<nsRange> mRange;

  // The editor for this transaction.
  EditorBase* mEditorBase;

  // Range updater object.
  RangeUpdater* mRangeUpdater;
};

} // namespace mozilla

#endif // #ifndef DeleteRangeTransaction_h
