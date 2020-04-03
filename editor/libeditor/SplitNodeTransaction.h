/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SplitNodeTransaction_h
#define SplitNodeTransaction_h

#include "mozilla/EditorDOMPoint.h"  // for RangeBoundary, EditorRawDOMPoint
#include "mozilla/EditTransactionBase.h"  // for EditTxn, etc.
#include "nsCOMPtr.h"                     // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsISupportsImpl.h"  // for NS_DECL_ISUPPORTS_INHERITED
#include "nscore.h"           // for NS_IMETHOD

namespace mozilla {

class EditorBase;

/**
 * A transaction that splits a node into two identical nodes, with the children
 * divided between the new nodes.
 */
class SplitNodeTransaction final : public EditTransactionBase {
 private:
  template <typename PT, typename CT>
  SplitNodeTransaction(EditorBase& aEditorBase,
                       const EditorDOMPointBase<PT, CT>& aStartOfRightContent);

 public:
  /**
   * Creates a transaction to create a new node (left node) identical to an
   * existing node (right node), and split the contents between the same point
   * in both nodes.
   *
   * @param aEditorBase             The provider of core editing operations.
   * @param aStartOfRightContent    The point to split.  Its container will be
   *                                the right node, i.e., become the new node's
   *                                next sibling.  And the point will be start
   *                                of the right node.
   */
  template <typename PT, typename CT>
  static already_AddRefed<SplitNodeTransaction> Create(
      EditorBase& aEditorBase,
      const EditorDOMPointBase<PT, CT>& aStartOfRightContent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SplitNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;

  nsIContent* GetNewLeftContent() const { return mNewLeftContent; }

 protected:
  virtual ~SplitNodeTransaction() = default;

  RefPtr<EditorBase> mEditorBase;

  // The container is existing right node (will be split).
  // The point referring this is start of the right node after it's split.
  EditorDOMPoint mStartOfRightContent;

  // The node we create when splitting mExistingRightContent.
  nsCOMPtr<nsIContent> mNewLeftContent;

  // The parent shared by mExistingRightContent and mNewLeftContent.
  nsCOMPtr<nsINode> mContainerParentNode;
};

}  // namespace mozilla

#endif  // #ifndef SplitNodeTransaction_h
