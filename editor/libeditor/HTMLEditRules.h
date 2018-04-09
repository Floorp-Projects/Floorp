/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditRules_h
#define HTMLEditRules_h

#include "TypeInState.h"
#include "mozilla/EditorDOMPoint.h" // for EditorDOMPoint
#include "mozilla/SelectionState.h"
#include "mozilla/TextEditRules.h"
#include "nsCOMPtr.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nscore.h"

class nsAtom;
class nsIEditor;
class nsINode;
class nsRange;

namespace mozilla {

class EditActionResult;
class HTMLEditor;
class RulesInfo;
class SplitNodeResult;
class TextEditor;
enum class EditAction : int32_t;

namespace dom {
class Element;
class Selection;
} // namespace dom

struct StyleCache final : public PropItem
{
  bool mPresent;

  StyleCache()
    : PropItem()
    , mPresent(false)
  {
    MOZ_COUNT_CTOR(StyleCache);
  }

  StyleCache(nsAtom* aTag,
             nsAtom* aAttr,
             const nsAString& aValue)
    : PropItem(aTag, aAttr, aValue)
    , mPresent(false)
  {
    MOZ_COUNT_CTOR(StyleCache);
  }

  StyleCache(nsAtom* aTag,
             nsAtom* aAttr)
    : PropItem(aTag, aAttr, EmptyString())
    , mPresent(false)
  {
    MOZ_COUNT_CTOR(StyleCache);
  }

  ~StyleCache()
  {
    MOZ_COUNT_DTOR(StyleCache);
  }
};

#define SIZE_STYLE_TABLE 19

class HTMLEditRules : public TextEditRules
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLEditRules, TextEditRules)

  HTMLEditRules();

  // TextEditRules methods
  virtual nsresult Init(TextEditor* aTextEditor) override;
  virtual nsresult DetachEditor() override;
  virtual nsresult BeforeEdit(EditAction aAction,
                              nsIEditor::EDirection aDirection) override;
  virtual nsresult AfterEdit(EditAction aAction,
                             nsIEditor::EDirection aDirection) override;
  virtual nsresult WillDoAction(Selection* aSelection,
                                RulesInfo* aInfo,
                                bool* aCancel,
                                bool* aHandled) override;
  virtual nsresult DidDoAction(Selection* aSelection,
                               RulesInfo* aInfo,
                               nsresult aResult) override;
  virtual bool DocumentIsEmpty() override;
  virtual nsresult DocumentModified() override;

  nsresult GetListState(bool* aMixed, bool* aOL, bool* aUL, bool* aDL);
  nsresult GetListItemState(bool* aMixed, bool* aLI, bool* aDT, bool* aDD);
  nsresult GetIndentState(bool* aCanIndent, bool* aCanOutdent);
  nsresult GetAlignment(bool* aMixed, nsIHTMLEditor::EAlignment* aAlign);
  nsresult GetParagraphState(bool* aMixed, nsAString& outFormat);
  nsresult MakeSureElemStartsOrEndsOnCR(nsINode& aNode);

  void DidCreateNode(Element* aNewElement);
  void DidInsertNode(nsIContent& aNode);
  void WillDeleteNode(nsINode* aChild);
  void DidSplitNode(nsINode* aExistingRightNode,
                    nsINode* aNewLeftNode);
  void WillJoinNodes(nsINode& aLeftNode, nsINode& aRightNode);
  void DidJoinNodes(nsINode& aLeftNode, nsINode& aRightNode);
  void DidInsertText(nsINode* aTextNode, int32_t aOffset,
                     const nsAString& aString);
  void DidDeleteText(nsINode* aTextNode, int32_t aOffset, int32_t aLength);
  void WillDeleteSelection(Selection* aSelection);

  void DeleteNodeIfCollapsedText(nsINode& aNode);

  void StartToListenToEditActions() { mListenerEnabled = true; }
  void EndListeningToEditActions() { mListenerEnabled = false; }

