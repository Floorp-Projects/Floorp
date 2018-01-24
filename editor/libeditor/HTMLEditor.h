/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HTMLEditor_h
#define mozilla_HTMLEditor_h

#include "mozilla/Attributes.h"
#include "mozilla/CSSEditUtils.h"
#include "mozilla/ManualNAC.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/File.h"

#include "nsAttrName.h"
#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIEditorMailSupport.h"
#include "nsIEditorStyleSheets.h"
#include "nsIEditorUtils.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsISelectionListener.h"
#include "nsITableEditor.h"
#include "nsPoint.h"
#include "nsStubMutationObserver.h"
#include "nsTArray.h"

class nsDocumentFragment;
class nsITransferable;
class nsIClipboard;
class nsIDOMMouseEvent;
class nsILinkHandler;
class nsTableWrapperFrame;
class nsIDOMRange;
class nsRange;

namespace mozilla {
class AutoSelectionSetterAfterTableEdit;
class HTMLEditorEventListener;
class HTMLEditRules;
class TypeInState;
class WSRunObject;
enum class EditAction : int32_t;
struct PropItem;
template<class T> class OwningNonNull;
namespace dom {
class DocumentFragment;
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
                       , public nsICSSLoaderObserver
                       , public nsStubMutationObserver
{
private:
  enum BlockTransformationType
  {
    eNoOp,
    eReplaceParent = 1,
    eInsertParent = 2
  };

  const char16_t kNBSP = 160;

public:
  enum ResizingRequestID
  {
    kX      = 0,
    kY      = 1,
    kWidth  = 2,
    kHeight = 3
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLEditor, TextEditor)

  HTMLEditor();

  bool GetReturnInParagraphCreatesNewParagraph();
  Element* GetSelectionContainer();

  // nsIEditor overrides
  NS_IMETHOD GetPreferredIMEState(widget::IMEState* aState) override;

  // TextEditor overrides
  NS_IMETHOD BeginningOfDocument() override;
  virtual nsresult HandleKeyPressEvent(
                     WidgetKeyboardEvent* aKeyboardEvent) override;
  virtual nsIContent* GetFocusedContent() override;
  virtual already_AddRefed<nsIContent> GetFocusedContentForIME() override;
  virtual bool IsActiveInDOMWindow() override;
  virtual dom::EventTarget* GetDOMEventTarget() override;
  virtual Element* GetEditorRoot() override;
  virtual already_AddRefed<nsIContent> FindSelectionRoot(
                                         nsINode *aNode) override;
  virtual bool IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent) override;
  virtual already_AddRefed<nsIContent> GetInputEventTargetContent() override;
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

  // XXX Following methods are not overriding but defined here...
  nsresult CopyLastEditableChildStyles(nsINode* aPreviousBlock,
                                       nsINode* aNewBlock,
                                       Element** aOutBrNode);

  nsresult LoadHTML(const nsAString& aInputString);

  nsresult GetCSSBackgroundColorState(bool* aMixed, nsAString& aOutColor,
                                      bool aBlockLevel);
  NS_IMETHOD GetHTMLBackgroundColorState(bool* aMixed, nsAString& outColor);

  // nsIEditorStyleSheets methods
  NS_DECL_NSIEDITORSTYLESHEETS

  // nsIEditorMailSupport methods
  NS_DECL_NSIEDITORMAILSUPPORT

  // nsITableEditor methods
  NS_DECL_NSITABLEEDITOR

  nsresult GetLastCellInRow(nsIDOMNode* aRowNode,
                            nsIDOMNode** aCellNode);

  nsresult GetCellFromRange(nsRange* aRange, nsIDOMElement** aCell);

  // Miscellaneous

  /**
   * This sets background on the appropriate container element (table, cell,)
   * or calls into nsTextEditor to set the page background.
   */
  nsresult SetCSSBackgroundColor(const nsAString& aColor);
  nsresult SetHTMLBackgroundColor(const nsAString& aColor);

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

