/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HTMLEditor_h
#define mozilla_HTMLEditor_h

#include "mozilla/Attributes.h"
#include "mozilla/ComposerCommandsUpdater.h"
#include "mozilla/CSSEditUtils.h"
#include "mozilla/ManualNAC.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/TextEditor.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/File.h"

#include "nsAttrName.h"
#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMEventListener.h"
#include "nsIEditorMailSupport.h"
#include "nsIEditorStyleSheets.h"
#include "nsIEditorUtils.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsITableEditor.h"
#include "nsPoint.h"
#include "nsStubMutationObserver.h"
#include "nsTArray.h"

class nsDocumentFragment;
class nsHTMLDocument;
class nsITransferable;
class nsIClipboard;
class nsILinkHandler;
class nsTableWrapperFrame;
class nsRange;

namespace mozilla {
class AutoSelectionSetterAfterTableEdit;
class EmptyEditableFunctor;
class ResizerSelectionListener;
enum class EditSubAction : int32_t;
struct PropItem;
template<class T> class OwningNonNull;
namespace dom {
class DocumentFragment;
class Event;
class MouseEvent;
} // namespace dom
namespace widget {
struct IMEState;
} // namespace widget

enum class ParagraphSeparator { div, p, br };

/**
 * The HTML editor implementation.<br>
 * Use to edit HTML document represented as a DOM tree.
 */
class HTMLEditor final : public TextEditor
                       , public nsIHTMLEditor
                       , public nsIHTMLObjectResizer
                       , public nsIHTMLAbsPosEditor
                       , public nsITableEditor
                       , public nsIHTMLInlineTableEditor
                       , public nsIEditorStyleSheets
                       , public nsStubMutationObserver
{
public:
  /****************************************************************************
   * NOTE: DO NOT MAKE YOUR NEW METHODS PUBLIC IF they are called by other
   *       classes under libeditor except EditorEventListener and
   *       HTMLEditorEventListener because each public method which may fire
   *       eEditorInput event will need to instantiate new stack class for
   *       managing input type value of eEditorInput and cache some objects
   *       for smarter handling.  In other words, when you add new root
   *       method to edit the DOM tree, you can make your new method public.
   ****************************************************************************/

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLEditor, TextEditor)

  // nsStubMutationObserver overrides
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  // nsIHTMLEditor methods
  NS_DECL_NSIHTMLEDITOR

  // nsIHTMLObjectResizer methods (implemented in HTMLObjectResizer.cpp)
  NS_DECL_NSIHTMLOBJECTRESIZER

  // nsIHTMLAbsPosEditor methods (implemented in HTMLAbsPositionEditor.cpp)
  NS_DECL_NSIHTMLABSPOSEDITOR

  // nsIHTMLInlineTableEditor methods (implemented in HTMLInlineTableEditor.cpp)
  NS_DECL_NSIHTMLINLINETABLEEDITOR

  // nsIEditorStyleSheets methods
  NS_DECL_NSIEDITORSTYLESHEETS

  // nsIEditorMailSupport methods
  NS_DECL_NSIEDITORMAILSUPPORT

  // nsITableEditor methods
  NS_DECL_NSITABLEEDITOR

  // nsISelectionListener overrides
  NS_DECL_NSISELECTIONLISTENER

  HTMLEditor();

  nsHTMLDocument* GetHTMLDocument() const;

  virtual void PreDestroy(bool aDestroyingFrames) override;

  bool GetReturnInParagraphCreatesNewParagraph();

  /**
   * Returns the deepest container of the selection
   */
  Element* GetSelectionContainer();

  // TextEditor overrides
  virtual nsresult Init(nsIDocument& aDoc, Element* aRoot,
                        nsISelectionController* aSelCon, uint32_t aFlags,
                        const nsAString& aValue) override;
  NS_IMETHOD BeginningOfDocument() override;
  NS_IMETHOD SetFlags(uint32_t aFlags) override;

  NS_IMETHOD Paste(int32_t aSelectionType) override;
  NS_IMETHOD CanPaste(int32_t aSelectionType, bool* aCanPaste) override;

  NS_IMETHOD PasteTransferable(nsITransferable* aTransferable) override;

  NS_IMETHOD DeleteNode(nsINode* aNode) override;

  NS_IMETHOD DebugUnitTests(int32_t* outNumTests,
                            int32_t* outNumTestsFailed) override;

  virtual nsresult HandleKeyPressEvent(
                     WidgetKeyboardEvent* aKeyboardEvent) override;
  virtual nsIContent* GetFocusedContent() override;
  virtual already_AddRefed<nsIContent> GetFocusedContentForIME() override;
  virtual bool IsActiveInDOMWindow() override;
  virtual dom::EventTarget* GetDOMEventTarget() override;
  virtual already_AddRefed<nsIContent> FindSelectionRoot(
                                         nsINode *aNode) override;
  virtual bool IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent) override;
  virtual nsresult GetPreferredIMEState(widget::IMEState* aState) override;

  /**
   * Can we paste |aTransferable| or, if |aTransferable| is null, will a call
   * to pasteTransferable later possibly succeed if given an instance of
   * nsITransferable then? True if the doc is modifiable, and, if
   * |aTransfeable| is non-null, we have pasteable data in |aTransfeable|.
   */
  virtual bool CanPasteTransferable(nsITransferable* aTransferable) override;

  /**
   * OnInputLineBreak() is called when user inputs a line break with
   * Shift + Enter or something.
   */
  nsresult OnInputLineBreak();

  /**
   * event callback when a mouse button is pressed
   * @param aX      [IN] horizontal position of the pointer
   * @param aY      [IN] vertical position of the pointer
   * @param aTarget [IN] the element triggering the event
   * @param aMouseEvent [IN] the event
   */
  nsresult OnMouseDown(int32_t aX, int32_t aY, Element* aTarget,
                       dom::Event* aMouseEvent);

  /**
   * event callback when a mouse button is released
   * @param aX      [IN] horizontal position of the pointer
   * @param aY      [IN] vertical position of the pointer
   * @param aTarget [IN] the element triggering the event
   */
  nsresult OnMouseUp(int32_t aX, int32_t aY, Element* aTarget);

  /**
   * event callback when the mouse pointer is moved
   * @param aMouseEvent [IN] the event
   */
  nsresult OnMouseMove(dom::MouseEvent* aMouseEvent);

  // non-virtual methods of interface methods
  bool AbsolutePositioningEnabled() const
  {
    return mIsAbsolutelyPositioningEnabled;
  }

  /**
   * returns the deepest absolutely positioned container of the selection
   * if it exists or null.
   */
  already_AddRefed<Element> GetAbsolutelyPositionedSelectionContainer();

  Element* GetPositionedElement() const
  {
    return mAbsolutelyPositionedObject;
  }

  /**
   * extracts the selection from the normal flow of the document and
   * positions it.
   * @param aEnabled [IN] true to absolutely position the selection,
   *                      false to put it back in the normal flow
   */
  nsresult SetSelectionToAbsoluteOrStatic(bool aEnabled);

  /**
   * returns the absolute z-index of a positioned element. Never returns 'auto'
   * @return         the z-index of the element
   * @param aElement [IN] the element.
   */
  int32_t GetZIndex(Element& aElement);

  /**
   * adds aChange to the z-index of the currently positioned element.
   * @param aChange [IN] relative change to apply to current z-index
   */
  nsresult AddZIndex(int32_t aChange);

  nsresult SetInlineProperty(nsAtom* aProperty,
                             nsAtom* aAttribute,
                             const nsAString& aValue);
  nsresult GetInlineProperty(nsAtom* aProperty,
                             nsAtom* aAttribute,
                             const nsAString& aValue,
                             bool* aFirst,
                             bool* aAny,
                             bool* aAll);
  nsresult GetInlinePropertyWithAttrValue(nsAtom* aProperty,
                                          nsAtom* aAttr,
                                          const nsAString& aValue,
                                          bool* aFirst,
                                          bool* aAny,
                                          bool* aAll,
                                          nsAString& outValue);
  nsresult RemoveInlineProperty(nsAtom* aProperty,
                                nsAtom* aAttribute);

  /**
   * SetComposerCommandsUpdater() sets or unsets mComposerCommandsUpdater.
   * This will crash in debug build if the editor already has an instance
   * but called with another instance.
   */
  void SetComposerCommandsUpdater(
         ComposerCommandsUpdater* aComposerCommandsUpdater)
  {
    MOZ_ASSERT(!aComposerCommandsUpdater || !mComposerCommandsUpdater ||
               aComposerCommandsUpdater == mComposerCommandsUpdater);
    mComposerCommandsUpdater = aComposerCommandsUpdater;
  }

  ParagraphSeparator GetDefaultParagraphSeparator() const
  {
    return mDefaultParagraphSeparator;
  }
  void SetDefaultParagraphSeparator(ParagraphSeparator aSep)
  {
    mDefaultParagraphSeparator = aSep;
  }

  /**
   * Modifies the table containing the selection according to the
   * activation of an inline table editing UI element
   * @param aUIAnonymousElement [IN] the inline table editing UI element
   */
  nsresult DoInlineTableEditingAction(const Element& aUIAnonymousElement);

  already_AddRefed<Element>
  GetElementOrParentByTagName(const nsAString& aTagName, nsINode* aNode);

  /**
    * Get an active editor's editing host in DOM window.  If this editor isn't
    * active in the DOM window, this returns NULL.
    */
  Element* GetActiveEditingHost();

