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

#ifndef nsTextEditor_h__
#define nsTextEditor_h__

#include "nsITextEditor.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsEditor.h"
#include "nsTextEditRules.h"
#include "TypeInState.h"
#include "nsString.h"

class nsIStyleContext;
class nsIDOMRange;
class nsIDOMNode;

/**
 * The text editor implementation.<br>
 * Use to edit text represented as a DOM tree. 
 * This class is used for editing both plain text and rich text (attributed text).
 */
class nsTextEditor : public nsEditor, public nsITextEditor
{
public:
  // see nsITextEditor for documentation

//Interfaces for addref and release and queryinterface
//NOTE macro used is for classes that inherit from 
// another class. Only the base class should use NS_DECL_ISUPPORTS
  NS_DECL_ISUPPORTS_INHERITED

  nsTextEditor();
  virtual ~nsTextEditor();

//Initialization
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell);

// Editing Operations
  NS_IMETHOD SetTextProperty(nsIAtom        *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue);
  NS_IMETHOD GetTextProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAny, PRBool &aAll);
  NS_IMETHOD RemoveTextProperty(nsIAtom *aProperty, const nsString *aAttribute);
  NS_IMETHOD DeleteSelection(nsIEditor::Direction aDir);
  NS_IMETHOD InsertText(const nsString& aStringToInsert);
  NS_IMETHOD InsertBreak();

// Transaction control
  NS_IMETHOD EnableUndo(PRBool aEnable);
  NS_IMETHOD Undo(PRUint32 aCount);
  NS_IMETHOD CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo);
  NS_IMETHOD Redo(PRUint32 aCount);
  NS_IMETHOD CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo);
  NS_IMETHOD BeginTransaction();
  NS_IMETHOD EndTransaction();

// Selection and navigation -- exposed here for convenience
  NS_IMETHOD MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection); 
  NS_IMETHOD SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD SelectAll();
  NS_IMETHOD ScrollUp(nsIAtom *aIncrement);
  NS_IMETHOD ScrollDown(nsIAtom *aIncrement);
  NS_IMETHOD ScrollIntoView(PRBool aScrollToBegin);

// cut, copy & paste
  NS_IMETHOD Cut();
  NS_IMETHOD Copy();
  NS_IMETHOD Paste();

// Input/Output
  NS_IMETHOD Insert(nsString& aInputString);
  NS_IMETHOD OutputText(nsString& aOutputString);
  NS_IMETHOD OutputHTML(nsString& aOutputString);


protected:

// rules initialization

  virtual void  InitRules();
  
