/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <glazman@netscape.com>
 *   Sebastian Kromp <46b@gulli.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHTMLEditRules_h__
#define nsHTMLEditRules_h__

#include "nsTextEditRules.h"
#include "nsIHTMLEditor.h"
#include "nsIEditActionListener.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsEditorUtils.h"
#include "TypeInState.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"

class nsIDOMElement;
class nsIEditor;
class nsHTMLEditor;

struct StyleCache : public PropItem
{
  bool mPresent;
  
  StyleCache() : PropItem(), mPresent(PR_FALSE) {
    MOZ_COUNT_CTOR(StyleCache);
  }

  StyleCache(nsIAtom *aTag, const nsAString &aAttr, const nsAString &aValue) : 
             PropItem(aTag, aAttr, aValue), mPresent(PR_FALSE) {
    MOZ_COUNT_CTOR(StyleCache);
  }

  ~StyleCache() {
    MOZ_COUNT_DTOR(StyleCache);
  }
};


#define SIZE_STYLE_TABLE 19

class nsHTMLEditRules : public nsTextEditRules, public nsIEditActionListener
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  
            nsHTMLEditRules();
  virtual   ~nsHTMLEditRules();


  // nsIEditRules methods
  NS_IMETHOD Init(nsPlaintextEditor *aEditor);
  NS_IMETHOD DetachEditor();
  NS_IMETHOD BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection);
  NS_IMETHOD AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection);
  NS_IMETHOD WillDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, bool *aCancel, bool *aHandled);
  NS_IMETHOD DidDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
  NS_IMETHOD DocumentModified();

  nsresult GetListState(bool *aMixed, bool *aOL, bool *aUL, bool *aDL);
  nsresult GetListItemState(bool *aMixed, bool *aLI, bool *aDT, bool *aDD);
  nsresult GetIndentState(bool *aCanIndent, bool *aCanOutdent);
  nsresult GetAlignment(bool *aMixed, nsIHTMLEditor::EAlignment *aAlign);
  nsresult GetParagraphState(bool *aMixed, nsAString &outFormat);
  nsresult MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode);

  // nsIEditActionListener methods
  
  NS_IMETHOD WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, PRInt32 aPosition);
  NS_IMETHOD DidCreateNode(const nsAString& aTag, nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult);
  NS_IMETHOD WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition);
  NS_IMETHOD DidInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult);
  NS_IMETHOD WillDeleteNode(nsIDOMNode *aChild);
  NS_IMETHOD DidDeleteNode(nsIDOMNode *aChild, nsresult aResult);
  NS_IMETHOD WillSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset);
  NS_IMETHOD DidSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset, nsIDOMNode *aNewLeftNode, nsresult aResult);
  NS_IMETHOD WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent);
  NS_IMETHOD DidJoinNodes(nsIDOMNode  *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent, nsresult aResult);
  NS_IMETHOD WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString);
  NS_IMETHOD DidInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString, nsresult aResult);
  NS_IMETHOD WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength);
  NS_IMETHOD DidDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength, nsresult aResult);
  NS_IMETHOD WillDeleteRange(nsIDOMRange *aRange);
  NS_IMETHOD DidDeleteRange(nsIDOMRange *aRange);
  NS_IMETHOD WillDeleteSelection(nsISelection *aSelection);
  NS_IMETHOD DidDeleteSelection(nsISelection *aSelection);

protected:

  enum RulesEndpoint
  {
    kStart,
    kEnd
  };

  enum BRLocation
  {
    kBeforeBlock,
    kBlockEnd
  };

  // nsHTMLEditRules implementation methods
  nsresult WillInsert(nsISelection *aSelection, bool *aCancel);
#ifdef XXX_DEAD_CODE
  nsresult DidInsert(nsISelection *aSelection, nsresult aResult);
