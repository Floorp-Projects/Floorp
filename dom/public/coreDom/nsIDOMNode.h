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

#ifndef nsIDOMNode_h__
#define nsIDOMNode_h__

#include "nsDOM.h"
#include "nsISupports.h"

// forward declaration
class nsIDOMNodeIterator;

#define NS_IDOMNODE_IID \
{ /* 8f6bca74-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca74, 0xce42, 0x11d1, \
{0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * The Node object is the primary datatype for the entire Document
 * Object Model.  It represents a single node in the document
 * tree. Nodes may have, but are not required to have, an arbitrary
 * number of child nodes.  */
class nsIDOMNode : public nsISupports {
public:
  // NodeType
  enum NodeType {
    DOCUMENT             = 1,
    ELEMENT              = 2,
    ATTRIBUTE            = 3,
    PI                   = 4,
    COMMENT              = 5,
    TEXT                 = 6
  };

  /**
   * Returns an indication of the underlying Node object's type.
   *
   * @param aType [out]     The type of the node.
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetNodeType(PRInt32 *aType) = 0;

  /**
   * Returns the parent of the given Node instance. If this node is
   * the root of the document object tree, or if the node has not been
   * added to a document tree, null is returned.
   *
   * @param aNode [out]     The parent of the node.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetParentNode(nsIDOMNode **aNode) = 0;

  /**
   * Returns a NodeIterator object that will enumerate all children
   * of this node.  If there are no children, an iterator that will
   * return no nodes is returned.  The content of the returned
   * NodeIterator is "live" in the sense that changes to the children
   * of the Node object that it was created from will be immediately
   * reflected in the nodes returned by the iterator; it is not a
   * static snapshot of the content of the Node. Similarly, changes
   * made to the nodes returned by the iterator will be immediately
   * reflected in the tree, including the set of children of the Node
   * that the NodeIterator was created from.
   *
   * @param aIterator [out]   An iterator through the children of the node.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetChildNodes(nsIDOMNodeIterator **aIterator) = 0;
  
  /**
   * Returns true if the node has any children, false if the node has
   * no children at all.
   *
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD HasChildNodes() = 0;

  /**
   * Returns the first child of a node. If there is no such node,
   * null is returned.
   *
   * @param aNode [out]     The first child of the node, or null.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetFirstChild(nsIDOMNode **aNode) = 0;
  
  /**
   * Returns the node immediately preceding the current node in a
   * breadth-first traversal of the tree. If there is no such node,
   * null is returned.
   *
   * @param aNode [out]     The the node immediately preceeding, or null.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetPreviousSibling(nsIDOMNode **aNode) = 0;

  /**
   * Returns the node immediately following the current node in a
   * breadth-first traversal of the tree. If there is no such node,
   * null is returned.
   *
   * @param aNode [out]     The node immediately following, or null.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetNextSibling(nsIDOMNode **aNode) = 0;
  
  /**
   * Inserts a child node (newChildbefore the existing child node refChild. 
   * If refChild is null, insert newChild at the end of the list of children. 
   *
   * @param newChild [in]   The node to be inserted
   * @param refChild [in]   The node before which the new node will be inserted
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild) = 0;
  
  /**
   * Replaces the child node oldChild with newChild in the set of
   * children of the given node, and return the oldChild node.
   *
   * @param newChild [in]   The node to be inserted
   * @param oldChild [in]   The node to be replaced
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD ReplaceChild(nsIDOMNode *newChild, nsIDOMNode *oldChild) = 0;

  /**
   * Removes the child node indicated by oldChild from the list of
   * children and returns it.
   *
   * @param oldChild [in]   The node to be deleted
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD RemoveChild(nsIDOMNode *oldChild) = 0;
};

#endif // nsIDOMNode_h__

