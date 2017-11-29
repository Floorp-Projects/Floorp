/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SplitNodeTransaction_h
#define SplitNodeTransaction_h

#include "mozilla/EditorDOMPoint.h"      // for RangeBoundary, EditorRawDOMPoint
#include "mozilla/EditTransactionBase.h" // for EditTxn, etc.
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED
#include "nscore.h"                     // for NS_IMETHOD

class nsIContent;
class nsINode;

namespace mozilla {

class EditorBase;

/**
 * A transaction that splits a node into two identical nodes, with the children
 * divided between the new nodes.
 */
class SplitNodeTransaction final : public EditTransactionBase
{
public:
  /**
   * @param aEditorBase         The provider of core editing operations.
   * @param aStartOfRightNode   The point to split.  Its container will be
   *                            the right node, i.e., become the new node's
   *                            next sibling.  And the point will be start
   *                            of the right node.
   */
  SplitNodeTransaction(EditorBase& aEditorBase,
                       const EditorRawDOMPoint& aStartOfRightNode);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SplitNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;

  nsIContent* GetNewNode();

protected:
  virtual ~SplitNodeTransaction();

  RefPtr<EditorBase> mEditorBase;

  // The container is existing right node (will be split).
  // The point referring this is start of the right node after it's split.
  EditorDOMPoint mStartOfRightNode;

  // The node we create when splitting mExistingRightNode.
  nsCOMPtr<nsIContent> mNewLeftNode;

  // The parent shared by mExistingRightNode and mNewLeftNode.
  nsCOMPtr<nsINode> mParent;
};

} // namespace mozilla

#endif // #ifndef SplitNodeTransaction_h
