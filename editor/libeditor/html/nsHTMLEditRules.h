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

#ifndef nsHTMLEditRules_h__
#define nsHTMLEditRules_h__

#include "nsTextEditRules.h"
#include "nsIEditActionListener.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsISupportsArray;
class nsVoidArray;
class nsIDOMElement;

class nsHTMLEditRules : public nsTextEditRules
{
public:

            nsHTMLEditRules();
  virtual   ~nsHTMLEditRules();

  // nsEditRules methods
  NS_IMETHOD Init(nsHTMLEditor *aEditor, PRUint32 aFlags);
  NS_IMETHOD WillDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, PRBool *aCancel, PRBool *aHandled);
  NS_IMETHOD DidDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);

protected:

  enum RulesEndpoint
  {
    kStart,
    kEnd
  };


  // nsHTMLEditRules implementation methods
  nsresult WillInsert(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillInsertText(  PRInt32          aAction,
                            nsIDOMSelection *aSelection, 
                            PRBool          *aCancel,
                            PRBool          *aHandled,
                            const nsString  *inString,
                            nsString        *outString,
                            TypeInState      typeInState,
                            PRInt32          aMaxLength);
  nsresult WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult WillDeleteSelection(nsIDOMSelection *aSelection, nsIEditor::ESelectionCollapseDirection aAction, 
                               PRBool *aCancel, PRBool *aHandled);
  nsresult WillMakeList(nsIDOMSelection *aSelection, PRBool aOrderd, PRBool *aCancel, PRBool *aHandled);
  nsresult WillRemoveList(nsIDOMSelection *aSelection, PRBool aOrderd, PRBool *aCancel, PRBool *aHandled);
  nsresult WillIndent(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult WillOutdent(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult WillAlign(nsIDOMSelection *aSelection, const nsString *alignType, PRBool *aCancel, PRBool *aHandled);
  nsresult WillMakeBasicBlock(nsIDOMSelection *aSelection, const nsString *aBlockType, PRBool *aCancel, PRBool *aHandled);

  nsresult InsertTab(nsIDOMSelection *aSelection, nsString *outString);
  nsresult InsertSpace(nsIDOMSelection *aSelection, nsString *outString);

  nsresult ReturnInHeader(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);
  nsresult ReturnInParagraph(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset, PRBool *aCancel, PRBool *aHandled);
  nsresult ReturnInListItem(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);

  // helper methods
  static nsresult GetTabAsNBSPs(nsString *outString);
  static nsresult GetTabAsNBSPsAndSpace(nsString *outString);
    
  static PRBool IsHeader(nsIDOMNode *aNode);
  static PRBool IsParagraph(nsIDOMNode *aNode);
  static PRBool IsListItem(nsIDOMNode *aNode);
  static PRBool IsTableCell(nsIDOMNode *aNode);
  static PRBool IsList(nsIDOMNode *aNode);
  static PRBool IsUnorderedList(nsIDOMNode *aNode);
  static PRBool IsOrderedList(nsIDOMNode *aNode);
  static PRBool IsBreak(nsIDOMNode *aNode);
  static PRBool IsBody(nsIDOMNode *aNode);
  static PRBool IsBlockquote(nsIDOMNode *aNode);
  static PRBool IsAnchor(nsIDOMNode *aNode);
  static PRBool IsDiv(nsIDOMNode *aNode);
  static PRBool IsNormalDiv(nsIDOMNode *aNode);
  static PRBool IsMozDiv(nsIDOMNode *aNode);
  static PRBool IsMozBR(nsIDOMNode *aNode);
  static PRBool IsMailCite(nsIDOMNode *aNode);
  static PRBool HasMozAttr(nsIDOMNode *aNode);
  
  static PRBool InBody(nsIDOMNode *aNode);
  
  nsresult IsEmptyBlock(nsIDOMNode *aNode, 
                       PRBool *outIsEmptyBlock, 
                       PRBool aMozBRDoesntCount = PR_FALSE,
                       PRBool aListItemsNotEmpty = PR_FALSE);
  nsresult IsEmptyNode(nsIDOMNode *aNode, 
                       PRBool *outIsEmptyBlock, 
                       PRBool aMozBRDoesntCount = PR_FALSE,
                       PRBool aListItemsNotEmpty = PR_FALSE);
  PRBool IsDescendantOf(nsIDOMNode *aNode, nsIDOMNode *aParent);
  PRBool IsFirstNode(nsIDOMNode *aNode);
  PRBool IsLastNode(nsIDOMNode *aNode);
  PRBool AtStartOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock);
  PRBool AtEndOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock);
  nsresult CreateMozBR(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outBRNode);
  
  nsresult GetPriorHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetPriorHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetPriorHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);

  nsresult GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                            PRInt32 actionID, nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset);
  nsresult GetPromotedRanges(nsIDOMSelection *inSelection, 
                             nsCOMPtr<nsISupportsArray> *outArrayOfRanges, 
                             PRInt32 inOperationType);
  nsresult PromoteRange(nsIDOMRange *inRange, PRInt32 inOperationType);
  nsresult GetNodesForOperation(nsISupportsArray *inArrayOfRanges, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes, 
                                   PRInt32 inOperationType);
  nsresult GetChildNodesForOperation(nsIDOMNode *inNode, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes);
  nsresult MakeTransitionList(nsISupportsArray *inArrayOfNodes, 
                                   nsVoidArray *inTransitionArray);
                                   
  nsresult ShouldMakeEmptyBlock(nsIDOMSelection *aSelection, const nsString *blockTag, PRBool *outMakeEmpty);
  nsresult ApplyBlockStyle(nsISupportsArray *arrayOfNodes, const nsString *aBlockTag);

  nsresult ReplaceContainer(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, const nsString &aNodeType);
  nsresult RemoveContainer(nsIDOMNode *inNode);
  nsresult InsertContainerAbove(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, const nsString &aNodeType);

  nsresult IsFirstEditableChild( nsIDOMNode *aNode, PRBool *aOutIsFirst);
  nsresult IsLastEditableChild( nsIDOMNode *aNode, PRBool *aOutIsLast);
  nsresult GetFirstEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstChild);
  nsresult GetLastEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastChild);

  nsresult JoinNodesSmart( nsIDOMNode *aNodeLeft, 
                           nsIDOMNode *aNodeRight, 
                           nsCOMPtr<nsIDOMNode> *aOutMergeParent, 
                           PRInt32 *aOutMergeOffset);

  nsresult GetTopEnclosingMailCite(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutCiteNode);
  nsresult PopListItem(nsIDOMNode *aListItem, PRBool *aOutOfList);

  nsresult AdjustSpecialBreaks();
  nsresult AdjustWhitespace();
  nsresult AdjustSelection(nsIDOMSelection *aSelection, nsIEditor::ESelectionCollapseDirection aAction);
  nsresult RemoveEmptyNodes();
  nsresult DoTextNodeWhitespace(nsIDOMCharacterData *aTextNode, PRInt32 aStart, PRInt32 aEnd);
  nsresult UpdateDocChangeRange(nsIDOMRange *aRange);

  nsresult ConvertWhitespace(const nsString & inString, const nsString & outString);

