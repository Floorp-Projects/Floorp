/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLEditor_h__
#define nsHTMLEditor_h__

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsPlaintextEditor.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsITableEditor.h"
#include "nsIEditorMailSupport.h"
#include "nsIEditorStyleSheets.h"

#include "nsEditor.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsICSSLoaderObserver.h"

#include "nsEditRules.h"

#include "nsHTMLCSSUtils.h"

#include "nsHTMLObjectResizer.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizeListener.h"
#include "nsIHTMLObjectResizer.h"

#include "nsIDocumentObserver.h"

#include "nsPoint.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsAttrName.h"
#include "nsStubMutationObserver.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"

class nsIDOMKeyEvent;
class nsITransferable;
class nsIDocumentEncoder;
class nsIClipboard;
class TypeInState;
class nsIContentFilter;
class nsIURL;
class nsILinkHandler;
class nsTableOuterFrame;
class nsIDOMRange;
class nsRange;
struct PropItem;

namespace mozilla {
namespace widget {
struct IMEState;
} // namespace widget
} // namespace mozilla

/**
 * The HTML editor implementation.<br>
 * Use to edit HTML document represented as a DOM tree. 
 */
class nsHTMLEditor final : public nsPlaintextEditor,
                           public nsIHTMLEditor,
                           public nsIHTMLObjectResizer,
                           public nsIHTMLAbsPosEditor,
                           public nsITableEditor,
                           public nsIHTMLInlineTableEditor,
                           public nsIEditorStyleSheets,
                           public nsICSSLoaderObserver,
                           public nsStubMutationObserver
{
  typedef enum {eNoOp, eReplaceParent=1, eInsertParent=2} BlockTransformationType;

public:

  enum ResizingRequestID
  {
    kX      = 0,
    kY      = 1,
    kWidth  = 2,
    kHeight = 3
  };

  // see nsIHTMLEditor for documentation

//Interfaces for addref and release and queryinterface
//NOTE macro used is for classes that inherit from 
// another class. Only the base class should use NS_DECL_ISUPPORTS
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLEditor, nsPlaintextEditor)


  nsHTMLEditor();

  bool GetReturnInParagraphCreatesNewParagraph();

  /* ------------ nsPlaintextEditor overrides -------------- */
  NS_IMETHOD GetIsDocumentEditable(bool *aIsDocumentEditable) override;
  NS_IMETHOD BeginningOfDocument() override;
  virtual nsresult HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent) override;
  virtual already_AddRefed<nsIContent> GetFocusedContent() override;
  virtual already_AddRefed<nsIContent> GetFocusedContentForIME() override;
  virtual bool IsActiveInDOMWindow() override;
  virtual already_AddRefed<mozilla::dom::EventTarget> GetDOMEventTarget() override;
  virtual mozilla::dom::Element* GetEditorRoot() override;
  virtual already_AddRefed<nsIContent> FindSelectionRoot(nsINode *aNode) override;
  virtual bool IsAcceptableInputEvent(nsIDOMEvent* aEvent) override;
  virtual already_AddRefed<nsIContent> GetInputEventTargetContent() override;
  virtual bool IsEditable(nsINode* aNode) override;
  using nsEditor::IsEditable;

  /* ------------ nsStubMutationObserver overrides --------- */
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  /* ------------ nsIEditorIMESupport overrides ------------ */
  NS_IMETHOD GetPreferredIMEState(mozilla::widget::IMEState *aState) override;

  /* ------------ nsIHTMLEditor methods -------------- */

  NS_DECL_NSIHTMLEDITOR

  /* ------------ nsIHTMLObjectResizer methods -------------- */
  /* -------- Implemented in nsHTMLObjectResizer.cpp -------- */
  NS_DECL_NSIHTMLOBJECTRESIZER

  /* ------------ nsIHTMLAbsPosEditor methods -------------- */
  /* -------- Implemented in nsHTMLAbsPosition.cpp --------- */
  NS_DECL_NSIHTMLABSPOSEDITOR

  /* ------------ nsIHTMLInlineTableEditor methods -------------- */
  /* ------- Implemented in nsHTMLInlineTableEditor.cpp --------- */
  NS_DECL_NSIHTMLINLINETABLEEDITOR

  /* ------------ nsIHTMLEditor methods -------------- */
  nsresult CopyLastEditableChildStyles(nsIDOMNode *aPreviousBlock, nsIDOMNode *aNewBlock,
                                         nsIDOMNode **aOutBrNode);

  nsresult LoadHTML(const nsAString &aInputString);

  nsresult GetCSSBackgroundColorState(bool *aMixed, nsAString &aOutColor,
                                      bool aBlockLevel);
  NS_IMETHOD GetHTMLBackgroundColorState(bool *aMixed, nsAString &outColor);

  /* ------------ nsIEditorStyleSheets methods -------------- */

  NS_IMETHOD AddStyleSheet(const nsAString & aURL) override;
  NS_IMETHOD ReplaceStyleSheet(const nsAString& aURL) override;
  NS_IMETHOD RemoveStyleSheet(const nsAString &aURL) override;

  NS_IMETHOD AddOverrideStyleSheet(const nsAString & aURL) override;
  NS_IMETHOD ReplaceOverrideStyleSheet(const nsAString& aURL) override;
  NS_IMETHOD RemoveOverrideStyleSheet(const nsAString &aURL) override;

  NS_IMETHOD EnableStyleSheet(const nsAString& aURL, bool aEnable) override;

  /* ------------ nsIEditorMailSupport methods -------------- */

  NS_DECL_NSIEDITORMAILSUPPORT

  /* ------------ nsITableEditor methods -------------- */

  NS_IMETHOD InsertTableCell(int32_t aNumber, bool aAfter) override;
  NS_IMETHOD InsertTableColumn(int32_t aNumber, bool aAfter) override;
  NS_IMETHOD InsertTableRow(int32_t aNumber, bool aAfter) override;
  NS_IMETHOD DeleteTable() override;
  NS_IMETHOD DeleteTableCell(int32_t aNumber) override;
  NS_IMETHOD DeleteTableCellContents() override;
  NS_IMETHOD DeleteTableColumn(int32_t aNumber) override;
  NS_IMETHOD DeleteTableRow(int32_t aNumber) override;
  NS_IMETHOD SelectTableCell() override;
  NS_IMETHOD SelectBlockOfCells(nsIDOMElement *aStartCell, nsIDOMElement *aEndCell) override;
  NS_IMETHOD SelectTableRow() override;
  NS_IMETHOD SelectTableColumn() override;
  NS_IMETHOD SelectTable() override;
  NS_IMETHOD SelectAllTableCells() override;
  NS_IMETHOD SwitchTableCellHeaderType(nsIDOMElement *aSourceCell, nsIDOMElement **aNewCell) override;
  NS_IMETHOD JoinTableCells(bool aMergeNonContiguousContents) override;
  NS_IMETHOD SplitTableCell() override;
  NS_IMETHOD NormalizeTable(nsIDOMElement *aTable) override;
  NS_IMETHOD GetCellIndexes(nsIDOMElement *aCell,
                            int32_t* aRowIndex, int32_t* aColIndex) override;
  NS_IMETHOD GetTableSize(nsIDOMElement *aTable,
                          int32_t* aRowCount, int32_t* aColCount) override;
  NS_IMETHOD GetCellAt(nsIDOMElement* aTable, int32_t aRowIndex, int32_t aColIndex, nsIDOMElement **aCell) override;
  NS_IMETHOD GetCellDataAt(nsIDOMElement* aTable,
                           int32_t aRowIndex, int32_t aColIndex,
                           nsIDOMElement **aCell,
                           int32_t* aStartRowIndex, int32_t* aStartColIndex,
                           int32_t* aRowSpan, int32_t* aColSpan, 
                           int32_t* aActualRowSpan, int32_t* aActualColSpan, 
                           bool* aIsSelected) override;
  NS_IMETHOD GetFirstRow(nsIDOMElement* aTableElement, nsIDOMNode** aRowNode) override;
  NS_IMETHOD GetNextRow(nsIDOMNode* aCurrentRowNode, nsIDOMNode** aRowNode) override;
  nsresult GetLastCellInRow(nsIDOMNode* aRowNode, nsIDOMNode** aCellNode);

  NS_IMETHOD SetSelectionAfterTableEdit(nsIDOMElement* aTable, int32_t aRow, int32_t aCol, 
                                        int32_t aDirection, bool aSelected) override;
  NS_IMETHOD GetSelectedOrParentTableElement(nsAString& aTagName,
                                             int32_t *aSelectedCount,
                                             nsIDOMElement** aTableElement) override;
  NS_IMETHOD GetSelectedCellsType(nsIDOMElement *aElement, uint32_t *aSelectionType) override;

  nsresult GetCellFromRange(nsRange* aRange, nsIDOMElement** aCell);

  // Finds the first selected cell in first range of selection
  // This is in the *order of selection*, not order in the table
  // (i.e., each cell added to selection is added in another range 
  //  in the selection's rangelist, independent of location in table)
  // aRange is optional: returns the range around the cell
  NS_IMETHOD GetFirstSelectedCell(nsIDOMRange **aRange, nsIDOMElement **aCell) override;
  // Get next cell until no more are found. Always use GetFirstSelected cell first
  // aRange is optional: returns the range around the cell
  NS_IMETHOD GetNextSelectedCell(nsIDOMRange **aRange, nsIDOMElement **aCell) override;

  // Upper-left-most selected cell in table
  NS_IMETHOD GetFirstSelectedCellInTable(int32_t *aRowIndex, int32_t *aColIndex, nsIDOMElement **aCell) override;
    
  /* miscellaneous */
  // This sets background on the appropriate container element (table, cell,)
  //   or calls into nsTextEditor to set the page background
  nsresult SetCSSBackgroundColor(const nsAString& aColor);
  nsresult SetHTMLBackgroundColor(const nsAString& aColor);

  /* ------------ Block methods moved from nsEditor -------------- */
  static already_AddRefed<mozilla::dom::Element> GetBlockNodeParent(nsINode* aNode);
  static already_AddRefed<nsIDOMNode> GetBlockNodeParent(nsIDOMNode *aNode);

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

  /* ------------ Overrides of nsEditor interface methods -------------- */

  nsresult EndUpdateViewBatch() override;

  /** prepare the editor for use */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIContent *aRoot,
                  nsISelectionController *aSelCon, uint32_t aFlags,
                  const nsAString& aValue) override;
  NS_IMETHOD PreDestroy(bool aDestroyingFrames) override;

  /** Internal, static version */
  // aElement must not be null.
  static bool NodeIsBlockStatic(const mozilla::dom::Element* aElement);
  static nsresult NodeIsBlockStatic(nsIDOMNode *aNode, bool *aIsBlock);