  // Overrides of EditorBase interface methods
  virtual nsresult EndUpdateViewBatch() override;

  NS_IMETHOD Init(nsIDOMDocument* aDoc, nsIContent* aRoot,
                  nsISelectionController* aSelCon, uint32_t aFlags,
                  const nsAString& aValue) override;
  NS_IMETHOD PreDestroy(bool aDestroyingFrames) override;

  /**
   * @param aElement        Must not be null.
   */
  static bool NodeIsBlockStatic(const nsINode* aElement);
  static nsresult NodeIsBlockStatic(nsIDOMNode *aNode, bool *aIsBlock);

  // non-virtual methods of interface methods
  bool AbsolutePositioningEnabled() const
  {
    return mIsAbsolutelyPositioningEnabled;
  }
  nsresult GetAbsolutelyPositionedSelectionContainer(nsINode** aContainer);
  Element* GetPositionedElement() const
  {
    return mAbsolutelyPositionedObject;
  }
  nsresult GetElementZIndex(Element* aElement, int32_t* aZindex);

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
protected:
  virtual ~HTMLEditor();

  using EditorBase::IsBlockNode;
  virtual bool IsBlockNode(nsINode *aNode) override;

public:
  // XXX Why don't we move following methods above for grouping by the origins?
  NS_IMETHOD SetFlags(uint32_t aFlags) override;

  NS_IMETHOD Paste(int32_t aSelectionType) override;
  NS_IMETHOD CanPaste(int32_t aSelectionType, bool* aCanPaste) override;

  NS_IMETHOD PasteTransferable(nsITransferable* aTransferable) override;
  NS_IMETHOD CanPasteTransferable(nsITransferable* aTransferable,
                                  bool* aCanPaste) override;

  NS_IMETHOD DebugUnitTests(int32_t* outNumTests,
                            int32_t* outNumTestsFailed) override;

  /**
   * All editor operations which alter the doc should be prefaced
   * with a call to StartOperation, naming the action and direction.
   */
  NS_IMETHOD StartOperation(EditAction opID,
                            nsIEditor::EDirection aDirection) override;

  /**
   * All editor operations which alter the doc should be followed
   * with a call to EndOperation.
   */
  NS_IMETHOD EndOperation() override;

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
   * Make the given selection span the entire document.
   */
  virtual nsresult SelectEntireDocument(Selection* aSelection) override;

  /**
   * Join together any adjacent editable text nodes in the range.
   */
  nsresult CollapseAdjacentTextNodes(nsRange* aRange);

  virtual bool AreNodesSameType(nsIContent* aNode1,
                                nsIContent* aNode2) override;

  NS_IMETHOD DeleteSelectionImpl(EDirection aAction,
                                 EStripWrappers aStripWrappers) override;
  nsresult DeleteNode(nsINode* aNode);
  NS_IMETHOD DeleteNode(nsIDOMNode* aNode) override;
  nsresult DeleteText(nsGenericDOMDataNode& aTextNode, uint32_t aOffset,
                      uint32_t aLength);
  virtual nsresult
  InsertTextImpl(nsIDocument& aDocument,
                 const nsAString& aStringToInsert,
                 const EditorRawDOMPoint& aPointToInsert,
                 EditorRawDOMPoint* aPointAfterInsertedString =
                   nullptr) override;
  NS_IMETHOD_(bool) IsModifiableNode(nsIDOMNode* aNode) override;
  virtual bool IsModifiableNode(nsINode* aNode) override;

  NS_IMETHOD SelectAll() override;

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet,
                              bool aWasAlternate, nsresult aStatus) override;

  // Utility Routines, not part of public API
  NS_IMETHOD TypedText(const nsAString& aString,
                       ETypingAction aAction) override;

