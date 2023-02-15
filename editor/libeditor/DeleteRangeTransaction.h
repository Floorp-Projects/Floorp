/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteRangeTransaction_h
#define DeleteRangeTransaction_h

#include "DeleteContentTransactionBase.h"
#include "EditAggregateTransaction.h"

#include "EditorBase.h"
#include "EditorDOMPoint.h"
#include "EditorForwards.h"

#include "mozilla/RefPtr.h"

#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsIEditor.h"
#include "nsISupportsImpl.h"
#include "nsRange.h"
#include "nscore.h"

class nsINode;

namespace mozilla {

/**
 * A transaction that deletes an entire range in the content tree
 */
class DeleteRangeTransaction final : public EditAggregateTransaction {
 protected:
  DeleteRangeTransaction(EditorBase& aEditorBase,
                         const nsRange& aRangeToDelete);

 public:
  /**
   * Creates a delete range transaction.  This never returns nullptr.
   *
   * @param aEditorBase         The object providing basic editing operations.
   * @param aRangeToDelete      The range to delete.
   */
  static already_AddRefed<DeleteRangeTransaction> Create(
      EditorBase& aEditorBase, const nsRange& aRangeToDelete) {
    RefPtr<DeleteRangeTransaction> transaction =
        new DeleteRangeTransaction(aEditorBase, aRangeToDelete);
    return transaction.forget();
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteRangeTransaction,
                                           EditAggregateTransaction)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(DeleteRangeTransaction)

  void AppendChild(DeleteContentTransactionBase& aTransaction);

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;

  /**
   * Return a good point to put caret after calling DoTransaction().
   */
  EditorDOMPoint SuggestPointToPutCaret() const {
    return mPointToPutCaret.IsSetAndValid() ? mPointToPutCaret
                                            : EditorDOMPoint();
  }

 protected:
  /**
   * Extend the range by adding a surrounding whitespace character to the range
   * that is about to be deleted. This method depends on the pref
   * `layout.word_select.delete_space_after_doubleclick_selection`.
   *
   * Considered cases:
   *   "one [two] three" -> "one [two ]three" -> "one three"
   *   "[one] two" -> "[one ]two" -> "two"
   *   "one [two]" -> "one[ two]" -> "one"
   *   "one [two], three" -> "one[ two], three" -> "one, three"
   *   "one  [two]" -> "one [ two]" -> "one "
   *
   * @param aRange  [inout] The range that is about to be deleted.
   * @return                NS_OK, unless nsRange::SetStart / ::SetEnd fails.
   */
  nsresult MaybeExtendDeletingRangeWithSurroundingWhitespace(
      nsRange& aRange) const;

  /**
   * AppendTransactionsToDeleteIn() creates a DeleteTextTransaction or some
   * DeleteNodeTransactions to remove text or nodes between aStart and aEnd
   * and appends the created transactions to the array.
   *
   * @param aRangeToDelete      Must be positioned, valid and in same container.
   * @return            Returns NS_OK in most cases.
   *                    When the arguments are invalid, returns
   *                    NS_ERROR_INVALID_ARG.
   *                    When mEditorBase isn't available, returns
   *                    NS_ERROR_NOT_AVAILABLE.
   *                    When created DeleteTextTransaction cannot do its
   *                    transaction, returns NS_ERROR_FAILURE.
   *                    Note that even if one of created DeleteNodeTransaction
   *                    cannot do its transaction, this returns NS_OK.
   */
  nsresult AppendTransactionsToDeleteIn(
      const EditorRawDOMRange& aRangeToDelete);

  /**
   * AppendTransactionsToDeleteNodesWhoseEndBoundaryIn() creates
   * DeleteNodeTransaction instances to remove nodes whose end is in the range
   * (in other words, its end tag is in the range if it's an element) and append
   * them to the array.
   *
   * @param aRangeToDelete      Must be positioned and valid.
   */
  nsresult AppendTransactionsToDeleteNodesWhoseEndBoundaryIn(
      const EditorRawDOMRange& aRangeToDelete);

  /**
   * AppendTransactionToDeleteText() creates a DeleteTextTransaction to delete
   * text between start of aPoint.GetContainer() and aPoint or aPoint and end of
   * aPoint.GetContainer() and appends the created transaction to the array.
   *
   * @param aMaybePointInText   Must be set and valid.  If the point is not
   *                            in a text node, this method does nothing.
   * @param aAction     If nsIEditor::eNext, this method creates a transaction
   *                    to delete text from aPoint to the end of the data node.
   *                    Otherwise, this method creates a transaction to delete
   *                    text from start of the data node to aPoint.
   * @return            Returns NS_OK in most cases.
   *                    When the arguments are invalid, returns
   *                    NS_ERROR_INVALID_ARG.
   *                    When mEditorBase isn't available, returns
   *                    NS_ERROR_NOT_AVAILABLE.
   *                    When created DeleteTextTransaction cannot do its
   *                    transaction, returns NS_ERROR_FAILURE.
   *                    Note that even if no character will be deleted,
   *                    this returns NS_OK.
   */
  nsresult AppendTransactionToDeleteText(
      const EditorRawDOMPoint& aMaybePointInText,
      nsIEditor::EDirection aAction);

  // The editor for this transaction.
  RefPtr<EditorBase> mEditorBase;

  // P1 in the range.  This is only non-null until DoTransaction is called and
  // we convert it into child transactions.
  RefPtr<nsRange> mRangeToDelete;

  EditorDOMPoint mPointToPutCaret;
};

}  // namespace mozilla

#endif  // #ifndef DeleteRangeTransaction_h
