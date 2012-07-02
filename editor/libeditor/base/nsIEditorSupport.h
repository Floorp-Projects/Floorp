/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIEditorSupport_h__
#define nsIEditorSupport_h__
#include "nsISupports.h"

class nsIDOMNode;

/*
Private Editor interface for a class that can provide helper functions
*/

#define NS_IEDITORSUPPORT_IID \
{/* c4cbcda8-58ec-4f03-9c99-5e46b6828b7a*/ \
0xc4cbcda8, 0x58ec, 0x4f03, \
{0x0c, 0x99, 0x5e, 0x46, 0xb6, 0x82, 0x8b, 0x7a} }


/**
 */
class nsIEditorSupport  : public nsISupports {

public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IEDITORSUPPORT_IID)

  /** 
   * SplitNode() creates a new node identical to an existing node, and split the contents between the two nodes
   * @param aExistingRightNode   the node to split.  It will become the new node's next sibling.
   * @param aOffset              the offset of aExistingRightNode's content|children to do the split at
   * @param aNewLeftNode         [OUT] the new node resulting from the split, becomes aExistingRightNode's previous sibling.
   * @param aParent              the parent of aExistingRightNode
   */
  NS_IMETHOD SplitNodeImpl(nsIDOMNode * aExistingRightNode,
                           PRInt32      aOffset,
                           nsIDOMNode * aNewLeftNode,
                           nsIDOMNode * aParent)=0;

  /** 
   * JoinNodes() takes 2 nodes and merge their content|children.
   * @param aNodeToKeep   The node that will remain after the join.
   * @param aNodeToJoin   The node that will be joined with aNodeToKeep.
   *                      There is no requirement that the two nodes be of the same type.
   * @param aParent       The parent of aExistingRightNode
   * @param aNodeToKeepIsFirst  if true, the contents|children of aNodeToKeep come before the
   *                            contents|children of aNodeToJoin, otherwise their positions are switched.
   */
  NS_IMETHOD JoinNodesImpl(nsIDOMNode *aNodeToKeep,
                           nsIDOMNode  *aNodeToJoin,
                           nsIDOMNode  *aParent,
                           bool         aNodeToKeepIsFirst)=0;

  static PRInt32 GetChildOffset(nsIDOMNode* aChild, nsIDOMNode* aParent);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIEditorSupport, NS_IEDITORSUPPORT_IID)

#endif //nsIEditorSupport_h__