  /**
   * InsertNodeIntoProperAncestor() attempts to insert aNode into the document,
   * at aPointToInsert.  Checks with strict dtd to see if containment is
   * allowed.  If not allowed, will attempt to find a parent in the parent
   * hierarchy of aPointToInsert.GetContainer() that will accept aNode as a
   * child.  If such a parent is found, will split the document tree from
   * aPointToInsert up to parent, and then insert aNode. aPointToInsert is then
   * adjusted to point to the actual location that aNode was inserted at.
   * aSplitAtEdges specifies if the splitting process is allowed to result in
   * empty nodes.
   *
   * @param aNode             Node to insert.
   * @param aPointToInsert    Insertion point.
   * @param aSplitAtEdges     Splitting can result in empty nodes?
   * @return                  Returns inserted point if succeeded.
   *                          Otherwise, the result is not set.
   */
  EditorDOMPoint
  InsertNodeIntoProperAncestor(nsIContent& aNode,
                               const EditorRawDOMPoint& aPointToInsert,
                               SplitAtEdges aSplitAtEdges);

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
  nsresult IsEmptyNode(nsIDOMNode* aNode, bool* outIsEmptyBlock,
                       bool aMozBRDoesntCount = false,
                       bool aListOrCellNotEmpty = false,
                       bool aSafeToAskFrames = false);
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

  bool IsCSSEnabled()
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

  ParagraphSeparator GetDefaultParagraphSeparator() const
  {
    return mDefaultParagraphSeparator;
  }
  void SetDefaultParagraphSeparator(ParagraphSeparator aSep)
  {
    mDefaultParagraphSeparator = aSep;
  }

  /**
   * event callback when a mouse button is pressed
   * @param aX      [IN] horizontal position of the pointer
   * @param aY      [IN] vertical position of the pointer
   * @param aTarget [IN] the element triggering the event
   * @param aMouseEvent [IN] the event
   */
  nsresult OnMouseDown(int32_t aX, int32_t aY, nsIDOMElement* aTarget,
                       nsIDOMEvent* aMouseEvent);

  /**
   * event callback when a mouse button is released
   * @param aX      [IN] horizontal position of the pointer
   * @param aY      [IN] vertical position of the pointer
   * @param aTarget [IN] the element triggering the event
   */
  nsresult OnMouseUp(int32_t aX, int32_t aY, nsIDOMElement* aTarget);

  /**
   * event callback when the mouse pointer is moved
   * @param aMouseEvent [IN] the event
   */
  nsresult OnMouseMove(nsIDOMMouseEvent* aMouseEvent);

  /**
   * Modifies the table containing the selection according to the
   * activation of an inline table editing UI element
   * @param aUIAnonymousElement [IN] the inline table editing UI element
   */
  nsresult DoInlineTableEditingAction(Element& aUIAnonymousElement);

protected:
  class BlobReader final : public nsIEditorBlobListener
  {
  public:
    BlobReader(dom::BlobImpl* aBlob, HTMLEditor* aHTMLEditor,
               bool aIsSafe, nsIDOMDocument* aSourceDoc,
               nsIDOMNode* aDestinationNode, int32_t aDestOffset,
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
    nsCOMPtr<nsIDOMDocument> mSourceDoc;
    nsCOMPtr<nsIDOMNode> mDestinationNode;
    int32_t mDestOffset;
    bool mDoDeleteSelection;
  };

  NS_IMETHOD InitRules() override;

  virtual void CreateEventListeners() override;
  virtual nsresult InstallEventListeners() override;
  virtual void RemoveEventListeners() override;

  bool ShouldReplaceRootElement();
  void NotifyRootChanged();
  nsresult GetBodyElement(nsIDOMHTMLElement** aBody);

  /**
   * Get the focused node of this editor.
   * @return    If the editor has focus, this returns the focused node.
   *            Otherwise, returns null.
   */
  already_AddRefed<nsINode> GetFocusedNode();

  /**
   * Return TRUE if aElement is a table-related elemet and caret was set.
   */
  bool SetCaretInTableCell(nsIDOMElement* aElement);

  nsresult TabInTable(bool inIsShift, bool* outHandled);

  /**
   * InsertBR() inserts a new <br> element at selection.  If there is
   * non-collapsed selection ranges, the selected ranges is deleted first.
   */
  nsresult InsertBR();

