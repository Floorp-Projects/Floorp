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
#include "nscore.h"

class nsHTMLEditor;
class nsIAtom;
class nsIDOMCharacterData;
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMNode;
class nsIEditor;
class nsINode;
class nsPlaintextEditor;
class nsRange;
class nsRulesInfo;
namespace mozilla {
namespace dom {
class Element;
class Selection;
} // namespace dom
} // namespace mozilla
struct DOMPoint;

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

  // nsIEditRules methods
  NS_IMETHOD Init(nsPlaintextEditor *aEditor) override;
  NS_IMETHOD DetachEditor() override;
  NS_IMETHOD BeforeEdit(EditAction action,
                        nsIEditor::EDirection aDirection) override;
  NS_IMETHOD AfterEdit(EditAction action,
                       nsIEditor::EDirection aDirection) override;
  NS_IMETHOD WillDoAction(mozilla::dom::Selection* aSelection, nsRulesInfo* aInfo,
                          bool* aCancel, bool* aHandled) override;
  NS_IMETHOD DidDoAction(mozilla::dom::Selection* aSelection,
                         nsRulesInfo* aInfo, nsresult aResult) override;
  NS_IMETHOD DocumentModified() override;

  nsresult GetListState(bool *aMixed, bool *aOL, bool *aUL, bool *aDL);
  nsresult GetListItemState(bool *aMixed, bool *aLI, bool *aDT, bool *aDD);
  nsresult GetIndentState(bool *aCanIndent, bool *aCanOutdent);
  nsresult GetAlignment(bool *aMixed, nsIHTMLEditor::EAlignment *aAlign);
  nsresult GetParagraphState(bool *aMixed, nsAString &outFormat);
  nsresult MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode);

  // nsIEditActionListener methods

  NS_IMETHOD WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, int32_t aPosition) override;
  NS_IMETHOD DidCreateNode(const nsAString& aTag, nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aPosition, nsresult aResult) override;
  NS_IMETHOD WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aPosition) override;
  NS_IMETHOD DidInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aPosition, nsresult aResult) override;
  NS_IMETHOD WillDeleteNode(nsIDOMNode *aChild) override;
  NS_IMETHOD DidDeleteNode(nsIDOMNode *aChild, nsresult aResult) override;
  NS_IMETHOD WillSplitNode(nsIDOMNode *aExistingRightNode, int32_t aOffset) override;
  NS_IMETHOD DidSplitNode(nsIDOMNode *aExistingRightNode, int32_t aOffset, nsIDOMNode *aNewLeftNode, nsresult aResult) override;
  NS_IMETHOD WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent) override;
  NS_IMETHOD DidJoinNodes(nsIDOMNode  *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent, nsresult aResult) override;
  NS_IMETHOD WillInsertText(nsIDOMCharacterData *aTextNode, int32_t aOffset, const nsAString &aString) override;
  NS_IMETHOD DidInsertText(nsIDOMCharacterData *aTextNode, int32_t aOffset, const nsAString &aString, nsresult aResult) override;
  NS_IMETHOD WillDeleteText(nsIDOMCharacterData *aTextNode, int32_t aOffset, int32_t aLength) override;
  NS_IMETHOD DidDeleteText(nsIDOMCharacterData *aTextNode, int32_t aOffset, int32_t aLength, nsresult aResult) override;
  NS_IMETHOD WillDeleteSelection(nsISelection *aSelection) override;
  NS_IMETHOD DidDeleteSelection(nsISelection *aSelection) override;