protected:
  virtual ~nsHTMLEditor();

  using nsEditor::IsBlockNode;
  virtual bool IsBlockNode(nsINode *aNode) override;

public:
  NS_IMETHOD SetFlags(uint32_t aFlags) override;

  NS_IMETHOD Paste(int32_t aSelectionType) override;
  NS_IMETHOD CanPaste(int32_t aSelectionType, bool *aCanPaste) override;

  NS_IMETHOD PasteTransferable(nsITransferable *aTransferable) override;
  NS_IMETHOD CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste) override;

  NS_IMETHOD DebugUnitTests(int32_t *outNumTests, int32_t *outNumTestsFailed) override;

  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(EditAction opID,
                            nsIEditor::EDirection aDirection) override;

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation() override;

  /** returns true if aParentTag can contain a child of type aChildTag */
  virtual bool TagCanContainTag(nsIAtom& aParentTag, nsIAtom& aChildTag)
    override;
  
  /** returns true if aNode is a container */
  virtual bool IsContainer(nsINode* aNode) override;
  virtual bool IsContainer(nsIDOMNode* aNode) override;

  /** make the given selection span the entire document */
  virtual nsresult SelectEntireDocument(mozilla::dom::Selection* aSelection) override;

  NS_IMETHOD SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      const nsAString & aValue,
                                      bool aSuppressTransaction) override;
  NS_IMETHOD RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                         const nsAString & aAttribute,
                                         bool aSuppressTransaction) override;

  /** join together any adjacent editable text nodes in the range */
  nsresult CollapseAdjacentTextNodes(nsRange* aRange);

  virtual bool AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2)
    override;

  NS_IMETHOD DeleteSelectionImpl(EDirection aAction,
                                 EStripWrappers aStripWrappers) override;
  nsresult DeleteNode(nsINode* aNode);
  NS_IMETHOD DeleteNode(nsIDOMNode * aNode) override;
  nsresult DeleteText(nsGenericDOMDataNode& aTextNode, uint32_t aOffset,
                      uint32_t aLength);
  virtual nsresult InsertTextImpl(const nsAString& aStringToInsert,
                                  nsCOMPtr<nsINode>* aInOutNode,
                                  int32_t* aInOutOffset,
                                  nsIDocument* aDoc) override;
  NS_IMETHOD_(bool) IsModifiableNode(nsIDOMNode *aNode) override;
  virtual bool IsModifiableNode(nsINode *aNode) override;

  NS_IMETHOD GetIsSelectionEditable(bool* aIsSelectionEditable) override;

  NS_IMETHOD SelectAll() override;

  NS_IMETHOD GetRootElement(nsIDOMElement **aRootElement) override;

  /* ------------ nsICSSLoaderObserver -------------- */
  NS_IMETHOD StyleSheetLoaded(mozilla::CSSStyleSheet* aSheet,
                              bool aWasAlternate, nsresult aStatus) override;

  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD TypedText(const nsAString& aString, ETypingAction aAction) override;
  nsresult InsertNodeAtPoint( nsIDOMNode *aNode, 
                              nsCOMPtr<nsIDOMNode> *ioParent, 
                              int32_t *ioOffset, 
                              bool aNoEmptyNodes);

  // Use this to assure that selection is set after attribute nodes when 
  //  trying to collapse selection at begining of a block node
  //  e.g., when setting at beginning of a table cell
  // This will stop at a table, however, since we don't want to
  //  "drill down" into nested tables.
  // aSelection is optional -- if null, we get current seletion
  void CollapseSelectionToDeepestNonTableFirstChild(
                          mozilla::dom::Selection* aSelection, nsINode* aNode);

  /**
   * aNode must be a non-null text node.
   * outIsEmptyNode must be non-null.
   */
  nsresult IsVisTextNode(nsIContent* aNode,
                         bool* outIsEmptyNode,
                         bool aSafeToAskFrames);
  nsresult IsEmptyNode(nsIDOMNode *aNode, bool *outIsEmptyBlock, 
                       bool aMozBRDoesntCount = false,
                       bool aListOrCellNotEmpty = false,
                       bool aSafeToAskFrames = false);
  nsresult IsEmptyNode(nsINode* aNode, bool* outIsEmptyBlock,
                       bool aMozBRDoesntCount = false,
                       bool aListOrCellNotEmpty = false,
                       bool aSafeToAskFrames = false);
  nsresult IsEmptyNodeImpl(nsINode* aNode,
                           bool *outIsEmptyBlock, 
                           bool aMozBRDoesntCount,
                           bool aListOrCellNotEmpty,
                           bool aSafeToAskFrames,
                           bool *aSeenBR);

  // Returns TRUE if sheet was loaded, false if it wasn't
  bool     EnableExistingStyleSheet(const nsAString& aURL);

  // Dealing with the internal style sheet lists:
  NS_IMETHOD GetStyleSheetForURL(const nsAString &aURL,
                                 mozilla::CSSStyleSheet** _retval) override;
  NS_IMETHOD GetURLForStyleSheet(mozilla::CSSStyleSheet* aStyleSheet,
                                 nsAString& aURL) override;

  // Add a url + known style sheet to the internal lists:
  nsresult AddNewStyleSheetToList(const nsAString &aURL,
                                  mozilla::CSSStyleSheet* aStyleSheet);

  nsresult RemoveStyleSheetFromList(const nsAString &aURL);

  bool IsCSSEnabled()
  {
    // TODO: removal of mCSSAware and use only the presence of mHTMLCSSUtils
    return mCSSAware && mHTMLCSSUtils && mHTMLCSSUtils->IsCSSPrefChecked();
  }

  static bool HasAttributes(mozilla::dom::Element* aElement)
  {
    MOZ_ASSERT(aElement);
    uint32_t attrCount = aElement->GetAttrCount();
    return attrCount > 1 ||
           (1 == attrCount && !aElement->GetAttrNameAt(0)->Equals(nsGkAtoms::mozdirty));
  }

