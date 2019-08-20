/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditRules_h
#define HTMLEditRules_h

#include "TypeInState.h"
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint
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
class SplitNodeResult;
class TextEditor;
enum class EditSubAction : int32_t;

namespace dom {
class Document;
class Element;
class Selection;
}  // namespace dom

struct StyleCache final : public PropItem {
  bool mPresent;

  StyleCache() : PropItem(), mPresent(false) { MOZ_COUNT_CTOR(StyleCache); }

  StyleCache(nsAtom* aTag, nsAtom* aAttr, const nsAString& aValue)
      : PropItem(aTag, aAttr, aValue), mPresent(false) {
    MOZ_COUNT_CTOR(StyleCache);
  }

  StyleCache(nsAtom* aTag, nsAtom* aAttr)
      : PropItem(aTag, aAttr, EmptyString()), mPresent(false) {
    MOZ_COUNT_CTOR(StyleCache);
  }

  ~StyleCache() { MOZ_COUNT_DTOR(StyleCache); }
};

/**
 * Same as TextEditRules, any methods which may modify the DOM tree or
 * Selection should be marked as MOZ_MUST_USE and return nsresult directly
 * or with simple class like EditActionResult.  And every caller of them
 * has to check whether the result is NS_ERROR_EDITOR_DESTROYED and if it is,
 * its callers should stop handling edit action since after mutation event
 * listener or selectionchange event listener disables the editor, we should
 * not modify the DOM tree nor Selection anymore.  And also when methods of
 * this class call methods of other classes like HTMLEditor and WSRunObject,
 * they should check whether CanHandleEditAtion() returns false immediately
 * after the calls.  If it returns false, they should return
 * NS_ERROR_EDITOR_DESTROYED.
 */

#define SIZE_STYLE_TABLE 19

class HTMLEditRules : public TextEditRules {
 public:
  HTMLEditRules();

