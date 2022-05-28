/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MoveNodeTransaction_h
#define MoveNodeTransaction_h

#include "EditTransactionBase.h"  // for EditTransactionBase, etc.

#include "EditorForwards.h"

#include "nsCOMPtr.h"  // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"       // for nsIContent
#include "nsISupportsImpl.h"  // for NS_DECL_ISUPPORTS_INHERITED

namespace mozilla {

/**
 * A transaction that moves a content node to a specified point.
 */
class MoveNodeTransaction final : public EditTransactionBase {
 protected:
  template <typename PT, typename CT>
  MoveNodeTransaction(HTMLEditor& aHTMLEditor, nsIContent& aContentToMove,
                      const EditorDOMPointBase<PT, CT>& aPointToInsert);

 public:
  /**
   * Create a transaction for moving aContentToMove before the child at
   * aPointToInsert.
   *
   * @param aHTMLEditor         The editor which manages the transaction.
   * @param aContentToMove      The node to be moved.
   * @param aPointToInsert      The insertion point of aContentToMove.
   * @return                    A MoveNodeTransaction which was initialized
   *                            with the arguments.
   */
  template <typename PT, typename CT>
  static already_AddRefed<MoveNodeTransaction> MaybeCreate(
      HTMLEditor& aHTMLEditor, nsIContent& aContentToMove,
      const EditorDOMPointBase<PT, CT>& aPointToInsert);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MoveNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(MoveNodeTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;

  /**
   * SuggestPointToPutCaret() suggests a point after doing or redoing the
   * transaction.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType SuggestPointToPutCaret() const {
    if (MOZ_UNLIKELY(!mContainer || !mContentToMove)) {
      return EditorDOMPointType();
    }
    return EditorDOMPointType::After(mContentToMove);
  }

  /**
   * Suggest next insertion point if the caller wants to move another content
   * node around the insertion point.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType SuggestNextInsertionPoint() const {
    if (MOZ_UNLIKELY(!mContainer)) {
      return EditorDOMPointType();
    }
    if (!mReference) {
      return EditorDOMPointType::AtEndOf(mContainer);
    }
    if (MOZ_UNLIKELY(mReference->GetParentNode() != mContainer)) {
      if (MOZ_LIKELY(mContentToMove->GetParentNode() == mContainer)) {
        return EditorDOMPointType(mContentToMove).NextPoint();
      }
      return EditorDOMPointType::AtEndOf(mContainer);
    }
    return EditorDOMPointType(mReference);
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const MoveNodeTransaction& aTransaction);

 protected:
  virtual ~MoveNodeTransaction() = default;

  MOZ_CAN_RUN_SCRIPT nsresult DoTransactionInternal();

  // The content which will be or was moved from mOldContainer to mContainer.
  nsCOMPtr<nsIContent> mContentToMove;

  // mContainer is new container of mContentToInsert after (re-)doing the
  // transaction.
  nsCOMPtr<nsINode> mContainer;

  // mReference is the child content where mContentToMove should be or was
  // inserted into the mContainer.  This is typically the next sibling of
  // mContentToMove after moving.
  nsCOMPtr<nsIContent> mReference;

  // mOldContainer is the original container of mContentToMove before moving.
  nsCOMPtr<nsINode> mOldContainer;

  // mOldNextSibling is the next sibling of mCOntentToMove before moving.
  nsCOMPtr<nsIContent> mOldNextSibling;

  // The editor for this transaction.
  RefPtr<HTMLEditor> mHTMLEditor;
};

}  // namespace mozilla

#endif  // #ifndef MoveNodeTransaction_h
