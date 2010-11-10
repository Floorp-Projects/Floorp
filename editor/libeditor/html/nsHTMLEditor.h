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
 *   Kathleen Brade <brade@netscape.com>
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

class nsIDOMKeyEvent;
class nsITransferable;
class nsIDOMNSRange;
class nsIDocumentEncoder;
class nsIClipboard;
class TypeInState;
class nsIContentFilter;
class nsIURL;
class nsIRangeUtils;
class nsILinkHandler;
struct PropItem;

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

  enum OperationID
  {
    kOpInsertBreak         = 3000,
    kOpMakeList            = 3001,
    kOpIndent              = 3002,
    kOpOutdent             = 3003,
    kOpAlign               = 3004,
    kOpMakeBasicBlock      = 3005,
    kOpRemoveList          = 3006,
    kOpMakeDefListItem     = 3007,
    kOpInsertElement       = 3008,
    kOpInsertQuotation     = 3009,
    kOpSetTextProperty     = 3010,
    kOpRemoveTextProperty  = 3011,
    kOpHTMLPaste           = 3012,
    kOpLoadHTML            = 3013,
    kOpResetTextProperties = 3014,
    kOpSetAbsolutePosition = 3015,
    kOpRemoveAbsolutePosition = 3016,
    kOpDecreaseZIndex      = 3017,
    kOpIncreaseZIndex      = 3018
  };

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


           nsHTMLEditor();
  virtual  ~nsHTMLEditor();

  /* ------------ nsPlaintextEditor overrides -------------- */
  NS_IMETHOD GetIsDocumentEditable(PRBool *aIsDocumentEditable);
  NS_IMETHODIMP BeginningOfDocument();
  virtual nsresult HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent);
  virtual already_AddRefed<nsIContent> GetFocusedContent();
  virtual PRBool IsActiveInDOMWindow();
  virtual already_AddRefed<nsPIDOMEventTarget> GetPIDOMEventTarget();
  virtual already_AddRefed<nsIContent> FindSelectionRoot(nsINode *aNode);
  virtual PRBool IsAcceptableInputEvent(nsIDOMEvent* aEvent);

  /* ------------ nsStubMutationObserver overrides --------- */
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  /* ------------ nsIEditorIMESupport overrides ------------ */
  NS_IMETHOD GetPreferredIMEState(PRUint32 *aState);

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

  nsresult GetCSSBackgroundColorState(PRBool *aMixed, nsAString &aOutColor,
                                      PRBool aBlockLevel);
  NS_IMETHOD GetHTMLBackgroundColorState(PRBool *aMixed, nsAString &outColor);

  /* ------------ nsIEditorStyleSheets methods -------------- */

  NS_IMETHOD AddStyleSheet(const nsAString & aURL);
  NS_IMETHOD ReplaceStyleSheet(const nsAString& aURL);
  NS_IMETHOD RemoveStyleSheet(const nsAString &aURL);

  NS_IMETHOD AddOverrideStyleSheet(const nsAString & aURL);
  NS_IMETHOD ReplaceOverrideStyleSheet(const nsAString& aURL);
  NS_IMETHOD RemoveOverrideStyleSheet(const nsAString &aURL);

  NS_IMETHOD EnableStyleSheet(const nsAString& aURL, PRBool aEnable);

  /* ------------ nsIEditorMailSupport methods -------------- */

  NS_DECL_NSIEDITORMAILSUPPORT

  /* ------------ nsITableEditor methods -------------- */

  NS_IMETHOD InsertTableCell(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD InsertTableColumn(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD InsertTableRow(PRInt32 aNumber, PRBool aAfter);
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
  NS_IMETHOD JoinTableCells(PRBool aMergeNonContiguousContents);
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
                           PRBool* aIsSelected);
  NS_IMETHOD GetFirstRow(nsIDOMElement* aTableElement, nsIDOMNode** aRowNode);
  NS_IMETHOD GetNextRow(nsIDOMNode* aCurrentRowNode, nsIDOMNode** aRowNode);
  NS_IMETHOD GetLastCellInRow(nsIDOMNode* aRowNode, nsIDOMNode** aCellNode);

  NS_IMETHOD SetSelectionAfterTableEdit(nsIDOMElement* aTable, PRInt32 aRow, PRInt32 aCol, 
                                        PRInt32 aDirection, PRBool aSelected);
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
  static nsCOMPtr<nsIDOMNode> GetBlockNodeParent(nsIDOMNode *aNode);
  /** Determines the bounding nodes for the block section containing aNode.
    * The calculation is based on some nodes intrinsically being block elements
    * acording to HTML.  Style sheets are not considered in this calculation.
    * <BR> tags separate block content sections.  So the HTML markup:
    * <PRE>
    *      <P>text1<BR>text2<B>text3</B></P>
    * </PRE>
    * contains two block content sections.  The first has the text node "text1"
    * for both endpoints.  The second has "text2" as the left endpoint and
    * "text3" as the right endpoint.
    * Notice that offsets aren't required, only leaf nodes.  Offsets are implicit.
    *
    * @param aNode      the block content returned includes aNode
    * @param aLeftNode  [OUT] the left endpoint of the block content containing aNode
    * @param aRightNode [OUT] the right endpoint of the block content containing aNode
    *
    */
  static nsresult GetBlockSection(nsIDOMNode  *aNode,
                                  nsIDOMNode **aLeftNode, 
                                  nsIDOMNode **aRightNode);

  /** Compute the set of block sections in a given range.
    * A block section is the set of (leftNode, rightNode) pairs given
    * by GetBlockSection.  The set is computed by computing the 
    * block section for every leaf node in the range and throwing 
    * out duplicates.
    *
    * @param aRange     The range to compute block sections for.
    * @param aSections  Allocated storage for the resulting set, stored as nsIDOMRanges.
    */
  static nsresult GetBlockSectionsForRange(nsIDOMRange      *aRange, 
                                           nsCOMArray<nsIDOMRange>& aSections);

  static nsCOMPtr<nsIDOMNode> NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir);
  nsresult IsNextCharWhitespace(nsIDOMNode *aParentNode, 
                                PRInt32 aOffset, 
                                PRBool *outIsSpace, 
                                PRBool *outIsNBSP,
                                nsCOMPtr<nsIDOMNode> *outNode = 0,
                                PRInt32 *outOffset = 0);
  nsresult IsPrevCharWhitespace(nsIDOMNode *aParentNode, 
                                PRInt32 aOffset, 
                                PRBool *outIsSpace, 
                                PRBool *outIsNBSP,
                                nsCOMPtr<nsIDOMNode> *outNode = 0,
                                PRInt32 *outOffset = 0);

  /* ------------ Overrides of nsEditor interface methods -------------- */

  nsresult EndUpdateViewBatch();

  /** prepare the editor for use */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell,  nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags);
  NS_IMETHOD PreDestroy(PRBool aDestroyingFrames);

  /** Internal, static version */
  static nsresult NodeIsBlockStatic(nsIDOMNode *aNode, PRBool *aIsBlock);

  NS_IMETHOD SetFlags(PRUint32 aFlags);

  NS_IMETHOD Paste(PRInt32 aSelectionType);
  NS_IMETHOD CanPaste(PRInt32 aSelectionType, PRBool *aCanPaste);

  NS_IMETHOD PasteTransferable(nsITransferable *aTransferable);
  NS_IMETHOD CanPasteTransferable(nsITransferable *aTransferable, PRBool *aCanPaste);

  NS_IMETHOD DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed);

  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection);

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation();

  /** returns PR_TRUE if aParentTag can contain a child of type aChildTag */
  virtual PRBool TagCanContainTag(const nsAString& aParentTag, const nsAString& aChildTag);
  
  /** returns PR_TRUE if aNode is a container */
  virtual PRBool IsContainer(nsIDOMNode *aNode);

  /** make the given selection span the entire document */
  NS_IMETHOD SelectEntireDocument(nsISelection *aSelection);

  NS_IMETHOD SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      const nsAString & aValue,
                                      PRBool aSuppressTransaction);
  NS_IMETHOD RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                         const nsAString & aAttribute,
                                         PRBool aSuppressTransaction);

  /** join together any afjacent editable text nodes in the range */
  NS_IMETHOD CollapseAdjacentTextNodes(nsIDOMRange *aInRange);

  virtual PRBool NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2);

  NS_IMETHODIMP DeleteNode(nsIDOMNode * aNode);
  NS_IMETHODIMP DeleteText(nsIDOMCharacterData *aTextNode,
                           PRUint32             aOffset,
                           PRUint32             aLength);
  NS_IMETHOD InsertTextImpl(const nsAString& aStringToInsert, 
                            nsCOMPtr<nsIDOMNode> *aInOutNode, 
                            PRInt32 *aInOutOffset,
                            nsIDOMDocument *aDoc);
  NS_IMETHOD_(PRBool) IsModifiableNode(nsIDOMNode *aNode);

  NS_IMETHOD SelectAll();

  NS_IMETHOD GetRootElement(nsIDOMElement **aRootElement);

  /* ------------ nsICSSLoaderObserver -------------- */
  NS_IMETHOD StyleSheetLoaded(nsCSSStyleSheet*aSheet, PRBool aWasAlternate,
                              nsresult aStatus);

  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD TypedText(const nsAString& aString, PRInt32 aAction);
  nsresult InsertNodeAtPoint( nsIDOMNode *aNode, 
                              nsCOMPtr<nsIDOMNode> *ioParent, 
                              PRInt32 *ioOffset, 
                              PRBool aNoEmptyNodes);
  nsCOMPtr<nsIDOMNode> FindUserSelectAllNode(nsIDOMNode *aNode);
                                

  /** returns the absolute position of the end points of aSelection
    * in the document as a text stream.
    */
  nsresult GetTextSelectionOffsets(nsISelection *aSelection,
                                   PRInt32 &aStartOffset, 
                                   PRInt32 &aEndOffset);

  // Use this to assure that selection is set after attribute nodes when 
  //  trying to collapse selection at begining of a block node
  //  e.g., when setting at beginning of a table cell
  // This will stop at a table, however, since we don't want to
  //  "drill down" into nested tables.
  // aSelection is optional -- if null, we get current seletion
  nsresult CollapseSelectionToDeepestNonTableFirstChild(nsISelection *aSelection, nsIDOMNode *aNode);

  virtual PRBool IsTextInDirtyFrameVisible(nsIDOMNode *aNode);

  nsresult IsVisTextNode( nsIDOMNode *aNode, 
                          PRBool *outIsEmptyNode, 
                          PRBool aSafeToAskFrames);
  nsresult IsEmptyNode(nsIDOMNode *aNode, PRBool *outIsEmptyBlock, 
                       PRBool aMozBRDoesntCount = PR_FALSE,
                       PRBool aListOrCellNotEmpty = PR_FALSE,
                       PRBool aSafeToAskFrames = PR_FALSE);
  nsresult IsEmptyNodeImpl(nsIDOMNode *aNode,
                           PRBool *outIsEmptyBlock, 
                           PRBool aMozBRDoesntCount,
                           PRBool aListOrCellNotEmpty,
                           PRBool aSafeToAskFrames,
                           PRBool *aSeenBR);

  // Returns TRUE if sheet was loaded, false if it wasn't
  PRBool   EnableExistingStyleSheet(const nsAString& aURL);

  // Dealing with the internal style sheet lists:
  NS_IMETHOD GetStyleSheetForURL(const nsAString &aURL,
                                 nsCSSStyleSheet **_retval);
  NS_IMETHOD GetURLForStyleSheet(nsCSSStyleSheet *aStyleSheet, nsAString &aURL);

  // Add a url + known style sheet to the internal lists:
  nsresult AddNewStyleSheetToList(const nsAString &aURL,
                                  nsCSSStyleSheet *aStyleSheet);

  nsresult RemoveStyleSheetFromList(const nsAString &aURL);
                       
