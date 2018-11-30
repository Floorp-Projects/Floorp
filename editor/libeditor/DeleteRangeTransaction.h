/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteRangeTransaction_h
#define DeleteRangeTransaction_h

#include "EditAggregateTransaction.h"
#include "mozilla/RangeBoundary.h"
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
class DeleteRangeTransaction final : public EditAggregateTransaction {
 protected:
  DeleteRangeTransaction(EditorBase& aEditorBase, nsRange& aRangeToDelete);

 public:
  /**
   * Creates a delete range transaction.  This never returns nullptr.
   *
   * @param aEditorBase         The object providing basic editing operations.
   * @param aRangeToDelete      The range to delete.
   */
  static already_AddRefed<DeleteRangeTransaction> Create(
      EditorBase& aEditorBase, nsRange& aRangeToDelete) {
    RefPtr<DeleteRangeTransaction> transaction =
        new DeleteRangeTransaction(aEditorBase, aRangeToDelete);
    return transaction.forget();
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteRangeTransaction,
                                           EditAggregateTransaction)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;

 protected:
  /**
   * CreateTxnsToDeleteBetween() creates a DeleteTextTransaction or some
   * DeleteNodeTransactions to remove text or nodes between aStart and aEnd
   * and appends the created transactions to the array.
   *
   * @param aStart      Must be set and valid point.
   * @param aEnd        Must be set and valid point.  Additionally, the
   *                    container must be same as aStart's container.
   *                    And of course, this must not be before aStart in
   *                    the DOM tree order.
   * @return            Returns NS_OK in most cases.
   *                    When the arguments are invalid, returns
   *                    NS_ERROR_INVALID_ARG.
   *                    When mEditorBase isn't available, returns
   *                    NS_ERROR_NOT_AVAIALBLE.
   *                    When created DeleteTextTransaction cannot do its
   *                    transaction, returns NS_ERROR_FAILURE.
   *                    Note that even if one of created DeleteNodeTransaction
   *                    cannot do its transaction, this returns NS_OK.
   */
  nsresult CreateTxnsToDeleteBetween(const RawRangeBoundary& aStart,
                                     const RawRangeBoundary& aEnd);

  nsresult CreateTxnsToDeleteNodesBetween(nsRange* aRangeToDelete);

  /**
   * CreateTxnsToDeleteContent() creates a DeleteTextTransaction to delete
   * text between start of aPoint.GetContainer() and aPoint or aPoint and end of
   * aPoint.GetContainer() and appends the created transaction to the array.
   *
   * @param aPoint      Must be set and valid point.  If the container is not
   *                    a data node, this method does nothing.
   * @param aAction     If nsIEditor::eNext, this method creates a transaction
   *                    to delete text from aPoint to the end of the data node.
   *                    Otherwise, this method creates a transaction to delete
   *                    text from start of the data node to aPoint.
   * @return            Returns NS_OK in most cases.
   *                    When the arguments are invalid, returns
   *                    NS_ERROR_INVALID_ARG.
   *                    When mEditorBase isn't available, returns
   *                    NS_ERROR_NOT_AVAIALBLE.
   *                    When created DeleteTextTransaction cannot do its
   *                    transaction, returns NS_ERROR_FAILURE.
   *                    Note that even if no character will be deleted,
   *                    this returns NS_OK.
   */
  nsresult CreateTxnsToDeleteContent(const RawRangeBoundary& aPoint,
                                     nsIEditor::EDirection aAction);

  // The editor for this transaction.
  RefPtr<EditorBase> mEditorBase;

  // P1 in the range.  This is only non-null until DoTransaction is called and
  // we convert it into child transactions.
  RefPtr<nsRange> mRangeToDelete;
};

}  // namespace mozilla

#endif  // #ifndef DeleteRangeTransaction_h