// Utility Methods

  /** content-based query returns PR_TRUE if <aProperty aAttribute=aValue> effects aNode
    * If <aProperty aAttribute=aValue> contains aNode, 
    * but <aProperty aAttribute=SomeOtherValue> also contains aNode and the second is
    * more deeply nested than the first, then the first does not effect aNode.
    *
    * @param aNode      The target of the query
    * @param aProperty  The property that we are querying for
    * @param aAttribute The attribute of aProperty, example: color in <FONT color="blue">
    *                   May be null.
    * @param aValue     The value of aAttribute, example: blue in <FONT color="blue">
    *                   May be null.  Ignored if aAttribute is null.
    * @param aIsSet     [OUT] PR_TRUE if <aProperty aAttribute=aValue> effects aNode.
    * @param aStyleNode [OUT] set to the node representing <aProperty aAttribute=aValue>, if found.
    *                   null if aIsSet is returned as PR_FALSE;
    */
  virtual void IsTextPropertySetByContent(nsIDOMNode     *aNode,
                                          nsIAtom        *aProperty, 
                                          const nsString *aAttribute,
                                          const nsString *aValue,
                                          PRBool         &aIsSet,
                                          nsIDOMNode    **aStyleNode) const;

  /** style-based query returns PR_TRUE if (aProperty, aAttribute) is set in aSC.
    * WARNING: not well tested yet since we don't do style-based queries anywhere.
    */
  virtual void IsTextStyleSet(nsIStyleContext *aSC, 
                              nsIAtom         *aProperty, 
                              const nsString  *aAttributes, 
                              PRBool          &aIsSet) const;

  /** returns PR_TRUE in out-param aIsInline if aNode is inline as defined by HTML DTD */
  NS_IMETHOD IsNodeInline(nsIDOMNode *aNode, PRBool &aIsInline) const;

  /** returns the closest block parent of aNode, not including aNode itself.
    * can return null, for example if aNode is in a document fragment.
    * @param aNode        The node whose parent we seek.
    * @param aBlockParent [OUT] The block parent, if any.
    * @return a success value unless an unexpected error occurs.
    */
  NS_IMETHOD GetBlockParent(nsIDOMNode *aNode, nsIDOMElement **aBlockParent) const;

  NS_IMETHOD GetBlockDelimitedContent(nsIDOMNode *aParent, nsIDOMNode *aChild,
                                      nsIDOMNode **aLeftNode, nsIDOMNode **aRightNode) const;

  /** returns PR_TRUE in out-param aResult if all nodes between (aStartNode, aStartOffset)
    * and (aEndNode, aEndOffset) are inline as defined by HTML DTD. 
    */
  NS_IMETHOD IntermediateNodesAreInline(nsIDOMRange  *aRange,
                                        nsIDOMNode   *aStartNode, 
                                        PRInt32       aStartOffset, 
                                        nsIDOMNode   *aEndNode,
                                        PRInt32       aEndOffset,
                                        PRBool       &aResult) const;

  /** returns the number of things inside aNode in the out-param aCount.  
    * If aNode is text, returns number of characters. 
    * If not, returns number of children nodes.
    */
  NS_IMETHOD GetLengthOfDOMNode(nsIDOMNode *aNode, PRUint32 &aCount) const;

  /** Moves the content between (aNode, aStartOffset) and (aNode, aEndOffset)
    * into aNewParentNode, splitting aNode as necessary to maintain the relative
    * position of all leaf content.
    * @param aNode          The node whose content we're repositioning.
    *                       aNode can be either a text node or a container node.
    * @param aNewParentNode The node that will be the repositioned contents' parent.
    *                       The caller is responsible for allocating aNewParentNode
    * @param aStartOffset   The start offset of the content of aNode
    * @param aEndOffset     The end offset of the content of aNode.
    */
  NS_IMETHOD MoveContentOfNodeIntoNewParent(nsIDOMNode  *aNode, 
                                            nsIDOMNode  *aNewParentNode, 
                                            PRInt32      aStartOffset, 
                                            PRInt32      aEndOffset);

  /** Moves the content between (aStartNode, aStartOffset) and (aEndNode, aEndOffset)
    * into aNewParentNode, splitting aStartNode and aEndNode as necessary to maintain 
    * the relative position of all leaf content.
    * The content between the two endpoints MUST be "contiguous" in the sense that 
    * it is all in the same block.  Another way of saying this is all content nodes
    * between aStartNode and aEndNode must be inline. 
    * @see IntermediateNodesAreInline
    *
    * @param aStartNode       The left node,  can be either a text node or a container node.
    * @param aStartOffset     The start offset in the content of aStartNode
    * @param aEndNode         The right node,  can be either a text node or a container node.
    * @param aEndOffset       The end offset in the content of aEndNode.
    * @param aGrandParentNode The common ancestor of aStartNode and aEndNode.
    *                         aGrandParentNode will be the parent of aNewParentNode.
    * @param aNewParentNode   The node that will be the repositioned contents' parent.
    *                         The caller is responsible for allocating aNewParentNode
    */
  NS_IMETHOD MoveContiguousContentIntoNewParent(nsIDOMNode *aStartNode, 
                                                PRInt32     aStartOffset, 
                                                nsIDOMNode *aEndNode, 
                                                PRInt32     aEndOffset, 
                                                nsIDOMNode *aGrandParentNode,
                                                nsIDOMNode *aNewParentNode);


  NS_IMETHOD SetTextPropertiesForNode(nsIDOMNode  *aNode, 
                                      nsIDOMNode  *aParent,
                                      PRInt32      aStartOffset,
                                      PRInt32      aEndOffset,
                                      nsIAtom     *aPropName,
                                      const nsString *aAttribute,
                                      const nsString *aValue);

  NS_IMETHOD SetTextPropertiesForNodesWithSameParent(nsIDOMNode  *aStartNode,
                                                     PRInt32      aStartOffset,
                                                     nsIDOMNode  *aEndNode,
                                                     PRInt32      aEndOffset,
                                                     nsIDOMNode  *aParent,
                                                     nsIAtom     *aPropName,
                                                     const nsString *aAttribute,
                                                     const nsString *aValue);

  NS_IMETHOD SetTextPropertiesForNodeWithDifferentParents(nsIDOMRange *aRange,
                                                          nsIDOMNode  *aStartNode,
                                                          PRInt32      aStartOffset,
                                                          nsIDOMNode  *aEndNode,
                                                          PRInt32      aEndOffset,
                                                          nsIDOMNode  *aParent,
                                                          nsIAtom     *aPropName,
                                                          const nsString *aAttribute,
                                                          const nsString *aValue);

  NS_IMETHOD RemoveTextPropertiesForNode(nsIDOMNode  *aNode, 
                                         nsIDOMNode  *aParent,
                                         PRInt32      aStartOffset,
                                         PRInt32      aEndOffset,
                                         nsIAtom     *aPropName,
                                         const nsString *aAttribute);

  NS_IMETHOD RemoveTextPropertiesForNodesWithSameParent(nsIDOMNode  *aStartNode,
                                                        PRInt32      aStartOffset,
                                                        nsIDOMNode  *aEndNode,
                                                        PRInt32      aEndOffset,
                                                        nsIDOMNode  *aParent,
                                                        nsIAtom     *aPropName, 
                                                        const nsString *aAttribute);

  NS_IMETHOD RemoveTextPropertiesForNodeWithDifferentParents(nsIDOMNode  *aStartNode,
                                                             PRInt32      aStartOffset,
                                                             nsIDOMNode  *aEndNode,
                                                             PRInt32      aEndOffset,
                                                             nsIDOMNode  *aParent,
                                                             nsIAtom     *aPropName,
                                                             const nsString *aAttribute);

  NS_IMETHOD SetTypeInStateForProperty(TypeInState &aTypeInState, 
                                       nsIAtom     *aPropName, 
                                       const nsString *aAttribute,
                                       const nsString *aValue);
  
  TypeInState GetTypeInState() { return mTypeInState;}


// Data members
protected:
  TypeInState      mTypeInState;  // xxx - isn't it wrong to have xpcom classes as members?  shouldn't it be a pointer?
  nsTextEditRules* mRules;
  nsCOMPtr<nsIDOMEventListener> mKeyListenerP;
  nsCOMPtr<nsIDOMEventListener> mMouseListenerP;
  nsCOMPtr<nsIDOMEventListener> mTextListenerP;
  nsCOMPtr<nsIDOMEventListener> mDragListenerP;

// friends
friend class nsTextEditRules;
};

#endif //nsTextEditor_h__