protected:

  NS_IMETHOD  InitRules() override;

  // Create the event listeners for the editor to install
  virtual void CreateEventListeners() override;

  virtual nsresult InstallEventListeners() override;
  virtual void RemoveEventListeners() override;

  bool ShouldReplaceRootElement();
  void ResetRootElementAndEventTarget();
  nsresult GetBodyElement(nsIDOMHTMLElement** aBody);
  // Get the focused node of this editor.
  // @return    If the editor has focus, this returns the focused node.
  //            Otherwise, returns null.
  already_AddRefed<nsINode> GetFocusedNode();

  // Return TRUE if aElement is a table-related elemet and caret was set
  bool SetCaretInTableCell(nsIDOMElement* aElement);

  // key event helpers
  NS_IMETHOD TabInTable(bool inIsShift, bool *outHandled);
  already_AddRefed<mozilla::dom::Element> CreateBR(nsINode* aNode,
      int32_t aOffset, EDirection aSelect = eNone);
  NS_IMETHOD CreateBR(nsIDOMNode *aNode, int32_t aOffset, 
                      nsCOMPtr<nsIDOMNode> *outBRNode, nsIEditor::EDirection aSelect = nsIEditor::eNone) override;

// Table Editing (implemented in nsTableEditor.cpp)

  // Table utilities

  // Insert a new cell after or before supplied aCell. 
  //  Optional: If aNewCell supplied, returns the newly-created cell (addref'd, of course)
  // This doesn't change or use the current selection
  NS_IMETHOD InsertCell(nsIDOMElement *aCell, int32_t aRowSpan, int32_t aColSpan,
                        bool aAfter, bool aIsHeader, nsIDOMElement **aNewCell);

  // Helpers that don't touch the selection or do batch transactions
  NS_IMETHOD DeleteRow(nsIDOMElement *aTable, int32_t aRowIndex);
  NS_IMETHOD DeleteColumn(nsIDOMElement *aTable, int32_t aColIndex);
  NS_IMETHOD DeleteCellContents(nsIDOMElement *aCell);

  // Move all contents from aCellToMerge into aTargetCell (append at end)
  NS_IMETHOD MergeCells(nsCOMPtr<nsIDOMElement> aTargetCell, nsCOMPtr<nsIDOMElement> aCellToMerge, bool aDeleteCellToMerge);

  nsresult DeleteTable2(nsIDOMElement* aTable,
                        mozilla::dom::Selection* aSelection);
  NS_IMETHOD SetColSpan(nsIDOMElement *aCell, int32_t aColSpan);
  NS_IMETHOD SetRowSpan(nsIDOMElement *aCell, int32_t aRowSpan);

  // Helper used to get nsTableOuterFrame for a table.
  nsTableOuterFrame* GetTableFrame(nsIDOMElement* aTable);
  // Needed to do appropriate deleting when last cell or row is about to be deleted
  // This doesn't count cells that don't start in the given row (are spanning from row above)
  int32_t  GetNumberOfCellsInRow(nsIDOMElement* aTable, int32_t rowIndex);
  // Test if all cells in row or column at given index are selected
  bool AllCellsInRowSelected(nsIDOMElement *aTable, int32_t aRowIndex, int32_t aNumberOfColumns);
  bool AllCellsInColumnSelected(nsIDOMElement *aTable, int32_t aColIndex, int32_t aNumberOfRows);

  bool IsEmptyCell(mozilla::dom::Element* aCell);

  // Most insert methods need to get the same basic context data
  // Any of the pointers may be null if you don't need that datum (for more efficiency)
  // Input: *aCell is a known cell,
  //        if null, cell is obtained from the anchor node of the selection
  // Returns NS_EDITOR_ELEMENT_NOT_FOUND if cell is not found even if aCell is null
  nsresult GetCellContext(mozilla::dom::Selection** aSelection,
                          nsIDOMElement** aTable, nsIDOMElement** aCell,
                          nsIDOMNode** aCellParent, int32_t* aCellOffset,
                          int32_t* aRowIndex, int32_t* aColIndex);

  NS_IMETHOD GetCellSpansAt(nsIDOMElement* aTable, int32_t aRowIndex, int32_t aColIndex, 
                            int32_t& aActualRowSpan, int32_t& aActualColSpan);

  NS_IMETHOD SplitCellIntoColumns(nsIDOMElement *aTable, int32_t aRowIndex, int32_t aColIndex,
                                  int32_t aColSpanLeft, int32_t aColSpanRight, nsIDOMElement **aNewCell);

  NS_IMETHOD SplitCellIntoRows(nsIDOMElement *aTable, int32_t aRowIndex, int32_t aColIndex,
                               int32_t aRowSpanAbove, int32_t aRowSpanBelow, nsIDOMElement **aNewCell);

  nsresult CopyCellBackgroundColor(nsIDOMElement *destCell, nsIDOMElement *sourceCell);

  // Reduce rowspan/colspan when cells span into nonexistent rows/columns
  NS_IMETHOD FixBadRowSpan(nsIDOMElement *aTable, int32_t aRowIndex, int32_t& aNewRowCount);
  NS_IMETHOD FixBadColSpan(nsIDOMElement *aTable, int32_t aColIndex, int32_t& aNewColCount);

  // Fallback method: Call this after using ClearSelection() and you
  //  failed to set selection to some other content in the document
  nsresult SetSelectionAtDocumentStart(mozilla::dom::Selection* aSelection);

