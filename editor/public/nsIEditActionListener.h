/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIEditActionListener_h__
#define nsIEditActionListener_h__
#include "nsISupports.h"
#include "nscore.h"

class nsIDOMNode;
class nsString;
class nsIDOMCharacterData;
class nsISelection;

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
 *
 * Note:  this is the wrong class to implement if you are interested in generic
 * change notifications.  For generic notifications, you should implement
 * nsIDocumentObserver.
 */
class nsIEditActionListener : public nsISupports{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITACTIONLISTENER_IID; return iid; }

  /** 
   * Called before the editor creates a node.
   * @param aTag      The tag name of the DOM Node to create.
   * @param aParent   The node to insert the new object into
   * @param aPosition The place in aParent to insert the new node
   *                  0=first child, 1=second child, etc.
   *                  any number > number of current children = last child
   */
  NS_IMETHOD WillCreateNode(const nsString& aTag,
                            nsIDOMNode     *aParent,
                            PRInt32         aPosition)=0;

  /** 
   * Called after the editor creates a node.
   * @param aTag      The tag name of the DOM Node to create.
   * @param aNode     The DOM Node that was created.
   * @param aParent   The node to insert the new object into
   * @param aPosition The place in aParent to insert the new node
   *                  0=first child, 1=second child, etc.
   *                  any number > number of current children = last child
   * @param aResult   The result of the create node operation.
   */
  NS_IMETHOD DidCreateNode(const nsString& aTag,
                           nsIDOMNode     *aNode,
                           nsIDOMNode     *aParent,
                           PRInt32         aPosition,
                           nsresult        aResult)=0;

  /** 
   * Called before the editor inserts a node.
   * @param aNode     The DOM Node to insert.
   * @param aParent   The node to insert the new object into
   * @param aPosition The place in aParent to insert the new node
   *                  0=first child, 1=second child, etc.
   *                  any number > number of current children = last child
   */
  NS_IMETHOD WillInsertNode(nsIDOMNode *aNode,
                            nsIDOMNode *aParent,
                            PRInt32     aPosition)=0;

  /** 
   * Called after the editor inserts a node.
   * @param aNode     The DOM Node to insert.
   * @param aParent   The node to insert the new object into
   * @param aPosition The place in aParent to insert the new node
   *                  0=first child, 1=second child, etc.
   *                  any number > number of current children = last child
   * @param aResult   The result of the insert node operation.
   */
  NS_IMETHOD DidInsertNode(nsIDOMNode *aNode,
                           nsIDOMNode *aParent,
                           PRInt32     aPosition,
                           nsresult    aResult)=0;

  /** 
   * Called before the editor deletes a node.
   * @param aChild    The node to delete
   */
  NS_IMETHOD WillDeleteNode(nsIDOMNode *aChild)=0;

  /** 
   * Called after the editor deletes a node.
   * @param aChild    The node to delete
   * @param aResult   The result of the delete node operation.
   */
  NS_IMETHOD DidDeleteNode(nsIDOMNode *aChild, nsresult aResult)=0;

  /** 
   * Called before the editor splits a node.
   * @param aExistingRightNode   the node to split.  It will become the new node's next sibling.
   * @param aOffset              the offset of aExistingRightNode's content|children to do the split at
   * @param aNewLeftNode         [OUT] the new node resulting from the split, becomes aExistingRightNode's previous sibling.
   */
  NS_IMETHOD WillSplitNode(nsIDOMNode *aExistingRightNode,
                           PRInt32     aOffset)=0;

  /** 
   * Called after the editor splits a node.
   * @param aExistingRightNode   the node to split.  It will become the new node's next sibling.
   * @param aOffset              the offset of aExistingRightNode's content|children to do the split at
   * @param aNewLeftNode         [OUT] the new node resulting from the split, becomes aExistingRightNode's previous sibling.
   */
  NS_IMETHOD DidSplitNode(nsIDOMNode *aExistingRightNode,
                          PRInt32     aOffset,
                          nsIDOMNode *aNewLeftNode,
                          nsresult    aResult)=0;

  /** 
   * Called before the editor joins 2 nodes.
   * @param aLeftNode   This node will be merged into the right node
   * @param aRightNode  The node that will be merged into.
   *                    There is no requirement that the two nodes be of
   *                    the same type.
   * @param aParent     The parent of aRightNode
   */
  NS_IMETHOD WillJoinNodes(nsIDOMNode  *aLeftNode,
                           nsIDOMNode  *aRightNode,
                           nsIDOMNode  *aParent)=0;

  /** 
   * Called after the editor joins 2 nodes.
   * @param aLeftNode   This node will be merged into the right node
   * @param aRightNode  The node that will be merged into.
   *                    There is no requirement that the two nodes be of
   *                    the same type.
   * @param aParent     The parent of aRightNode
   * @param aResult     The result of the join operation.
   */
  NS_IMETHOD DidJoinNodes(nsIDOMNode  *aLeftNode,
                          nsIDOMNode  *aRightNode,
                          nsIDOMNode  *aParent,
                          nsresult    aResult)=0;

  /** 
   * Called before the editor inserts text.
   * @param aTextNode   This node getting inserted text
   * @param aOffset     The offset in aTextNode to insert at.
   * @param aString     The string that gets inserted.
   */
  NS_IMETHOD WillInsertText(nsIDOMCharacterData  *aTextNode,
                            PRInt32               aOffset,
                            const nsString       &aString)=0;

  /** 
   * Called after the editor inserts text.
   * @param aTextNode   This node getting inserted text
   * @param aOffset     The offset in aTextNode to insert at.
   * @param aString     The string that gets inserted.
   * @param aResult     The result of the insert text operation.
   */
  NS_IMETHOD DidInsertText(nsIDOMCharacterData  *aTextNode,
                           PRInt32               aOffset,
                           const nsString       &aString,
                           nsresult              aResult)=0;

  /** 
   * Called before the editor deletes text.
   * @param aTextNode   This node getting text deleted
   * @param aOffset     The offset in aTextNode to delete at.
   * @param aLength     The amount of text to delete.
   */
  NS_IMETHOD WillDeleteText(nsIDOMCharacterData  *aTextNode,
                            PRInt32               aOffset,
                            PRInt32               aLength)=0;

  /** 
   * Called before the editor deletes text.
   * @param aTextNode   This node getting text deleted
   * @param aOffset     The offset in aTextNode to delete at.
   * @param aLength     The amount of text to delete.
   * @param aResult     The result of the delete text operation.
   */
  NS_IMETHOD DidDeleteText(nsIDOMCharacterData  *aTextNode,
                           PRInt32               aOffset,
                           PRInt32               aLength,
                           nsresult              aResult)=0;

  /** 
   * Called before the editor deletes the selection.
   * @param aSelection   The selection to be deleted
   */
  NS_IMETHOD WillDeleteSelection(nsISelection *aSelection)=0;

  /** 
   * Called after the editor deletes the selection.
   * @param aSelection   The selection, after deletion
   */
  NS_IMETHOD DidDeleteSelection(nsISelection *aSelection)=0;
};

#endif //nsIEditActionListener_h__