protected:
  virtual ~nsHTMLEditRules();

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

  void InitFields();

  // nsHTMLEditRules implementation methods
  nsresult WillInsert(mozilla::dom::Selection* aSelection, bool* aCancel);
  nsresult WillInsertText(  EditAction aAction,
                            mozilla::dom::Selection* aSelection,
                            bool            *aCancel,
                            bool            *aHandled,
                            const nsAString *inString,
                            nsAString       *outString,
                            int32_t          aMaxLength);
  nsresult WillLoadHTML(mozilla::dom::Selection* aSelection, bool* aCancel);
  nsresult WillInsertBreak(mozilla::dom::Selection* aSelection,
                           bool* aCancel, bool* aHandled);
  nsresult StandardBreakImpl(nsIDOMNode* aNode, int32_t aOffset,
                             mozilla::dom::Selection* aSelection);
  nsresult DidInsertBreak(mozilla::dom::Selection* aSelection,
                          nsresult aResult);
  nsresult SplitMailCites(mozilla::dom::Selection* aSelection, bool* aHandled);
  nsresult WillDeleteSelection(mozilla::dom::Selection* aSelection,
                               nsIEditor::EDirection aAction,
                               nsIEditor::EStripWrappers aStripWrappers,
                               bool* aCancel, bool* aHandled);
  nsresult DidDeleteSelection(mozilla::dom::Selection* aSelection,
                              nsIEditor::EDirection aDir,
                              nsresult aResult);
  nsresult InsertBRIfNeeded(mozilla::dom::Selection* aSelection);
  ::DOMPoint GetGoodSelPointForNode(nsINode& aNode,
                                    nsIEditor::EDirection aAction);
  nsresult JoinBlocks(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, bool *aCanceled);
  nsresult MoveBlock(nsIDOMNode *aLeft, nsIDOMNode *aRight, int32_t aLeftOffset, int32_t aRightOffset);
  nsresult MoveNodeSmart(nsIDOMNode *aSource, nsIDOMNode *aDest, int32_t *aOffset);
  nsresult MoveContents(nsIDOMNode *aSource, nsIDOMNode *aDest, int32_t *aOffset);
  nsresult DeleteNonTableElements(nsINode* aNode);
  nsresult WillMakeList(mozilla::dom::Selection* aSelection,
                        const nsAString* aListType,
                        bool aEntireList,
                        const nsAString* aBulletType,
                        bool* aCancel, bool* aHandled,
                        const nsAString* aItemType = nullptr);
  nsresult WillRemoveList(mozilla::dom::Selection* aSelection,
                          bool aOrdered, bool* aCancel, bool* aHandled);
  nsresult WillIndent(mozilla::dom::Selection* aSelection,
                      bool* aCancel, bool* aHandled);
  nsresult WillCSSIndent(mozilla::dom::Selection* aSelection,
                         bool* aCancel, bool* aHandled);
  nsresult WillHTMLIndent(mozilla::dom::Selection* aSelection,
                          bool* aCancel, bool* aHandled);
  nsresult WillOutdent(mozilla::dom::Selection* aSelection,
                       bool* aCancel, bool* aHandled);
  nsresult WillAlign(mozilla::dom::Selection* aSelection,
                     const nsAString* alignType,
                     bool* aCancel, bool* aHandled);
  nsresult WillAbsolutePosition(mozilla::dom::Selection* aSelection,
                                bool* aCancel, bool* aHandled);
  nsresult WillRemoveAbsolutePosition(mozilla::dom::Selection* aSelection,
                                      bool* aCancel, bool* aHandled);
  nsresult WillRelativeChangeZIndex(mozilla::dom::Selection* aSelection,
                                    int32_t aChange,
                                    bool* aCancel, bool* aHandled);
  nsresult WillMakeDefListItem(mozilla::dom::Selection* aSelection,
                               const nsAString* aBlockType, bool aEntireList,
                               bool* aCancel, bool* aHandled);
  nsresult WillMakeBasicBlock(mozilla::dom::Selection* aSelection,
                              const nsAString* aBlockType,
                              bool* aCancel, bool* aHandled);
  nsresult DidMakeBasicBlock(mozilla::dom::Selection* aSelection,
                             nsRulesInfo* aInfo, nsresult aResult);
  nsresult DidAbsolutePosition();
  nsresult AlignInnerBlocks(nsINode& aNode, const nsAString* alignType);
  nsresult AlignBlockContents(nsIDOMNode *aNode, const nsAString *alignType);
  nsresult AppendInnerFormatNodes(nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aArray,
                                  nsINode* aNode);
  nsresult GetFormatString(nsIDOMNode *aNode, nsAString &outFormat);
  enum class Lists { no, yes };
  enum class Tables { no, yes };
  void GetInnerContent(nsINode& aNode,
                       nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aOutArrayOfNodes,
                       int32_t* aIndex, Lists aLists = Lists::yes,
                       Tables aTables = Tables::yes);
  already_AddRefed<nsIDOMNode> IsInListItem(nsIDOMNode* aNode);
  mozilla::dom::Element* IsInListItem(nsINode* aNode);
  nsresult ReturnInHeader(mozilla::dom::Selection* aSelection,
                          nsIDOMNode* aHeader, nsIDOMNode* aTextNode,
                          int32_t aOffset);
  nsresult ReturnInParagraph(mozilla::dom::Selection* aSelection,
                             nsIDOMNode* aHeader, nsIDOMNode* aTextNode,
                             int32_t aOffset, bool* aCancel, bool* aHandled);
  nsresult SplitParagraph(nsIDOMNode *aPara,
                          nsIDOMNode *aBRNode,
                          mozilla::dom::Selection* aSelection,
                          nsCOMPtr<nsIDOMNode> *aSelNode,
                          int32_t *aOffset);
  nsresult ReturnInListItem(mozilla::dom::Selection* aSelection,
                            nsIDOMNode* aHeader, nsIDOMNode* aTextNode,
                            int32_t aOffset);
  nsresult AfterEditInner(EditAction action,
                          nsIEditor::EDirection aDirection);
  nsresult RemovePartOfBlock(mozilla::dom::Element& aBlock,
                             nsIContent& aStartChild,
                             nsIContent& aEndChild);
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
  nsresult ConvertListType(mozilla::dom::Element* aList,
                           mozilla::dom::Element** aOutList,
                           nsIAtom* aListType,
                           nsIAtom* aItemType);

  nsresult CreateStyleForInsertText(mozilla::dom::Selection* aSelection,
                                    nsIDOMDocument* aDoc);
  nsresult IsEmptyBlock(nsIDOMNode *aNode,
                        bool *outIsEmptyBlock,
                        bool aMozBRDoesntCount = false,
                        bool aListItemsNotEmpty = false);
  nsresult CheckForEmptyBlock(nsINode* aStartNode,
                              mozilla::dom::Element* aBodyNode,
                              mozilla::dom::Selection* aSelection,
                              bool* aHandled);
  nsresult CheckForInvisibleBR(nsIDOMNode *aBlock, nsHTMLEditRules::BRLocation aWhere,
                               nsCOMPtr<nsIDOMNode> *outBRNode, int32_t aOffset=0);
  nsresult ExpandSelectionForDeletion(mozilla::dom::Selection* aSelection);
  bool IsFirstNode(nsIDOMNode *aNode);
  bool IsLastNode(nsIDOMNode *aNode);
  nsresult NormalizeSelection(mozilla::dom::Selection* aSelection);
  void GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode* aNode,
                        int32_t aOffset, EditAction actionID,
                        nsCOMPtr<nsIDOMNode>* outNode, int32_t* outOffset);
  void GetPromotedRanges(mozilla::dom::Selection& aSelection,
                         nsTArray<nsRefPtr<nsRange>>& outArrayOfRanges,
                         EditAction inOperationType);
  void PromoteRange(nsRange& aRange, EditAction inOperationType);
  enum class TouchContent { no, yes };
  nsresult GetNodesForOperation(nsTArray<nsRefPtr<nsRange>>& aArrayOfRanges,
                                nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aOutArrayOfNodes,
                                EditAction aOperationType,
                                TouchContent aTouchContent = TouchContent::yes);
  void GetChildNodesForOperation(nsINode& aNode,
      nsTArray<mozilla::dom::OwningNonNull<nsINode>>& outArrayOfNodes);
  nsresult GetNodesFromPoint(::DOMPoint aPoint,
                             EditAction aOperation,
                             nsTArray<mozilla::dom::OwningNonNull<nsINode>>& outArrayOfNodes,
                             TouchContent aTouchContent);
  nsresult GetNodesFromSelection(mozilla::dom::Selection& aSelection,
                                 EditAction aOperation,
                                 nsTArray<mozilla::dom::OwningNonNull<nsINode>>& outArrayOfNodes,
                                 TouchContent aTouchContent = TouchContent::yes);
  enum class EntireList { no, yes };
  nsresult GetListActionNodes(nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aOutArrayOfNodes,
                              EntireList aEntireList,
                              TouchContent aTouchContent = TouchContent::yes);
  void GetDefinitionListItemTypes(mozilla::dom::Element* aElement, bool* aDT, bool* aDD);
  nsresult GetParagraphFormatNodes(
      nsTArray<mozilla::dom::OwningNonNull<nsINode>>& outArrayOfNodes,
      TouchContent aTouchContent = TouchContent::yes);
  void LookInsideDivBQandList(nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aNodeArray);
  nsresult BustUpInlinesAtRangeEndpoints(nsRangeStore &inRange);
  nsresult BustUpInlinesAtBRs(nsINode& aNode,
                              nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aOutArrayOfNodes);
  nsCOMPtr<nsIDOMNode> GetHighestInlineParent(nsIDOMNode* aNode);
  void MakeTransitionList(nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aNodeArray,
                          nsTArray<bool>& aTransitionArray);
  nsresult RemoveBlockStyle(nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aNodeArray);
  nsresult ApplyBlockStyle(nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aNodeArray,
                           nsIAtom& aBlockTag);
  nsresult MakeBlockquote(nsTArray<mozilla::dom::OwningNonNull<nsINode>>& aNodeArray);
  nsresult SplitAsNeeded(nsIAtom& aTag, nsCOMPtr<nsINode>& inOutParent,
                         int32_t& inOutOffset);
  nsresult AddTerminatingBR(nsIDOMNode *aBlock);
  ::DOMPoint JoinNodesSmart(nsIContent& aNodeLeft, nsIContent& aNodeRight);
  mozilla::dom::Element* GetTopEnclosingMailCite(nsINode& aNode);
  nsresult PopListItem(nsIDOMNode *aListItem, bool *aOutOfList);
  nsresult RemoveListStructure(nsIDOMNode *aList);
  nsresult CacheInlineStyles(nsIDOMNode *aNode);
  nsresult ReapplyCachedStyles();
  void ClearCachedStyles();
  void AdjustSpecialBreaks();
  nsresult AdjustWhitespace(mozilla::dom::Selection* aSelection);
  nsresult PinSelectionToNewBlock(mozilla::dom::Selection* aSelection);
  nsresult CheckInterlinePosition(mozilla::dom::Selection* aSelection);
  nsresult AdjustSelection(mozilla::dom::Selection* aSelection,
                           nsIEditor::EDirection aAction);
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
  nsresult UpdateDocChangeRange(nsRange* aRange);
  nsresult ConfirmSelectionInBody();
  nsresult InsertMozBRIfNeeded(nsIDOMNode *aNode);
  bool     IsEmptyInline(nsIDOMNode *aNode);
  bool     ListIsEmptyLine(nsTArray<mozilla::dom::OwningNonNull<nsINode>>& arrayOfNodes);
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