// End of Table Editing utilities
  
  static already_AddRefed<mozilla::dom::Element>
    GetEnclosingTable(nsINode* aNode);
  static nsCOMPtr<nsIDOMNode> GetEnclosingTable(nsIDOMNode *aNode);

  /** content-based query returns true if <aProperty aAttribute=aValue> effects aNode
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
    * @param aIsSet     [OUT] true if <aProperty aAttribute=aValue> effects aNode.
    * @param outValue   [OUT] the value of the attribute, if aIsSet is true
    *
    * The nsIContent variant returns aIsSet instead of using an out parameter.
    */
  bool IsTextPropertySetByContent(nsINode*         aNode,
                                  nsIAtom*         aProperty,
                                  const nsAString* aAttribute,
                                  const nsAString* aValue,
                                  nsAString*       outValue = nullptr);

  void IsTextPropertySetByContent(nsIDOMNode*      aNode,
                                  nsIAtom*         aProperty,
                                  const nsAString* aAttribute,
                                  const nsAString* aValue,
                                  bool&            aIsSet,
                                  nsAString*       outValue = nullptr);

  // Methods for handling plaintext quotations
  NS_IMETHOD PasteAsPlaintextQuotation(int32_t aSelectionType);

  /** Insert a string as quoted text,
    * replacing the selected text (if any).
    * @param aQuotedText     The string to insert.
    * @param aAddCites       Whether to prepend extra ">" to each line
    *                        (usually true, unless those characters
    *                        have already been added.)
    * @return aNodeInserted  The node spanning the insertion, if applicable.
    *                        If aAddCites is false, this will be null.
    */
  NS_IMETHOD InsertAsPlaintextQuotation(const nsAString & aQuotedText,
                                        bool aAddCites,
                                        nsIDOMNode **aNodeInserted);

  nsresult InsertObject(const char* aType, nsISupports* aObject, bool aIsSafe,
                        nsIDOMDocument *aSourceDoc,
                        nsIDOMNode *aDestinationNode,
                        int32_t aDestOffset,
                        bool aDoDeleteSelection);

  // factored methods for handling insertion of data from transferables (drag&drop or clipboard)
  NS_IMETHOD PrepareTransferable(nsITransferable **transferable) override;
  nsresult PrepareHTMLTransferable(nsITransferable **transferable, bool havePrivFlavor);
  nsresult InsertFromTransferable(nsITransferable *transferable, 
                                    nsIDOMDocument *aSourceDoc,
                                    const nsAString & aContextStr,
                                    const nsAString & aInfoStr,
                                    nsIDOMNode *aDestinationNode,
                                    int32_t aDestinationOffset,
                                    bool aDoDeleteSelection);
  nsresult InsertFromDataTransfer(mozilla::dom::DataTransfer *aDataTransfer,
                                  int32_t aIndex,
                                  nsIDOMDocument *aSourceDoc,
                                  nsIDOMNode *aDestinationNode,
                                  int32_t aDestOffset,
                                  bool aDoDeleteSelection) override;
  bool HavePrivateHTMLFlavor( nsIClipboard *clipboard );
  nsresult   ParseCFHTML(nsCString & aCfhtml, char16_t **aStuffToPaste, char16_t **aCfcontext);
  nsresult   DoContentFilterCallback(const nsAString &aFlavor,
                                     nsIDOMDocument *aSourceDoc,
                                     bool aWillDeleteSelection,
                                     nsIDOMNode **aFragmentAsNode,      
                                     nsIDOMNode **aFragStartNode,
                                     int32_t *aFragStartOffset,
                                     nsIDOMNode **aFragEndNode,
                                     int32_t *aFragEndOffset,
                                     nsIDOMNode **aTargetNode,       
                                     int32_t *aTargetOffset,   
                                     bool *aDoContinue);

  bool       IsInLink(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *outLink = nullptr);
  nsresult   StripFormattingNodes(nsIDOMNode *aNode, bool aOnlyList = false);
  nsresult   CreateDOMFragmentFromPaste(const nsAString & aInputString,
                                        const nsAString & aContextStr,
                                        const nsAString & aInfoStr,
                                        nsCOMPtr<nsIDOMNode> *outFragNode,
                                        nsCOMPtr<nsIDOMNode> *outStartNode,
                                        nsCOMPtr<nsIDOMNode> *outEndNode,
                                        int32_t *outStartOffset,
                                        int32_t *outEndOffset,
                                        bool aTrustedInput);
  nsresult   ParseFragment(const nsAString & aStr, nsIAtom* aContextLocalName,
                           nsIDocument* aTargetDoc,
                           nsCOMPtr<nsIDOMNode> *outNode,
                           bool aTrustedInput);
  nsresult   CreateListOfNodesToPaste(nsIDOMNode  *aFragmentAsNode,
                                      nsCOMArray<nsIDOMNode>& outNodeList,
                                      nsIDOMNode *aStartNode,
                                      int32_t aStartOffset,
                                      nsIDOMNode *aEndNode,
                                      int32_t aEndOffset);
  nsresult CreateTagStack(nsTArray<nsString> &aTagStack,
                          nsIDOMNode *aNode);
  nsresult GetListAndTableParents( bool aEnd, 
                                   nsCOMArray<nsIDOMNode>& aListOfNodes,
                                   nsCOMArray<nsIDOMNode>& outArray);
  nsresult DiscoverPartialListsAndTables(nsCOMArray<nsIDOMNode>& aPasteNodes,
                                         nsCOMArray<nsIDOMNode>& aListsAndTables,
                                         int32_t *outHighWaterMark);
  nsresult ScanForListAndTableStructure(bool aEnd,
                                        nsCOMArray<nsIDOMNode>& aNodes,
                                        nsIDOMNode *aListOrTable,
                                        nsCOMPtr<nsIDOMNode> *outReplaceNode);
  nsresult ReplaceOrphanedStructure( bool aEnd,
                                     nsCOMArray<nsIDOMNode>& aNodeArray,
                                     nsCOMArray<nsIDOMNode>& aListAndTableArray,
                                     int32_t aHighWaterMark);
  nsIDOMNode* GetArrayEndpoint(bool aEnd, nsCOMArray<nsIDOMNode>& aNodeArray);

  /* small utility routine to test if a break node is visible to user */
  bool     IsVisBreak(nsINode* aNode);
  bool     IsVisBreak(nsIDOMNode *aNode);

  /* utility routine to possibly adjust the insertion position when 
     inserting a block level element */
  void NormalizeEOLInsertPosition(nsIDOMNode *firstNodeToInsert,
                                  nsCOMPtr<nsIDOMNode> *insertParentNode,
                                  int32_t *insertOffset);

  /* small utility routine to test the eEditorReadonly bit */
  bool IsModifiable();

  /* helpers for block transformations */
  nsresult MakeDefinitionItem(const nsAString & aItemType);
  nsresult InsertBasicBlock(const nsAString & aBlockType);
  
  /* increase/decrease the font size of selection */
  nsresult RelativeFontChange( int32_t aSizeChange);
  
  /* helper routines for font size changing */
  nsresult RelativeFontChangeOnTextNode( int32_t aSizeChange, 
                                         nsIDOMCharacterData *aTextNode, 
                                         int32_t aStartOffset,
                                         int32_t aEndOffset);
  nsresult RelativeFontChangeOnNode(int32_t aSizeChange, nsIContent* aNode);
  nsresult RelativeFontChangeHelper(int32_t aSizeChange, nsINode* aNode);

  /* helper routines for inline style */
  nsresult SetInlinePropertyOnTextNode(mozilla::dom::Text& aData,
                                       int32_t aStartOffset,
                                       int32_t aEndOffset,
                                       nsIAtom& aProperty,
                                       const nsAString* aAttribute,
                                       const nsAString& aValue);
  nsresult SetInlinePropertyOnNode( nsIDOMNode *aNode,
                                    nsIAtom *aProperty, 
                                    const nsAString *aAttribute,
                                    const nsAString *aValue);
  nsresult SetInlinePropertyOnNode(nsIContent* aNode,
                                   nsIAtom* aProperty,
                                   const nsAString* aAttribute,
                                   const nsAString* aValue);

  nsresult PromoteInlineRange(nsRange* aRange);
  nsresult PromoteRangeIfStartsOrEndsInNamedAnchor(nsRange* aRange);
  nsresult SplitStyleAboveRange(nsRange* aRange,
                                nsIAtom *aProperty, 
                                const nsAString *aAttribute);
  nsresult SplitStyleAbovePoint(nsCOMPtr<nsIDOMNode> *aNode,
                                int32_t *aOffset,
                                nsIAtom *aProperty, 
                                const nsAString *aAttribute,
                                nsCOMPtr<nsIDOMNode> *outLeftNode = nullptr,
                                nsCOMPtr<nsIDOMNode> *outRightNode = nullptr);
  nsresult ApplyDefaultProperties();
  nsresult RemoveStyleInside(nsIDOMNode *aNode, 
                             nsIAtom *aProperty, 
                             const nsAString *aAttribute, 
                             const bool aChildrenOnly = false);
  nsresult RemoveInlinePropertyImpl(nsIAtom* aProperty,
                                    const nsAString* aAttribute);

  bool NodeIsProperty(nsIDOMNode *aNode);
  bool HasAttr(nsIDOMNode *aNode, const nsAString *aAttribute);
  bool IsAtFrontOfNode(nsIDOMNode *aNode, int32_t aOffset);
  bool IsAtEndOfNode(nsIDOMNode *aNode, int32_t aOffset);
  bool IsOnlyAttribute(nsIDOMNode *aElement, const nsAString *aAttribute);
  bool IsOnlyAttribute(const nsIContent* aElement, const nsAString& aAttribute);

  nsresult RemoveBlockContainer(nsIDOMNode *inNode);

  nsIContent* GetPriorHTMLSibling(nsINode* aNode);
  nsresult GetPriorHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsIContent* GetPriorHTMLSibling(nsINode* aParent, int32_t aOffset);
  nsresult GetPriorHTMLSibling(nsIDOMNode *inParent, int32_t inOffset, nsCOMPtr<nsIDOMNode> *outNode);

  nsIContent* GetNextHTMLSibling(nsINode* aNode);
  nsresult GetNextHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsIContent* GetNextHTMLSibling(nsINode* aParent, int32_t aOffset);
  nsresult GetNextHTMLSibling(nsIDOMNode *inParent, int32_t inOffset, nsCOMPtr<nsIDOMNode> *outNode);

  nsIContent* GetPriorHTMLNode(nsINode* aNode, bool aNoBlockCrossing = false);
  nsresult GetPriorHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing = false);
  nsIContent* GetPriorHTMLNode(nsINode* aParent, int32_t aOffset,
                               bool aNoBlockCrossing = false);
  nsresult GetPriorHTMLNode(nsIDOMNode *inParent, int32_t inOffset, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing = false);

  nsIContent* GetNextHTMLNode(nsINode* aNode, bool aNoBlockCrossing = false);
  nsresult GetNextHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing = false);
  nsIContent* GetNextHTMLNode(nsINode* aParent, int32_t aOffset,
                              bool aNoBlockCrossing = false);
  nsresult GetNextHTMLNode(nsIDOMNode *inParent, int32_t inOffset, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing = false);

  nsresult IsFirstEditableChild( nsIDOMNode *aNode, bool *aOutIsFirst);
  nsresult IsLastEditableChild( nsIDOMNode *aNode, bool *aOutIsLast);
  nsIContent* GetFirstEditableChild(nsINode& aNode);
  nsIContent* GetLastEditableChild(nsINode& aNode);

  nsIContent* GetFirstEditableLeaf(nsINode& aNode);
  nsIContent* GetLastEditableLeaf(nsINode& aNode);

  nsresult GetInlinePropertyBase(nsIAtom& aProperty,
                                 const nsAString* aAttribute,
                                 const nsAString* aValue,
                                 bool* aFirst,
                                 bool* aAny,
                                 bool* aAll,
                                 nsAString* outValue,
                                 bool aCheckDefaults = true);
  bool HasStyleOrIdOrClass(mozilla::dom::Element* aElement);
  nsresult RemoveElementIfNoStyleOrIdOrClass(nsIDOMNode* aElement);

  // Whether the outer window of the DOM event target has focus or not.
  bool     OurWindowHasFocus();

  // This function is used to insert a string of HTML input optionally with some
  // context information into the editable field.  The HTML input either comes
  // from a transferable object created as part of a drop/paste operation, or from
  // the InsertHTML method.  We may want the HTML input to be sanitized (for example,
  // if it's coming from a transferable object), in which case aTrustedInput should
  // be set to false, otherwise, the caller should set it to true, which means that
  // the HTML will be inserted in the DOM verbatim.
  //
  // aClearStyle should be set to false if you want the paste to be affected by
  // local style (e.g., for the insertHTML command).
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

  nsresult ClearStyle(nsCOMPtr<nsIDOMNode>* aNode, int32_t* aOffset,
                      nsIAtom* aProperty, const nsAString* aAttribute);