protected:

  NS_IMETHOD  InitRules();

  // Create the event listeners for the editor to install
  virtual nsresult CreateEventListeners();

  virtual nsresult InstallEventListeners();
  virtual void RemoveEventListeners();

  PRBool ShouldReplaceRootElement();
  void ResetRootElementAndEventTarget();
  nsresult GetBodyElement(nsIDOMHTMLElement** aBody);
  // Get the focused node of this editor.
  // @return    If the editor has focus, this returns the focused node.
  //            Otherwise, returns null.
  already_AddRefed<nsINode> GetFocusedNode();

  // Return TRUE if aElement is a table-related elemet and caret was set
  PRBool SetCaretInTableCell(nsIDOMElement* aElement);
  PRBool IsElementInBody(nsIDOMElement* aElement);

  // key event helpers
  NS_IMETHOD TabInTable(PRBool inIsShift, PRBool *outHandled);
  NS_IMETHOD CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, 
                      nsCOMPtr<nsIDOMNode> *outBRNode, nsIEditor::EDirection aSelect = nsIEditor::eNone);
  NS_IMETHOD CreateBRImpl(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                         PRInt32 *aInOutOffset, 
                         nsCOMPtr<nsIDOMNode> *outBRNode, 
                         nsIEditor::EDirection aSelect);
  NS_IMETHOD InsertBR(nsCOMPtr<nsIDOMNode> *outBRNode);

