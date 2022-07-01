/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SplitNodeTransaction_h
#define SplitNodeTransaction_h

#include "EditorForwards.h"
#include "EditTransactionBase.h"  // for EditorTransactionBase

#include "nsCOMPtr.h"  // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsISupportsImpl.h"  // for NS_DECL_ISUPPORTS_INHERITED
#include "nscore.h"           // for NS_IMETHOD

namespace mozilla {

/**
 * A transaction that splits a node into two identical nodes, with the children
 * divided between the new nodes.
 */
class SplitNodeTransaction final : public EditTransactionBase {
 private:
  template <typename PT, typename CT>
  SplitNodeTransaction(HTMLEditor& aHTMLEditor,
                       const EditorDOMPointBase<PT, CT>& aStartOfRightContent);

 public:
  /**
   * Creates a transaction to create a new node identical to an existing node,
   * and split the contents between the same point in both nodes.
   *
   * @param aHTMLEditor             The provider of core editing operations.
   * @param aStartOfRightContent    The point to split.  Its container will be
   *                                split, and its preceding or following
   *                                content will be moved to the new node.  And
   *                                the point will be start of the right node or
   *                                end of the left node.
   */
  template <typename PT, typename CT>
  static already_AddRefed<SplitNodeTransaction> Create(
      HTMLEditor& aHTMLEditor,
      const EditorDOMPointBase<PT, CT>& aStartOfRightContent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SplitNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(SplitNodeTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;

  // Note that we don't support join/split node direction switching per
  // transaction.
  [[nodiscard]] SplitNodeDirection GetSplitNodeDirection() const;
  [[nodiscard]] JoinNodesDirection GetJoinNodesDirection() const;

  nsIContent* GetSplitContent() const { return mSplitContent; }
  nsIContent* GetNewContent() const { return mNewContent; }
  nsINode* GetParentNode() const { return mParentNode; }

  // The split offset.  At undoing, this is recomputed with tracking the
  // first child of mSplitContent.
  uint32_t SplitOffset() const { return mSplitOffset; }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const SplitNodeTransaction& aTransaction);

 protected:
  virtual ~SplitNodeTransaction() = default;

  MOZ_CAN_RUN_SCRIPT SplitNodeResult
  DoTransactionInternal(HTMLEditor& aHTMLEditor, nsIContent& aSplittingContent,
                        nsIContent& aNewContent, uint32_t aSplitOffset);

  RefPtr<HTMLEditor> mHTMLEditor;

  // The node which should be parent of both mNewContent and mSplitContent.
  nsCOMPtr<nsINode> mParentNode;

  // The node we create when splitting mSplitContent.
  nsCOMPtr<nsIContent> mNewContent;

  // The content node which we split.
  nsCOMPtr<nsIContent> mSplitContent;

  // The offset where we split in mSplitContent.  This is required for doing and
  // redoing.  Therefore, this is updated when undoing.
  uint32_t mSplitOffset;
};

}  // namespace mozilla

#endif  // #ifndef SplitNodeTransaction_h