  // Table Editing (implemented in nsTableEditor.cpp)

  /**
   * Insert a new cell after or before supplied aCell.
   * Optional: If aNewCell supplied, returns the newly-created cell (addref'd,
   * of course)
   * This doesn't change or use the current selection.
   */
  NS_IMETHOD InsertCell(nsIDOMElement* aCell, int32_t aRowSpan,
                        int32_t aColSpan, bool aAfter, bool aIsHeader,
                        nsIDOMElement** aNewCell);

  /**
   * Helpers that don't touch the selection or do batch transactions.
   */
  NS_IMETHOD DeleteRow(nsIDOMElement* aTable, int32_t aRowIndex);
  NS_IMETHOD DeleteColumn(nsIDOMElement* aTable, int32_t aColIndex);
  NS_IMETHOD DeleteCellContents(nsIDOMElement* aCell);

  /**
   * Move all contents from aCellToMerge into aTargetCell (append at end).
   */
  NS_IMETHOD MergeCells(nsCOMPtr<nsIDOMElement> aTargetCell,
                        nsCOMPtr<nsIDOMElement> aCellToMerge,
                        bool aDeleteCellToMerge);

  nsresult DeleteTable2(nsIDOMElement* aTable, Selection* aSelection);
  NS_IMETHOD SetColSpan(nsIDOMElement* aCell, int32_t aColSpan);
  NS_IMETHOD SetRowSpan(nsIDOMElement* aCell, int32_t aRowSpan);

  /**
   * Helper used to get nsTableWrapperFrame for a table.
   */
  nsTableWrapperFrame* GetTableFrame(nsIDOMElement* aTable);

  /**
   * Needed to do appropriate deleting when last cell or row is about to be
   * deleted.  This doesn't count cells that don't start in the given row (are
   * spanning from row above).
   */
  int32_t GetNumberOfCellsInRow(nsIDOMElement* aTable, int32_t rowIndex);