// Table Editing (implemented in nsTableEditor.cpp)

  // Table utilities

  // Insert a new cell after or before supplied aCell. 
  //  Optional: If aNewCell supplied, returns the newly-created cell (addref'd, of course)
  // This doesn't change or use the current selection
  NS_IMETHOD InsertCell(nsIDOMElement *aCell, PRInt32 aRowSpan, PRInt32 aColSpan,
                        PRBool aAfter, PRBool aIsHeader, nsIDOMElement **aNewCell);

  // Helpers that don't touch the selection or do batch transactions
  NS_IMETHOD DeleteRow(nsIDOMElement *aTable, PRInt32 aRowIndex);
  NS_IMETHOD DeleteColumn(nsIDOMElement *aTable, PRInt32 aColIndex);
  NS_IMETHOD DeleteCellContents(nsIDOMElement *aCell);

  // Move all contents from aCellToMerge into aTargetCell (append at end)
  NS_IMETHOD MergeCells(nsCOMPtr<nsIDOMElement> aTargetCell, nsCOMPtr<nsIDOMElement> aCellToMerge, PRBool aDeleteCellToMerge);

  NS_IMETHOD DeleteTable2(nsIDOMElement *aTable, nsISelection *aSelection);
  NS_IMETHOD SetColSpan(nsIDOMElement *aCell, PRInt32 aColSpan);
  NS_IMETHOD SetRowSpan(nsIDOMElement *aCell, PRInt32 aRowSpan);

  // Helper used to get nsITableLayout interface for methods implemented in nsTableFrame
  NS_IMETHOD GetTableLayoutObject(nsIDOMElement* aTable, nsITableLayout **tableLayoutObject);
  // Needed to do appropriate deleting when last cell or row is about to be deleted
  // This doesn't count cells that don't start in the given row (are spanning from row above)
  PRInt32  GetNumberOfCellsInRow(nsIDOMElement* aTable, PRInt32 rowIndex);
  // Test if all cells in row or column at given index are selected
  PRBool AllCellsInRowSelected(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32 aNumberOfColumns);
  PRBool AllCellsInColumnSelected(nsIDOMElement *aTable, PRInt32 aColIndex, PRInt32 aNumberOfRows);

  PRBool   IsEmptyCell(nsIDOMElement *aCell);

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
  
  NS_IMETHOD IsRootTag(nsString &aTag, PRBool &aIsTag);

  virtual PRBool IsBlockNode(nsIDOMNode *aNode);
  
  static nsCOMPtr<nsIDOMNode> GetEnclosingTable(nsIDOMNode *aNode);

  /** content-based query returns PR_TRUE if <aProperty aAttribute=aValue> effects aNode
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
    * @param aIsSet     [OUT] PR_TRUE if <aProperty aAttribute=aValue> effects aNode.
    * @param aStyleNode [OUT] set to the node representing <aProperty aAttribute=aValue>, if found.
    *                   null if aIsSet is returned as PR_FALSE;
    */
  virtual void IsTextPropertySetByContent(nsIDOMNode        *aNode,
                                          nsIAtom           *aProperty, 
                                          const nsAString   *aAttribute,
                                          const nsAString   *aValue,
                                          PRBool            &aIsSet,
                                          nsIDOMNode       **aStyleNode,
                                          nsAString *outValue = nsnull);

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
                                        PRBool aAddCites,
                                        nsIDOMNode **aNodeInserted);

  // factored methods for handling insertion of data from transferables (drag&drop or clipboard)
  NS_IMETHOD PrepareTransferable(nsITransferable **transferable);
  NS_IMETHOD PrepareHTMLTransferable(nsITransferable **transferable, PRBool havePrivFlavor);
  nsresult   PutDragDataInTransferable(nsITransferable **aTransferable);
  NS_IMETHOD InsertFromTransferable(nsITransferable *transferable, 
                                    nsIDOMDocument *aSourceDoc,
                                    const nsAString & aContextStr,
                                    const nsAString & aInfoStr,
                                    nsIDOMNode *aDestinationNode,
                                    PRInt32 aDestinationOffset,
                                    PRBool aDoDeleteSelection);
  PRBool HavePrivateHTMLFlavor( nsIClipboard *clipboard );
  nsresult   ParseCFHTML(nsCString & aCfhtml, PRUnichar **aStuffToPaste, PRUnichar **aCfcontext);
  nsresult   DoContentFilterCallback(const nsAString &aFlavor,
                                     nsIDOMDocument *aSourceDoc,
                                     PRBool aWillDeleteSelection,
                                     nsIDOMNode **aFragmentAsNode,      
                                     nsIDOMNode **aFragStartNode,
                                     PRInt32 *aFragStartOffset,
                                     nsIDOMNode **aFragEndNode,
                                     PRInt32 *aFragEndOffset,
                                     nsIDOMNode **aTargetNode,       
                                     PRInt32 *aTargetOffset,   
                                     PRBool *aDoContinue);
  nsresult   RelativizeURIInFragmentList(const nsCOMArray<nsIDOMNode> &aNodeList,
                                        const nsAString &aFlavor,
                                        nsIDOMDocument *aSourceDoc,
                                        nsIDOMNode *aTargetNode);
  nsresult   RelativizeURIForNode(nsIDOMNode *aNode, nsIURL *aDestURL);
  nsresult   GetAttributeToModifyOnNode(nsIDOMNode *aNode, nsAString &aAttrib);

  PRBool     IsInLink(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *outLink = nsnull);
  nsresult   StripFormattingNodes(nsIDOMNode *aNode, PRBool aOnlyList = PR_FALSE);
  nsresult   CreateDOMFragmentFromPaste(const nsAString & aInputString,
                                        const nsAString & aContextStr,
                                        const nsAString & aInfoStr,
                                        nsCOMPtr<nsIDOMNode> *outFragNode,
                                        nsCOMPtr<nsIDOMNode> *outStartNode,
                                        nsCOMPtr<nsIDOMNode> *outEndNode,
                                        PRInt32 *outStartOffset,
                                        PRInt32 *outEndOffset,
                                        PRBool aTrustedInput);
  nsresult   ParseFragment(const nsAString & aStr, nsTArray<nsString> &aTagStack,
                           nsIDocument* aTargetDoc,
                           nsCOMPtr<nsIDOMNode> *outNode,
                           PRBool aTrustedInput);
  nsresult   CreateListOfNodesToPaste(nsIDOMNode  *aFragmentAsNode,
                                      nsCOMArray<nsIDOMNode>& outNodeList,
                                      nsIDOMNode *aStartNode,
                                      PRInt32 aStartOffset,
                                      nsIDOMNode *aEndNode,
                                      PRInt32 aEndOffset);
  nsresult CreateTagStack(nsTArray<nsString> &aTagStack,
                          nsIDOMNode *aNode);
  nsresult GetListAndTableParents( PRBool aEnd, 
                                   nsCOMArray<nsIDOMNode>& aListOfNodes,
                                   nsCOMArray<nsIDOMNode>& outArray);
  nsresult DiscoverPartialListsAndTables(nsCOMArray<nsIDOMNode>& aPasteNodes,
                                         nsCOMArray<nsIDOMNode>& aListsAndTables,
                                         PRInt32 *outHighWaterMark);
  nsresult ScanForListAndTableStructure(PRBool aEnd,
                                        nsCOMArray<nsIDOMNode>& aNodes,
                                        nsIDOMNode *aListOrTable,
                                        nsCOMPtr<nsIDOMNode> *outReplaceNode);
  nsresult ReplaceOrphanedStructure( PRBool aEnd,
                                     nsCOMArray<nsIDOMNode>& aNodeArray,
                                     nsCOMArray<nsIDOMNode>& aListAndTableArray,
                                     PRInt32 aHighWaterMark);
  nsIDOMNode* GetArrayEndpoint(PRBool aEnd, nsCOMArray<nsIDOMNode>& aNodeArray);

  /* small utility routine to test if a break node is visible to user */
  PRBool   IsVisBreak(nsIDOMNode *aNode);

  /* utility routine to possibly adjust the insertion position when 
     inserting a block level element */
  void NormalizeEOLInsertPosition(nsIDOMNode *firstNodeToInsert,
                                  nsCOMPtr<nsIDOMNode> *insertParentNode,
                                  PRInt32 *insertOffset);

  /* small utility routine to test the eEditorReadonly bit */
  PRBool IsModifiable();

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
  nsresult RelativeFontChangeOnNode( PRInt32 aSizeChange, 
                                     nsIDOMNode *aNode);
  nsresult RelativeFontChangeHelper( PRInt32 aSizeChange, 
                                     nsIDOMNode *aNode);

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
                             PRBool aChildrenOnly = PR_FALSE);
  nsresult RemoveInlinePropertyImpl(nsIAtom *aProperty, const nsAString *aAttribute);

  PRBool NodeIsProperty(nsIDOMNode *aNode);
  PRBool HasAttr(nsIDOMNode *aNode, const nsAString *aAttribute);
  PRBool HasAttrVal(nsIDOMNode *aNode, const nsAString *aAttribute, const nsAString *aValue);
  PRBool IsAtFrontOfNode(nsIDOMNode *aNode, PRInt32 aOffset);
  PRBool IsAtEndOfNode(nsIDOMNode *aNode, PRInt32 aOffset);
  PRBool IsOnlyAttribute(nsIDOMNode *aElement, const nsAString *aAttribute);

  nsresult RemoveBlockContainer(nsIDOMNode *inNode);
  nsresult GetPriorHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetPriorHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetPriorHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, PRBool bNoBlockCrossing = PR_FALSE);
  nsresult GetPriorHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode, PRBool bNoBlockCrossing = PR_FALSE);
  nsresult GetNextHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, PRBool bNoBlockCrossing = PR_FALSE);
  nsresult GetNextHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode, PRBool bNoBlockCrossing = PR_FALSE);

  nsresult IsFirstEditableChild( nsIDOMNode *aNode, PRBool *aOutIsFirst);
  nsresult IsLastEditableChild( nsIDOMNode *aNode, PRBool *aOutIsLast);
  nsresult GetFirstEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstChild);
  nsresult GetLastEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastChild);

  nsresult GetFirstEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstLeaf);
  nsresult GetLastEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastLeaf);

  //XXX Kludge: Used to suppress spurious drag/drop events (bug 50703)
  PRBool   mIgnoreSpuriousDragEvent;

  nsresult GetInlinePropertyBase(nsIAtom *aProperty, 
                             const nsAString *aAttribute,
                             const nsAString *aValue,
                             PRBool *aFirst, 
                             PRBool *aAny, 
                             PRBool *aAll,
                             nsAString *outValue,
                             PRBool aCheckDefaults = PR_TRUE);
  nsresult HasStyleOrIdOrClass(nsIDOMElement * aElement, PRBool *aHasStyleOrIdOrClass);
  nsresult RemoveElementIfNoStyleOrIdOrClass(nsIDOMElement * aElement, nsIAtom * aTag);

  // Whether the outer window of the DOM event target has focus or not.
  PRBool   OurWindowHasFocus();

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
                                   PRBool aDeleteSelection,
                                   PRBool aTrustedInput);

