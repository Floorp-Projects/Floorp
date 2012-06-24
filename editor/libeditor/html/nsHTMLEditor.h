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
#include "nsITextServicesDocument.h"

#include "nsEditor.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsICSSLoaderObserver.h"
#include "nsITableLayout.h"

#include "nsEditRules.h"

#include "nsEditProperty.h"
#include "nsHTMLCSSUtils.h"

#include "nsHTMLObjectResizer.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizeListener.h"

#include "nsIDocumentObserver.h"

#include "nsPoint.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsAttrName.h"

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
class nsHTMLEditor : public nsPlaintextEditor,
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
  virtual  ~nsHTMLEditor();

  bool GetReturnInParagraphCreatesNewParagraph();

  /* ------------ nsPlaintextEditor overrides -------------- */
  NS_IMETHOD GetIsDocumentEditable(bool *aIsDocumentEditable);
  NS_IMETHOD BeginningOfDocument();
  virtual nsresult HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent);
  virtual already_AddRefed<nsIContent> GetFocusedContent();
  virtual bool IsActiveInDOMWindow();
  virtual already_AddRefed<nsIDOMEventTarget> GetDOMEventTarget();
  virtual mozilla::dom::Element* GetEditorRoot() MOZ_OVERRIDE;
  virtual already_AddRefed<nsIContent> FindSelectionRoot(nsINode *aNode);
  virtual bool IsAcceptableInputEvent(nsIDOMEvent* aEvent);
  virtual already_AddRefed<nsIContent> GetInputEventTargetContent();
  virtual bool IsEditable(nsIContent *aNode);
  using nsEditor::IsEditable;

  /* ------------ nsStubMutationObserver overrides --------- */
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  /* ------------ nsIEditorIMESupport overrides ------------ */
  NS_IMETHOD GetPreferredIMEState(mozilla::widget::IMEState *aState);

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
  NS_IMETHOD CopyLastEditableChildStyles(nsIDOMNode *aPreviousBlock, nsIDOMNode *aNewBlock,
                                         nsIDOMNode **aOutBrNode);

  NS_IMETHOD LoadHTML(const nsAString &aInputString);

  nsresult GetCSSBackgroundColorState(bool *aMixed, nsAString &aOutColor,
                                      bool aBlockLevel);
  NS_IMETHOD GetHTMLBackgroundColorState(bool *aMixed, nsAString &outColor);

  /* ------------ nsIEditorStyleSheets methods -------------- */

  NS_IMETHOD AddStyleSheet(const nsAString & aURL);
  NS_IMETHOD ReplaceStyleSheet(const nsAString& aURL);
  NS_IMETHOD RemoveStyleSheet(const nsAString &aURL);

  NS_IMETHOD AddOverrideStyleSheet(const nsAString & aURL);
  NS_IMETHOD ReplaceOverrideStyleSheet(const nsAString& aURL);
  NS_IMETHOD RemoveOverrideStyleSheet(const nsAString &aURL);

  NS_IMETHOD EnableStyleSheet(const nsAString& aURL, bool aEnable);

  /* ------------ nsIEditorMailSupport methods -------------- */

  NS_DECL_NSIEDITORMAILSUPPORT

  /* ------------ nsITableEditor methods -------------- */

  NS_IMETHOD InsertTableCell(PRInt32 aNumber, bool aAfter);
  NS_IMETHOD InsertTableColumn(PRInt32 aNumber, bool aAfter);
  NS_IMETHOD InsertTableRow(PRInt32 aNumber, bool aAfter);
  NS_IMETHOD DeleteTable();
  NS_IMETHOD DeleteTableCell(PRInt32 aNumber);
  NS_IMETHOD DeleteTableCellContents();
  NS_IMETHOD DeleteTableColumn(PRInt32 aNumber);
  NS_IMETHOD DeleteTableRow(PRInt32 aNumber);
  NS_IMETHOD SelectTableCell();
  NS_IMETHOD SelectBlockOfCells(nsIDOMElement *aStartCell, nsIDOMElement *aEndCell);
  NS_IMETHOD SelectTableRow();
  NS_IMETHOD SelectTableColumn();
  NS_IMETHOD SelectTable();
  NS_IMETHOD SelectAllTableCells();
  NS_IMETHOD SwitchTableCellHeaderType(nsIDOMElement *aSourceCell, nsIDOMElement **aNewCell);
  NS_IMETHOD JoinTableCells(bool aMergeNonContiguousContents);
  NS_IMETHOD SplitTableCell();
  NS_IMETHOD NormalizeTable(nsIDOMElement *aTable);
  NS_IMETHOD GetCellIndexes(nsIDOMElement *aCell,
                            PRInt32* aRowIndex, PRInt32* aColIndex);
  NS_IMETHOD GetTableSize(nsIDOMElement *aTable,
                          PRInt32* aRowCount, PRInt32* aColCount);
  NS_IMETHOD GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement **aCell);
  NS_IMETHOD GetCellDataAt(nsIDOMElement* aTable,
                           PRInt32 aRowIndex, PRInt32 aColIndex,
                           nsIDOMElement **aCell,
                           PRInt32* aStartRowIndex, PRInt32* aStartColIndex,
                           PRInt32* aRowSpan, PRInt32* aColSpan, 
                           PRInt32* aActualRowSpan, PRInt32* aActualColSpan, 
                           bool* aIsSelected);
  NS_IMETHOD GetFirstRow(nsIDOMElement* aTableElement, nsIDOMNode** aRowNode);
  NS_IMETHOD GetNextRow(nsIDOMNode* aCurrentRowNode, nsIDOMNode** aRowNode);
  NS_IMETHOD GetLastCellInRow(nsIDOMNode* aRowNode, nsIDOMNode** aCellNode);

  NS_IMETHOD SetSelectionAfterTableEdit(nsIDOMElement* aTable, PRInt32 aRow, PRInt32 aCol, 
                                        PRInt32 aDirection, bool aSelected);
  NS_IMETHOD GetSelectedOrParentTableElement(nsAString& aTagName,
                                             PRInt32 *aSelectedCount,
                                             nsIDOMElement** aTableElement);
  NS_IMETHOD GetSelectedCellsType(nsIDOMElement *aElement, PRUint32 *aSelectionType);

  nsresult GetCellFromRange(nsIDOMRange *aRange, nsIDOMElement **aCell);

  // Finds the first selected cell in first range of selection
  // This is in the *order of selection*, not order in the table
  // (i.e., each cell added to selection is added in another range 
  //  in the selection's rangelist, independent of location in table)
  // aRange is optional: returns the range around the cell
  NS_IMETHOD GetFirstSelectedCell(nsIDOMRange **aRange, nsIDOMElement **aCell);
  // Get next cell until no more are found. Always use GetFirstSelected cell first
  // aRange is optional: returns the range around the cell
  NS_IMETHOD GetNextSelectedCell(nsIDOMRange **aRange, nsIDOMElement **aCell);

  // Upper-left-most selected cell in table
  NS_IMETHOD GetFirstSelectedCellInTable(PRInt32 *aRowIndex, PRInt32 *aColIndex, nsIDOMElement **aCell);
    
  /* miscellaneous */
  // This sets background on the appropriate container element (table, cell,)
  //   or calls into nsTextEditor to set the page background
  NS_IMETHOD SetCSSBackgroundColor(const nsAString& aColor);
  NS_IMETHOD SetHTMLBackgroundColor(const nsAString& aColor);

  /* ------------ Block methods moved from nsEditor -------------- */
  static already_AddRefed<nsIDOMNode> GetBlockNodeParent(nsIDOMNode *aNode);

  static already_AddRefed<nsIDOMNode> NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir);
  void     IsNextCharWhitespace(nsIDOMNode *aParentNode,
                                PRInt32 aOffset, 
                                bool *outIsSpace, 
                                bool *outIsNBSP,
                                nsCOMPtr<nsIDOMNode> *outNode = 0,
                                PRInt32 *outOffset = 0);
  void     IsPrevCharWhitespace(nsIDOMNode *aParentNode,
                                PRInt32 aOffset, 
                                bool *outIsSpace, 
                                bool *outIsNBSP,
                                nsCOMPtr<nsIDOMNode> *outNode = 0,
                                PRInt32 *outOffset = 0);

  /* ------------ Overrides of nsEditor interface methods -------------- */

  nsresult EndUpdateViewBatch();

  /** prepare the editor for use */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags);
  NS_IMETHOD PreDestroy(bool aDestroyingFrames);

  /** Internal, static version */
  static nsresult NodeIsBlockStatic(nsIDOMNode *aNode, bool *aIsBlock);

  NS_IMETHOD SetFlags(PRUint32 aFlags);

  NS_IMETHOD Paste(PRInt32 aSelectionType);
  NS_IMETHOD CanPaste(PRInt32 aSelectionType, bool *aCanPaste);

  NS_IMETHOD PasteTransferable(nsITransferable *aTransferable);
  NS_IMETHOD CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste);

  NS_IMETHOD DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed);

  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(OperationID opID,
                            nsIEditor::EDirection aDirection);

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation();

  /** returns true if aParentTag can contain a child of type aChildTag */
  virtual bool TagCanContainTag(nsIAtom* aParentTag, nsIAtom* aChildTag);
  
  /** returns true if aNode is a container */
  virtual bool IsContainer(nsIDOMNode *aNode);

  /** make the given selection span the entire document */
  NS_IMETHOD SelectEntireDocument(nsISelection *aSelection);

  NS_IMETHOD SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      const nsAString & aValue,
                                      bool aSuppressTransaction);
  NS_IMETHOD RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                         const nsAString & aAttribute,
                                         bool aSuppressTransaction);

  /** join together any adjacent editable text nodes in the range */
  NS_IMETHOD CollapseAdjacentTextNodes(nsIDOMRange *aInRange);

  virtual bool AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2)
    MOZ_OVERRIDE;

  NS_IMETHOD DeleteSelectionImpl(EDirection aAction,
                                 EStripWrappers aStripWrappers);
  nsresult DeleteNode(nsINode* aNode);
  NS_IMETHODIMP DeleteNode(nsIDOMNode * aNode);
  NS_IMETHODIMP DeleteText(nsIDOMCharacterData *aTextNode,
                           PRUint32             aOffset,
                           PRUint32             aLength);
  NS_IMETHOD InsertTextImpl(const nsAString& aStringToInsert, 
                            nsCOMPtr<nsIDOMNode> *aInOutNode, 
                            PRInt32 *aInOutOffset,
                            nsIDOMDocument *aDoc);
  NS_IMETHOD_(bool) IsModifiableNode(nsIDOMNode *aNode);
  virtual bool IsModifiableNode(nsINode *aNode);

  NS_IMETHOD GetIsSelectionEditable(bool* aIsSelectionEditable);

  NS_IMETHOD SelectAll();

  NS_IMETHOD GetRootElement(nsIDOMElement **aRootElement);

  /* ------------ nsICSSLoaderObserver -------------- */
  NS_IMETHOD StyleSheetLoaded(nsCSSStyleSheet*aSheet, bool aWasAlternate,
                              nsresult aStatus);

  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD TypedText(const nsAString& aString, ETypingAction aAction);
  nsresult InsertNodeAtPoint( nsIDOMNode *aNode, 
                              nsCOMPtr<nsIDOMNode> *ioParent, 
                              PRInt32 *ioOffset, 
                              bool aNoEmptyNodes);
  virtual already_AddRefed<nsIDOMNode> FindUserSelectAllNode(nsIDOMNode* aNode);

  // Use this to assure that selection is set after attribute nodes when 
  //  trying to collapse selection at begining of a block node
  //  e.g., when setting at beginning of a table cell
  // This will stop at a table, however, since we don't want to
  //  "drill down" into nested tables.
  // aSelection is optional -- if null, we get current seletion
  nsresult CollapseSelectionToDeepestNonTableFirstChild(nsISelection *aSelection, nsIDOMNode *aNode);

  /**
   * aNode must be a non-null text node.
   */
  virtual bool IsTextInDirtyFrameVisible(nsIContent *aNode);

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
                                 nsCSSStyleSheet **_retval);
  NS_IMETHOD GetURLForStyleSheet(nsCSSStyleSheet *aStyleSheet, nsAString &aURL);

  // Add a url + known style sheet to the internal lists:
  nsresult AddNewStyleSheetToList(const nsAString &aURL,
                                  nsCSSStyleSheet *aStyleSheet);

  nsresult RemoveStyleSheetFromList(const nsAString &aURL);

  bool IsCSSEnabled()
  {
    // TODO: removal of mCSSAware and use only the presence of mHTMLCSSUtils
    return mCSSAware && mHTMLCSSUtils && mHTMLCSSUtils->IsCSSPrefChecked();
  }

  static bool HasAttributes(mozilla::dom::Element* aElement)
  {
    MOZ_ASSERT(aElement);
    PRUint32 attrCount = aElement->GetAttrCount();
    return attrCount > 1 ||
           (1 == attrCount && !aElement->GetAttrNameAt(0)->Equals(nsGkAtoms::mozdirty));
  }

