/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIEditActionListener_h__
#define nsIEditActionListener_h__
#include "nsISupports.h"
#include "nscore.h"

class nsIDOMNode;

/*
Editor Action Listener interface to outside world
*/

#define NS_IEDITACTIONLISTENER_IID \
{/* B22907B1-EE93-11d2-8D50-000064657374*/ \
0xb22907b1, 0xee93, 0x11d2, \
{ 0x8d, 0x50, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A generic editor action listener interface. 
 * <P>
 * nsIEditActionListener is the interface used by applications wishing to be notified
 * when the editor modifies the DOM tree.
 */
class nsIEditActionListener : public nsISupports{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITACTIONLISTENER_IID; return iid; }

  /** 
   * Called when the editor inserts a node.
   * @param aNode     The DOM Node to insert.
   * @param aParent   The node to insert the new object into
   * @param aPosition The place in aParent to insert the new node
   *                  0=first child, 1=second child, etc.
   *                  any number > number of current children = last child
   */
  NS_IMETHOD InsertNode(nsIDOMNode * aNode,
                        nsIDOMNode * aParent,
                        PRInt32      aPosition)=0;

  /** 
   * Called when the editor deletes a node.
   * @param aChild    The node to delete
   */
  NS_IMETHOD DeleteNode(nsIDOMNode * aChild)=0;

  /** 
   * Called when the editor splits a node.
   * @param aExistingRightNode   the node to split.  It will become the new node's next sibling.
   * @param aOffset              the offset of aExistingRightNode's content|children to do the split at
   * @param aNewLeftNode         [OUT] the new node resulting from the split, becomes aExistingRightNode's previous sibling.
   */
  NS_IMETHOD SplitNode(nsIDOMNode * aExistingRightNode,
                       PRInt32      aOffset,
                       nsIDOMNode * aNewLeftNode)=0;

  /** 
   * Called when the editor joins 2 nodes.
   * @param aNodeToKeep   The left node.  It will remain after the join.
   * @param aNodeToJoin   The right node.
   *                      There is no requirement that the two nodes be of the same type.
   * @param aParent       The parent of aExistingRightNode
   */
  NS_IMETHOD JoinNodes(nsIDOMNode  *aLeftNode,
                       nsIDOMNode  *aRightNode,
                       nsIDOMNode  *aParent)=0;
  
};

#endif //nsIEditActionListener_h__

