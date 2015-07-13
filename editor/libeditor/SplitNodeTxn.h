/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SplitNodeTxn_h__
#define SplitNodeTxn_h__

#include "EditTxn.h"                    // for EditTxn, NS_DECL_EDITTXN
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED
#include "nscore.h"                     // for NS_IMETHOD

class nsEditor;
class nsIContent;
class nsINode;

namespace mozilla {
namespace dom {

/**
 * A transaction that splits a node into two identical nodes, with the children
 * divided between the new nodes.
 */
class SplitNodeTxn : public EditTxn
{
public:
  /** @param aEditor  The provider of core editing operations
    * @param aNode    The node to split
    * @param aOffset  The location within aNode to do the split.
    *                 aOffset may refer to children of aNode, or content of aNode.
    *                 The left node will have child|content 0..aOffset-1.
    */
  SplitNodeTxn(nsEditor& aEditor, nsIContent& aNode, int32_t aOffset);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SplitNodeTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction() override;

  nsIContent* GetNewNode();

protected:
  virtual ~SplitNodeTxn();

  nsEditor& mEditor;

  /** The node to operate upon */
  nsCOMPtr<nsIContent> mExistingRightNode;

  /** The offset into mExistingRightNode where its children are split.  mOffset
    * is the index of the first child in the right node.  -1 means the new node
    * gets no children.
    */
  int32_t mOffset;

  /** The node we create when splitting mExistingRightNode */
  nsCOMPtr<nsIContent> mNewLeftNode;

  /** The parent shared by mExistingRightNode and mNewLeftNode */
  nsCOMPtr<nsINode> mParent;
};

} // namespace dom
} // namespace mozilla

#endif