protected:

  NS_IMETHOD  InitRules();

  // Create the event listeners for the editor to install
  virtual void CreateEventListeners();

  virtual nsresult InstallEventListeners();
  virtual void RemoveEventListeners();

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
  NS_IMETHOD CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, 
                      nsCOMPtr<nsIDOMNode> *outBRNode, nsIEditor::EDirection aSelect = nsIEditor::eNone);

// Table Editing (implemented in nsTableEditor.cpp)

  // Table utilities

  // Insert a new cell after or before supplied aCell. 
  //  Optional: If aNewCell supplied, returns the newly-created cell (addref'd, of course)
  // This doesn't change or use the current selection
  NS_IMETHOD InsertCell(nsIDOMElement *aCell, PRInt32 aRowSpan, PRInt32 aColSpan,
                        bool aAfter, bool aIsHeader, nsIDOMElement **aNewCell);

  // Helpers that don't touch the selection or do batch transactions
  NS_IMETHOD DeleteRow(nsIDOMElement *aTable, PRInt32 aRowIndex);
  NS_IMETHOD DeleteColumn(nsIDOMElement *aTable, PRInt32 aColIndex);
  NS_IMETHOD DeleteCellContents(nsIDOMElement *aCell);

  // Move all contents from aCellToMerge into aTargetCell (append at end)
  NS_IMETHOD MergeCells(nsCOMPtr<nsIDOMElement> aTargetCell, nsCOMPtr<nsIDOMElement> aCellToMerge, bool aDeleteCellToMerge);

  NS_IMETHOD DeleteTable2(nsIDOMElement *aTable, nsISelection *aSelection);
  NS_IMETHOD SetColSpan(nsIDOMElement *aCell, PRInt32 aColSpan);
  NS_IMETHOD SetRowSpan(nsIDOMElement *aCell, PRInt32 aRowSpan);

  // Helper used to get nsITableLayout interface for methods implemented in nsTableFrame
  NS_IMETHOD GetTableLayoutObject(nsIDOMElement* aTable, nsITableLayout **tableLayoutObject);
  // Needed to do appropriate deleting when last cell or row is about to be deleted
  // This doesn't count cells that don't start in the given row (are spanning from row above)
  PRInt32  GetNumberOfCellsInRow(nsIDOMElement* aTable, PRInt32 rowIndex);
  // Test if all cells in row or column at given index are selected
  bool AllCellsInRowSelected(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32 aNumberOfColumns);
  bool AllCellsInColumnSelected(nsIDOMElement *aTable, PRInt32 aColIndex, PRInt32 aNumberOfRows);

  bool IsEmptyCell(mozilla::dom::Element* aCell);

  // Most insert methods need to get the same basic context data
  // Any of the pointers may be null if you don't need that datum (for more efficiency)
  // Input: *aCell is a known cell,
  //        if null, cell is obtained from the anchor node of the selection
  // Returns NS_EDITOR_ELEMENT_NOT_FOUND if cell is not found even if aCell is null
  NS_IMETHOD GetCellContext(nsISelection **aSelection,
                            nsIDOMElement   **aTable,
                            nsIDOMElement   **aCell,
                            nsIDOMNode      **aCellParent, PRInt32 *aCellOffset,
                            PRInt32 *aRowIndex, PRInt32 *aColIndex);

  NS_IMETHOD GetCellSpansAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, 
                            PRInt32& aActualRowSpan, PRInt32& aActualColSpan);

  NS_IMETHOD SplitCellIntoColumns(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32 aColIndex,
                                  PRInt32 aColSpanLeft, PRInt32 aColSpanRight, nsIDOMElement **aNewCell);

  NS_IMETHOD SplitCellIntoRows(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32 aColIndex,
                               PRInt32 aRowSpanAbove, PRInt32 aRowSpanBelow, nsIDOMElement **aNewCell);

  nsresult CopyCellBackgroundColor(nsIDOMElement *destCell, nsIDOMElement *sourceCell);

  // Reduce rowspan/colspan when cells span into nonexistent rows/columns
  NS_IMETHOD FixBadRowSpan(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32& aNewRowCount);
  NS_IMETHOD FixBadColSpan(nsIDOMElement *aTable, PRInt32 aColIndex, PRInt32& aNewColCount);

  // Fallback method: Call this after using ClearSelection() and you
  //  failed to set selection to some other content in the document
  NS_IMETHOD SetSelectionAtDocumentStart(nsISelection *aSelection);