// removed from use:
#if 0
  nsresult AddTrailerBR(nsIDOMNode *aNode);
  nsresult CreateMozDiv(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outDiv);
#endif

// data members
protected:
  nsCOMPtr<nsIEditActionListener>   mListener;
  nsCOMPtr<nsIDOMRange> mDocChangeRange;
  
// friends
  friend class nsHTMLEditListener;
  friend nsresult NS_NewEditListener(nsIEditActionListener **aResult, 
                                     nsHTMLEditor *htmlEditor,
                                     nsHTMLEditRules *htmlRules);

};

class nsHTMLEditListener : public nsIEditActionListener
{
public:
  nsHTMLEditListener(nsHTMLEditor *htmlEditor, nsHTMLEditRules *htmlRules);
  virtual ~nsHTMLEditListener();
  NS_DECL_ISUPPORTS
 
  // nsIEditActionListener methods
  
  NS_IMETHOD WillCreateNode(const nsString& aTag, nsIDOMNode *aParent, PRInt32 aPosition);
  NS_IMETHOD DidCreateNode(const nsString& aTag, nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult);
  NS_IMETHOD WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition);
  NS_IMETHOD DidInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult);
  NS_IMETHOD WillDeleteNode(nsIDOMNode *aChild);
  NS_IMETHOD DidDeleteNode(nsIDOMNode *aChild, nsresult aResult);
  NS_IMETHOD WillSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset);
  NS_IMETHOD DidSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset, nsIDOMNode *aNewLeftNode, nsresult aResult);
  NS_IMETHOD WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent);
  NS_IMETHOD DidJoinNodes(nsIDOMNode  *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent, nsresult aResult);
  NS_IMETHOD WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsString &aString);
  NS_IMETHOD DidInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsString &aString, nsresult aResult);
  NS_IMETHOD WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength);
  NS_IMETHOD DidDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength, nsresult aResult);

protected:

  nsresult MakeRangeFromNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMRange> *outRange);
  nsresult MakeRangeFromTextOffsets(nsIDOMCharacterData *inNode, 
                                    PRInt32 inStart, 
                                    PRInt32 inEnd, 
                                    nsCOMPtr<nsIDOMRange> *outRange);
  PRBool IsDescendantOfBody(nsIDOMNode *inNode) ;
  
// data members
  nsHTMLEditor     *mEditor;
  nsHTMLEditRules  *mRules;
  nsCOMPtr<nsIDOMNode> mBody;
};

#endif //nsHTMLEditRules_h__