// Data members
protected:

  nsCOMArray<nsIContentFilter> mContentFilters;

  nsRefPtr<TypeInState>        mTypeInState;

  bool mCRInParagraphCreatesParagraph;

  bool mCSSAware;
  nsAutoPtr<nsHTMLCSSUtils> mHTMLCSSUtils;

  // Used by GetFirstSelectedCell and GetNextSelectedCell
  int32_t  mSelectedCellIndex;

  nsString mLastStyleSheetURL;
  nsString mLastOverrideStyleSheetURL;

  // Maintain a list of associated style sheets and their urls.
  nsTArray<nsString> mStyleSheetURLs;
  nsTArray<nsRefPtr<mozilla::CSSStyleSheet>> mStyleSheets;
  
  // an array for holding default style settings
  nsTArray<PropItem*> mDefaultStyles;

protected:

  /* ANONYMOUS UTILS */
  void     RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                      nsIDOMEventListener* aListener,
                                      bool aUseCapture,
                                      mozilla::dom::Element* aElement,
                                      nsIContent* aParentContent,
                                      nsIPresShell* aShell);
  void     DeleteRefToAnonymousNode(nsIDOMElement* aElement,
                                    nsIContent * aParentContent,
                                    nsIPresShell* aShell);

  nsresult ShowResizersInner(nsIDOMElement *aResizedElement);

  // Returns the offset of an element's frame to its absolute containing block.
  nsresult GetElementOrigin(nsIDOMElement * aElement, int32_t & aX, int32_t & aY);
  nsresult GetPositionAndDimensions(nsIDOMElement * aElement,
                                    int32_t & aX, int32_t & aY,
                                    int32_t & aW, int32_t & aH,
                                    int32_t & aBorderLeft,
                                    int32_t & aBorderTop,
                                    int32_t & aMarginLeft,
                                    int32_t & aMarginTop);

  /* PACKED BOOLEANS FOR RESIZING, ABSOLUTE POSITIONING AND */
  /* INLINE TABLE EDITING */

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

  /* RESIZING */

  nsCOMPtr<mozilla::dom::Element> mTopLeftHandle;
  nsCOMPtr<mozilla::dom::Element> mTopHandle;
  nsCOMPtr<mozilla::dom::Element> mTopRightHandle;
  nsCOMPtr<mozilla::dom::Element> mLeftHandle;
  nsCOMPtr<mozilla::dom::Element> mRightHandle;
  nsCOMPtr<mozilla::dom::Element> mBottomLeftHandle;
  nsCOMPtr<mozilla::dom::Element> mBottomHandle;
  nsCOMPtr<mozilla::dom::Element> mBottomRightHandle;

  nsCOMPtr<mozilla::dom::Element> mActivatedHandle;

  nsCOMPtr<mozilla::dom::Element> mResizingShadow;
  nsCOMPtr<mozilla::dom::Element> mResizingInfo;

  nsCOMPtr<mozilla::dom::Element> mResizedObject;

  nsCOMPtr<nsIDOMEventListener>  mMouseMotionListenerP;
  nsCOMPtr<nsISelectionListener> mSelectionListenerP;
  nsCOMPtr<nsIDOMEventListener>  mResizeEventListenerP;

  nsCOMArray<nsIHTMLObjectResizeListener> objectResizeEventListeners;

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

  already_AddRefed<mozilla::dom::Element>
    CreateResizer(int16_t aLocation, nsIDOMNode* aParentNode);
  void     SetAnonymousElementPosition(int32_t aX, int32_t aY, nsIDOMElement *aResizer);

  already_AddRefed<mozilla::dom::Element>
    CreateShadow(nsIDOMNode* aParentNode, nsIDOMElement* aOriginalObject);
  nsresult SetShadowPosition(mozilla::dom::Element* aShadow,
                             mozilla::dom::Element* aOriginalObject,
                             int32_t aOriginalObjectX,
                             int32_t aOriginalObjectY);

  already_AddRefed<mozilla::dom::Element> CreateResizingInfo(nsIDOMNode* aParentNode);
  nsresult SetResizingInfoPosition(int32_t aX, int32_t aY,
                                   int32_t aW, int32_t aH);

  int32_t  GetNewResizingIncrement(int32_t aX, int32_t aY, int32_t aID);
  nsresult StartResizing(nsIDOMElement * aHandle);
  int32_t  GetNewResizingX(int32_t aX, int32_t aY);
  int32_t  GetNewResizingY(int32_t aX, int32_t aY);
  int32_t  GetNewResizingWidth(int32_t aX, int32_t aY);
  int32_t  GetNewResizingHeight(int32_t aX, int32_t aY);
  void     HideShadowAndInfo();
  void     SetFinalSize(int32_t aX, int32_t aY);
  void     DeleteRefToAnonymousNode(nsIDOMNode * aNode);
  void     SetResizeIncrements(int32_t aX, int32_t aY, int32_t aW, int32_t aH, bool aPreserveRatio);
  void     HideAnonymousEditingUIs();

  /* ABSOLUTE POSITIONING */

  int32_t mPositionedObjectX;
  int32_t mPositionedObjectY;
  int32_t mPositionedObjectWidth;
  int32_t mPositionedObjectHeight;

  int32_t mPositionedObjectMarginLeft;
  int32_t mPositionedObjectMarginTop;
  int32_t mPositionedObjectBorderLeft;
  int32_t mPositionedObjectBorderTop;

  nsCOMPtr<mozilla::dom::Element> mAbsolutelyPositionedObject;
  nsCOMPtr<mozilla::dom::Element> mGrabber;
  nsCOMPtr<mozilla::dom::Element> mPositioningShadow;

  int32_t      mGridSize;

  already_AddRefed<mozilla::dom::Element> CreateGrabber(nsINode* aParentNode);
  nsresult StartMoving(nsIDOMElement * aHandle);
  nsresult SetFinalPosition(int32_t aX, int32_t aY);
  void     AddPositioningOffset(int32_t & aX, int32_t & aY);
  void     SnapToGrid(int32_t & newX, int32_t & newY);
  nsresult GrabberClicked();
  nsresult EndMoving();
  nsresult CheckPositionedElementBGandFG(nsIDOMElement * aElement,
                                         nsAString & aReturn);

  /* INLINE TABLE EDITING */

  nsCOMPtr<nsIDOMElement> mInlineEditedCell;

  nsCOMPtr<nsIDOMElement> mAddColumnBeforeButton;
  nsCOMPtr<nsIDOMElement> mRemoveColumnButton;
  nsCOMPtr<nsIDOMElement> mAddColumnAfterButton;

  nsCOMPtr<nsIDOMElement> mAddRowBeforeButton;
  nsCOMPtr<nsIDOMElement> mRemoveRowButton;
  nsCOMPtr<nsIDOMElement> mAddRowAfterButton;

  void     AddMouseClickListener(nsIDOMElement * aElement);
  void     RemoveMouseClickListener(nsIDOMElement * aElement);

  nsCOMPtr<nsILinkHandler> mLinkHandler;

public:

// friends
friend class nsHTMLEditRules;
friend class nsTextEditRules;
friend class nsWSRunObject;
friend class nsHTMLEditorEventListener;

private:
  // Helpers
  bool IsSimpleModifiableNode(nsIContent* aContent,
                              nsIAtom* aProperty,
                              const nsAString* aAttribute,
                              const nsAString* aValue);
  nsresult SetInlinePropertyOnNodeImpl(nsIContent* aNode,
                                       nsIAtom* aProperty,
                                       const nsAString* aAttribute,
                                       const nsAString* aValue);
  typedef enum { eInserted, eAppended } InsertedOrAppended;
  void DoContentInserted(nsIDocument* aDocument, nsIContent* aContainer,
                         nsIContent* aChild, int32_t aIndexInContainer,
                         InsertedOrAppended aInsertedOrAppended);
  already_AddRefed<mozilla::dom::Element> GetElementOrParentByTagName(
      const nsAString& aTagName, nsINode* aNode);
  already_AddRefed<mozilla::dom::Element> CreateElementWithDefaults(
      const nsAString& aTagName);
};
#endif //nsHTMLEditor_h__