protected: // May be called by friends.
  /****************************************************************************
   * Some classes like TextEditRules, HTMLEditRules, WSRunObject which are
   * part of handling edit actions are allowed to call the following protected
   * methods.  However, those methods won't prepare caches of some objects
   * which are necessary for them.  So, if you want some following methods
   * to do that for you, you need to create a wrapper method in public scope
   * and call it.
   ****************************************************************************/

  /**
   * DeleteSelectionWithTransaction() removes selected content or content
   * around caret with transactions.
   *
   * @param aDirection          How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.
   */
  virtual nsresult
  DeleteSelectionWithTransaction(EDirection aAction,
                                 EStripWrappers aStripWrappers) override;

  /**
   * DeleteNodeWithTransaction() removes aNode from the DOM tree if it's
   * modifiable.  Note that this is not an override of same method of
   * EditorBase.
   *
   * @param aNode       The node to be removed from the DOM tree.
   */
  nsresult DeleteNodeWithTransaction(nsINode& aNode);

  /**
   * DeleteTextWithTransaction() removes text in the range from aCharData if
   * it's modifiable.  Note that this not an override of same method of
   * EditorBase.
   *
   * @param aCharData           The data node which should be modified.
   * @param aOffset             Start offset of removing text in aCharData.
   * @param aLength             Length of removing text.
   */
  nsresult DeleteTextWithTransaction(dom::CharacterData& aTextNode,
                                     uint32_t aOffset, uint32_t aLength);

  /**
   * InsertTextWithTransaction() inserts aStringToInsert at aPointToInsert.
   */
  virtual nsresult
  InsertTextWithTransaction(nsIDocument& aDocument,
                            const nsAString& aStringToInsert,
                            const EditorRawDOMPoint& aPointToInsert,
                            EditorRawDOMPoint* aPointAfterInsertedString =
                              nullptr) override;

  /**
   * CopyLastEditableChildStyles() clones inline container elements into
   * aPreviousBlock to aNewBlock to keep using same style in it.
   *
   * @param aPreviousBlock      The previous block element.  All inline
   *                            elements which are last sibling of each level
   *                            are cloned to aNewBlock.
   * @param aNewBlock           New block container element.
   * @param aNewBrElement       If this method creates a new <br> element for
   *                            placeholder, this is set to the new <br>
   *                            element.
   */
  nsresult
  CopyLastEditableChildStylesWithTransaction(Element& aPreviousBlock,
                                             Element& aNewBlock,
                                             RefPtr<Element>* aNewBrElement);

  /**
   * RemoveBlockContainerWithTransaction() removes aElement from the DOM tree
   * but moves its all children to its parent node and if its parent needs <br>
   * element to have at least one line-height, this inserts <br> element
   * automatically.
   *
   * @param aElement            Block element to be removed.
   */
  nsresult RemoveBlockContainerWithTransaction(Element& aElement);

  virtual Element* GetEditorRoot() override;
  using EditorBase::IsEditable;
  virtual nsresult RemoveAttributeOrEquivalent(
                     Element* aElement,
                     nsAtom* aAttribute,
                     bool aSuppressTransaction) override;
  virtual nsresult SetAttributeOrEquivalent(Element* aElement,
                                            nsAtom* aAttribute,
                                            const nsAString& aValue,
                                            bool aSuppressTransaction) override;
  using EditorBase::RemoveAttributeOrEquivalent;
  using EditorBase::SetAttributeOrEquivalent;

  /**
   * GetBlockNodeParent() returns parent or nearest ancestor of aNode if
   * there is a block parent.  If aAncestorLimiter is not nullptr,
   * this stops looking for the result.
   */
  static Element* GetBlockNodeParent(nsINode* aNode,
                                     nsINode* aAncestorLimiter = nullptr);

  /**
   * GetBlock() returns aNode itself, or parent or nearest ancestor of aNode
   * if there is a block parent.  If aAncestorLimiter is not nullptr,
   * this stops looking for the result.
   */
  static Element* GetBlock(nsINode& aNode,
                           nsINode* aAncestorLimiter = nullptr);

  void IsNextCharInNodeWhitespace(nsIContent* aContent,
                                  int32_t aOffset,
                                  bool* outIsSpace,
                                  bool* outIsNBSP,
                                  nsIContent** outNode = nullptr,
                                  int32_t* outOffset = 0);
  void IsPrevCharInNodeWhitespace(nsIContent* aContent,
                                  int32_t aOffset,
                                  bool* outIsSpace,
                                  bool* outIsNBSP,
                                  nsIContent** outNode = nullptr,
                                  int32_t* outOffset = 0);

  /**
   * @param aElement        Must not be null.
   */
  static bool NodeIsBlockStatic(const nsINode* aElement);

  /**
   * extracts an element from the normal flow of the document and
   * positions it, and puts it back in the normal flow.
   * @param aElement [IN] the element
   * @param aEnabled [IN] true to absolutely position the element,
   *                      false to put it back in the normal flow
   */
  nsresult SetPositionToAbsoluteOrStatic(Element& aElement,
                                         bool aEnabled);

  /**
   * adds aChange to the z-index of an arbitrary element.
   * @param aElement [IN] the element
   * @param aChange  [IN] relative change to apply to current z-index of
   *                      the element
   * @param aReturn  [OUT] the new z-index of the element
   */
  nsresult RelativeChangeElementZIndex(Element& aElement, int32_t aChange,
                                       int32_t* aReturn);

  virtual bool IsModifiableNode(nsINode* aNode) override;

  virtual bool IsBlockNode(nsINode *aNode) override;
  using EditorBase::IsBlockNode;

  /**
   * returns true if aParentTag can contain a child of type aChildTag.
   */
  virtual bool TagCanContainTag(nsAtom& aParentTag,
                                nsAtom& aChildTag) const override;

  /**
   * Returns true if aNode is a container.
   */
  virtual bool IsContainer(nsINode* aNode) override;

  /**
   * Join together any adjacent editable text nodes in the range.
   */
  nsresult CollapseAdjacentTextNodes(nsRange* aRange);

  virtual bool AreNodesSameType(nsIContent* aNode1,
                                nsIContent* aNode2) override;

  /**
   * IsInVisibleTextFrames() returns true if all text in aText is in visible
   * text frames.  Callers have to guarantee that there is no pending reflow.
   */
  bool IsInVisibleTextFrames(dom::Text& aText);

  /**
   * IsVisibleTextNode() returns true if aText has visible text.  If it has
   * only whitespaces and they are collapsed, returns false.
   */
  bool IsVisibleTextNode(Text& aText);

  /**
   * aNode must be a non-null text node.
   * outIsEmptyNode must be non-null.
   */
  nsresult IsEmptyNode(nsINode* aNode, bool* outIsEmptyBlock,
                       bool aMozBRDoesntCount = false,
                       bool aListOrCellNotEmpty = false,
                       bool aSafeToAskFrames = false);
  nsresult IsEmptyNodeImpl(nsINode* aNode,
                           bool* outIsEmptyBlock,
                           bool aMozBRDoesntCount,
                           bool aListOrCellNotEmpty,
                           bool aSafeToAskFrames,
                           bool* aSeenBR);

  bool IsCSSEnabled() const
  {
    // TODO: removal of mCSSAware and use only the presence of mCSSEditUtils
    return mCSSAware && mCSSEditUtils && mCSSEditUtils->IsCSSPrefChecked();
  }

  static bool HasAttributes(Element* aElement)
  {
    MOZ_ASSERT(aElement);
    uint32_t attrCount = aElement->GetAttrCount();
    return attrCount > 1 ||
           (1 == attrCount &&
            !aElement->GetAttrNameAt(0)->Equals(nsGkAtoms::mozdirty));
  }

  /**
   * Content-based query returns true if <aProperty aAttribute=aValue> effects
   * aNode.  If <aProperty aAttribute=aValue> contains aNode, but
   * <aProperty aAttribute=SomeOtherValue> also contains aNode and the second is
   * more deeply nested than the first, then the first does not effect aNode.
   *
   * @param aNode      The target of the query
   * @param aProperty  The property that we are querying for
   * @param aAttribute The attribute of aProperty, example: color in
   *                   <FONT color="blue"> May be null.
   * @param aValue     The value of aAttribute, example: blue in
   *                   <FONT color="blue"> May be null.  Ignored if aAttribute
   *                   is null.
   * @param outValue   [OUT] the value of the attribute, if aIsSet is true
   * @return           true if <aProperty aAttribute=aValue> effects
   *                   aNode.
   *
   * The nsIContent variant returns aIsSet instead of using an out parameter.
   */
  bool IsTextPropertySetByContent(nsINode* aNode,
                                  nsAtom* aProperty,
                                  nsAtom* aAttribute,
                                  const nsAString* aValue,
                                  nsAString* outValue = nullptr);

  bool IsInLink(nsINode* aNode, nsCOMPtr<nsINode>* outLink = nullptr);

  /**
   * Small utility routine to test if a break node is visible to user.
   */
  bool IsVisibleBRElement(nsINode* aNode);

  /**
   * Helper routines for font size changing.
   */
  enum class FontSize { incr, decr };
  nsresult RelativeFontChangeOnTextNode(FontSize aDir,
                                        Text& aTextNode,
                                        int32_t aStartOffset,
                                        int32_t aEndOffset);

  nsresult SetInlinePropertyOnNode(nsIContent& aNode,
                                   nsAtom& aProperty,
                                   nsAtom* aAttribute,
                                   const nsAString& aValue);

  nsresult SplitStyleAbovePoint(nsCOMPtr<nsINode>* aNode, int32_t* aOffset,
                                nsAtom* aProperty,
                                nsAtom* aAttribute,
                                nsIContent** aOutLeftNode = nullptr,
                                nsIContent** aOutRightNode = nullptr);

  nsIContent* GetPriorHTMLSibling(nsINode* aNode);

  nsIContent* GetNextHTMLSibling(nsINode* aNode);

  /**
   * GetPreviousHTMLElementOrText*() methods are similar to
   * EditorBase::GetPreviousElementOrText*() but this won't return nodes
   * outside active editing host.
   */
  nsIContent* GetPreviousHTMLElementOrText(nsINode& aNode)
  {
    return GetPreviousHTMLElementOrTextInternal(aNode, false);
  }
  nsIContent* GetPreviousHTMLElementOrTextInBlock(nsINode& aNode)
  {
    return GetPreviousHTMLElementOrTextInternal(aNode, true);
  }
  template<typename PT, typename CT>
  nsIContent*
  GetPreviousHTMLElementOrText(const EditorDOMPointBase<PT, CT>& aPoint)
  {
    return GetPreviousHTMLElementOrTextInternal(aPoint, false);
  }
  template<typename PT, typename CT>
  nsIContent*
  GetPreviousHTMLElementOrTextInBlock(const EditorDOMPointBase<PT, CT>& aPoint)
  {
    return GetPreviousHTMLElementOrTextInternal(aPoint, true);
  }

  /**
   * GetPreviousHTMLElementOrTextInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetPreviousHTMLElementOrTextInternal(nsINode& aNode,
                                                   bool aNoBlockCrossing);
  template<typename PT, typename CT>
  nsIContent*
  GetPreviousHTMLElementOrTextInternal(const EditorDOMPointBase<PT, CT>& aPoint,
                                       bool aNoBlockCrossing);

  /**
   * GetPreviousEditableHTMLNode*() methods are similar to
   * EditorBase::GetPreviousEditableNode() but this won't return nodes outside
   * active editing host.
   */
  nsIContent* GetPreviousEditableHTMLNode(nsINode& aNode)
  {
    return GetPreviousEditableHTMLNodeInternal(aNode, false);
  }
  nsIContent* GetPreviousEditableHTMLNodeInBlock(nsINode& aNode)
  {
    return GetPreviousEditableHTMLNodeInternal(aNode, true);
  }
  template<typename PT, typename CT>
  nsIContent*
  GetPreviousEditableHTMLNode(const EditorDOMPointBase<PT, CT>& aPoint)
  {
    return GetPreviousEditableHTMLNodeInternal(aPoint, false);
  }
  template<typename PT, typename CT>
  nsIContent* GetPreviousEditableHTMLNodeInBlock(
                const EditorDOMPointBase<PT, CT>& aPoint)
  {
    return GetPreviousEditableHTMLNodeInternal(aPoint, true);
  }

  /**
   * GetPreviousEditableHTMLNodeInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetPreviousEditableHTMLNodeInternal(nsINode& aNode,
                                                  bool aNoBlockCrossing);
  template<typename PT, typename CT>
  nsIContent* GetPreviousEditableHTMLNodeInternal(
                const EditorDOMPointBase<PT, CT>& aPoint,
                bool aNoBlockCrossing);

  /**
   * GetNextHTMLElementOrText*() methods are similar to
   * EditorBase::GetNextElementOrText*() but this won't return nodes outside
   * active editing host.
   *
   * Note that same as EditorBase::GetTextEditableNode(), methods which take
   * |const EditorRawDOMPoint&| start to search from the node pointed by it.
   * On the other hand, methods which take |nsINode&| start to search from
   * next node of aNode.
   */
  nsIContent* GetNextHTMLElementOrText(nsINode& aNode)
  {
    return GetNextHTMLElementOrTextInternal(aNode, false);
  }
  nsIContent* GetNextHTMLElementOrTextInBlock(nsINode& aNode)
  {
    return GetNextHTMLElementOrTextInternal(aNode, true);
  }
  template<typename PT, typename CT>
  nsIContent*
  GetNextHTMLElementOrText(const EditorDOMPointBase<PT, CT>& aPoint)
  {
    return GetNextHTMLElementOrTextInternal(aPoint, false);
  }
  template<typename PT, typename CT>
  nsIContent*
  GetNextHTMLElementOrTextInBlock(const EditorDOMPointBase<PT, CT>& aPoint)
  {
    return GetNextHTMLElementOrTextInternal(aPoint, true);
  }

  /**
   * GetNextHTMLNodeInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetNextHTMLElementOrTextInternal(nsINode& aNode,
                                               bool aNoBlockCrossing);
  template<typename PT, typename CT>
  nsIContent*
  GetNextHTMLElementOrTextInternal(const EditorDOMPointBase<PT, CT>& aPoint,
                                   bool aNoBlockCrossing);

  /**
   * GetNextEditableHTMLNode*() methods are similar to
   * EditorBase::GetNextEditableNode() but this won't return nodes outside
   * active editing host.
   *
   * Note that same as EditorBase::GetTextEditableNode(), methods which take
   * |const EditorRawDOMPoint&| start to search from the node pointed by it.
   * On the other hand, methods which take |nsINode&| start to search from
   * next node of aNode.
   */
  nsIContent* GetNextEditableHTMLNode(nsINode& aNode)
  {
    return GetNextEditableHTMLNodeInternal(aNode, false);
  }
  nsIContent* GetNextEditableHTMLNodeInBlock(nsINode& aNode)
  {
    return GetNextEditableHTMLNodeInternal(aNode, true);
  }
  template<typename PT, typename CT>
  nsIContent* GetNextEditableHTMLNode(const EditorDOMPointBase<PT, CT>& aPoint)
  {
    return GetNextEditableHTMLNodeInternal(aPoint, false);
  }
  template<typename PT, typename CT>
  nsIContent* GetNextEditableHTMLNodeInBlock(
                const EditorDOMPointBase<PT, CT>& aPoint)
  {
    return GetNextEditableHTMLNodeInternal(aPoint, true);
  }

  /**
   * GetNextEditableHTMLNodeInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetNextEditableHTMLNodeInternal(nsINode& aNode,
                                              bool aNoBlockCrossing);
  template<typename PT, typename CT>
  nsIContent* GetNextEditableHTMLNodeInternal(
                const EditorDOMPointBase<PT, CT>& aPoint,
                bool aNoBlockCrossing);

  bool IsFirstEditableChild(nsINode* aNode);
  bool IsLastEditableChild(nsINode* aNode);
  nsIContent* GetFirstEditableChild(nsINode& aNode);
  nsIContent* GetLastEditableChild(nsINode& aNode);

  nsIContent* GetFirstEditableLeaf(nsINode& aNode);
  nsIContent* GetLastEditableLeaf(nsINode& aNode);

  nsresult GetInlinePropertyBase(nsAtom& aProperty,
                                 nsAtom* aAttribute,
                                 const nsAString* aValue,
                                 bool* aFirst,
                                 bool* aAny,
                                 bool* aAll,
                                 nsAString* outValue);

  nsresult ClearStyle(nsCOMPtr<nsINode>* aNode, int32_t* aOffset,
                      nsAtom* aProperty, nsAtom* aAttribute);

  nsresult SetPositionToAbsolute(Element& aElement);
  nsresult SetPositionToStatic(Element& aElement);

protected: // Called by helper classes.
  virtual void
  OnStartToHandleTopLevelEditSubAction(
    EditSubAction aEditSubAction, nsIEditor::EDirection aDirection) override;
  virtual void OnEndHandlingTopLevelEditSubAction() override;

  virtual nsresult EndUpdateViewBatch() override;

protected: // Shouldn't be used by friend classes
  virtual ~HTMLEditor();

  virtual nsresult SelectAllInternal() override;

  /**
   * InsertNodeIntoProperAncestorWithTransaction() attempts to insert aNode
   * into the document, at aPointToInsert.  Checks with strict dtd to see if
   * containment is allowed.  If not allowed, will attempt to find a parent
   * in the parent hierarchy of aPointToInsert.GetContainer() that will accept
   * aNode as a child.  If such a parent is found, will split the document
   * tree from aPointToInsert up to parent, and then insert aNode.
   * aPointToInsert is then adjusted to point to the actual location that
   * aNode was inserted at.  aSplitAtEdges specifies if the splitting process
   * is allowed to result in empty nodes.
   *
   * @param aNode             Node to insert.
   * @param aPointToInsert    Insertion point.
   * @param aSplitAtEdges     Splitting can result in empty nodes?
   * @return                  Returns inserted point if succeeded.
   *                          Otherwise, the result is not set.
   */
  template<typename PT, typename CT>
  EditorDOMPoint
  InsertNodeIntoProperAncestorWithTransaction(
    nsIContent& aNode,
    const EditorDOMPointBase<PT, CT>& aPointToInsert,
    SplitAtEdges aSplitAtEdges);

  /**
   * InsertBrElementAtSelectionWithTransaction() inserts a new <br> element at
   * selection.  If there is non-collapsed selection ranges, the selected
   * ranges is deleted first.
   */
  nsresult InsertBrElementAtSelectionWithTransaction();

  nsresult LoadHTML(const nsAString& aInputString);

  nsresult GetCSSBackgroundColorState(bool* aMixed, nsAString& aOutColor,
                                      bool aBlockLevel);
  nsresult GetHTMLBackgroundColorState(bool* aMixed, nsAString& outColor);

  nsresult GetLastCellInRow(nsINode* aRowNode,
                            nsINode** aCellNode);

  nsresult GetCellFromRange(nsRange* aRange, Element** aCell);

  /**
   * This sets background on the appropriate container element (table, cell,)
   * or calls into nsTextEditor to set the page background.
   */
  nsresult SetCSSBackgroundColorWithTransaction(const nsAString& aColor);
  nsresult SetHTMLBackgroundColorWithTransaction(const nsAString& aColor);

  virtual void
  InitializeSelectionAncestorLimit(Selection& aSelection,
                                   nsIContent& aAncestorLimit) override;

  /**
   * Make the given selection span the entire document.
   */
  virtual nsresult SelectEntireDocument(Selection* aSelection) override;

  /**
   * Use this to assure that selection is set after attribute nodes when
   * trying to collapse selection at begining of a block node
   * e.g., when setting at beginning of a table cell
   * This will stop at a table, however, since we don't want to
   * "drill down" into nested tables.
   * @param aSelection      Optional. If null, we get current selection.
   */
  void CollapseSelectionToDeepestNonTableFirstChild(Selection* aSelection,
                                                    nsINode* aNode);

  /**
   * Returns TRUE if sheet was loaded, false if it wasn't.
   */
  bool EnableExistingStyleSheet(const nsAString& aURL);

  /**
   * Dealing with the internal style sheet lists.
   */
  StyleSheet* GetStyleSheetForURL(const nsAString& aURL);
  void GetURLForStyleSheet(StyleSheet* aStyleSheet,
                           nsAString& aURL);

  /**
   * Add a url + known style sheet to the internal lists.
   */
  nsresult AddNewStyleSheetToList(const nsAString &aURL,
                                  StyleSheet* aStyleSheet);
  nsresult RemoveStyleSheetFromList(const nsAString &aURL);

  /**
   * MaybeCollapseSelectionAtFirstEditableNode() may collapse selection at
   * proper position to staring to edit.  If there is a non-editable node
   * before any editable text nodes or inline elements which can have text
   * nodes as their children, collapse selection at start of the editing
   * host.  If there is an editable text node which is not collapsed, collapses
   * selection at the start of the text node.  If there is an editable inline
   * element which cannot have text nodes as its child, collapses selection at
   * before the element node.  Otherwise, collapses selection at start of the
   * editing host.
   *
   * @param aIgnoreIfSelectionInEditingHost
   *                        This method does nothing if selection is in the
   *                        editing host except if it's collapsed at start of
   *                        the editing host.
   *                        Note that if selection ranges were outside of
   *                        current selection limiter, selection was collapsed
   *                        at the start of the editing host therefore, if
   *                        you call this with setting this to true, you can
   *                        keep selection ranges if user has already been
   *                        changed.
   */
  nsresult
  MaybeCollapseSelectionAtFirstEditableNode(
    bool aIgnoreIfSelectionInEditingHost);

  class BlobReader final : public nsIEditorBlobListener
  {
  public:
    BlobReader(dom::BlobImpl* aBlob, HTMLEditor* aHTMLEditor,
               bool aIsSafe, nsIDocument* aSourceDoc,
               nsINode* aDestinationNode, int32_t aDestOffset,
               bool aDoDeleteSelection);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIEDITORBLOBLISTENER

  private:
    ~BlobReader()
    {
    }

    RefPtr<dom::BlobImpl> mBlob;
    RefPtr<HTMLEditor> mHTMLEditor;
    bool mIsSafe;
    nsCOMPtr<nsIDocument> mSourceDoc;
    nsCOMPtr<nsINode> mDestinationNode;
    int32_t mDestOffset;
    bool mDoDeleteSelection;
  };

  virtual nsresult InitRules() override;

  virtual void CreateEventListeners() override;
  virtual nsresult InstallEventListeners() override;
  virtual void RemoveEventListeners() override;

  bool ShouldReplaceRootElement();
  void NotifyRootChanged();
  Element* GetBodyElement();

  /**
   * Get the focused node of this editor.
   * @return    If the editor has focus, this returns the focused node.
   *            Otherwise, returns null.
   */
  already_AddRefed<nsINode> GetFocusedNode();

  virtual already_AddRefed<nsIContent> GetInputEventTargetContent() override;

  /**
   * Return TRUE if aElement is a table-related elemet and caret was set.
   */
  bool SetCaretInTableCell(dom::Element* aElement);

  nsresult TabInTable(bool inIsShift, bool* outHandled);

  /**
   * Insert a new cell after or before supplied aCell.
   * Optional: If aNewCell supplied, returns the newly-created cell (addref'd,
   * of course)
   * This doesn't change or use the current selection.
   */
  nsresult InsertCell(Element* aCell, int32_t aRowSpan,
                      int32_t aColSpan, bool aAfter, bool aIsHeader,
                      Element** aNewCell);

  /**
   * Helpers that don't touch the selection or do batch transactions.
   */
  nsresult DeleteRow(Element* aTable, int32_t aRowIndex);
  nsresult DeleteColumn(Element* aTable, int32_t aColIndex);
  nsresult DeleteCellContents(Element* aCell);

  /**
   * Move all contents from aCellToMerge into aTargetCell (append at end).
   */
  nsresult MergeCells(RefPtr<Element> aTargetCell,
                      RefPtr<Element> aCellToMerge,
                      bool aDeleteCellToMerge);

  nsresult DeleteTable2(Element* aTable, Selection* aSelection);
  nsresult SetColSpan(Element* aCell, int32_t aColSpan);
  nsresult SetRowSpan(Element* aCell, int32_t aRowSpan);

  /**
   * Helper used to get nsTableWrapperFrame for a table.
   */
  nsTableWrapperFrame* GetTableFrame(Element* aTable);

  /**
   * Needed to do appropriate deleting when last cell or row is about to be
   * deleted.  This doesn't count cells that don't start in the given row (are
   * spanning from row above).
   */
  int32_t GetNumberOfCellsInRow(Element* aTable, int32_t rowIndex);

  /**
   * Test if all cells in row or column at given index are selected.
   */
  bool AllCellsInRowSelected(Element* aTable, int32_t aRowIndex,
                             int32_t aNumberOfColumns);
  bool AllCellsInColumnSelected(Element* aTable, int32_t aColIndex,
                                int32_t aNumberOfRows);

  bool IsEmptyCell(Element* aCell);

  /**
   * Most insert methods need to get the same basic context data.
   * Any of the pointers may be null if you don't need that datum (for more
   * efficiency).
   * Input: *aCell is a known cell,
   *        if null, cell is obtained from the anchor node of the selection.
   * Returns NS_EDITOR_ELEMENT_NOT_FOUND if cell is not found even if aCell is
   * null.
   */
  nsresult GetCellContext(Selection** aSelection, Element** aTable,
                          Element** aCell, nsINode** aCellParent,
                          int32_t* aCellOffset, int32_t* aRowIndex,
                          int32_t* aColIndex);

  nsresult GetCellSpansAt(Element* aTable, int32_t aRowIndex,
                          int32_t aColIndex, int32_t& aActualRowSpan,
                          int32_t& aActualColSpan);

  nsresult SplitCellIntoColumns(Element* aTable, int32_t aRowIndex,
                                int32_t aColIndex, int32_t aColSpanLeft,
                                int32_t aColSpanRight,
                                Element** aNewCell);

  nsresult SplitCellIntoRows(Element* aTable, int32_t aRowIndex,
                             int32_t aColIndex, int32_t aRowSpanAbove,
                             int32_t aRowSpanBelow, Element** aNewCell);

  nsresult CopyCellBackgroundColor(Element* aDestCell,
                                   Element* aSourceCell);

  /**
   * Reduce rowspan/colspan when cells span into nonexistent rows/columns.
   */
  nsresult FixBadRowSpan(Element* aTable, int32_t aRowIndex,
                         int32_t& aNewRowCount);
  nsresult FixBadColSpan(Element* aTable, int32_t aColIndex,
                         int32_t& aNewColCount);

  /**
   * Fallback method: Call this after using ClearSelection() and you
   * failed to set selection to some other content in the document.
   */
  nsresult SetSelectionAtDocumentStart(Selection* aSelection);

  static Element* GetEnclosingTable(nsINode* aNode);

  // Methods for handling plaintext quotations
  nsresult PasteAsPlaintextQuotation(int32_t aSelectionType);

  /**
   * Insert a string as quoted text, replacing the selected text (if any).
   * @param aQuotedText     The string to insert.
   * @param aAddCites       Whether to prepend extra ">" to each line
   *                        (usually true, unless those characters
   *                        have already been added.)
   * @return aNodeInserted  The node spanning the insertion, if applicable.
   *                        If aAddCites is false, this will be null.
   */
  nsresult InsertAsPlaintextQuotation(const nsAString& aQuotedText,
                                      bool aAddCites,
                                      nsINode** aNodeInserted);

  nsresult InsertObject(const nsACString& aType, nsISupports* aObject,
                        bool aIsSafe,
                        nsIDocument* aSourceDoc,
                        nsINode* aDestinationNode,
                        int32_t aDestOffset,
                        bool aDoDeleteSelection);

  // factored methods for handling insertion of data from transferables
  // (drag&drop or clipboard)
  virtual nsresult PrepareTransferable(nsITransferable** transferable) override;
  nsresult PrepareHTMLTransferable(nsITransferable** transferable);
  nsresult InsertFromTransferable(nsITransferable* transferable,
                                    nsIDocument* aSourceDoc,
                                    const nsAString& aContextStr,
                                    const nsAString& aInfoStr,
                                    bool havePrivateHTMLFlavor,
                                    bool aDoDeleteSelection);
  virtual nsresult InsertFromDataTransfer(dom::DataTransfer* aDataTransfer,
                                          int32_t aIndex,
                                          nsIDocument* aSourceDoc,
                                          nsINode* aDestinationNode,
                                          int32_t aDestOffset,
                                          bool aDoDeleteSelection) override;
  bool HavePrivateHTMLFlavor(nsIClipboard* clipboard );
  nsresult ParseCFHTML(nsCString& aCfhtml, char16_t** aStuffToPaste,
                       char16_t** aCfcontext);

  nsresult StripFormattingNodes(nsIContent& aNode, bool aOnlyList = false);
  nsresult CreateDOMFragmentFromPaste(const nsAString& aInputString,
                                      const nsAString& aContextStr,
                                      const nsAString& aInfoStr,
                                      nsCOMPtr<nsINode>* outFragNode,
                                      nsCOMPtr<nsINode>* outStartNode,
                                      nsCOMPtr<nsINode>* outEndNode,
                                      int32_t* outStartOffset,
                                      int32_t* outEndOffset,
                                      bool aTrustedInput);
  nsresult ParseFragment(const nsAString& aStr, nsAtom* aContextLocalName,
                         nsIDocument* aTargetDoc,
                         dom::DocumentFragment** aFragment, bool aTrustedInput);
  void CreateListOfNodesToPaste(dom::DocumentFragment& aFragment,
                                nsTArray<OwningNonNull<nsINode>>& outNodeList,
                                nsINode* aStartContainer,
                                int32_t aStartOffset,
                                nsINode* aEndContainer,
                                int32_t aEndOffset);
  enum class StartOrEnd { start, end };
  void GetListAndTableParents(StartOrEnd aStartOrEnd,
                              nsTArray<OwningNonNull<nsINode>>& aNodeList,
                              nsTArray<OwningNonNull<Element>>& outArray);
  int32_t DiscoverPartialListsAndTables(
            nsTArray<OwningNonNull<nsINode>>& aPasteNodes,
            nsTArray<OwningNonNull<Element>>& aListsAndTables);
  nsINode* ScanForListAndTableStructure(
             StartOrEnd aStartOrEnd,
             nsTArray<OwningNonNull<nsINode>>& aNodes,
             Element& aListOrTable);
  void ReplaceOrphanedStructure(
         StartOrEnd aStartOrEnd,
         nsTArray<OwningNonNull<nsINode>>& aNodeArray,
         nsTArray<OwningNonNull<Element>>& aListAndTableArray,
         int32_t aHighWaterMark);

  /**
   * GetBetterInsertionPointFor() returns better insertion point to insert
   * aNodeToInsert.
   *
   * @param aNodeToInsert       The node to insert.
   * @param aPointToInsert      A candidate point to insert the node.
   * @return                    Better insertion point if next visible node
   *                            is a <br> element and previous visible node
   *                            is neither none, another <br> element nor
   *                            different block level element.
   */
  EditorRawDOMPoint
  GetBetterInsertionPointFor(nsINode& aNodeToInsert,
                             const EditorRawDOMPoint& aPointToInsert);

  /**
   * MakeDefinitionListItemWithTransaction() replaces parent list of current
   * selection with <dl> or create new <dl> element and creates a definition
   * list item whose name is aTagName.
   *
   * @param aTagName            Must be nsGkAtoms::dt or nsGkAtoms::dd.
   */
  nsresult MakeDefinitionListItemWithTransaction(nsAtom& aTagName);

  /**
   * InsertBasicBlockWithTransaction() inserts a block element whose name
   * is aTagName at selection.
   *
   * @param aTagName            A block level element name.  Must NOT be
   *                            nsGkAtoms::dt nor nsGkAtoms::dd.
   */
  nsresult InsertBasicBlockWithTransaction(nsAtom& aTagName);

  /**
   * Increase/decrease the font size of selection.
   */
  nsresult RelativeFontChange(FontSize aDir);

  nsresult RelativeFontChangeOnNode(int32_t aSizeChange, nsIContent* aNode);
  nsresult RelativeFontChangeHelper(int32_t aSizeChange, nsINode* aNode);

  /**
   * Helper routines for inline style.
   */
  nsresult SetInlinePropertyOnTextNode(Text& aData,
                                       int32_t aStartOffset,
                                       int32_t aEndOffset,
                                       nsAtom& aProperty,
                                       nsAtom* aAttribute,
                                       const nsAString& aValue);

  nsresult PromoteInlineRange(nsRange& aRange);
  nsresult PromoteRangeIfStartsOrEndsInNamedAnchor(nsRange& aRange);
  nsresult SplitStyleAboveRange(nsRange* aRange,
                                nsAtom* aProperty,
                                nsAtom* aAttribute);
  nsresult RemoveStyleInside(nsIContent& aNode,
                             nsAtom* aProperty,
                             nsAtom* aAttribute,
                             const bool aChildrenOnly = false);

  bool NodeIsProperty(nsINode& aNode);
  bool IsAtFrontOfNode(nsINode& aNode, int32_t aOffset);
  bool IsAtEndOfNode(nsINode& aNode, int32_t aOffset);
  bool IsOnlyAttribute(const Element* aElement, nsAtom* aAttribute);

  bool HasStyleOrIdOrClass(Element* aElement);
  nsresult RemoveElementIfNoStyleOrIdOrClass(Element& aElement);

  /**
   * Whether the outer window of the DOM event target has focus or not.
   */
  bool OurWindowHasFocus();

  /**
   * This function is used to insert a string of HTML input optionally with some
   * context information into the editable field.  The HTML input either comes
   * from a transferable object created as part of a drop/paste operation, or
   * from the InsertHTML method.  We may want the HTML input to be sanitized
   * (for example, if it's coming from a transferable object), in which case
   * aTrustedInput should be set to false, otherwise, the caller should set it
   * to true, which means that the HTML will be inserted in the DOM verbatim.
   *
   * aClearStyle should be set to false if you want the paste to be affected by
   * local style (e.g., for the insertHTML command).
   */
  nsresult DoInsertHTMLWithContext(const nsAString& aInputString,
                                   const nsAString& aContextStr,
                                   const nsAString& aInfoStr,
                                   const nsAString& aFlavor,
                                   nsIDocument* aSourceDoc,
                                   nsINode* aDestNode,
                                   int32_t aDestOffset,
                                   bool aDeleteSelection,
                                   bool aTrustedInput,
                                   bool aClearStyle = true);

  /**
   * sets the position of an element; warning it does NOT check if the
   * element is already positioned or not and that's on purpose.
   * @param aElement [IN] the element
   * @param aX       [IN] the x position in pixels.
   * @param aY       [IN] the y position in pixels.
   */
  void SetTopAndLeft(Element& aElement, int32_t aX, int32_t aY);

  /**
   * Reset a selected cell or collapsed selection (the caret) after table
   * editing.
   *
   * @param aTable      A table in the document.
   * @param aRow        The row ...
   * @param aCol        ... and column defining the cell where we will try to
   *                    place the caret.
   * @param aSelected   If true, we select the whole cell instead of setting
   *                    caret.
   * @param aDirection  If cell at (aCol, aRow) is not found, search for
   *                    previous cell in the same column (aPreviousColumn) or
   *                    row (ePreviousRow) or don't search for another cell
   *                    (aNoSearch).  If no cell is found, caret is place just
   *                    before table; and if that fails, at beginning of
   *                    document.  Thus we generally don't worry about the
   *                    return value and can use the
   *                    AutoSelectionSetterAfterTableEdit stack-based object to
   *                    insure we reset the caret in a table-editing method.
   */
  void SetSelectionAfterTableEdit(Element* aTable,
                                  int32_t aRow, int32_t aCol,
                                  int32_t aDirection, bool aSelected);

  /**
   * A more C++-friendly version of nsIHTMLEditor::GetSelectedElement
   * that just returns null on errors.
   */
  already_AddRefed<dom::Element> GetSelectedElement(const nsAString& aTagName);

  void RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                  nsIDOMEventListener* aListener,
                                  bool aUseCapture,
                                  ManualNACPtr aElement,
                                  nsIPresShell* aShell);
  void DeleteRefToAnonymousNode(ManualNACPtr aContent,
                                nsIPresShell* aShell);

  nsresult ShowResizersInner(Element& aResizedElement);

  /**
   * Returns the offset of an element's frame to its absolute containing block.
   */
  nsresult GetElementOrigin(Element& aElement,
                            int32_t& aX, int32_t& aY);
  nsresult GetPositionAndDimensions(Element& aElement,
                                    int32_t& aX, int32_t& aY,
                                    int32_t& aW, int32_t& aH,
                                    int32_t& aBorderLeft,
                                    int32_t& aBorderTop,
                                    int32_t& aMarginLeft,
                                    int32_t& aMarginTop);

  bool IsInObservedSubtree(nsIContent* aChild);

  void UpdateRootElement();

  nsresult SetAllResizersPosition();

  /**
   * Shows active resizers around an element's frame
   * @param aResizedElement [IN] a DOM Element
   */
  nsresult ShowResizers(Element& aResizedElement);

  ManualNACPtr CreateResizer(int16_t aLocation, nsIContent& aParentContent);
  void SetAnonymousElementPosition(int32_t aX, int32_t aY,
                                   Element* aResizer);

  ManualNACPtr CreateShadow(nsIContent& aParentContent,
                            Element& aOriginalObject);
  nsresult SetShadowPosition(Element* aShadow, Element* aOriginalObject,
                             int32_t aOriginalObjectX,
                             int32_t aOriginalObjectY);

  ManualNACPtr CreateResizingInfo(nsIContent& aParentContent);
  nsresult SetResizingInfoPosition(int32_t aX, int32_t aY,
                                   int32_t aW, int32_t aH);

  enum class ResizeAt
  {
    eX,
    eY,
    eWidth,
    eHeight,
  };
  int32_t GetNewResizingIncrement(int32_t aX, int32_t aY,
                                  ResizeAt aResizeAt);

  nsresult StartResizing(Element* aHandle);
  int32_t GetNewResizingX(int32_t aX, int32_t aY);
  int32_t GetNewResizingY(int32_t aX, int32_t aY);
  int32_t GetNewResizingWidth(int32_t aX, int32_t aY);
  int32_t GetNewResizingHeight(int32_t aX, int32_t aY);
  void HideShadowAndInfo();
  void SetFinalSize(int32_t aX, int32_t aY);
  void SetResizeIncrements(int32_t aX, int32_t aY, int32_t aW, int32_t aH,
                           bool aPreserveRatio);
  void HideAnonymousEditingUIs();

  /**
   * sets the z-index of an element.
   * @param aElement [IN] the element
   * @param aZorder  [IN] the z-index
   */
  void SetZIndex(Element& aElement, int32_t aZorder);

  /**
   * shows a grabber attached to an arbitrary element. The grabber is an image
   * positioned on the left hand side of the top border of the element. Draggin
   * and dropping it allows to change the element's absolute position in the
   * document. See chrome://editor/content/images/grabber.gif
   * @param aElement [IN] the element
   */
  nsresult ShowGrabber(Element& aElement);

  /**
   * hide the grabber if it shown.
   */
  void HideGrabber();

  ManualNACPtr CreateGrabber(nsIContent& aParentContent);
  nsresult StartMoving();
  nsresult SetFinalPosition(int32_t aX, int32_t aY);
  void AddPositioningOffset(int32_t& aX, int32_t& aY);
  void SnapToGrid(int32_t& newX, int32_t& newY);
  nsresult GrabberClicked();
  nsresult EndMoving();
  nsresult GetTemporaryStyleForFocusedPositionedElement(Element& aElement,
                                                        nsAString& aReturn);

  /**
   * Shows inline table editing UI around a table cell
   * @param aCell [IN] a DOM Element being a table cell, td or th
   */
  nsresult ShowInlineTableEditingUI(Element* aCell);

  /**
   * Hide all inline table editing UI
   */
  nsresult HideInlineTableEditingUI();

  /**
   * IsEmptyTextNode() returns true if aNode is a text node and does not have
   * any visible characters.
   */
  bool IsEmptyTextNode(nsINode& aNode);

  bool IsSimpleModifiableNode(nsIContent* aContent,
                              nsAtom* aProperty,
                              nsAtom* aAttribute,
                              const nsAString* aValue);
  nsresult SetInlinePropertyOnNodeImpl(nsIContent& aNode,
                                       nsAtom& aProperty,
                                       nsAtom* aAttribute,
                                       const nsAString& aValue);
  typedef enum { eInserted, eAppended } InsertedOrAppended;
  void DoContentInserted(nsIContent* aChild, InsertedOrAppended);
  // XXX Shouldn't this used by external classes instead of nsIHTMLEditor's
  //     method?
  already_AddRefed<Element> CreateElementWithDefaults(
                              const nsAString& aTagName);
  /**
   * Returns an anonymous Element of type aTag,
   * child of aParentContent. If aIsCreatedHidden is true, the class
   * "hidden" is added to the created element. If aAnonClass is not
   * the empty string, it becomes the value of the attribute "_moz_anonclass"
   * @return a Element
   * @param aTag             [IN] desired type of the element to create
   * @param aParentContent   [IN] the parent node of the created anonymous
   *                              element
   * @param aAnonClass       [IN] contents of the _moz_anonclass attribute
   * @param aIsCreatedHidden [IN] a boolean specifying if the class "hidden"
   *                              is to be added to the created anonymous
   *                              element
   */
  ManualNACPtr CreateAnonymousElement(nsAtom* aTag,
                                      nsIContent& aParentContent,
                                      const nsAString& aAnonClass,
                                      bool aIsCreatedHidden);