// End of Table Editing utilities
  
  virtual bool IsBlockNode(nsIDOMNode *aNode);
  virtual bool IsBlockNode(nsINode *aNode);
  
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
  bool IsTextPropertySetByContent(nsIContent*      aContent,
                                  nsIAtom*         aProperty,
                                  const nsAString* aAttribute,
                                  const nsAString* aValue,
                                  nsAString*       outValue = nsnull);

  void IsTextPropertySetByContent(nsIDOMNode*      aNode,
                                  nsIAtom*         aProperty,
                                  const nsAString* aAttribute,
                                  const nsAString* aValue,
                                  bool&            aIsSet,
                                  nsAString*       outValue = nsnull);

  // Methods for handling plaintext quotations
  NS_IMETHOD PasteAsPlaintextQuotation(PRInt32 aSelectionType);

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
  // Return true if the data is safe to insert as the source and destination
  // principals match, or we are in a editor context where this doesn't matter.
  // Otherwise, the data must be sanitized first.
  bool IsSafeToInsertData(nsIDOMDocument* aSourceDoc);

  nsresult InsertObject(const char* aType, nsISupports* aObject, bool aIsSafe,
                        nsIDOMDocument *aSourceDoc,
                        nsIDOMNode *aDestinationNode,
                        PRInt32 aDestOffset,
                        bool aDoDeleteSelection);

  // factored methods for handling insertion of data from transferables (drag&drop or clipboard)
  NS_IMETHOD PrepareTransferable(nsITransferable **transferable);
  NS_IMETHOD PrepareHTMLTransferable(nsITransferable **transferable, bool havePrivFlavor);
  NS_IMETHOD InsertFromTransferable(nsITransferable *transferable, 
                                    nsIDOMDocument *aSourceDoc,
                                    const nsAString & aContextStr,
                                    const nsAString & aInfoStr,
                                    nsIDOMNode *aDestinationNode,
                                    PRInt32 aDestinationOffset,
                                    bool aDoDeleteSelection);
  nsresult InsertFromDataTransfer(nsIDOMDataTransfer *aDataTransfer,
                                  PRInt32 aIndex,
                                  nsIDOMDocument *aSourceDoc,
                                  nsIDOMNode *aDestinationNode,
                                  PRInt32 aDestOffset,
                                  bool aDoDeleteSelection);
  bool HavePrivateHTMLFlavor( nsIClipboard *clipboard );
  nsresult   ParseCFHTML(nsCString & aCfhtml, PRUnichar **aStuffToPaste, PRUnichar **aCfcontext);
  nsresult   DoContentFilterCallback(const nsAString &aFlavor,
                                     nsIDOMDocument *aSourceDoc,
                                     bool aWillDeleteSelection,
                                     nsIDOMNode **aFragmentAsNode,      
                                     nsIDOMNode **aFragStartNode,
                                     PRInt32 *aFragStartOffset,
                                     nsIDOMNode **aFragEndNode,
                                     PRInt32 *aFragEndOffset,
                                     nsIDOMNode **aTargetNode,       
                                     PRInt32 *aTargetOffset,   
                                     bool *aDoContinue);
  nsresult   GetAttributeToModifyOnNode(nsIDOMNode *aNode, nsAString &aAttrib);

  bool       IsInLink(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *outLink = nsnull);
  nsresult   StripFormattingNodes(nsIDOMNode *aNode, bool aOnlyList = false);
  nsresult   CreateDOMFragmentFromPaste(const nsAString & aInputString,
                                        const nsAString & aContextStr,
                                        const nsAString & aInfoStr,
                                        nsCOMPtr<nsIDOMNode> *outFragNode,
                                        nsCOMPtr<nsIDOMNode> *outStartNode,
                                        nsCOMPtr<nsIDOMNode> *outEndNode,
                                        PRInt32 *outStartOffset,
                                        PRInt32 *outEndOffset,
                                        bool aTrustedInput);
  nsresult   ParseFragment(const nsAString & aStr, nsIAtom* aContextLocalName,
                           nsIDocument* aTargetDoc,
                           nsCOMPtr<nsIDOMNode> *outNode,
                           bool aTrustedInput);
  nsresult   CreateListOfNodesToPaste(nsIDOMNode  *aFragmentAsNode,
                                      nsCOMArray<nsIDOMNode>& outNodeList,
                                      nsIDOMNode *aStartNode,
                                      PRInt32 aStartOffset,
                                      nsIDOMNode *aEndNode,
                                      PRInt32 aEndOffset);
  nsresult CreateTagStack(nsTArray<nsString> &aTagStack,
                          nsIDOMNode *aNode);
  nsresult GetListAndTableParents( bool aEnd, 
                                   nsCOMArray<nsIDOMNode>& aListOfNodes,
                                   nsCOMArray<nsIDOMNode>& outArray);
  nsresult DiscoverPartialListsAndTables(nsCOMArray<nsIDOMNode>& aPasteNodes,
                                         nsCOMArray<nsIDOMNode>& aListsAndTables,
                                         PRInt32 *outHighWaterMark);
  nsresult ScanForListAndTableStructure(bool aEnd,
                                        nsCOMArray<nsIDOMNode>& aNodes,
                                        nsIDOMNode *aListOrTable,
                                        nsCOMPtr<nsIDOMNode> *outReplaceNode);
  nsresult ReplaceOrphanedStructure( bool aEnd,
                                     nsCOMArray<nsIDOMNode>& aNodeArray,
                                     nsCOMArray<nsIDOMNode>& aListAndTableArray,
                                     PRInt32 aHighWaterMark);
  nsIDOMNode* GetArrayEndpoint(bool aEnd, nsCOMArray<nsIDOMNode>& aNodeArray);

  /* small utility routine to test if a break node is visible to user */
  bool     IsVisBreak(nsIDOMNode *aNode);

  /* utility routine to possibly adjust the insertion position when 
     inserting a block level element */
  void NormalizeEOLInsertPosition(nsIDOMNode *firstNodeToInsert,
                                  nsCOMPtr<nsIDOMNode> *insertParentNode,
                                  PRInt32 *insertOffset);

  /* small utility routine to test the eEditorReadonly bit */
  bool IsModifiable();

  /* helpers for block transformations */
  nsresult MakeDefinitionItem(const nsAString & aItemType);
  nsresult InsertBasicBlock(const nsAString & aBlockType);
  
  /* increase/decrease the font size of selection */
  nsresult RelativeFontChange( PRInt32 aSizeChange);
  
  /* helper routines for font size changing */
  nsresult RelativeFontChangeOnTextNode( PRInt32 aSizeChange, 
                                         nsIDOMCharacterData *aTextNode, 
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset);
  nsresult RelativeFontChangeOnNode(PRInt32 aSizeChange, nsINode* aNode);
  nsresult RelativeFontChangeHelper(PRInt32 aSizeChange, nsINode* aNode);

  /* helper routines for inline style */
  nsresult SetInlinePropertyOnTextNode( nsIDOMCharacterData *aTextNode, 
                                        PRInt32 aStartOffset,
                                        PRInt32 aEndOffset,
                                        nsIAtom *aProperty, 
                                        const nsAString *aAttribute,
                                        const nsAString *aValue);
  nsresult SetInlinePropertyOnNode( nsIDOMNode *aNode,
                                    nsIAtom *aProperty, 
                                    const nsAString *aAttribute,
                                    const nsAString *aValue);
  nsresult SetInlinePropertyOnNode(nsIContent* aNode,
                                   nsIAtom* aProperty,
                                   const nsAString* aAttribute,
                                   const nsAString* aValue);

  nsresult PromoteInlineRange(nsIDOMRange *inRange);
  nsresult PromoteRangeIfStartsOrEndsInNamedAnchor(nsIDOMRange *inRange);
  nsresult SplitStyleAboveRange(nsIDOMRange *aRange, 
                                nsIAtom *aProperty, 
                                const nsAString *aAttribute);
  nsresult SplitStyleAbovePoint(nsCOMPtr<nsIDOMNode> *aNode,
                                PRInt32 *aOffset,
                                nsIAtom *aProperty, 
                                const nsAString *aAttribute,
                                nsCOMPtr<nsIDOMNode> *outLeftNode = nsnull,
                                nsCOMPtr<nsIDOMNode> *outRightNode = nsnull);
  nsresult ApplyDefaultProperties();
  nsresult RemoveStyleInside(nsIDOMNode *aNode, 
                             nsIAtom *aProperty, 
                             const nsAString *aAttribute, 
                             const bool aChildrenOnly = false);
  nsresult RemoveInlinePropertyImpl(nsIAtom *aProperty, const nsAString *aAttribute);

  bool NodeIsProperty(nsIDOMNode *aNode);
  bool HasAttr(nsIDOMNode *aNode, const nsAString *aAttribute);
  bool IsAtFrontOfNode(nsIDOMNode *aNode, PRInt32 aOffset);
  bool IsAtEndOfNode(nsIDOMNode *aNode, PRInt32 aOffset);
  bool IsOnlyAttribute(nsIDOMNode *aElement, const nsAString *aAttribute);
  bool IsOnlyAttribute(const nsIContent* aElement, const nsAString& aAttribute);

  nsresult RemoveBlockContainer(nsIDOMNode *inNode);
  nsIContent* GetPriorHTMLSibling(nsINode* aNode);
  nsresult GetPriorHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsIContent* GetPriorHTMLSibling(nsINode* aParent, PRInt32 aOffset);
  nsresult GetPriorHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);
  nsIContent* GetNextHTMLSibling(nsINode* aNode);
  nsresult GetNextHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsIContent* GetNextHTMLSibling(nsINode* aParent, PRInt32 aOffset);
  nsresult GetNextHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetPriorHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing = false);
  nsresult GetPriorHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing = false);
  nsresult GetNextHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing = false);
  nsresult GetNextHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing = false);

  nsresult IsFirstEditableChild( nsIDOMNode *aNode, bool *aOutIsFirst);
  nsresult IsLastEditableChild( nsIDOMNode *aNode, bool *aOutIsLast);
  nsresult GetFirstEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstChild);
  nsresult GetLastEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastChild);

  nsresult GetFirstEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstLeaf);
  nsresult GetLastEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastLeaf);

  nsresult GetInlinePropertyBase(nsIAtom *aProperty, 
                             const nsAString *aAttribute,
                             const nsAString *aValue,
                             bool *aFirst, 
                             bool *aAny, 
                             bool *aAll,
                             nsAString *outValue,
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
  nsresult DoInsertHTMLWithContext(const nsAString& aInputString,
                                   const nsAString& aContextStr,
                                   const nsAString& aInfoStr,
                                   const nsAString& aFlavor,
                                   nsIDOMDocument* aSourceDoc,
                                   nsIDOMNode* aDestNode,
                                   PRInt32 aDestOffset,
                                   bool aDeleteSelection,
                                   bool aTrustedInput);

  nsresult ClearStyle(nsCOMPtr<nsIDOMNode>* aNode, PRInt32* aOffset,
                      nsIAtom* aProperty, const nsAString* aAttribute);

// Data members
protected:

  nsCOMArray<nsIContentFilter> mContentFilters;

  nsRefPtr<TypeInState>        mTypeInState;

  bool mCRInParagraphCreatesParagraph;

  bool mCSSAware;
  nsAutoPtr<nsHTMLCSSUtils> mHTMLCSSUtils;

  // Used by GetFirstSelectedCell and GetNextSelectedCell
  PRInt32  mSelectedCellIndex;

  nsString mLastStyleSheetURL;
  nsString mLastOverrideStyleSheetURL;

  // Maintain a list of associated style sheets and their urls.
  nsTArray<nsString> mStyleSheetURLs;
  nsTArray<nsRefPtr<nsCSSStyleSheet> > mStyleSheets;
  
  // an array for holding default style settings
  nsTArray<PropItem*> mDefaultStyles;

   // for real-time spelling
   nsCOMPtr<nsITextServicesDocument> mTextServices;

protected:

  /* ANONYMOUS UTILS */
  void     RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                      nsIDOMEventListener* aListener,
                                      bool aUseCapture,
                                      nsIDOMElement* aElement,
                                      nsIContent* aParentContent,
                                      nsIPresShell* aShell);
  void     DeleteRefToAnonymousNode(nsIDOMElement* aElement,
                                    nsIContent * aParentContent,
                                    nsIPresShell* aShell);

  nsresult ShowResizersInner(nsIDOMElement *aResizedElement);

  // Returns the offset of an element's frame to its absolute containing block.
  nsresult GetElementOrigin(nsIDOMElement * aElement, PRInt32 & aX, PRInt32 & aY);
  nsresult GetPositionAndDimensions(nsIDOMElement * aElement,
                                    PRInt32 & aX, PRInt32 & aY,
                                    PRInt32 & aW, PRInt32 & aH,
                                    PRInt32 & aBorderLeft,
                                    PRInt32 & aBorderTop,
                                    PRInt32 & aMarginLeft,
                                    PRInt32 & aMarginTop);

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

  nsCOMPtr<nsIDOMElement> mTopLeftHandle;
  nsCOMPtr<nsIDOMElement> mTopHandle;
  nsCOMPtr<nsIDOMElement> mTopRightHandle;
  nsCOMPtr<nsIDOMElement> mLeftHandle;
  nsCOMPtr<nsIDOMElement> mRightHandle;
  nsCOMPtr<nsIDOMElement> mBottomLeftHandle;
  nsCOMPtr<nsIDOMElement> mBottomHandle;
  nsCOMPtr<nsIDOMElement> mBottomRightHandle;

  nsCOMPtr<nsIDOMElement> mActivatedHandle;

  nsCOMPtr<nsIDOMElement> mResizingShadow;
  nsCOMPtr<nsIDOMElement> mResizingInfo;

  nsCOMPtr<nsIDOMElement> mResizedObject;

  nsCOMPtr<nsIDOMEventListener>  mMouseMotionListenerP;
  nsCOMPtr<nsISelectionListener> mSelectionListenerP;
  nsCOMPtr<nsIDOMEventListener>  mResizeEventListenerP;

  nsCOMArray<nsIHTMLObjectResizeListener> objectResizeEventListeners;

  PRInt32 mOriginalX;
  PRInt32 mOriginalY;

  PRInt32 mResizedObjectX;
  PRInt32 mResizedObjectY;
  PRInt32 mResizedObjectWidth;
  PRInt32 mResizedObjectHeight;

  PRInt32 mResizedObjectMarginLeft;
  PRInt32 mResizedObjectMarginTop;
  PRInt32 mResizedObjectBorderLeft;
  PRInt32 mResizedObjectBorderTop;

  PRInt32 mXIncrementFactor;
  PRInt32 mYIncrementFactor;
  PRInt32 mWidthIncrementFactor;
  PRInt32 mHeightIncrementFactor;

  PRInt8  mInfoXIncrement;
  PRInt8  mInfoYIncrement;

  nsresult SetAllResizersPosition();

  nsresult CreateResizer(nsIDOMElement ** aReturn, PRInt16 aLocation, nsIDOMNode * aParentNode);
  void     SetAnonymousElementPosition(PRInt32 aX, PRInt32 aY, nsIDOMElement *aResizer);

  nsresult CreateShadow(nsIDOMElement ** aReturn, nsIDOMNode * aParentNode,
                        nsIDOMElement * aOriginalObject);
  nsresult SetShadowPosition(nsIDOMElement * aShadow,
                             nsIDOMElement * aOriginalObject,
                             PRInt32 aOriginalObjectX,
                             PRInt32 aOriginalObjectY);

  nsresult CreateResizingInfo(nsIDOMElement ** aReturn, nsIDOMNode * aParentNode);
  nsresult SetResizingInfoPosition(PRInt32 aX, PRInt32 aY,
                                   PRInt32 aW, PRInt32 aH);

  PRInt32  GetNewResizingIncrement(PRInt32 aX, PRInt32 aY, PRInt32 aID);
  nsresult StartResizing(nsIDOMElement * aHandle);
  PRInt32  GetNewResizingX(PRInt32 aX, PRInt32 aY);
  PRInt32  GetNewResizingY(PRInt32 aX, PRInt32 aY);
  PRInt32  GetNewResizingWidth(PRInt32 aX, PRInt32 aY);
  PRInt32  GetNewResizingHeight(PRInt32 aX, PRInt32 aY);
  void     HideShadowAndInfo();
  void     SetFinalSize(PRInt32 aX, PRInt32 aY);
  void     DeleteRefToAnonymousNode(nsIDOMNode * aNode);
  void     SetResizeIncrements(PRInt32 aX, PRInt32 aY, PRInt32 aW, PRInt32 aH, bool aPreserveRatio);
  void     HideAnonymousEditingUIs();

  /* ABSOLUTE POSITIONING */

  PRInt32 mPositionedObjectX;
  PRInt32 mPositionedObjectY;
  PRInt32 mPositionedObjectWidth;
  PRInt32 mPositionedObjectHeight;

  PRInt32 mPositionedObjectMarginLeft;
  PRInt32 mPositionedObjectMarginTop;
  PRInt32 mPositionedObjectBorderLeft;
  PRInt32 mPositionedObjectBorderTop;

  nsCOMPtr<nsIDOMElement> mAbsolutelyPositionedObject;
  nsCOMPtr<nsIDOMElement> mGrabber;
  nsCOMPtr<nsIDOMElement> mPositioningShadow;

  PRInt32      mGridSize;

  nsresult CreateGrabber(nsIDOMNode * aParentNode, nsIDOMElement ** aReturn);
  nsresult StartMoving(nsIDOMElement * aHandle);
  nsresult SetFinalPosition(PRInt32 aX, PRInt32 aY);
  void     AddPositioningOffset(PRInt32 & aX, PRInt32 & aY);
  void     SnapToGrid(PRInt32 & newX, PRInt32 & newY);
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

};
#endif //nsHTMLEditor_h__