  // TextEditRules methods
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult Init(TextEditor* aTextEditor) override;
  virtual nsresult DetachEditor() override;
  virtual nsresult BeforeEdit() override;
  MOZ_CAN_RUN_SCRIPT virtual nsresult AfterEdit() override;
  // NOTE: Don't mark WillDoAction() nor DidDoAction() as MOZ_CAN_RUN_SCRIPT
  //       because they are too generic and doing it makes a lot of public
  //       editor methods marked as MOZ_CAN_RUN_SCRIPT too, but some of them
  //       may not causes running script.  So, ideal fix must be that we make
  //       each method callsed by this method public.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult WillDoAction(EditSubActionInfo& aInfo, bool* aCancel,
                                bool* aHandled) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult DidDoAction(EditSubActionInfo& aInfo,
                               nsresult aResult) override;
  virtual bool DocumentIsEmpty() const override;

  /**
   * DocumentModified() is called when editor content is changed.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult DocumentModified();

  MOZ_CAN_RUN_SCRIPT
  nsresult GetListState(bool* aMixed, bool* aOL, bool* aUL, bool* aDL);
  MOZ_CAN_RUN_SCRIPT
  nsresult GetListItemState(bool* aMixed, bool* aLI, bool* aDT, bool* aDD);
  MOZ_CAN_RUN_SCRIPT
  nsresult GetAlignment(bool* aMixed, nsIHTMLEditor::EAlignment* aAlign);
  MOZ_CAN_RUN_SCRIPT
  nsresult GetParagraphState(bool* aMixed, nsAString& outFormat);

  /**
   * MakeSureElemStartsAndEndsOnCR() inserts <br> element at start (and/or end)
   * of aNode if neither:
   * - first (last) editable child of aNode is a block or a <br>,
   * - previous (next) sibling of aNode is block or a <br>
   * - nor no previous (next) sibling of aNode.
   *
   * @param aNode               The node which may be inserted <br> elements.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult MakeSureElemStartsAndEndsOnCR(nsINode& aNode);

  void DidCreateNode(Element& aNewElement);
  void DidInsertNode(nsIContent& aNode);
  void WillDeleteNode(nsINode& aChild);
  void DidSplitNode(nsINode& aExistingRightNode, nsINode& aNewLeftNode);
  void WillJoinNodes(nsINode& aLeftNode, nsINode& aRightNode);
  void DidJoinNodes(nsINode& aLeftNode, nsINode& aRightNode);
  void DidInsertText(nsINode& aTextNode, int32_t aOffset,
                     const nsAString& aString);
  void DidDeleteText(nsINode& aTextNode, int32_t aOffset, int32_t aLength);
  void WillDeleteSelection();

  void StartToListenToEditSubActions() { mListenerEnabled = true; }
  void EndListeningToEditSubActions() { mListenerEnabled = false; }

 protected:
  virtual ~HTMLEditRules() = default;

  HTMLEditor& HTMLEditorRef() const {
    MOZ_ASSERT(mData);
    return mData->HTMLEditorRef();
  }

  static bool IsBlockNode(const nsINode& aNode) {
    return HTMLEditor::NodeIsBlockStatic(&aNode);
  }
  static bool IsInlineNode(const nsINode& aNode) { return !IsBlockNode(aNode); }

  enum RulesEndpoint { kStart, kEnd };

  void InitFields();

  /**
   * Called before inserting something into the editor.
   * This method may removes mBougsNode if there is.  Therefore, this method
   * might cause destroying the editor.
   *
   * @param aCancel             Returns true if the operation is canceled.
   *                            This can be nullptr.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult WillInsert(bool* aCancel = nullptr);

  /**
   * Called before inserting text.
   * This method may actually inserts text into the editor.  Therefore, this
   * might cause destroying the editor.
   *
   * @param aEditSubAction      Must be EditSubAction::eInsertTextComingFromIME
   *                            or EditSubAction::eInsertText.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   * @param inString            String to be inserted.
   * @param outString           String actually inserted.
   * @param aMaxLength          The maximum string length which the editor
   *                            allows to set.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillInsertText(EditSubAction aEditSubAction,
                                       bool* aCancel, bool* aHandled,
                                       const nsAString* inString,
                                       nsAString* outString,
                                       int32_t aMaxLength);

  /**
   * WillInsertParagraphSeparator() is called when insertParagraph command is
   * executed or something equivalent.  This method actually tries to insert
   * new paragraph or <br> element, etc.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE EditActionResult WillInsertParagraphSeparator();

  /**
   * If aNode is a text node that contains only collapsed whitespace, delete
   * it.  It doesn't serve any useful purpose, and we don't want it to confuse
   * code that doesn't correctly skip over it.
   *
   * If deleting the node fails (like if it's not editable), the caller should
   * proceed as usual, so don't return any errors.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult DeleteNodeIfCollapsedText(nsINode& aNode);

  /**
   * InsertBRElement() inserts a <br> element into aInsertToBreak.
   *
   * @param aInsertToBreak      The point where new <br> element will be
   *                            inserted before.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult InsertBRElement(const EditorDOMPoint& aInsertToBreak);

  /**
   * SplitMailCites() splits mail-cite elements at start of Selection if
   * Selection starts from inside a mail-cite element.  Of course, if it's
   * necessary, this inserts <br> node to new left nodes or existing right
   * nodes.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE EditActionResult SplitMailCites();

  /**
   * Called before deleting selected contents.  This method actually removes
   * selected contents.
   *
   * @param aAction             Direction of the deletion.
   * @param aStripWrappers      Must be eStrip or eNoStrip.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillDeleteSelection(
      nsIEditor::EDirection aAction, nsIEditor::EStripWrappers aStripWrappers,
      bool* aCancel, bool* aHandled);

  /**
   * Called after deleting selected content.
   * This method removes unnecessary empty nodes and/or inserts <br> if
   * necessary.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult DidDeleteSelection();

  /**
   * InsertBRIfNeeded() determines if a br is needed for current selection to
   * not be spastic.  If so, it inserts one.  Callers responsibility to only
   * call with collapsed selection.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult InsertBRIfNeeded();

  /**
   * CanContainParagraph() returns true if aElement can have a <p> element as
   * its child or its descendant.
   */
  bool CanContainParagraph(Element& aElement) const;

  /**
   * Insert normal <br> element into aNode when aNode is a block and it has
   * no children.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult InsertBRIfNeeded(nsINode& aNode) {
    return InsertBRIfNeededInternal(aNode, false);
  }

  /**
   * Insert padding <br> element for empty last line into aNode when aNode is a
   * block and it has no children.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult
  InsertPaddingBRElementForEmptyLastLineIfNeeded(nsINode& aNode) {
    return InsertBRIfNeededInternal(aNode, true);
  }

  /**
   * Insert a normal <br> element or a padding <br> element for empty last line
   * to aNode when aNode is a block and it has no children.  Use
   * InsertBRIfNeeded() or InsertPaddingBRElementForEmptyLastLineIfNeeded()
   * instead.
   *
   * @param aNode               Reference to a block parent.
   * @param aForPadding         true if this should insert a <br> element for
   *                            placing caret at empty last line.
   *                            Otherwise, i.e., this should insert a normal
   *                            <br> element, false.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult
  InsertBRIfNeededInternal(nsINode& aNode, bool aForPadding);

  /**
   * GetGoodSelPointForNode() finds where at a node you would want to set the
   * selection if you were trying to have a caret next to it.  Always returns a
   * valid value (unless mHTMLEditor has gone away).
   *
   * @param aNode         The node
   * @param aAction       Which edge to find:
   *                        eNext/eNextWord/eToEndOfLine indicates beginning,
   *                        ePrevious/PreviousWord/eToBeginningOfLine ending.
   */
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
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE EditActionResult
  TryToJoinBlocksWithTransaction(nsIContent& aLeftNode, nsIContent& aRightNode);

  /**
   * MoveBlock() moves the content from aRightBlock starting from aRightOffset
   * into aLeftBlock at aLeftOffset. Note that the "block" can be inline nodes
   * between <br>s, or between blocks, etc.  DTD containment rules are followed
   * throughout.
   *
   * @return            Sets handled to true if this actually joins the nodes.
   *                    canceled is always false.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE EditActionResult MoveBlock(Element& aLeftBlock,
                                          Element& aRightBlock,
                                          int32_t aLeftOffset,
                                          int32_t aRightOffset);

  /**
   * MoveNodeSmart() moves aNode to (aDestElement, aInOutDestOffset).
   * DTD containment rules are followed throughout.
   *
   * @param aOffset                 returns the point after inserted content.
   * @return                        Sets true to handled if this actually moves
   *                                the nodes.
   *                                canceled is always false.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE EditActionResult MoveNodeSmart(nsIContent& aNode,
                                              Element& aDestElement,
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
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE EditActionResult MoveContents(Element& aElement,
                                             Element& aDestElement,
                                             int32_t* aInOutDestOffset);

  /**
   * DeleteElementsExceptTableRelatedElements() removes elements except
   * table related elements (except <table> itself) and their contents
   * from the DOM tree.
   *
   * @param aNode               If this is not a table related element, this
   *                            node will be removed from the DOM tree.
   *                            Otherwise, this method calls itself recursively
   *                            with its children.
   *
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult
  DeleteElementsExceptTableRelatedElements(nsINode& aNode);

  /**
   * XXX Should document what this does.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillMakeList(const nsAString* aListType,
                                     bool aEntireList,
                                     const nsAString* aBulletType,
                                     bool* aCancel, bool* aHandled,
                                     const nsAString* aItemType = nullptr);

  /**
   * Called before removing a list element.  This method actually removes
   * list elements and list item elements at Selection.  And move contents
   * in them where the removed list was.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillRemoveList(bool* aCancel, bool* aHandled);

  /**
   * Called before indenting around Selection.  This method actually tries to
   * indent the contents.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillIndent(bool* aCancel, bool* aHandled);

  /**
   * Called before indenting around Selection and it's in CSS mode.
   * This method actually tries to indent the contents.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillCSSIndent(bool* aCancel, bool* aHandled);

  /**
   * Called before indenting around Selection and it's not in CSS mode.
   * This method actually tries to indent the contents.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillHTMLIndent(bool* aCancel, bool* aHandled);

  /**
   * Called before outdenting around Selection.  This method actually tries
   * to indent the contents.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillOutdent(bool* aCancel, bool* aHandled);

  /**
   * Called before aligning contents around Selection.  This method actually
   * sets align attributes to align contents.
   *
   * @param aAlignType          New align attribute value where the contents
   *                            should be aligned to.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult WillAlign(const nsAString& aAlignType, bool* aCancel,
                     bool* aHandled);

  /**
   * Called before changing absolute positioned element to static positioned.
   * This method actually changes the position property of nearest absolute
   * positioned element.  Therefore, this might cause destroying the HTML
   * editor.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillRemoveAbsolutePosition(bool* aCancel,
                                                   bool* aHandled);

  /**
   * Called before changing z-index.
   * This method actually changes z-index of nearest absolute positioned
   * element relatively.  Therefore, this might cause destroying the HTML
   * editor.
   *
   * @param aChange             Amount to change z-index.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillRelativeChangeZIndex(int32_t aChange, bool* aCancel,
                                                 bool* aHandled);

  /**
   * Called before creating aDefinitionListItemTag around Selection.  This
   * method just calls WillMakeList() with "dl" as aListType and
   * aDefinitionListItemTag as aItemType.
   *
   * @param aDefinitionListItemTag  Should be "dt" or "dd".
   * @param aEntireList             XXX not sure
   * @param aCancel                 Returns true if the operation is canceled.
   * @param aHandled                Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillMakeDefListItem(const nsAString* aBlockType,
                                            bool aEntireList, bool* aCancel,
                                            bool* aHandled);

  /**
   * WillMakeBasicBlock() called before changing block style around Selection.
   * This method actually does something with calling MakeBasicBlock().
   *
   * @param aBlockType          Necessary block style as string.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillMakeBasicBlock(const nsAString& aBlockType,
                                           bool* aCancel, bool* aHandled);

  /**
   * MakeBasicBlock() applies or clears block style around Selection.
   * This method creates AutoSelectionRestorer.  Therefore, each caller
   * need to check if the editor is still available even if this returns
   * NS_OK.
   *
   * @param aBlockType          New block tag name.
   *                            If nsGkAtoms::normal or nsGkAtoms::_empty,
   *                            RemoveBlockStyle() will be called.
   *                            If nsGkAtoms::blockquote, MakeBlockquote()
   *                            will be called.
   *                            Otherwise, ApplyBlockStyle() will be called.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult MakeBasicBlock(nsAtom& aBlockType);

  /**
   * Called after creating a basic block, indenting, outdenting or aligning
   * contents.  This method inserts a padding <br> element for empty last line
   * if start container of Selection needs it.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult DidMakeBasicBlock();

  /**
   * Called before changing an element to absolute positioned.
   * This method only prepares the operation since DidAbsolutePosition() will
   * change it actually later.  mNewBlock is set to the target element and
   * if necessary, some ancestor nodes of selection may be split.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillAbsolutePosition(bool* aCancel, bool* aHandled);

  /**
   * PrepareToMakeElementAbsolutePosition() is helper method of
   * WillAbsolutePosition() since in some cases, needs to restore selection
   * with AutoSelectionRestorer.  So, all callers have to check if
   * CanHandleEditAction() still returns true after a call of this method.
   * XXX Should be documented outline of this method.
   *
   * @param aHandled            Returns true if the edit action is handled.
   * @param aTargetElement      Returns target element which should be
   *                            changed to absolute positioned.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult PrepareToMakeElementAbsolutePosition(
      bool* aHandled, RefPtr<Element>* aTargetElement);

  /**
   * Called if nobody handles the edit action to make an element absolute
   * positioned.
   * This method actually changes the element which is computed by
   * WillAbsolutePosition() to absolute positioned.
   * Therefore, this might cause destroying the HTML editor.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult DidAbsolutePosition();

  /**
   * AlignInnerBlocks() calls AlignBlockContents() for every list item element
   * and table cell element in aNode.
   *
   * @param aNode               The node whose descendants should be aligned
   *                            to aAlignType.
   * @param aAlignType          New value of align attribute of <div>.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult AlignInnerBlocks(nsINode& aNode,
                                         const nsAString& aAlignType);

  /**
   * AlignBlockContents() sets align attribute of <div> element which is
   * only child of aNode to aAlignType.  If aNode has 2 or more children or
   * does not have a <div> element has only child, inserts a <div> element
   * into aNode and move all children of aNode into the new <div> element.
   *
   * @param aNode               The node whose contents should be aligned
   *                            to aAlignType.
   * @param aAlignType          New value of align attribute of <div> which
   *                            is only child of aNode.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult AlignBlockContents(nsINode& aNode,
                                           const nsAString& aAlignType);

  /**
   * AlignContentsAtSelection() aligns contents around Selection to aAlignType.
   * This creates AutoSelectionRestorer.  Therefore, even if this returns
   * NS_OK, CanHandleEditAction() may return false if the editor is destroyed
   * during restoring the Selection.  So, every caller needs to check if
   * CanHandleEditAction() returns true before modifying the DOM tree or
   * changing Selection.
   *
   * @param aAlignType          New align attribute value where the contents
   *                            should be aligned to.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult AlignContentsAtSelection(const nsAString& aAlignType);

  nsresult AppendInnerFormatNodes(nsTArray<OwningNonNull<nsINode>>& aArray,
                                  nsINode* aNode);
  nsresult GetFormatString(nsINode* aNode, nsAString& outFormat);

  /**
   * aLists and aTables allow the caller to specify what kind of content to
   * "look inside".  If aTables is Tables::yes, look inside any table content,
   * and insert the inner content into the supplied nsTArray at offset
   * aIndex.  Similarly with aLists and list content.  aIndex is updated to
   * point past inserted elements.
   */
  enum class Lists { no, yes };
  enum class Tables { no, yes };
  void GetInnerContent(nsINode& aNode,
                       nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                       int32_t* aIndex, Lists aLists = Lists::yes,
                       Tables aTables = Tables::yes) const;

  /**
   * If aNode is the descendant of a listitem, return that li.  But table
   * element boundaries are stoppers on the search.  Also stops on the active
   * editor host (contenteditable).  Also test if aNode is an li itself.
   */
  Element* IsInListItem(nsINode* aNode);

  nsAtom& DefaultParagraphSeparator();

  /**
   * ReturnInHeader() handles insertParagraph command (i.e., handling Enter
   * key press) in a heading element.  This splits aHeader element at
   * aOffset in aNode.  Then, if right heading element is empty, it'll be
   * removed and new paragraph is created (its type is decided with default
   * paragraph separator).
   *
   * @param aHeader             The heading element to be split.
   * @param aNode               Typically, Selection start container,
   *                            where to be split.
   * @param aOffset             Typically, Selection start offset in the
   *                            start container, where to be split.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult ReturnInHeader(Element& aHeader, nsINode& aNode,
                                       int32_t aOffset);

  /**
   * ReturnInParagraph() does the right thing for Enter key press or
   * 'insertParagraph' command in aParentDivOrP.  aParentDivOrP will be
   * split at start of first selection range.
   *
   * @param aParentDivOrP   The parent block.  This must be <p> or <div>
   *                        element.
   * @return                Returns with NS_OK if this doesn't meat any
   *                        unexpected situation.  If this method tries to
   *                        split the paragraph, marked as handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE EditActionResult ReturnInParagraph(Element& aParentDivOrP);

  /**
   * SplitParagraph() splits the parent block, aPara, at aSelNode - aOffset.
   *
   * @param aParentDivOrP       The parent block to be split.  This must be <p>
   *                            or <div> element.
   * @param aStartOfRightNode   The point to be start of right node after
   *                            split.  This must be descendant of
   *                            aParentDivOrP.
   * @param aNextBRNode         Next <br> node if there is.  Otherwise, nullptr.
   *                            If this is not nullptr, the <br> node may be
   *                            removed.
   */
  template <typename PT, typename CT>
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult SplitParagraph(
      Element& aParentDivOrP,
      const EditorDOMPointBase<PT, CT>& aStartOfRightNode, nsIContent* aBRNode);

  /**
   * ReturnInListItem() handles insertParagraph command (i.e., handling
   * Enter key press) in a list item element.
   *
   * @param aListItem           The list item which has the following point.
   * @param aNode               Typically, Selection start container, where to
   *                            insert a break.
   * @param aOffset             Typically, Selection start offset in the
   *                            start container, where to insert a break.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult ReturnInListItem(Element& aListItem, nsINode& aNode,
                                         int32_t aOffset);

  /**
   * Called after handling edit action.  This may adjust Selection, remove
   * unnecessary empty nodes, create <br> elements if needed, etc.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult AfterEditInner();

  /**
   * IndentAroundSelectionWithCSS() indents around Selection with CSS.
   * This method creates AutoSelectionRestorer.  Therefore, each caller
   * need to check if the editor is still available even if this returns
   * NS_OK.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult IndentAroundSelectionWithCSS();

  /**
   * IndentAroundSelectionWithHTML() indents around Selection with HTML.
   * This method creates AutoSelectionRestorer.  Therefore, each caller
   * need to check if the editor is still available even if this returns
   * NS_OK.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult IndentAroundSelectionWithHTML();

  /**
   * OutdentAroundSelection() outdents contents around Selection.
   * This method creates AutoSelectionRestorer.  Therefore, each caller
   * need to check if the editor is still available even if this returns
   * NS_OK.
   *
   * @return                    The left content is left content of last
   *                            outdented element.
   *                            The right content is right content of last
   *                            outdented element.
   *                            The middle content is middle content of last
   *                            outdented element.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE SplitRangeOffFromNodeResult OutdentAroundSelection();

  /**
   * SplitRangeOffFromBlockAndRemoveMiddleContainer() splits the nodes
   * between aStartOfRange and aEndOfRange, then, removes the middle element
   * and moves its content to where the middle element was.
   *
   * @param aBlockElement           The node which will be split.
   * @param aStartOfRange           The first node which will be unwrapped
   *                                from aBlockElement.
   * @param aEndOfRange             The last node which will be unwrapped from
   *                                aBlockElement.
   * @return                        The left content is new created left
   *                                element of aBlockElement.
   *                                The right content is split element,
   *                                i.e., must be aBlockElement.
   *                                The middle content is nullptr since
   *                                removing it is the job of this method.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE SplitRangeOffFromNodeResult
  SplitRangeOffFromBlockAndRemoveMiddleContainer(Element& aBlockElement,
                                                 nsIContent& aStartOfRange,
                                                 nsIContent& aEndOfRange);

  /**
   * SplitRangeOffFromBlock() splits aBlock at two points, before aStartChild
   * and after aEndChild.  If they are very start or very end of aBlcok, this
   * won't create empty block.
   *
   * @param aBlockElement           A block element which will be split.
   * @param aStartOfMiddleElement   Start node of middle block element.
   * @param aEndOfMiddleElement     End node of middle block element.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE SplitRangeOffFromNodeResult SplitRangeOffFromBlock(
      Element& aBlockElement, nsIContent& aStartOfMiddleElement,
      nsIContent& aEndOfMiddleElement);

  /**
   * OutdentPartOfBlock() outdents the nodes between aStartOfOutdent and
   * aEndOfOutdent.  This splits the range off from aBlockElement first.
   * Then, removes the middle element if aIsBlockIndentedWithCSS is false.
   * Otherwise, decreases the margin of the middle element.
   *
   * @param aBlockElement           A block element which includes both
   *                                aStartOfOutdent and aEndOfOutdent.
   * @param aStartOfOutdent         First node which is descendant of
   *                                aBlockElement will be outdented.
   * @param aEndOfOutdent           Last node which is descandant of
   *                                aBlockElement will be outdented.
   * @param aIsBlockIndentedWithCSS true if aBlockElement is indented with
   *                                CSS margin property.
   *                                false if aBlockElement is <blockquote>
   *                                or something.
   * @return                        The left content is new created element
   *                                splitting before aStartOfOutdent.
   *                                The right content is existing element.
   *                                The middle content is outdented element
   *                                if aIsBlockIndentedWithCSS is true.
   *                                Otherwise, nullptr.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE SplitRangeOffFromNodeResult
  OutdentPartOfBlock(Element& aBlockElement, nsIContent& aStartOfOutdent,
                     nsIContent& aEndOutdent, bool aIsBlockIndentedWithCSS);

  /**
   * XXX Should document what this does.
   * This method creates AutoSelectionRestorer.  Therefore, each caller
   * need to check if the editor is still available even if this returns
   * NS_OK.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult MakeList(nsAtom& aListType, bool aEntireList,
                                 const nsAString* aBulletType, bool* aCancel,
                                 nsAtom& aItemType);

  /**
   * ConvertListType() replaces child list items of aListElement with
   * new list item element whose tag name is aNewListItemTag.
   * Note that if there are other list elements as children of aListElement,
   * this calls itself recursively even though it's invalid structure.
   *
   * @param aListElement        The list element whose list items will be
   *                            replaced.
   * @param aNewListTag         New list tag name.
   * @param aNewListItemTag     New list item tag name.
   * @return                    New list element or an error code if it fails.
   *                            New list element may be aListElement if its
   *                            tag name is same as aNewListTag.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE CreateElementResult ConvertListType(Element& aListElement,
                                                   nsAtom& aListType,
                                                   nsAtom& aItemType);

  /**
   * CreateStyleForInsertText() sets CSS properties which are stored in
   * TypeInState to proper element node.
   *
   * @param aDocument           The document of the editor.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult CreateStyleForInsertText(dom::Document& aDocument);

  /**
   * IsEmptyBlockElement() returns true if aElement is a block level element
   * and it doesn't have any visible content.
   */
  enum class IgnoreSingleBR { eYes, eNo };
  bool IsEmptyBlockElement(Element& aElement, IgnoreSingleBR aIgnoreSingleBR);

  /**
   * MaybeDeleteTopMostEmptyAncestor() looks for top most empty block ancestor
   * of aStartNode in aEditingHostElement.
   * If found empty ancestor is a list item element, inserts a <br> element
   * before its parent element if grand parent is a list element.  Then,
   * collapse Selection to after the empty block.
   * If found empty ancestor is not a list item element, collapse Selection to
   * somewhere depending on aAction.
   * Finally, removes the empty block ancestor.
   *
   * @param aStartNode          Start node to look for empty ancestors.
   * @param aEditingHostElement Current editing host.
   * @param aAction             If found empty ancestor block is a list item
   *                            element, this is ignored.  Otherwise:
   *                            - If eNext, eNextWord or eToEndOfLine, collapse
   *                              Selection to after found empty ancestor.
   *                            - If ePrevious, ePreviousWord or
   *                              eToBeginningOfLine, collapse Selection to
   *                              end of previous editable node.
   *                            Otherwise, eNone is allowed but does nothing.
   * @param aHandled            Returns true if this method removes an empty
   *                            block ancestor of aStartNode.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult MaybeDeleteTopMostEmptyAncestor(
      nsINode& aStartNode, Element& aEditingHostElement,
      nsIEditor::EDirection aAction, bool* aHandled);

  enum class BRLocation { beforeBlock, blockEnd };
  Element* CheckForInvisibleBR(Element& aBlock, BRLocation aWhere,
                               int32_t aOffset = 0);

  /**
   * ExpandSelectionForDeletion() may expand Selection range if it's not
   * collapsed and there is only one range.  This may expand to include
   * invisible <br> element for preventing delete action handler to keep
   * unexpected nodes.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult ExpandSelectionForDeletion();

  /**
   * NormalizeSelection() adjust Selection if it's not collapsed and there is
   * only one range.  If range start and/or end point is <br> node or something
   * non-editable point, they should be moved to nearest text node or something
   * where the other methods easier to handle edit action.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult NormalizeSelection();

  /**
   * GetPromotedPoint() figures out where a start or end point for a block
   * operation really is.
   */
  EditorDOMPoint GetPromotedPoint(RulesEndpoint aWhere, nsINode& aNode,
                                  int32_t aOffset,
                                  EditSubAction aEditSubAction) const;

  /**
   * GetPromotedRanges() runs all the selection range endpoint through
   * GetPromotedPoint().
   */
  void GetPromotedRanges(nsTArray<RefPtr<nsRange>>& outArrayOfRanges,
                         EditSubAction aEditSubAction) const;

  /**
   * PromoteRange() expands a range to include any parents for which all
   * editable children are already in range.
   */
  void PromoteRange(nsRange& aRange, EditSubAction aEditSubAction) const;

  /**
   * GetNodesForOperation() runs through the ranges in the array and construct a
   * new array of nodes to be acted on.
   *
   * XXX This name stats with "Get" but actually this modifies the DOM tree with
   *     transaction.  We should rename this to making clearer what this does.
   */
  enum class TouchContent { no, yes };
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult GetNodesForOperation(
      nsTArray<RefPtr<nsRange>>& aArrayOfRanges,
      nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
      EditSubAction aEditSubAction, TouchContent aTouchContent) const;

  void GetChildNodesForOperation(
      nsINode& aNode, nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes);

  /**
   * GetNodesFromPoint() constructs a list of nodes from a point that will be
   * operated on.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult
  GetNodesFromPoint(const EditorDOMPoint& aPoint, EditSubAction aEditSubAction,
                    nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                    TouchContent aTouchContent) const;

  /**
   * GetNodesFromSelection() constructs a list of nodes from the selection that
   * will be operated on.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult
  GetNodesFromSelection(EditSubAction aEditSubAction,
                        nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                        TouchContent aTouchContent) const;

  enum class EntireList { no, yes };
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult
  GetListActionNodes(nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                     EntireList aEntireList, TouchContent aTouchContent) const;
  void GetDefinitionListItemTypes(Element* aElement, bool* aDT,
                                  bool* aDD) const;
  MOZ_CAN_RUN_SCRIPT
  nsresult GetParagraphFormatNodes(
      nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes);
  void LookInsideDivBQandList(
      nsTArray<OwningNonNull<nsINode>>& aNodeArray) const;

  /**
   * BustUpInlinesAtRangeEndpoints() splits nodes at both start and end of
   * aRangeItem.  If this splits at every point, this modifies aRangeItem
   * to point each split point (typically, right node).  Note that this splits
   * nodes only in highest inline element at every point.
   *
   * @param aRangeItem          One or two DOM points where should be split.
   *                            Will be modified to split point if they're
   *                            split.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult
  BustUpInlinesAtRangeEndpoints(RangeItem& aRangeItem) const;

  /**
   * BustUpInlinesAtBRs() splits before all <br> elements in aNode.  All <br>
   * nodes will be moved before right node at splitting its parent.  Finally,
   * this returns all <br> elements, every left node and aNode with
   * aOutArrayNodes.
   *
   * @param aNode               An inline container element.
   * @param aOutArrayOfNodes    All found <br> elements, left nodes (may not
   *                            be set if <br> is at start edge of aNode) and
   *                            aNode itself.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult
  BustUpInlinesAtBRs(nsIContent& aNode,
                     nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes) const;

  /**
   * GetHiestInlineParent() returns the highest inline node parent between
   * aNode and the editing host.  Even if the editing host is an inline
   * element, this method never returns the editing host as the result.
   */
  nsIContent* GetHighestInlineParent(nsINode& aNode) const;

  /**
   * MakeTransitionList() detects all the transitions in the array, where a
   * transition means that adjacent nodes in the array don't have the same
   * parent.
   */
  void MakeTransitionList(nsTArray<OwningNonNull<nsINode>>& aNodeArray,
                          nsTArray<bool>& aTransitionArray);

  /**
   * RemoveBlockStyle() removes all format blocks, table related element,
   * etc in aNodeArray.
   * If aNodeArray has a format node, it will be removed and its contents
   * will be moved to where it was.
   * If aNodeArray has a table related element, <li>, <blockquote> or <div>,
   * it will removed and its contents will be moved to where it was.
   */
  MOZ_CAN_RUN_SCRIPT
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
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult ApplyBlockStyle(
      nsTArray<OwningNonNull<nsINode>>& aNodeArray, nsAtom& aBlockTag);

  /**
   * MakeBlockquote() inserts at least one <blockquote> element and moves
   * nodes in aNodeArray into new <blockquote> elements.  If aNodeArray
   * includes a table related element except <table>, this calls itself
   * recursively to insert <blockquote> into the cell.
   *
   * @param aNodeArray          Nodes which will be moved into created
   *                            <blockquote> elements.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult
  MakeBlockquote(nsTArray<OwningNonNull<nsINode>>& aNodeArray);

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
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE SplitNodeResult
  MaybeSplitAncestorsForInsertWithTransaction(
      nsAtom& aTag, const EditorDOMPoint& aStartOfDeepestRightNode);

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
   * @param aNewFirstChildOfRightNode
   *                    The point at the first child of aRightNode.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult JoinNearestEditableNodesWithTransaction(
      nsIContent& aLeftNode, nsIContent& aRightNode,
      EditorDOMPoint* aNewFirstChildOfRightNode);

  Element* GetTopEnclosingMailCite(nsINode& aNode);

  /**
   * PopListItem() tries to move aListItem outside its parent.  If it's
   * in a middle of a list element, the parent list element is split before
   * aListItem.  Then, moves aListItem to before its parent list element.
   * I.e., moves aListItem between the 2 list elements if original parent
   * was split.  Then, if new parent is not a list element, the list item
   * element is removed and its contents are moved to where the list item
   * element was.
   *
   * @param aListItem           Should be a <li>, <dt> or <dd> element.
   *                            If it's not so, returns NS_ERROR_FAILURE.
   * @param aOutOfList          Returns true if the list item element is
   *                            removed (i.e., unwrapped contents of
   *                            aListItem).  Otherwise, false.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult PopListItem(nsIContent& aListItem,
                                    bool* aOutOfList = nullptr);

  /**
   * RemoveListStructure() destroys the list structure of aListElement.
   * If aListElement has <li>, <dl> or <dt> as a child, the element is removed
   * but its descendants are moved to where the list item element was.
   * If aListElement has another <ul>, <ol> or <dl> as a child, this method
   * is called recursively.
   * If aListElement has other nodes as its child, they are just removed.
   * Finally, aListElement is removed. and its all children are moved to
   * where the aListElement was.
   *
   * @param aListElement        A <ul>, <ol> or <dl> element.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult RemoveListStructure(Element& aListElement);

  /**
   * CacheInlineStyles() caches style of aNode into mCachedStyles.
   * This may cause flushing layout at retrieving computed value of CSS
   * properties.
   */
  MOZ_MUST_USE nsresult CacheInlineStyles(nsINode* aNode);

  /**
   * ReapplyCachedStyles() restores some styles which are disappeared during
   * handling edit action and it should be restored.  This may cause flushing
   * layout at retrieving computed value of CSS properties.
   */
  MOZ_MUST_USE nsresult ReapplyCachedStyles();

  void ClearCachedStyles();

  /**
   * InsertBRElementToEmptyListItemsAndTableCellsInChangedRange() inserts
   * <br> element into empty list item or table cell elements.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult
  InsertBRElementToEmptyListItemsAndTableCellsInChangedRange();

  /**
   * PinSelectionToNewBlock() may collapse Selection around mNewNode if it's
   * necessary,
   */
  MOZ_MUST_USE nsresult PinSelectionToNewBlock();

  void CheckInterlinePosition();

  /**
   * AdjustSelection() may adjust Selection range to nearest editable content.
   * Despite of the name, this may change the DOM tree.  If it needs to create
   * a <br> to put caret, this tries to create a <br> element.
   *
   * @param aAction     Maybe used to look for a good point to put caret.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult
  AdjustSelection(nsIEditor::EDirection aAction);

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
  template <typename PT, typename CT>
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

  /**
   * RemoveEmptyNodesInChangedRange() removes all empty nodes in
   * mDocChangeRange.  However, if mail-cite node has only a <br> element,
   * the node will be removed but <br> element is moved to where the
   * mail-cite node was.
   * XXX This method is expensive if mDocChangeRange is too wide and may
   *     remove unexpected empty element, e.g., it was created by JS, but
   *     we haven't touched it.  Cannot we remove this method and make
   *     guarantee that empty nodes won't be created?
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult RemoveEmptyNodesInChangedRange();

  nsresult SelectionEndpointInNode(nsINode* aNode, bool* aResult);
  nsresult UpdateDocChangeRange(nsRange* aRange);

  /**
   * ConfirmSelectionInBody() makes sure that Selection is in editor root
   * element typically <body> element (see HTMLEditor::UpdateRootElement())
   * and only one Selection range.
   * XXX This method is not necessary because even if selection is outside the
   *     <body> element, elements outside the <body> element should be
   *     editable, e.g., any element can be inserted siblings as <body> element
   *     and other browsers allow to edit such elements.
   */
  MOZ_MUST_USE nsresult ConfirmSelectionInBody();

  /**
   * IsEmptyInline: Return true if aNode is an empty inline container
   */
  bool IsEmptyInline(nsINode& aNode);

  bool ListIsEmptyLine(nsTArray<OwningNonNull<nsINode>>& arrayOfNodes);

  /**
   * RemoveAlignment() removes align attributes, text-align properties and
   * <center> elements in aNode.
   *
   * @param aNode               Alignment information of the node and/or its
   *                            descendants will be removed.
   * @param aAlignType          New align value to be set only when it's in
   *                            CSS mode and this method meets <table> or <hr>.
   *                            XXX This is odd and not clear when you see
   *                                caller of this method.  Do you have better
   *                                idea?
   * @param aDescendantsOnly    true if align information of aNode itself
   *                            shouldn't be removed.  Otherwise, false.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult RemoveAlignment(nsINode& aNode,
                                        const nsAString& aAlignType,
                                        bool aDescendantsOnly);

  /**
   * MakeSureElemStartsOrEndsOnCR() inserts <br> element at start (end) of
   * aNode if neither:
   * - first (last) editable child of aNode is a block or a <br>,
   * - previous (next) sibling of aNode is block or a <br>
   * - nor no previous (next) sibling of aNode.
   *
   * @param aNode               The node which may be inserted <br> element.
   * @param aStarts             true for trying to insert <br> to the start.
   *                            false for trying to insert <br> to the end.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult MakeSureElemStartsOrEndsOnCR(nsINode& aNode,
                                                     bool aStarts);

  /**
   * AlignBlock() resets align attribute, text-align property, etc first.
   * Then, aligns contents of aElement on aAlignType.
   *
   * @param aElement            The element whose contents will be aligned.
   * @param aAlignType          Boundary or "center" which contents should be
   *                            aligned on.
   * @param aResetAlignOf       Resets align of whether element and its
   *                            descendants or only descendants.
   */
  enum class ResetAlignOf { ElementAndDescendants, OnlyDescendants };
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult AlignBlock(Element& aElement,
                                   const nsAString& aAlignType,
                                   ResetAlignOf aResetAlignOf);

  /**
   * IncreaseMarginToIndent() increases the margin of aElement.  See the
   * document of ChangeMarginStart() for the detail.
   * XXX This is not aware of vertical writing-mode.
   *
   * @param aElement            The element to be indented.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult IncreaseMarginToIndent(Element& aElement) {
    return ChangeMarginStart(aElement, true);
  }

  /**
   * DecreaseMarginToOutdent() decreases the margin of aElement.  See the
   * document of ChangeMarginStart() for the detail.
   * XXX This is not aware of vertical writing-mode.
   *
   * @param aElement            The element to be outdented.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult DecreaseMarginToOutdent(Element& aElement) {
    return ChangeMarginStart(aElement, false);
  }

  /**
   * ChangeMarginStart() changes margin of aElement to indent or outdent.
   * However, use IncreaseMarginToIndent() and DecreaseMarginToOutdent()
   * instead.  If it's rtl text, margin-right will be changed.  Otherwise,
   * margin-left.
   * XXX This is not aware of vertical writing-mode.
   *
   * @param aElement            The element to be indented or outdented.
   * @param aIncrease           true for indent, false for outdent.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult ChangeMarginStart(Element& aElement, bool aIncrease);

  /**
   * DocumentModifiedWorker() is called by DocumentModified() either
   * synchronously or asynchronously.
   */
  MOZ_CAN_RUN_SCRIPT void DocumentModifiedWorker();

  /**
   * InitStyleCacheArray() initializes aStyleCache for usable with
   * GetInlineStyles().
   */
  void InitStyleCacheArray(StyleCache aStyleCache[SIZE_STYLE_TABLE]);

  /**
   * GetInlineStyles() retrieves the style of aNode and modifies each item of
   * aStyleCache.  This might cause flushing layout at retrieving computed
   * values of CSS properties.
   */
  MOZ_MUST_USE nsresult
  GetInlineStyles(nsINode* aNode, StyleCache aStyleCache[SIZE_STYLE_TABLE]);

 protected:
  HTMLEditor* mHTMLEditor;
  RefPtr<nsRange> mDocChangeRange;
  bool mInitialized;
  bool mListenerEnabled;
  bool mReturnInEmptyLIKillsList;
  bool mDidRangedDelete;
  bool mDidEmptyParentBlocksRemoved;
  bool mRestoreContentEditableCount;
  RefPtr<nsRange> mUtilRange;
  // Need to remember an int across willJoin/didJoin...
  uint32_t mJoinOffset;
  RefPtr<Element> mNewBlock;
  RefPtr<RangeItem> mRangeItem;

  // XXX In strict speaking, mCachedStyles isn't enough to cache inline styles
  //     because inline style can be specified with "style" attribute and/or
  //     CSS in <style> elements or CSS files.  So, we need to look for better
  //     implementation about this.
  StyleCache mCachedStyles[SIZE_STYLE_TABLE];

  friend class NS_CYCLE_COLLECTION_CLASSNAME(TextEditRules);
};

}  // namespace mozilla

#endif  // #ifndef HTMLEditRules_h