protected:
  RefPtr<TypeInState> mTypeInState;
  RefPtr<ComposerCommandsUpdater> mComposerCommandsUpdater;

  bool mCRInParagraphCreatesParagraph;

  bool mCSSAware;
  UniquePtr<CSSEditUtils> mCSSEditUtils;

  // Used by GetFirstSelectedCell and GetNextSelectedCell
  int32_t  mSelectedCellIndex;

  nsString mLastStyleSheetURL;
  nsString mLastOverrideStyleSheetURL;

  // Maintain a list of associated style sheets and their urls.
  nsTArray<nsString> mStyleSheetURLs;
  nsTArray<RefPtr<StyleSheet>> mStyleSheets;

  // resizing
  // If the instance has shown resizers at least once, mHasShownResizers is
  // set to true.
  bool mHasShownResizers;
  bool mIsObjectResizingEnabled;
  bool mIsResizing;
  bool mPreserveRatio;
  bool mResizedObjectIsAnImage;

  // absolute positioning
  bool mIsAbsolutelyPositioningEnabled;
  bool mResizedObjectIsAbsolutelyPositioned;
  // If the instance has shown grabber at least once, mHasShownGrabber is
  // set to true.
  bool mHasShownGrabber;
  bool mGrabberClicked;
  bool mIsMoving;

  bool mSnapToGridEnabled;

  // inline table editing
  // If the instance has shown inline table editor at least once,
  // mHasShownInlineTableEditor is set to true.
  bool mHasShownInlineTableEditor;
  bool mIsInlineTableEditingEnabled;

  // resizing
  ManualNACPtr mTopLeftHandle;
  ManualNACPtr mTopHandle;
  ManualNACPtr mTopRightHandle;
  ManualNACPtr mLeftHandle;
  ManualNACPtr mRightHandle;
  ManualNACPtr mBottomLeftHandle;
  ManualNACPtr mBottomHandle;
  ManualNACPtr mBottomRightHandle;

  nsCOMPtr<Element> mActivatedHandle;

  ManualNACPtr mResizingShadow;
  ManualNACPtr mResizingInfo;

  RefPtr<Element> mResizedObject;

  nsCOMPtr<nsIDOMEventListener>  mMouseMotionListenerP;
  nsCOMPtr<nsIDOMEventListener>  mResizeEventListenerP;

  int32_t mOriginalX;
  int32_t mOriginalY;

  int32_t mResizedObjectX;
  int32_t mResizedObjectY;
  int32_t mResizedObjectWidth;
  int32_t mResizedObjectHeight;

  int32_t mResizedObjectMarginLeft;
  int32_t mResizedObjectMarginTop;
  int32_t mResizedObjectBorderLeft;
  int32_t mResizedObjectBorderTop;

  int32_t mXIncrementFactor;
  int32_t mYIncrementFactor;
  int32_t mWidthIncrementFactor;
  int32_t mHeightIncrementFactor;

  // When resizers, grabber and/or inline table editor are operated by user
  // actually, the following counters are increased.
  uint32_t mResizerUsedCount;
  uint32_t mGrabberUsedCount;
  uint32_t mInlineTableEditorUsedCount;

  int8_t  mInfoXIncrement;
  int8_t  mInfoYIncrement;

  // absolute positioning
  int32_t mPositionedObjectX;
  int32_t mPositionedObjectY;
  int32_t mPositionedObjectWidth;
  int32_t mPositionedObjectHeight;

  int32_t mPositionedObjectMarginLeft;
  int32_t mPositionedObjectMarginTop;
  int32_t mPositionedObjectBorderLeft;
  int32_t mPositionedObjectBorderTop;

  nsCOMPtr<Element> mAbsolutelyPositionedObject;
  ManualNACPtr mGrabber;
  ManualNACPtr mPositioningShadow;

  int32_t mGridSize;

  // inline table editing
  RefPtr<Element> mInlineEditedCell;

  ManualNACPtr mAddColumnBeforeButton;
  ManualNACPtr mRemoveColumnButton;
  ManualNACPtr mAddColumnAfterButton;

  ManualNACPtr mAddRowBeforeButton;
  ManualNACPtr mRemoveRowButton;
  ManualNACPtr mAddRowAfterButton;

  void AddMouseClickListener(Element* aElement);
  void RemoveMouseClickListener(Element* aElement);

  nsCOMPtr<nsILinkHandler> mLinkHandler;

  ParagraphSeparator mDefaultParagraphSeparator;

  friend class AutoSelectionSetterAfterTableEdit;
  friend class CSSEditUtils;
  friend class EmptyEditableFunctor;
  friend class HTMLEditRules;
  friend class WSRunObject;
};

} // namespace mozilla

mozilla::HTMLEditor*
nsIEditor::AsHTMLEditor()
{
  return static_cast<mozilla::EditorBase*>(this)->mIsHTMLEditorClass ?
           static_cast<mozilla::HTMLEditor*>(this) : nullptr;
}

const mozilla::HTMLEditor*
nsIEditor::AsHTMLEditor() const
{
  return static_cast<const mozilla::EditorBase*>(this)->mIsHTMLEditorClass ?
           static_cast<const mozilla::HTMLEditor*>(this) : nullptr;
}

#endif // #ifndef mozilla_HTMLEditor_h