// Data members
protected:

  nsCOMArray<nsIContentFilter> mContentFilters;

  TypeInState*         mTypeInState;

  PRPackedBool mCRInParagraphCreatesParagraph;

  PRPackedBool mCSSAware;
  nsHTMLCSSUtils *mHTMLCSSUtils;

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

  // And a static range utils service
  static nsIRangeUtils* sRangeHelper;

public:
  // ... which means that we need to listen to shutdown
  static void Shutdown();

protected:

  /* ANONYMOUS UTILS */
  void     RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                      nsIDOMEventListener* aListener,
                                      PRBool aUseCapture,
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
  PRPackedBool mIsObjectResizingEnabled;
  PRPackedBool mIsResizing;
  PRPackedBool mPreserveRatio;
  PRPackedBool mResizedObjectIsAnImage;

  // absolute positioning
  PRPackedBool mIsAbsolutelyPositioningEnabled;
  PRPackedBool mResizedObjectIsAbsolutelyPositioned;

  PRPackedBool mGrabberClicked;
  PRPackedBool mIsMoving;

  PRPackedBool mSnapToGridEnabled;

  // inline table editing
  PRPackedBool mIsInlineTableEditingEnabled;

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
  void     SetResizeIncrements(PRInt32 aX, PRInt32 aY, PRInt32 aW, PRInt32 aH, PRBool aPreserveRatio);

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

};
#endif //nsHTMLEditor_h__

