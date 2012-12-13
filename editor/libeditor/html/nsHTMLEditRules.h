/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLEditRules_h__
#define nsHTMLEditRules_h__

#include "TypeInState.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsEditor.h"
#include "nsIEditActionListener.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsISupportsImpl.h"
#include "nsSelectionState.h"
#include "nsTArray.h"
#include "nsTextEditRules.h"
#include "nsTraceRefcnt.h"
#include "nscore.h"

class nsHTMLEditor;
class nsIAtom;
class nsIDOMCharacterData;
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMNode;
class nsIDOMRange;
class nsIEditor;
class nsINode;
class nsISelection;
class nsPlaintextEditor;
class nsRange;
class nsRulesInfo;
namespace mozilla {
class Selection;
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla
struct DOMPoint;
template <class E> class nsCOMArray;

struct StyleCache : public PropItem
{
  bool mPresent;
  
  StyleCache() : PropItem(), mPresent(false) {
    MOZ_COUNT_CTOR(StyleCache);
  }

  StyleCache(nsIAtom *aTag, const nsAString &aAttr, const nsAString &aValue) : 
             PropItem(aTag, aAttr, aValue), mPresent(false) {
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
  NS_IMETHOD BeforeEdit(EditAction action,
                        nsIEditor::EDirection aDirection);
  NS_IMETHOD AfterEdit(EditAction action,
                       nsIEditor::EDirection aDirection);
  NS_IMETHOD WillDoAction(mozilla::Selection* aSelection, nsRulesInfo* aInfo,
                          bool* aCancel, bool* aHandled);
  NS_IMETHOD DidDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
  NS_IMETHOD DocumentModified();

  nsresult GetListState(bool *aMixed, bool *aOL, bool *aUL, bool *aDL);
  nsresult GetListItemState(bool *aMixed, bool *aLI, bool *aDT, bool *aDD);
  nsresult GetIndentState(bool *aCanIndent, bool *aCanOutdent);
  nsresult GetAlignment(bool *aMixed, nsIHTMLEditor::EAlignment *aAlign);
  nsresult GetParagraphState(bool *aMixed, nsAString &outFormat);
  nsresult MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode);

  // nsIEditActionListener methods
  