  /**
   * Test if all cells in row or column at given index are selected.
   */
  bool AllCellsInRowSelected(nsIDOMElement* aTable, int32_t aRowIndex,
                             int32_t aNumberOfColumns);
  bool AllCellsInColumnSelected(nsIDOMElement* aTable, int32_t aColIndex,
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
  nsresult GetCellContext(Selection** aSelection, nsIDOMElement** aTable,
                          nsIDOMElement** aCell, nsIDOMNode** aCellParent,
                          int32_t* aCellOffset, int32_t* aRowIndex,
                          int32_t* aColIndex);

  NS_IMETHOD GetCellSpansAt(nsIDOMElement* aTable, int32_t aRowIndex,
                            int32_t aColIndex, int32_t& aActualRowSpan,
                            int32_t& aActualColSpan);

  NS_IMETHOD SplitCellIntoColumns(nsIDOMElement* aTable, int32_t aRowIndex,
                                  int32_t aColIndex, int32_t aColSpanLeft,
                                  int32_t aColSpanRight,
                                  nsIDOMElement** aNewCell);

  NS_IMETHOD SplitCellIntoRows(nsIDOMElement* aTable, int32_t aRowIndex,
                               int32_t aColIndex, int32_t aRowSpanAbove,
                               int32_t aRowSpanBelow, nsIDOMElement** aNewCell);

  nsresult CopyCellBackgroundColor(nsIDOMElement* destCell,
                                   nsIDOMElement* sourceCell);

  /**
   * Reduce rowspan/colspan when cells span into nonexistent rows/columns.
   */
  NS_IMETHOD FixBadRowSpan(nsIDOMElement* aTable, int32_t aRowIndex,
                           int32_t& aNewRowCount);
  NS_IMETHOD FixBadColSpan(nsIDOMElement* aTable, int32_t aColIndex,
                           int32_t& aNewColCount);

  /**
   * Fallback method: Call this after using ClearSelection() and you
   * failed to set selection to some other content in the document.
   */
  nsresult SetSelectionAtDocumentStart(Selection* aSelection);

  nsresult GetTableSize(Element* aTable,
                        int32_t* aRowCount, int32_t* aColCount);

  // End of Table Editing utilities

  static Element* GetEnclosingTable(nsINode* aNode);
  static nsIDOMNode* GetEnclosingTable(nsIDOMNode* aNode);

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

  // Methods for handling plaintext quotations
  NS_IMETHOD PasteAsPlaintextQuotation(int32_t aSelectionType);

  /**
   * Insert a string as quoted text, replacing the selected text (if any).
   * @param aQuotedText     The string to insert.
   * @param aAddCites       Whether to prepend extra ">" to each line
   *                        (usually true, unless those characters
   *                        have already been added.)
   * @return aNodeInserted  The node spanning the insertion, if applicable.
   *                        If aAddCites is false, this will be null.
   */
  NS_IMETHOD InsertAsPlaintextQuotation(const nsAString& aQuotedText,
                                        bool aAddCites,
                                        nsIDOMNode** aNodeInserted);

  nsresult InsertObject(const nsACString& aType, nsISupports* aObject,
                        bool aIsSafe,
                        nsIDOMDocument* aSourceDoc,
                        nsIDOMNode* aDestinationNode,
                        int32_t aDestOffset,
                        bool aDoDeleteSelection);

  // factored methods for handling insertion of data from transferables
  // (drag&drop or clipboard)
  NS_IMETHOD PrepareTransferable(nsITransferable** transferable) override;
  nsresult PrepareHTMLTransferable(nsITransferable** transferable);
  nsresult InsertFromTransferable(nsITransferable* transferable,
                                    nsIDOMDocument* aSourceDoc,
                                    const nsAString& aContextStr,
                                    const nsAString& aInfoStr,
                                    bool havePrivateHTMLFlavor,
                                    nsIDOMNode *aDestinationNode,
                                    int32_t aDestinationOffset,
                                    bool aDoDeleteSelection);
  virtual nsresult InsertFromDataTransfer(dom::DataTransfer* aDataTransfer,
                                          int32_t aIndex,
                                          nsIDOMDocument* aSourceDoc,
                                          nsIDOMNode* aDestinationNode,
                                          int32_t aDestOffset,
                                          bool aDoDeleteSelection) override;
  bool HavePrivateHTMLFlavor(nsIClipboard* clipboard );
  nsresult ParseCFHTML(nsCString& aCfhtml, char16_t** aStuffToPaste,
                       char16_t** aCfcontext);

  bool IsInLink(nsIDOMNode* aNode, nsCOMPtr<nsIDOMNode>* outLink = nullptr);
  nsresult StripFormattingNodes(nsIContent& aNode, bool aOnlyList = false);
  nsresult CreateDOMFragmentFromPaste(const nsAString& aInputString,
                                      const nsAString& aContextStr,
                                      const nsAString& aInfoStr,
                                      nsCOMPtr<nsIDOMNode>* outFragNode,
                                      nsCOMPtr<nsIDOMNode>* outStartNode,
                                      nsCOMPtr<nsIDOMNode>* outEndNode,
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
  nsresult CreateTagStack(nsTArray<nsString>& aTagStack,
                          nsIDOMNode* aNode);
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
   * Small utility routine to test if a break node is visible to user.
   */
  bool IsVisibleBRElement(nsINode* aNode);

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
   * Helpers for block transformations.
   */
  nsresult MakeDefinitionItem(const nsAString& aItemType);
  nsresult InsertBasicBlock(const nsAString& aBlockType);

  /**
   * Increase/decrease the font size of selection.
   */
  enum class FontSize { incr, decr };
  nsresult RelativeFontChange(FontSize aDir);

  /**
   * Helper routines for font size changing.
   */
  nsresult RelativeFontChangeOnTextNode(FontSize aDir,
                                        Text& aTextNode,
                                        int32_t aStartOffset,
                                        int32_t aEndOffset);
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
  nsresult SetInlinePropertyOnNode(nsIContent& aNode,
                                   nsAtom& aProperty,
                                   nsAtom* aAttribute,
                                   const nsAString& aValue);

  nsresult PromoteInlineRange(nsRange& aRange);
  nsresult PromoteRangeIfStartsOrEndsInNamedAnchor(nsRange& aRange);
  nsresult SplitStyleAboveRange(nsRange* aRange,
                                nsAtom* aProperty,
                                nsAtom* aAttribute);
  nsresult SplitStyleAbovePoint(nsCOMPtr<nsINode>* aNode, int32_t* aOffset,
                                nsAtom* aProperty,
                                nsAtom* aAttribute,
                                nsIContent** aOutLeftNode = nullptr,
                                nsIContent** aOutRightNode = nullptr);
  nsresult RemoveStyleInside(nsIContent& aNode,
                             nsAtom* aProperty,
                             nsAtom* aAttribute,
                             const bool aChildrenOnly = false);

  bool NodeIsProperty(nsINode& aNode);
  bool IsAtFrontOfNode(nsINode& aNode, int32_t aOffset);
  bool IsAtEndOfNode(nsINode& aNode, int32_t aOffset);
  bool IsOnlyAttribute(const Element* aElement, nsAtom* aAttribute);

  nsresult RemoveBlockContainer(nsIContent& aNode);

  nsIContent* GetPriorHTMLSibling(nsINode* aNode);

  nsIContent* GetNextHTMLSibling(nsINode* aNode);

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
  nsIContent* GetPreviousEditableHTMLNode(const EditorRawDOMPoint& aPoint)
  {
    return GetPreviousEditableHTMLNodeInternal(aPoint, false);
  }
  nsIContent* GetPreviousEditableHTMLNodeInBlock(
                const EditorRawDOMPoint& aPoint)
  {
    return GetPreviousEditableHTMLNodeInternal(aPoint, true);
  }

  /**
   * GetPreviousEditableHTMLNodeInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetPreviousEditableHTMLNodeInternal(nsINode& aNode,
                                                  bool aNoBlockCrossing);
  nsIContent* GetPreviousEditableHTMLNodeInternal(
                const EditorRawDOMPoint& aPoint,
                bool aNoBlockCrossing);

  /**
   * GetNextEditableHTMLNode*() methods are similar to
   * EditorBase::GetNextEditableNode() but this won't return nodes outside
   * active editing host.
   *
   * Note that same as EditorBaseGetTextEditableNode(), methods which take
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
  nsIContent* GetNextEditableHTMLNode(const EditorRawDOMPoint& aPoint)
  {
    return GetNextEditableHTMLNodeInternal(aPoint, false);
  }
  nsIContent* GetNextEditableHTMLNodeInBlock(
                const EditorRawDOMPoint& aPoint)
  {
    return GetNextEditableHTMLNodeInternal(aPoint, true);
  }

  /**
   * GetNextEditableHTMLNodeInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetNextEditableHTMLNodeInternal(nsINode& aNode,
                                                  bool aNoBlockCrossing);
  nsIContent* GetNextEditableHTMLNodeInternal(
                const EditorRawDOMPoint& aPoint,
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
                                   nsIDOMDocument* aSourceDoc,
                                   nsIDOMNode* aDestNode,
                                   int32_t aDestOffset,
                                   bool aDeleteSelection,
                                   bool aTrustedInput,
                                   bool aClearStyle = true);

  nsresult ClearStyle(nsCOMPtr<nsINode>* aNode, int32_t* aOffset,
                      nsAtom* aProperty, nsAtom* aAttribute);

  void SetElementPosition(Element& aElement, int32_t aX, int32_t aY);

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
  void SetSelectionAfterTableEdit(nsIDOMElement* aTable,
                                  int32_t aRow, int32_t aCol,
                                  int32_t aDirection, bool aSelected);

protected:
  RefPtr<TypeInState> mTypeInState;

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

protected:
  // ANONYMOUS UTILS
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

  bool IsInObservedSubtree(nsIDocument* aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild);

  void UpdateRootElement();

  // resizing
  bool mIsObjectResizingEnabled;
  bool mIsResizing;
  bool mPreserveRatio;
  bool mResizedObjectIsAnImage;

  // absolute positioning
  bool mIsAbsolutelyPositioningEnabled;
  bool mResizedObjectIsAbsolutelyPositioned;

  bool mGrabberClicked;
  bool mIsMoving;

  bool mSnapToGridEnabled;

  // inline table editing
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

  nsCOMPtr<Element> mResizedObject;

  nsCOMPtr<nsIDOMEventListener>  mMouseMotionListenerP;
  nsCOMPtr<nsISelectionListener> mSelectionListenerP;
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

  int8_t  mInfoXIncrement;
  int8_t  mInfoYIncrement;

  nsresult SetAllResizersPosition();

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

  int32_t GetNewResizingIncrement(int32_t aX, int32_t aY, int32_t aID);
  nsresult StartResizing(nsIDOMElement* aHandle);
  int32_t GetNewResizingX(int32_t aX, int32_t aY);
  int32_t GetNewResizingY(int32_t aX, int32_t aY);
  int32_t GetNewResizingWidth(int32_t aX, int32_t aY);
  int32_t GetNewResizingHeight(int32_t aX, int32_t aY);
  void HideShadowAndInfo();
  void SetFinalSize(int32_t aX, int32_t aY);
  void SetResizeIncrements(int32_t aX, int32_t aY, int32_t aW, int32_t aH,
                           bool aPreserveRatio);
  void HideAnonymousEditingUIs();

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

  ManualNACPtr CreateGrabber(nsIContent& aParentContent);
  nsresult StartMoving(nsIDOMElement* aHandle);
  nsresult SetFinalPosition(int32_t aX, int32_t aY);
  void AddPositioningOffset(int32_t& aX, int32_t& aY);
  void SnapToGrid(int32_t& newX, int32_t& newY);
  nsresult GrabberClicked();
  nsresult EndMoving();
  nsresult CheckPositionedElementBGandFG(nsIDOMElement* aElement,
                                         nsAString& aReturn);

  // inline table editing
  RefPtr<Element> mInlineEditedCell;

  ManualNACPtr mAddColumnBeforeButton;
  ManualNACPtr mRemoveColumnButton;
  ManualNACPtr mAddColumnAfterButton;

  ManualNACPtr mAddRowBeforeButton;
  ManualNACPtr mRemoveRowButton;
  ManualNACPtr mAddRowAfterButton;

  /**
   * Shows inline table editing UI around a table cell
   * @param aCell [IN] a DOM Element being a table cell, td or th
   */
  nsresult ShowInlineTableEditingUI(Element* aCell);

  /**
   * Hide all inline table editing UI
   */
  nsresult HideInlineTableEditingUI();

  void AddMouseClickListener(Element* aElement);
  void RemoveMouseClickListener(Element* aElement);

  nsCOMPtr<nsILinkHandler> mLinkHandler;

  ParagraphSeparator mDefaultParagraphSeparator;

public:
  friend class AutoSelectionSetterAfterTableEdit;
  friend class HTMLEditorEventListener;
  friend class HTMLEditRules;
  friend class TextEditRules;
  friend class WSRunObject;

private:
  bool IsSimpleModifiableNode(nsIContent* aContent,
                              nsAtom* aProperty,
                              nsAtom* aAttribute,
                              const nsAString* aValue);
  nsresult SetInlinePropertyOnNodeImpl(nsIContent& aNode,
                                       nsAtom& aProperty,
                                       nsAtom* aAttribute,
                                       const nsAString& aValue);
  typedef enum { eInserted, eAppended } InsertedOrAppended;
  void DoContentInserted(nsIDocument* aDocument, nsIContent* aContainer,
                         nsIContent* aChild,
                         InsertedOrAppended aInsertedOrAppended);
  already_AddRefed<Element> GetElementOrParentByTagName(
                              const nsAString& aTagName, nsINode* aNode);
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