protected:
  virtual ~HTMLEditRules();

  enum RulesEndpoint
  {
    kStart,
    kEnd
  };

  void InitFields();

  void WillInsert(Selection& aSelection, bool* aCancel);
  nsresult WillInsertText(EditAction aAction,
                          Selection* aSelection,
                          bool* aCancel,
                          bool* aHandled,
                          const nsAString* inString,
                          nsAString* outString,
                          int32_t aMaxLength);
  nsresult WillLoadHTML(Selection* aSelection, bool* aCancel);
  nsresult WillInsertBreak(Selection& aSelection, bool* aCancel,
                           bool* aHandled);

  /**
   * InsertBRElement() inserts a <br> element into aInsertToBreak.
   *
   * @param aSelection          The selection.
   * @param aInsertToBreak      The point where new <br> element will be
   *                            inserted before.
   */
  nsresult InsertBRElement(Selection& aSelection,
                           const EditorDOMPoint& aInsertToBreak);

  nsresult DidInsertBreak(Selection* aSelection, nsresult aResult);
  nsresult SplitMailCites(Selection* aSelection, bool* aHandled);
  nsresult WillDeleteSelection(Selection* aSelection,
                               nsIEditor::EDirection aAction,
                               nsIEditor::EStripWrappers aStripWrappers,
                               bool* aCancel, bool* aHandled);
  nsresult DidDeleteSelection(Selection* aSelection,
                              nsIEditor::EDirection aDir,
                              nsresult aResult);
  nsresult InsertBRIfNeeded(Selection* aSelection);

  /**
   * CanContainParagraph() returns true if aElement can have a <p> element as
   * its child or its descendant.
   */
  bool CanContainParagraph(Element& aElement) const;

  /**
   * Insert a normal <br> element or a moz-<br> element to aNode when
   * aNode is a block and it has no children.
   *
   * @param aNode           Reference to a block parent.
   * @param aInsertMozBR    true if this should insert a moz-<br> element.
   *                        Otherwise, i.e., this should insert a normal <br>
   *                        element, false.
   */
  nsresult InsertBRIfNeededInternal(nsINode& aNode, bool aInsertMozBR);

  EditorDOMPoint GetGoodSelPointForNode(nsINode& aNode,
                                        nsIEditor::EDirection aAction);

  /**
   * TryToJoinBlocksWithTransaction() tries to join two block elements.  The
   * right element is always joined to the left element.  If the elements are
   * the same type and not nested within each other,
   * JoinEditableNodesWithTransaction() is called (example, joining two list
   * items together into one).  If the elements are not the same type, or one
   * is a descendant of the other, we instead destroy the right block placing
   * its children into leftblock.  DTD containment rules are followed
   * throughout.
   *
   * @return            Sets canceled to true if the operation should do
   *                    nothing anymore even if this doesn't join the blocks.
   *                    Sets handled to true if this actually handles the
   *                    request.  Note that this may set it to true even if this
   *                    does not join the block.  E.g., if the blocks shouldn't
   *                    be joined or it's impossible to join them but it's not
   *                    unexpected case, this returns true with this.
   */
  EditActionResult TryToJoinBlocksWithTransaction(nsIContent& aLeftNode,
                                                  nsIContent& aRightNode);

  /**
   * MoveBlock() moves the content from aRightBlock starting from aRightOffset
   * into aLeftBlock at aLeftOffset. Note that the "block" can be inline nodes
   * between <br>s, or between blocks, etc.  DTD containment rules are followed
   * throughout.
   *
   * @return            Sets handled to true if this actually joins the nodes.
   *                    canceled is always false.
   */
  EditActionResult MoveBlock(Element& aLeftBlock, Element& aRightBlock,
                             int32_t aLeftOffset, int32_t aRightOffset);

  /**
   * MoveNodeSmart() moves aNode to (aDestElement, aInOutDestOffset).
   * DTD containment rules are followed throughout.
   *
   * @param aOffset                 returns the point after inserted content.
   * @return                        Sets true to handled if this actually moves
   *                                the nodes.
   *                                canceled is always false.
   */
  EditActionResult MoveNodeSmart(nsIContent& aNode, Element& aDestElement,
                                 int32_t* aInOutDestOffset);

  /**
   * MoveContents() moves the contents of aElement to (aDestElement,
   * aInOutDestOffset).  DTD containment rules are followed throughout.
   *
   * @param aInOutDestOffset        updated to point after inserted content.
   * @return                        Sets true to handled if this actually moves
   *                                the nodes.
   *                                canceled is always false.
   */
  EditActionResult MoveContents(Element& aElement, Element& aDestElement,
                                int32_t* aInOutDestOffset);

  nsresult DeleteNonTableElements(nsINode* aNode);
  nsresult WillMakeList(Selection* aSelection,
                        const nsAString* aListType,
                        bool aEntireList,
                        const nsAString* aBulletType,
                        bool* aCancel, bool* aHandled,
                        const nsAString* aItemType = nullptr);
  nsresult WillRemoveList(Selection* aSelection, bool aOrdered, bool* aCancel,
                          bool* aHandled);
  nsresult WillIndent(Selection* aSelection, bool* aCancel, bool* aHandled);
  nsresult WillCSSIndent(Selection* aSelection, bool* aCancel, bool* aHandled);
  nsresult WillHTMLIndent(Selection* aSelection, bool* aCancel,
                          bool* aHandled);
  nsresult WillOutdent(Selection& aSelection, bool* aCancel, bool* aHandled);
  nsresult WillAlign(Selection& aSelection, const nsAString& aAlignType,
                     bool* aCancel, bool* aHandled);
  nsresult WillAbsolutePosition(Selection& aSelection, bool* aCancel,
                                bool* aHandled);
  nsresult WillRemoveAbsolutePosition(Selection* aSelection, bool* aCancel,
                                      bool* aHandled);
  nsresult WillRelativeChangeZIndex(Selection* aSelection, int32_t aChange,
                                    bool* aCancel, bool* aHandled);
  nsresult WillMakeDefListItem(Selection* aSelection,
                               const nsAString* aBlockType, bool aEntireList,
                               bool* aCancel, bool* aHandled);
  nsresult WillMakeBasicBlock(Selection& aSelection,
                              const nsAString& aBlockType,
                              bool* aCancel, bool* aHandled);
  nsresult MakeBasicBlock(Selection& aSelection, nsAtom& aBlockType);
  nsresult DidMakeBasicBlock(Selection* aSelection, RulesInfo* aInfo,
                             nsresult aResult);
  nsresult DidAbsolutePosition();
  nsresult AlignInnerBlocks(nsINode& aNode, const nsAString& aAlignType);
  nsresult AlignBlockContents(nsINode& aNode, const nsAString& aAlignType);
  nsresult AppendInnerFormatNodes(nsTArray<OwningNonNull<nsINode>>& aArray,
                                  nsINode* aNode);
  nsresult GetFormatString(nsINode* aNode, nsAString &outFormat);
  enum class Lists { no, yes };
  enum class Tables { no, yes };
  void GetInnerContent(nsINode& aNode,
                       nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                       int32_t* aIndex, Lists aLists = Lists::yes,
                       Tables aTables = Tables::yes);
  Element* IsInListItem(nsINode* aNode);
  nsAtom& DefaultParagraphSeparator();
  nsresult ReturnInHeader(Selection& aSelection, Element& aHeader,
                          nsINode& aNode, int32_t aOffset);

  /**
   * ReturnInParagraph() does the right thing for Enter key press or
   * 'insertParagraph' command in aParentDivOrP.  aParentDivOrP will be
   * split at start of first selection range.
   *
   * @param aSelection      The selection.  aParentDivOrP will be split at
   *                        start of the first selection range.
   * @param aParentDivOrP   The parent block.  This must be <p> or <div>
   *                        element.
   * @return                Returns with NS_OK if this doesn't meat any
   *                        unexpected situation.  If this method tries to
   *                        split the paragraph, marked as handled.
   */
  EditActionResult ReturnInParagraph(Selection& aSelection,
                                     Element& aParentDivOrP);

  /**
   * SplitParagraph() splits the parent block, aPara, at aSelNode - aOffset.
   *
   * @param aSelection          The selection.
   * @param aParentDivOrP       The parent block to be split.  This must be <p>
   *                            or <div> element.
   * @param aStartOfRightNode   The point to be start of right node after
   *                            split.  This must be descendant of
   *                            aParentDivOrP.
   * @param aNextBRNode         Next <br> node if there is.  Otherwise, nullptr.
   *                            If this is not nullptr, the <br> node may be
   *                            removed.
   */
  template<typename PT, typename CT>
  nsresult SplitParagraph(Selection& aSelection,
                          Element& aParentDivOrP,
                          const EditorDOMPointBase<PT, CT>& aStartOfRightNode,
                          nsIContent* aBRNode);

  nsresult ReturnInListItem(Selection& aSelection, Element& aHeader,
                            nsINode& aNode, int32_t aOffset);
  nsresult AfterEditInner(EditAction action,
                          nsIEditor::EDirection aDirection);
  nsresult RemovePartOfBlock(Element& aBlock, nsIContent& aStartChild,
                             nsIContent& aEndChild);
  void SplitBlock(Element& aBlock,
                  nsIContent& aStartChild,
                  nsIContent& aEndChild,
                  nsIContent** aOutLeftNode = nullptr,
                  nsIContent** aOutRightNode = nullptr,
                  nsIContent** aOutMiddleNode = nullptr);
  nsresult OutdentPartOfBlock(Element& aBlock,
                              nsIContent& aStartChild,
                              nsIContent& aEndChild,
                              bool aIsBlockIndentedWithCSS,
                              nsIContent** aOutLeftNode,
                              nsIContent** aOutRightNode);

  already_AddRefed<Element> ConvertListType(Element* aList, nsAtom* aListType,
                                            nsAtom* aItemType);

  nsresult CreateStyleForInsertText(Selection& aSelection, nsIDocument& aDoc);

  /**
   * IsEmptyBlockElement() returns true if aElement is a block level element
   * and it doesn't have any visible content.
   */
  enum class IgnoreSingleBR
  {
    eYes,
    eNo
  };
  bool IsEmptyBlockElement(Element& aElement,
                           IgnoreSingleBR aIgnoreSingleBR);

  nsresult CheckForEmptyBlock(nsINode* aStartNode, Element* aBodyNode,
                              Selection* aSelection,
                              nsIEditor::EDirection aAction, bool* aHandled);
  enum class BRLocation { beforeBlock, blockEnd };
  Element* CheckForInvisibleBR(Element& aBlock, BRLocation aWhere,
                               int32_t aOffset = 0);
  nsresult ExpandSelectionForDeletion(Selection& aSelection);
  nsresult NormalizeSelection(Selection* aSelection);
  EditorDOMPoint GetPromotedPoint(RulesEndpoint aWhere, nsINode& aNode,
                                  int32_t aOffset, EditAction actionID);
  void GetPromotedRanges(Selection& aSelection,
                         nsTArray<RefPtr<nsRange>>& outArrayOfRanges,
                         EditAction inOperationType);
  void PromoteRange(nsRange& aRange, EditAction inOperationType);
  enum class TouchContent { no, yes };
  nsresult GetNodesForOperation(
             nsTArray<RefPtr<nsRange>>& aArrayOfRanges,
             nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
             EditAction aOperationType,
             TouchContent aTouchContent = TouchContent::yes);
  void GetChildNodesForOperation(
         nsINode& aNode,
         nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes);
  nsresult GetNodesFromPoint(const EditorDOMPoint& aPoint,
                             EditAction aOperation,
                             nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                             TouchContent aTouchContent);
  nsresult GetNodesFromSelection(
             Selection& aSelection,
             EditAction aOperation,
             nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
             TouchContent aTouchContent = TouchContent::yes);
  enum class EntireList { no, yes };
  nsresult GetListActionNodes(
             nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
             EntireList aEntireList,
             TouchContent aTouchContent = TouchContent::yes);
  void GetDefinitionListItemTypes(Element* aElement, bool* aDT, bool* aDD);
  nsresult GetParagraphFormatNodes(
             nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
             TouchContent aTouchContent = TouchContent::yes);
  void LookInsideDivBQandList(nsTArray<OwningNonNull<nsINode>>& aNodeArray);
  nsresult BustUpInlinesAtRangeEndpoints(RangeItem& inRange);
  nsresult BustUpInlinesAtBRs(
             nsIContent& aNode,
             nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes);
  /**
   * GetHiestInlineParent() returns the highest inline node parent between
   * aNode and the editing host.  Even if the editing host is an inline
   * element, this method never returns the editing host as the result.
   */
  nsIContent* GetHighestInlineParent(nsINode& aNode);
  void MakeTransitionList(nsTArray<OwningNonNull<nsINode>>& aNodeArray,
                          nsTArray<bool>& aTransitionArray);
  nsresult RemoveBlockStyle(nsTArray<OwningNonNull<nsINode>>& aNodeArray);

  /**
   * ApplyBlockStyle() formats all nodes in aNodeArray with block elements
   * whose name is aBlockTag.
   * If aNodeArray has an inline element, a block element is created and the
   * inline element and following inline elements are moved into the new block
   * element.
   * If aNodeArray has <br> elements, they'll be removed from the DOM tree and
   * new block element will be created when there are some remaining inline
   * elements.
   * If aNodeArray has a block element, this calls itself with children of
   * the block element.  Then, new block element will be created when there
   * are some remaining inline elements.
   *
   * @param aNodeArray      Must be descendants of a node.
   * @param aBlockTag       The element name of new block elements.
   */
  nsresult ApplyBlockStyle(nsTArray<OwningNonNull<nsINode>>& aNodeArray,
                           nsAtom& aBlockTag);

  nsresult MakeBlockquote(nsTArray<OwningNonNull<nsINode>>& aNodeArray);

  /**
   * MaybeSplitAncestorsForInsertWithTransaction() does nothing if container of
   * aStartOfDeepestRightNode can have an element whose tag name is aTag.
   * Otherwise, looks for an ancestor node which is or is in active editing
   * host and can have an element whose name is aTag.  If there is such
   * ancestor, its descendants are split.
   *
   * Note that this may create empty elements while splitting ancestors.
   *
   * @param aTag                        The name of element to be inserted
   *                                    after calling this method.
   * @param aStartOfDeepestRightNode    The start point of deepest right node.
   *                                    This point must be descendant of
   *                                    active editing host.
   * @return                            When succeeded, SplitPoint() returns
   *                                    the point to insert the element.
   */
  template<typename PT, typename CT>
  SplitNodeResult MaybeSplitAncestorsForInsertWithTransaction(
                    nsAtom& aTag,
                    const EditorDOMPointBase<PT, CT>& aStartOfDeepestRightNode);

  /**
   * JoinNearestEditableNodesWithTransaction() joins two editable nodes which
   * are themselves or the nearest editable node of aLeftNode and aRightNode.
   * XXX This method's behavior is odd.  For example, if user types Backspace
   *     key at the second editable paragraph in this case:
   *     <div contenteditable>
   *       <p>first editable paragraph</p>
   *       <p contenteditable="false">non-editable paragraph</p>
   *       <p>second editable paragraph</p>
   *     </div>
   *     The first editable paragraph's content will be moved into the second
   *     editable paragraph and the non-editable paragraph becomes the first
   *     paragraph of the editor.  I don't think that it's expected behavior of
   *     any users...
   *
   * @param aLeftNode   The node which will be removed.
   * @param aRightNode  The node which will be inserted the content of
   *                    aLeftNode.
   * @return            The point at the first child of aRightNode.
   */
  EditorDOMPoint
  JoinNearestEditableNodesWithTransaction(nsIContent& aLeftNode,
                                          nsIContent& aRightNode);

  Element* GetTopEnclosingMailCite(nsINode& aNode);
  nsresult PopListItem(nsIContent& aListItem, bool* aOutOfList = nullptr);
  nsresult RemoveListStructure(Element& aList);
  nsresult CacheInlineStyles(nsINode* aNode);
  nsresult ReapplyCachedStyles();
  void ClearCachedStyles();
  void AdjustSpecialBreaks();
  nsresult AdjustWhitespace(Selection* aSelection);
  nsresult PinSelectionToNewBlock(Selection* aSelection);
  void CheckInterlinePosition(Selection& aSelection);
  nsresult AdjustSelection(Selection* aSelection,
                           nsIEditor::EDirection aAction);

  /**
   * FindNearEditableNode() tries to find an editable node near aPoint.
   *
   * @param aPoint      The DOM point where to start to search from.
   * @param aDirection  If nsIEditor::ePrevious is set, this searches an
   *                    editable node from next nodes.  Otherwise, from
   *                    previous nodes.
   * @return            If found, returns non-nullptr.  Otherwise, nullptr.
   *                    Note that if found node is in different table element,
   *                    this returns nullptr.
   *                    And also if aDirection is not nsIEditor::ePrevious,
   *                    the result may be the node pointed by aPoint.
   */
  template<typename PT, typename CT>
  nsIContent* FindNearEditableNode(const EditorDOMPointBase<PT, CT>& aPoint,
                                   nsIEditor::EDirection aDirection);
  /**
   * Returns true if aNode1 or aNode2 or both is the descendant of some type of
   * table element, but their nearest table element ancestors differ.  "Table
   * element" here includes not just <table> but also <td>, <tbody>, <tr>, etc.
   * The nodes count as being their own descendants for this purpose, so a
   * table element is its own nearest table element ancestor.
   */
  bool InDifferentTableElements(nsINode* aNode1, nsINode* aNode2);
  nsresult RemoveEmptyNodes();
  nsresult SelectionEndpointInNode(nsINode* aNode, bool* aResult);
  nsresult UpdateDocChangeRange(nsRange* aRange);
  nsresult ConfirmSelectionInBody();

  /**
   * Insert normal <br> element into aNode when aNode is a block and it has
   * no children.
   */
  nsresult InsertBRIfNeeded(nsINode& aNode)
  {
    return InsertBRIfNeededInternal(aNode, false);
  }

  /**
   * Insert moz-<br> element (<br type="_moz">) into aNode when aNode is a
   * block and it has no children.
   */
  nsresult InsertMozBRIfNeeded(nsINode& aNode)
  {
    return InsertBRIfNeededInternal(aNode, true);
  }

  bool IsEmptyInline(nsINode& aNode);
  bool ListIsEmptyLine(nsTArray<OwningNonNull<nsINode>>& arrayOfNodes);
  nsresult RemoveAlignment(nsINode& aNode, const nsAString& aAlignType,
                           bool aChildrenOnly);
  nsresult MakeSureElemStartsOrEndsOnCR(nsINode& aNode, bool aStarts);
  enum class ContentsOnly { no, yes };
  nsresult AlignBlock(Element& aElement,
                      const nsAString& aAlignType, ContentsOnly aContentsOnly);
  enum class Change { minus, plus };
  nsresult ChangeIndentation(Element& aElement, Change aChange);
  void DocumentModifiedWorker();

  /**
   * InitStyleCacheArray() initializes aStyleCache for usable with
   * GetInlineStyles().
   */
  void InitStyleCacheArray(StyleCache aStyleCache[SIZE_STYLE_TABLE]);

  /**
   * GetInlineStyles() retrieves the style of aNode and modifies each item of
   * aStyleCache.
   */
  nsresult GetInlineStyles(nsINode* aNode,
                           StyleCache aStyleCache[SIZE_STYLE_TABLE]);

protected:
  HTMLEditor* mHTMLEditor;
  RefPtr<nsRange> mDocChangeRange;
  bool mListenerEnabled;
  bool mReturnInEmptyLIKillsList;
  bool mDidDeleteSelection;
  bool mDidRangedDelete;
  bool mRestoreContentEditableCount;
  RefPtr<nsRange> mUtilRange;
  // Need to remember an int across willJoin/didJoin...
  uint32_t mJoinOffset;
  nsCOMPtr<Element> mNewBlock;
  RefPtr<RangeItem> mRangeItem;

  // XXX In strict speaking, mCachedStyles isn't enough to cache inline styles
  //     because inline style can be specified with "style" attribute and/or
  //     CSS in <style> elements or CSS files.  So, we need to look for better
  //     implementation about this.
  StyleCache mCachedStyles[SIZE_STYLE_TABLE];
};

} // namespace mozilla

#endif // #ifndef HTMLEditRules_h