  NS_IMETHOD WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, int32_t aPosition);
  NS_IMETHOD DidCreateNode(const nsAString& aTag, nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aPosition, nsresult aResult);
  NS_IMETHOD WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aPosition);
  NS_IMETHOD DidInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aPosition, nsresult aResult);
  NS_IMETHOD WillDeleteNode(nsIDOMNode *aChild);
  NS_IMETHOD DidDeleteNode(nsIDOMNode *aChild, nsresult aResult);
  NS_IMETHOD WillSplitNode(nsIDOMNode *aExistingRightNode, int32_t aOffset);
  NS_IMETHOD DidSplitNode(nsIDOMNode *aExistingRightNode, int32_t aOffset, nsIDOMNode *aNewLeftNode, nsresult aResult);
  NS_IMETHOD WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent);
  NS_IMETHOD DidJoinNodes(nsIDOMNode  *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent, nsresult aResult);
  NS_IMETHOD WillInsertText(nsIDOMCharacterData *aTextNode, int32_t aOffset, const nsAString &aString);
  NS_IMETHOD DidInsertText(nsIDOMCharacterData *aTextNode, int32_t aOffset, const nsAString &aString, nsresult aResult);
  NS_IMETHOD WillDeleteText(nsIDOMCharacterData *aTextNode, int32_t aOffset, int32_t aLength);
  NS_IMETHOD DidDeleteText(nsIDOMCharacterData *aTextNode, int32_t aOffset, int32_t aLength, nsresult aResult);
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
  nsresult WillInsertText(  EditAction aAction,
                            mozilla::Selection* aSelection,
                            bool            *aCancel,
                            bool            *aHandled,
                            const nsAString *inString,
                            nsAString       *outString,
                            int32_t          aMaxLength);
  nsresult WillLoadHTML(nsISelection *aSelection, bool *aCancel);
  nsresult WillInsertBreak(mozilla::Selection* aSelection,
                           bool* aCancel, bool* aHandled);
  nsresult StandardBreakImpl(nsIDOMNode *aNode, int32_t aOffset, nsISelection *aSelection);
  nsresult DidInsertBreak(nsISelection *aSelection, nsresult aResult);
  nsresult SplitMailCites(nsISelection *aSelection, bool aPlaintext, bool *aHandled);
  nsresult WillDeleteSelection(mozilla::Selection* aSelection,
                               nsIEditor::EDirection aAction,
                               nsIEditor::EStripWrappers aStripWrappers,
                               bool* aCancel, bool* aHandled);
  nsresult DidDeleteSelection(nsISelection *aSelection, 
                              nsIEditor::EDirection aDir, 
                              nsresult aResult);
  nsresult InsertBRIfNeeded(nsISelection *aSelection);
  nsresult GetGoodSelPointForNode(nsIDOMNode *aNode, nsIEditor::EDirection aAction, 
                                  nsCOMPtr<nsIDOMNode> *outSelNode, int32_t *outSelOffset);
  nsresult JoinBlocks(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, bool *aCanceled);
  nsresult MoveBlock(nsIDOMNode *aLeft, nsIDOMNode *aRight, int32_t aLeftOffset, int32_t aRightOffset);
  nsresult MoveNodeSmart(nsIDOMNode *aSource, nsIDOMNode *aDest, int32_t *aOffset);
  nsresult MoveContents(nsIDOMNode *aSource, nsIDOMNode *aDest, int32_t *aOffset);
  nsresult DeleteNonTableElements(nsINode* aNode);
  nsresult WillMakeList(mozilla::Selection* aSelection,
                        const nsAString* aListType,
                        bool aEntireList,
                        const nsAString* aBulletType,
                        bool* aCancel, bool* aHandled,
                        const nsAString* aItemType = nullptr);
  nsresult WillRemoveList(mozilla::Selection* aSelection,
                          bool aOrdered, bool* aCancel, bool* aHandled);
  nsresult WillIndent(mozilla::Selection* aSelection,
                      bool* aCancel, bool* aHandled);
  nsresult WillCSSIndent(mozilla::Selection* aSelection,
                         bool* aCancel, bool* aHandled);
  nsresult WillHTMLIndent(mozilla::Selection* aSelection,
                          bool* aCancel, bool* aHandled);
  nsresult WillOutdent(mozilla::Selection* aSelection,
                       bool* aCancel, bool* aHandled);
  nsresult WillAlign(mozilla::Selection* aSelection,
                     const nsAString* alignType,
                     bool* aCancel, bool* aHandled);
  nsresult WillAbsolutePosition(mozilla::Selection* aSelection,
                                bool* aCancel, bool* aHandled);
  nsresult WillRemoveAbsolutePosition(mozilla::Selection* aSelection,
                                      bool* aCancel, bool* aHandled);
  nsresult WillRelativeChangeZIndex(mozilla::Selection* aSelection,
                                    int32_t aChange,
                                    bool* aCancel, bool* aHandled);
  nsresult WillMakeDefListItem(mozilla::Selection* aSelection,
                               const nsAString* aBlockType, bool aEntireList,
                               bool* aCancel, bool* aHandled);
  nsresult WillMakeBasicBlock(mozilla::Selection* aSelection,
                              const nsAString* aBlockType,
                              bool* aCancel, bool* aHandled);
  nsresult DidMakeBasicBlock(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
  nsresult DidAbsolutePosition();
  nsresult AlignInnerBlocks(nsIDOMNode *aNode, const nsAString *alignType);
  nsresult AlignBlockContents(nsIDOMNode *aNode, const nsAString *alignType);
  nsresult AppendInnerFormatNodes(nsCOMArray<nsIDOMNode>& aArray,
                                  nsINode* aNode);
  nsresult AppendInnerFormatNodes(nsCOMArray<nsIDOMNode>& aArray,
                                  nsIDOMNode *aNode);
  nsresult GetFormatString(nsIDOMNode *aNode, nsAString &outFormat);
  nsresult GetInnerContent(nsIDOMNode *aNode, nsCOMArray<nsIDOMNode>& outArrayOfNodes, int32_t *aIndex, bool aList = true, bool aTble = true);
  already_AddRefed<nsIDOMNode> IsInListItem(nsIDOMNode* aNode);
  nsINode* IsInListItem(nsINode* aNode);
  nsresult ReturnInHeader(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, int32_t aOffset);
  nsresult ReturnInParagraph(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, int32_t aOffset, bool *aCancel, bool *aHandled);
  nsresult SplitParagraph(nsIDOMNode *aPara,
                          nsIDOMNode *aBRNode, 
                          nsISelection *aSelection,
                          nsCOMPtr<nsIDOMNode> *aSelNode, 
                          int32_t *aOffset);
  nsresult ReturnInListItem(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, int32_t aOffset);
  nsresult AfterEditInner(EditAction action,
                          nsIEditor::EDirection aDirection);
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

  nsresult ConvertListType(nsIDOMNode* aList,
                           nsCOMPtr<nsIDOMNode>* outList,
                           nsIAtom* aListType,
                           nsIAtom* aItemType);
  nsresult ConvertListType(nsINode* aList,
                           mozilla::dom::Element** aOutList,
                           nsIAtom* aListType,
                           nsIAtom* aItemType);

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
                               nsCOMPtr<nsIDOMNode> *outBRNode, int32_t aOffset=0);
  nsresult ExpandSelectionForDeletion(nsISelection *aSelection);
  bool IsFirstNode(nsIDOMNode *aNode);
  bool IsLastNode(nsIDOMNode *aNode);
  nsresult NormalizeSelection(nsISelection *inSelection);
  void GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode* aNode,
                        int32_t aOffset, EditAction actionID,
                        nsCOMPtr<nsIDOMNode>* outNode, int32_t* outOffset);
  nsresult GetPromotedRanges(nsISelection *inSelection, 
                             nsCOMArray<nsIDOMRange> &outArrayOfRanges, 
                             EditAction inOperationType);
  nsresult PromoteRange(nsIDOMRange *inRange,
                        EditAction inOperationType);
  nsresult GetNodesForOperation(nsCOMArray<nsIDOMRange>& inArrayOfRanges, 
                                nsCOMArray<nsIDOMNode>& outArrayOfNodes, 
                                EditAction inOperationType,
                                bool aDontTouchContent=false);
  nsresult GetChildNodesForOperation(nsIDOMNode *inNode, 
                                     nsCOMArray<nsIDOMNode>& outArrayOfNodes);
  nsresult GetNodesFromPoint(DOMPoint point,
                             EditAction operation,
                             nsCOMArray<nsIDOMNode>& arrayOfNodes,
                             bool dontTouchContent);
  nsresult GetNodesFromSelection(nsISelection *selection,
                                 EditAction operation,
                                 nsCOMArray<nsIDOMNode>& arrayOfNodes,
                                 bool aDontTouchContent=false);
  nsresult GetListActionNodes(nsCOMArray<nsIDOMNode> &outArrayOfNodes, bool aEntireList, bool aDontTouchContent=false);
  void GetDefinitionListItemTypes(mozilla::dom::Element* aElement, bool* aDT, bool* aDD);
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
  nsresult SplitAsNeeded(const nsAString *aTag, nsCOMPtr<nsIDOMNode> *inOutParent, int32_t *inOutOffset);
  nsresult AddTerminatingBR(nsIDOMNode *aBlock);
  nsresult JoinNodesSmart( nsIDOMNode *aNodeLeft, 
                           nsIDOMNode *aNodeRight, 
                           nsCOMPtr<nsIDOMNode> *aOutMergeParent, 
                           int32_t *aOutMergeOffset);
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
                                  int32_t aSelOffset, 
                                  nsIEditor::EDirection &aDirection,
                                  nsCOMPtr<nsIDOMNode> *outSelectableNode);
  /**
   * Returns true if aNode1 or aNode2 or both is the descendant of some type of
   * table element, but their nearest table element ancestors differ.  "Table
   * element" here includes not just <table> but also <td>, <tbody>, <tr>, etc.
   * The nodes count as being their own descendants for this purpose, so a
   * table element is its own nearest table element ancestor.
   */
  bool     InDifferentTableElements(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  bool     InDifferentTableElements(nsINode* aNode1, nsINode* aNode2);
  nsresult RemoveEmptyNodes();
  nsresult SelectionEndpointInNode(nsINode *aNode, bool *aResult);
  nsresult UpdateDocChangeRange(nsIDOMRange *aRange);
  nsresult ConfirmSelectionInBody();
  nsresult InsertMozBRIfNeeded(nsIDOMNode *aNode);
  bool     IsEmptyInline(nsIDOMNode *aNode);
  bool     ListIsEmptyLine(nsCOMArray<nsIDOMNode> &arrayOfNodes);
  nsresult RemoveAlignment(nsIDOMNode * aNode, const nsAString & aAlignType, bool aChildrenOnly);
  nsresult MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode, bool aStarts);
  nsresult AlignBlock(nsIDOMElement * aElement, const nsAString * aAlignType, bool aContentsOnly);
  nsresult RelativeChangeIndentationOfElementNode(nsIDOMNode *aNode, int8_t aRelativeChange);
  void DocumentModifiedWorker();

// data members
protected:
  nsHTMLEditor           *mHTMLEditor;
  nsRefPtr<nsRange>       mDocChangeRange;
  bool                    mListenerEnabled;
  bool                    mReturnInEmptyLIKillsList;
  bool                    mDidDeleteSelection;
  bool                    mDidRangedDelete;
  bool                    mRestoreContentEditableCount;
  nsRefPtr<nsRange>       mUtilRange;
  uint32_t                mJoinOffset;  // need to remember an int across willJoin/didJoin...
  nsCOMPtr<nsIDOMNode>    mNewBlock;
  nsRefPtr<nsRangeStore>  mRangeItem;
  StyleCache              mCachedStyles[SIZE_STYLE_TABLE];
};

#endif //nsHTMLEditRules_h__