#endif
  nsresult WillInsertText(  PRInt32          aAction,
                            nsISelection *aSelection, 
                            bool            *aCancel,
                            bool            *aHandled,
                            const nsAString *inString,
                            nsAString       *outString,
                            PRInt32          aMaxLength);
  nsresult WillLoadHTML(nsISelection *aSelection, bool *aCancel);
  nsresult WillInsertBreak(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult StandardBreakImpl(nsIDOMNode *aNode, PRInt32 aOffset, nsISelection *aSelection);
  nsresult DidInsertBreak(nsISelection *aSelection, nsresult aResult);
  nsresult SplitMailCites(nsISelection *aSelection, bool aPlaintext, bool *aHandled);
  nsresult WillDeleteSelection(nsISelection *aSelection, nsIEditor::EDirection aAction, 
                               bool *aCancel, bool *aHandled);
  nsresult DidDeleteSelection(nsISelection *aSelection, 
                              nsIEditor::EDirection aDir, 
                              nsresult aResult);
  nsresult InsertBRIfNeeded(nsISelection *aSelection);
  nsresult GetGoodSelPointForNode(nsIDOMNode *aNode, nsIEditor::EDirection aAction, 
                                  nsCOMPtr<nsIDOMNode> *outSelNode, PRInt32 *outSelOffset);
  nsresult JoinBlocks(nsCOMPtr<nsIDOMNode> *aLeftBlock, nsCOMPtr<nsIDOMNode> *aRightBlock, bool *aCanceled);
  nsresult MoveBlock(nsIDOMNode *aLeft, nsIDOMNode *aRight, PRInt32 aLeftOffset, PRInt32 aRightOffset);
  nsresult MoveNodeSmart(nsIDOMNode *aSource, nsIDOMNode *aDest, PRInt32 *aOffset);
  nsresult MoveContents(nsIDOMNode *aSource, nsIDOMNode *aDest, PRInt32 *aOffset);
  nsresult DeleteNonTableElements(nsIDOMNode *aNode);
  nsresult WillMakeList(nsISelection *aSelection, const nsAString *aListType, bool aEntireList, const nsAString *aBulletType, bool *aCancel, bool *aHandled, const nsAString *aItemType=nsnull);
  nsresult WillRemoveList(nsISelection *aSelection, bool aOrderd, bool *aCancel, bool *aHandled);
  nsresult WillIndent(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult WillCSSIndent(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult WillHTMLIndent(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult WillOutdent(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult WillAlign(nsISelection *aSelection, const nsAString *alignType, bool *aCancel, bool *aHandled);
  nsresult WillAbsolutePosition(nsISelection *aSelection, bool *aCancel, bool * aHandled);
  nsresult WillRemoveAbsolutePosition(nsISelection *aSelection, bool *aCancel, bool * aHandled);
  nsresult WillRelativeChangeZIndex(nsISelection *aSelection, PRInt32 aChange, bool *aCancel, bool * aHandled);
  nsresult WillMakeDefListItem(nsISelection *aSelection, const nsAString *aBlockType, bool aEntireList, bool *aCancel, bool *aHandled);
  nsresult WillMakeBasicBlock(nsISelection *aSelection, const nsAString *aBlockType, bool *aCancel, bool *aHandled);
  nsresult DidMakeBasicBlock(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
  nsresult DidAbsolutePosition();
  nsresult AlignInnerBlocks(nsIDOMNode *aNode, const nsAString *alignType);
  nsresult AlignBlockContents(nsIDOMNode *aNode, const nsAString *alignType);
  nsresult AppendInnerFormatNodes(nsCOMArray<nsIDOMNode>& aArray,
                                  nsIDOMNode *aNode);
  nsresult GetFormatString(nsIDOMNode *aNode, nsAString &outFormat);
  nsresult GetInnerContent(nsIDOMNode *aNode, nsCOMArray<nsIDOMNode>& outArrayOfNodes, PRInt32 *aIndex, bool aList = true, bool aTble = true);
  nsCOMPtr<nsIDOMNode> IsInListItem(nsIDOMNode *aNode);
  nsresult ReturnInHeader(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);
  nsresult ReturnInParagraph(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset, bool *aCancel, bool *aHandled);
  nsresult SplitParagraph(nsIDOMNode *aPara,
                          nsIDOMNode *aBRNode, 
                          nsISelection *aSelection,
                          nsCOMPtr<nsIDOMNode> *aSelNode, 
                          PRInt32 *aOffset);
  nsresult ReturnInListItem(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);
  nsresult AfterEditInner(PRInt32 action, nsIEditor::EDirection aDirection);
  nsresult RemovePartOfBlock(nsIDOMNode *aBlock, 
                             nsIDOMNode *aStartChild, 
                             nsIDOMNode *aEndChild,
                             nsCOMPtr<nsIDOMNode> *aLeftNode = 0,
                             nsCOMPtr<nsIDOMNode> *aRightNode = 0);
  nsresult SplitBlock(nsIDOMNode *aBlock, 
                      nsIDOMNode *aStartChild, 
                      nsIDOMNode *aEndChild,
                      nsCOMPtr<nsIDOMNode> *aLeftNode = 0,
                      nsCOMPtr<nsIDOMNode> *aRightNode = 0,
                      nsCOMPtr<nsIDOMNode> *aMiddleNode = 0);
  nsresult OutdentPartOfBlock(nsIDOMNode *aBlock, 
                              nsIDOMNode *aStartChild, 
                              nsIDOMNode *aEndChild,
                              bool aIsBlockIndentedWithCSS,
                              nsCOMPtr<nsIDOMNode> *aLeftNode = 0,
                              nsCOMPtr<nsIDOMNode> *aRightNode = 0);
  nsresult ConvertListType(nsIDOMNode *aList, nsCOMPtr<nsIDOMNode> *outList, const nsAString& aListType, const nsAString& aItemType);
  nsresult CreateStyleForInsertText(nsISelection *aSelection, nsIDOMDocument *aDoc);
  nsresult IsEmptyBlock(nsIDOMNode *aNode, 
                        bool *outIsEmptyBlock, 
                        bool aMozBRDoesntCount = false,
                        bool aListItemsNotEmpty = false);
  nsresult CheckForEmptyBlock(nsIDOMNode *aStartNode, 
                              nsIDOMNode *aBodyNode,
                              nsISelection *aSelection,
                              bool *aHandled);
  nsresult CheckForInvisibleBR(nsIDOMNode *aBlock, nsHTMLEditRules::BRLocation aWhere, 
                               nsCOMPtr<nsIDOMNode> *outBRNode, PRInt32 aOffset=0);
  nsresult ExpandSelectionForDeletion(nsISelection *aSelection);
  bool IsFirstNode(nsIDOMNode *aNode);
  bool IsLastNode(nsIDOMNode *aNode);
#ifdef XXX_DEAD_CODE
  bool AtStartOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock);
  bool AtEndOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock);
#endif
  nsresult NormalizeSelection(nsISelection *inSelection);
  nsresult GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                            PRInt32 actionID, nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset);
  nsresult GetPromotedRanges(nsISelection *inSelection, 
                             nsCOMArray<nsIDOMRange> &outArrayOfRanges, 
                             PRInt32 inOperationType);
  nsresult PromoteRange(nsIDOMRange *inRange, PRInt32 inOperationType);
  nsresult GetNodesForOperation(nsCOMArray<nsIDOMRange>& inArrayOfRanges, 
                                nsCOMArray<nsIDOMNode>& outArrayOfNodes, 
                                PRInt32 inOperationType,
                                bool aDontTouchContent=false);
  nsresult GetChildNodesForOperation(nsIDOMNode *inNode, 
                                     nsCOMArray<nsIDOMNode>& outArrayOfNodes);
  nsresult GetNodesFromPoint(DOMPoint point,
                             PRInt32 operation,
                             nsCOMArray<nsIDOMNode>& arrayOfNodes,
                             bool dontTouchContent);
  nsresult GetNodesFromSelection(nsISelection *selection,
                                 PRInt32 operation,
                                 nsCOMArray<nsIDOMNode>& arrayOfNodes,
                                 bool aDontTouchContent=false);
  nsresult GetListActionNodes(nsCOMArray<nsIDOMNode> &outArrayOfNodes, bool aEntireList, bool aDontTouchContent=false);
  nsresult GetDefinitionListItemTypes(nsIDOMNode *aNode, bool &aDT, bool &aDD);
  nsresult GetParagraphFormatNodes(nsCOMArray<nsIDOMNode>& outArrayOfNodes, bool aDontTouchContent=false);
  nsresult LookInsideDivBQandList(nsCOMArray<nsIDOMNode>& aNodeArray);
  nsresult BustUpInlinesAtRangeEndpoints(nsRangeStore &inRange);
  nsresult BustUpInlinesAtBRs(nsIDOMNode *inNode, 
                              nsCOMArray<nsIDOMNode>& outArrayOfNodes);
  nsCOMPtr<nsIDOMNode> GetHighestInlineParent(nsIDOMNode* aNode);
  nsresult MakeTransitionList(nsCOMArray<nsIDOMNode>& inArrayOfNodes, 
                              nsTArray<bool> &inTransitionArray);
  nsresult RemoveBlockStyle(nsCOMArray<nsIDOMNode>& arrayOfNodes);
  nsresult ApplyBlockStyle(nsCOMArray<nsIDOMNode>& arrayOfNodes, const nsAString *aBlockTag);
  nsresult MakeBlockquote(nsCOMArray<nsIDOMNode>& arrayOfNodes);
  nsresult SplitAsNeeded(const nsAString *aTag, nsCOMPtr<nsIDOMNode> *inOutParent, PRInt32 *inOutOffset);
  nsresult AddTerminatingBR(nsIDOMNode *aBlock);
  nsresult JoinNodesSmart( nsIDOMNode *aNodeLeft, 
                           nsIDOMNode *aNodeRight, 
                           nsCOMPtr<nsIDOMNode> *aOutMergeParent, 
                           PRInt32 *aOutMergeOffset);
  nsresult GetTopEnclosingMailCite(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutCiteNode, bool aPlaintext);
  nsresult PopListItem(nsIDOMNode *aListItem, bool *aOutOfList);
  nsresult RemoveListStructure(nsIDOMNode *aList);
  nsresult CacheInlineStyles(nsIDOMNode *aNode);
  nsresult ReapplyCachedStyles(); 
  nsresult ClearCachedStyles();
  nsresult AdjustSpecialBreaks(bool aSafeToAskFrames = false);
  nsresult AdjustWhitespace(nsISelection *aSelection);
  nsresult PinSelectionToNewBlock(nsISelection *aSelection);
  nsresult CheckInterlinePosition(nsISelection *aSelection);
  nsresult AdjustSelection(nsISelection *aSelection, nsIEditor::EDirection aAction);
  nsresult FindNearSelectableNode(nsIDOMNode *aSelNode, 
                                  PRInt32 aSelOffset, 
                                  nsIEditor::EDirection &aDirection,
                                  nsCOMPtr<nsIDOMNode> *outSelectableNode);
  nsresult InDifferentTableElements(nsIDOMNode *aNode1, nsIDOMNode *aNode2, bool *aResult);
  nsresult RemoveEmptyNodes();
  nsresult SelectionEndpointInNode(nsIDOMNode *aNode, bool *aResult);
  nsresult UpdateDocChangeRange(nsIDOMRange *aRange);
  nsresult ConfirmSelectionInBody();
  nsresult InsertMozBRIfNeeded(nsIDOMNode *aNode);
  bool     IsEmptyInline(nsIDOMNode *aNode);
  bool     ListIsEmptyLine(nsCOMArray<nsIDOMNode> &arrayOfNodes);
  nsresult RemoveAlignment(nsIDOMNode * aNode, const nsAString & aAlignType, bool aChildrenOnly);
  nsresult MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode, bool aStarts);
  nsresult AlignBlock(nsIDOMElement * aElement, const nsAString * aAlignType, bool aContentsOnly);
  nsresult RelativeChangeIndentationOfElementNode(nsIDOMNode *aNode, PRInt8 aRelativeChange);
  void DocumentModifiedWorker();

// data members
protected:
  nsHTMLEditor           *mHTMLEditor;
  nsCOMPtr<nsIDOMRange>   mDocChangeRange;
  bool                    mListenerEnabled;
  bool                    mReturnInEmptyLIKillsList;
  bool                    mDidDeleteSelection;
  bool                    mDidRangedDelete;
  bool                    mRestoreContentEditableCount;
  nsCOMPtr<nsIDOMRange>   mUtilRange;
  PRUint32                mJoinOffset;  // need to remember an int across willJoin/didJoin...
  nsCOMPtr<nsIDOMNode>    mNewBlock;
  nsRangeStore            mRangeItem;
  StyleCache              mCachedStyles[SIZE_STYLE_TABLE];
};

#endif //nsHTMLEditRules_h__

