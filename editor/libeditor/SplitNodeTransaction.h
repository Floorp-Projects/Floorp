/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SplitNodeTransaction_h
#define SplitNodeTransaction_h

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
   * @param aEditorBase The provider of core editing operations
   * @param aNode       The node to split
   * @param aOffset     The location within aNode to do the split.  aOffset may
   *                    refer to children of aNode, or content of aNode.  The
   *                    left node will have child|content 0..aOffset-1.
   */
  SplitNodeTransaction(EditorBase& aEditorBase, nsIContent& aNode,
                       int32_t aOffset);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SplitNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;

  nsIContent* GetNewNode();

protected:
  virtual ~SplitNodeTransaction();

  EditorBase& mEditorBase;

  // The node to operate upon.
  nsCOMPtr<nsIContent> mExistingRightNode;

  // The offset into mExistingRightNode where its children are split.  mOffset
  // is the index of the first child in the right node.  -1 means the new node
  // gets no children.
  int32_t mOffset;

  // The node we create when splitting mExistingRightNode.
  nsCOMPtr<nsIContent> mNewLeftNode;

  // The parent shared by mExistingRightNode and mNewLeftNode.
  nsCOMPtr<nsINode> mParent;
};

} // namespace mozilla

#endif // #ifndef SplitNodeTransaction_h
