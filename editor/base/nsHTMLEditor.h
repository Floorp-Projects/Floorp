/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsHTMLEditor_h__
#define nsHTMLEditor_h__

#include "nsCOMPtr.h"

#include "nsIHTMLEditor.h"
#include "nsITableEditor.h"
#include "nsIEditorMailSupport.h"
#include "nsIEditorStyleSheets.h"

#include "nsEditor.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsICSSLoader.h"
#include "nsICSSLoaderObserver.h"
#include "nsITableLayout.h"

#include "TypeInState.h"
#include "nsEditRules.h"
 
class nsIDOMKeyEvent;
class nsITransferable;
class nsIDOMEventReceiver;

/**
 * The HTML editor implementation.<br>
 * Use to edit HTML document represented as a DOM tree. 
 */
class nsHTMLEditor : public nsEditor,
                     public nsIHTMLEditor,
                     public nsIEditorMailSupport,
                     public nsITableEditor,
                     public nsIEditorStyleSheets,
                     public nsICSSLoaderObserver
                    
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
    kOpInsertQuotation     = 3009
  };


  // see nsIHTMLEditor for documentation

//Interfaces for addref and release and queryinterface
//NOTE macro used is for classes that inherit from 
// another class. Only the base class should use NS_DECL_ISUPPORTS
  NS_DECL_ISUPPORTS_INHERITED


           nsHTMLEditor();
  virtual  ~nsHTMLEditor();

  /* ------------ nsIHTMLEditor methods -------------- */

  NS_IMETHOD EditorKeyPress(nsIDOMKeyEvent* aKeyEvent);
  NS_IMETHOD TypedText(const nsString& aString, PRInt32 aAction);

  NS_IMETHOD GetDocumentIsEmpty(PRBool *aDocumentIsEmpty);
  NS_IMETHOD GetDocumentLength(PRInt32 *aCount);
  NS_IMETHOD SetMaxTextLength(PRInt32 aMaxTextLength);
  NS_IMETHOD GetMaxTextLength(PRInt32& aMaxTextLength);

  NS_IMETHOD SetInlineProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue);

  NS_IMETHOD GetInlineProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAny, PRBool &aAll);
  NS_IMETHOD GetInlinePropertyWithAttrValue(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAny, PRBool &aAll,
                             nsString *outValue);

  NS_IMETHOD RemoveAllInlineProperties();
  NS_IMETHOD RemoveInlineProperty(nsIAtom *aProperty, const nsString *aAttribute);
  NS_IMETHOD IncreaseFontSize();
  NS_IMETHOD DecreaseFontSize();

  NS_IMETHOD InsertBreak();
  NS_IMETHOD InsertText(const nsString& aStringToInsert);
  NS_IMETHOD InsertHTML(const nsString &aInputString);
  NS_IMETHOD InsertHTMLWithCharset(const nsString& aInputString,
                                   const nsString& aCharset);
  NS_IMETHOD RebuildDocumentFromSource(const nsString& aSourceString);
  NS_IMETHOD InsertElementAtSelection(nsIDOMElement* aElement, PRBool aDeleteSelection);
  
  NS_IMETHOD DeleteSelection(EDirection aAction);
  NS_IMETHOD DeleteSelectionAndCreateNode(const nsString& aTag, nsIDOMNode ** aNewNode);
  NS_IMETHOD SelectElement(nsIDOMElement* aElement);
  NS_IMETHOD SetCaretAfterElement(nsIDOMElement* aElement);
  NS_IMETHOD SetCaretToDocumentStart();

  NS_IMETHOD SetParagraphFormat(const nsString& aParagraphFormat);

  NS_IMETHOD GetParentBlockTags(nsStringArray *aTagList, PRBool aGetLists);

  NS_IMETHOD GetParagraphState(PRBool &aMixed, nsString &outFormat);
  NS_IMETHOD GetFontFaceState(PRBool &aMixed, nsString &outFace);
  NS_IMETHOD GetFontColorState(PRBool &aMixed, nsString &outColor);
  NS_IMETHOD GetBackgroundColorState(PRBool &aMixed, nsString &outColor);
  NS_IMETHOD GetListState(PRBool &aMixed, PRBool &aOL, PRBool &aUL, PRBool &aDL);
  NS_IMETHOD GetListItemState(PRBool &aMixed, PRBool &aLI, PRBool &aDT, PRBool &aDD);
  NS_IMETHOD GetAlignment(PRBool &aMixed, nsIHTMLEditor::EAlignment &aAlign);
  NS_IMETHOD GetIndentState(PRBool &aCanIndent, PRBool &aCanOutdent);

  NS_IMETHOD MakeOrChangeList(const nsString& aListType);
  NS_IMETHOD RemoveList(const nsString& aListType);
  NS_IMETHOD Indent(const nsString& aIndent);
  NS_IMETHOD Align(const nsString& aAlign);

  NS_IMETHOD GetElementOrParentByTagName(const nsString& aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn);
  NS_IMETHOD GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn);
  NS_IMETHOD CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn);
  NS_IMETHOD GetNextElementByTagName(nsIDOMElement *aCurrentElement, const nsString *aTagName, nsIDOMElement **aReturn);


  NS_IMETHOD InsertLinkAroundSelection(nsIDOMElement* aAnchorElement);

  /* ------------ nsIEditorIMESupport overrides -------------- */
  
  NS_IMETHOD SetCompositionString(const nsString& aCompositionString, nsIPrivateTextRangeList* aTextRangeList,nsTextEventReply* aReply);
  NS_IMETHOD GetReconversionString(nsReconversionEventReply* aReply);

  /* ------------ nsIEditorStyleSheets methods -------------- */

  NS_IMETHOD ApplyStyleSheet(const nsString& aURL, nsICSSStyleSheet **aStyleSheet);
  NS_IMETHOD ApplyOverrideStyleSheet(const nsString& aURL, nsICSSStyleSheet **aStyleSheet);
  /* Above 2 methods call this with appropriate aOverride value 
   * Not exposed to IDL interface 
  */
  nsresult   ApplyDocumentOrOverrideStyleSheet(const nsString& aURL, PRBool aOverride, nsICSSStyleSheet **aStyleSheet);
  NS_IMETHOD AddStyleSheet(nsICSSStyleSheet* aSheet);
  NS_IMETHOD RemoveStyleSheet(nsICSSStyleSheet* aSheet);
  NS_IMETHOD RemoveOverrideStyleSheet(nsICSSStyleSheet* aSheet);

  /* ------------ nsIEditorMailSupport methods -------------- */

  NS_IMETHOD GetBodyWrapWidth(PRInt32 *aWrapColumn);
  NS_IMETHOD SetBodyWrapWidth(PRInt32 aWrapColumn);
  NS_IMETHOD PasteAsQuotation(PRInt32 aSelectionType);
  NS_IMETHOD InsertAsQuotation(const nsString& aQuotedText, nsIDOMNode **aNodeInserted);
  NS_IMETHOD PasteAsCitedQuotation(const nsString& aCitation,
                                   PRInt32 aSelectionType);
  NS_IMETHOD InsertAsCitedQuotation(const nsString& aQuotedText,
                                    const nsString& aCitation,
                                    PRBool aInsertHTML,
                                    const nsString& aCharset,
                                    nsIDOMNode **aNodeInserted);
  NS_IMETHOD GetEmbeddedObjects(nsISupportsArray** aNodeList);


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
  NS_IMETHOD GetCellIndexes(nsIDOMElement *aCell, PRInt32& aRowIndex, PRInt32& aColIndex);
  NS_IMETHOD GetTableSize(nsIDOMElement *aTable, PRInt32& aRowCount, PRInt32& aColCount);
  NS_IMETHOD GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell);
  NS_IMETHOD GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell,
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex,
                           PRInt32& aRowSpan, PRInt32& aColSpan, 
                           PRInt32& aActualRowSpan, PRInt32& aActualColSpan, 
                           PRBool& aIsSelected);
  NS_IMETHOD GetFirstRow(nsIDOMElement* aTableElement, nsIDOMElement* &aRow);
  NS_IMETHOD GetNextRow(nsIDOMElement* aTableElement, nsIDOMElement* &aRow);
  NS_IMETHOD SetSelectionAfterTableEdit(nsIDOMElement* aTable, PRInt32 aRow, PRInt32 aCol, 
                                        PRInt32 aDirection, PRBool aSelected);
  NS_IMETHOD GetSelectedOrParentTableElement(nsIDOMElement* &aTableElement, nsString& aTagName, PRInt32 &aSelectedCount);
  NS_IMETHOD GetSelectedCellsType(nsIDOMElement *aElement, PRUint32 &aSelectionType);

  nsresult GetCellFromRange(nsIDOMRange *aRange, nsIDOMElement **aCell);

  // Finds the first selected cell in first range of selection
  // This is in the *order of selection*, not order in the table
  // (i.e., each cell added to selection is added in another range 
  //  in the selection's rangelist, independent of location in table)
  // aRange is optional: returns the range around the cell
  NS_IMETHOD GetFirstSelectedCell(nsIDOMElement **aCell, nsIDOMRange **aRange);
  // Get next cell until no more are found. Always use GetFirstSelected cell first
  // aRange is optional: returns the range around the cell
  NS_IMETHOD GetNextSelectedCell(nsIDOMElement **aCell, nsIDOMRange **aRange);

  // Upper-left-most selected cell in table
  NS_IMETHOD GetFirstSelectedCellInTable(nsIDOMElement **aCell, PRInt32 *aRowIndex, PRInt32 *aColIndex);
    
  /* miscellaneous */
  // This sets background on the appropriate container element (table, cell,)
  //   or calls into nsTextEditor to set the page background
  NS_IMETHOD SetBackgroundColor(const nsString& aColor);
  NS_IMETHOD SetBodyAttribute(const nsString& aAttr, const nsString& aValue);

  /* ------------ Overrides of nsEditor interface methods -------------- */

  /** prepare the editor for use */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell,  nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags);
  
  NS_IMETHOD SetDocumentCharacterSet(const PRUnichar* characterSet);

  /** we override this here to install event listeners */
  NS_IMETHOD PostCreate();

  NS_IMETHOD GetFlags(PRUint32 *aFlags);
  NS_IMETHOD SetFlags(PRUint32 aFlags);

  NS_IMETHOD Undo(PRUint32 aCount);
  NS_IMETHOD Redo(PRUint32 aCount);

  NS_IMETHOD Cut();
  NS_IMETHOD CanCut(PRBool &aCanCut);
  NS_IMETHOD Copy();
  NS_IMETHOD CanCopy(PRBool &aCanCopy);
  NS_IMETHOD Paste(PRInt32 aSelectionType);
  NS_IMETHOD CanPaste(PRInt32 aSelectionType, PRBool &aCanPaste);

  NS_IMETHOD CanDrag(nsIDOMEvent *aDragEvent, PRBool &aCanDrag);
  NS_IMETHOD DoDrag(nsIDOMEvent *aDragEvent);
  NS_IMETHOD InsertFromDrop(nsIDOMEvent* aDropEvent);

  NS_IMETHOD OutputToString(nsAWritableString& aOutputString,
                            const nsAReadableString& aFormatType,
                            PRUint32 aFlags);
                            
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsString& aFormatType,
                            const nsString* aCharsetOverride,
                            PRUint32 aFlags);

  NS_IMETHOD GetHeadContentsAsHTML(nsString& aOutputString);
  NS_IMETHOD ReplaceHeadContentsWithHTML(const nsString &aSourceToInsert);

  NS_IMETHOD DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed);

  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection);

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation();

  /** returns PR_TRUE if aParentTag can contain a child of type aChildTag */
  virtual PRBool TagCanContainTag(const nsString &aParentTag, const nsString &aChildTag);
  
  /** make the given selection span the entire document */
  NS_IMETHOD SelectEntireDocument(nsIDOMSelection *aSelection);

  /** join together any afjacent editable text nodes in the range */
  NS_IMETHOD CollapseAdjacentTextNodes(nsIDOMRange *aInRange);

  /* ------------ nsICSSLoaderObserver -------------- */
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet*aSheet, PRBool aNotify);

  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD GetBodyStyleContext(nsIStyleContext** aStyleContext);

  /** returns the absolute position of the end points of aSelection
    * in the document as a text stream.
    */
  nsresult GetTextSelectionOffsets(nsIDOMSelection *aSelection,
                                   PRInt32 &aStartOffset, 
                                   PRInt32 &aEndOffset);

  nsresult GetAbsoluteOffsetsForPoints(nsIDOMNode *aInStartNode,
                                       PRInt32 aInStartOffset,
                                       nsIDOMNode *aInEndNode,
                                       PRInt32 aInEndOffset,
                                       nsIDOMNode *aInCommonParentNode,
                                       PRInt32 &aOutStartOffset, 
                                       PRInt32 &aEndOffset);
  
  // Use this to assure that selection is set after attribute nodes when 
  //  trying to collapse selection at begining of a block node
  //  e.g., when setting at beginning of a table cell
  // This will stop at a table, however, since we don't want to
  //  "drill down" into nested tables.
  // aSelection is optional -- if null, we get current seletion
  nsresult CollapseSelectionToDeepestNonTableFirstChild(nsIDOMSelection *aSelection, nsIDOMNode *aNode);

  nsresult IsEmptyNode(nsIDOMNode *aNode, PRBool *outIsEmptyBlock, 
                       PRBool aMozBRDoesntCount = PR_FALSE,
                       PRBool aListOrCellNotEmpty = PR_FALSE,
                       PRBool aSafeToAskFrames = PR_FALSE);

protected:

  NS_IMETHOD  InitRules();

  /** install the event listeners for the editor 
    * used to be part of Init, but now broken out into a separate method
    * called by PostCreate, giving the caller the chance to interpose
    * their own listeners before we install our own backstops.
    */
  NS_IMETHOD InstallEventListeners();

  /** returns the layout object (nsIFrame in the real world) for aNode
    * @param aNode          the content to get a frame for
    * @param aLayoutObject  the "primary frame" for aNode, if one exists.  May be null
    * @return NS_OK whether a frame is found or not
    *         an error if some serious error occurs
    */
  NS_IMETHOD GetLayoutObject(nsIDOMNode *aInNode, nsISupports **aOutLayoutObject);

  NS_IMETHOD DeleteSelectionAndPrepareToCreateNode(nsCOMPtr<nsIDOMNode> &parentSelectedNode, PRInt32& offsetOfNewNode);

  /* StyleSheet load callback */
  static void ApplyStyleSheetToPresShellDocument(nsICSSStyleSheet* aSheet, void *aData);

  /* remove the old style sheet, and apply the supplied one */
  NS_IMETHOD ReplaceStyleSheet(nsICSSStyleSheet *aNewSheet);


  // Return TRUE if aElement is a table-related elemet and caret was set
  PRBool SetCaretInTableCell(nsIDOMElement* aElement);
  PRBool IsElementInBody(nsIDOMElement* aElement);

  // inline style caching
  void CacheInlineStyles(nsIDOMNode *aNode);
  void ClearInlineStylesCache();
  
  // key event helpers
  NS_IMETHOD TabInTable(PRBool inIsShift, PRBool *outHandled);
  NS_IMETHOD CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, 
                      nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect = eNone);
  NS_IMETHOD CreateBRImpl(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                         PRInt32 *aInOutOffset, 
                         nsCOMPtr<nsIDOMNode> *outBRNode, 
                         EDirection aSelect);
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

  NS_IMETHOD DeleteTable2(nsIDOMElement *aTable, nsIDOMSelection *aSelection);
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
  NS_IMETHOD GetCellContext(nsIDOMSelection **aSelection,
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

  // Reduce rowspan/colspan when cells span into non-existent rows/columns
  NS_IMETHOD FixBadRowSpan(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32& aNewRowCount);
  NS_IMETHOD FixBadColSpan(nsIDOMElement *aTable, PRInt32 aColIndex, PRInt32& aNewColCount);

  // Fallback method: Call this after using ClearSelection() and you
  //  failed to set selection to some other content in the document
  NS_IMETHOD SetSelectionAtDocumentStart(nsIDOMSelection *aSelection);

// End of Table Editing utilities
  
  NS_IMETHOD IsRootTag(nsString &aTag, PRBool &aIsTag);

  NS_IMETHOD IsSubordinateBlock(nsString &aTag, PRBool &aIsTag);

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
  virtual void IsTextPropertySetByContent(nsIDOMNode     *aNode,
                                          nsIAtom        *aProperty, 
                                          const nsString *aAttribute,
                                          const nsString *aValue,
                                          PRBool         &aIsSet,
                                          nsIDOMNode    **aStyleNode,
                                          nsString       *outValue = nsnull) const;

  /** style-based query returns PR_TRUE if (aProperty, aAttribute) is set in aSC.
    * WARNING: not well tested yet since we don't do style-based queries anywhere.
    */
  virtual void IsTextStyleSet(nsIStyleContext *aSC, 
                              nsIAtom         *aProperty, 
                              const nsString  *aAttributes, 
                              PRBool          &aIsSet) const;


  void ResetTextSelectionForRange(nsIDOMNode *aParent,
                                  PRInt32     aStartOffset,
                                  PRInt32     aEndOffset,
                                  nsIDOMSelection *aSelection);


  // Methods for handling plaintext quotations
  NS_IMETHOD PasteAsPlaintextQuotation(PRInt32 aSelectionType);
  NS_IMETHOD InsertAsPlaintextQuotation(const nsString& aQuotedText,
                                        nsIDOMNode **aNodeInserted);

  // factored methods for handling insertion of data from transferables (drag&drop or clipboard)
  NS_IMETHOD PrepareTransferable(nsITransferable **transferable);
  NS_IMETHOD InsertFromTransferable(nsITransferable *transferable);
  
  /** simple utility to handle any error with event listener allocation or registration */
  void HandleEventListenerError();

  /* small utility routine to test the eEditorReadonly bit */
  PRBool IsModifiable();

  /* helpers for block transformations */
  nsresult MakeDefinitionItem(const nsString& aItemType);
  nsresult InsertBasicBlock(const nsString& aBlockType);
  
  /* increase/decrease the font size of selection */
  nsresult RelativeFontChange( PRInt32 aSizeChange);
  
  /* helper routines for font size changing */
  nsresult RelativeFontChangeOnTextNode( PRInt32 aSizeChange, 
                                         nsIDOMCharacterData *aTextNode, 
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset);
  nsresult RelativeFontChangeOnNode( PRInt32 aSizeChange, 
                                     nsIDOMNode *aNode);

  /* helper routines for inline style */
  nsresult SetInlinePropertyOnTextNode( nsIDOMCharacterData *aTextNode, 
                                        PRInt32 aStartOffset,
                                        PRInt32 aEndOffset,
                                        nsIAtom *aProperty, 
                                        const nsString *aAttribute,
                                        const nsString *aValue);
  nsresult SetInlinePropertyOnNode( nsIDOMNode *aNode,
                                    nsIAtom *aProperty, 
                                    const nsString *aAttribute,
                                    const nsString *aValue);

  nsresult PromoteInlineRange(nsIDOMRange *inRange);
  nsresult SplitStyleAboveRange(nsIDOMRange *aRange, 
                                nsIAtom *aProperty, 
                                const nsString *aAttribute);
  nsresult SplitStyleAbovePoint(nsCOMPtr<nsIDOMNode> *aNode,
                                PRInt32 *aOffset,
                                nsIAtom *aProperty, 
                                const nsString *aAttribute,
                                nsCOMPtr<nsIDOMNode> *outLeftNode = nsnull,
                                nsCOMPtr<nsIDOMNode> *outRightNode = nsnull);
  nsresult RemoveStyleInside(nsIDOMNode *aNode, 
                             nsIAtom *aProperty, 
                             const nsString *aAttribute, 
                             PRBool aChildrenOnly = PR_FALSE);
  nsresult RemoveInlinePropertyImpl(nsIAtom *aProperty, const nsString *aAttribute);
  
  PRBool NodeIsProperty(nsIDOMNode *aNode);
  PRBool HasAttr(nsIDOMNode *aNode, const nsString *aAttribute);
  PRBool HasAttrVal(nsIDOMNode *aNode, const nsString *aAttribute, const nsString *aValue);
  PRBool IsAtFrontOfNode(nsIDOMNode *aNode, PRInt32 aOffset);
  PRBool IsAtEndOfNode(nsIDOMNode *aNode, PRInt32 aOffset);
  PRBool IsOnlyAttribute(nsIDOMNode *aElement, const nsString *aAttribute);
  PRBool HasMatchingAttributes(nsIDOMNode *aNode1, 
                               nsIDOMNode *aNode2);

  nsresult GetPriorHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetPriorHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetPriorHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetPriorHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode);
  nsresult GetNextHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode);

  nsresult IsFirstEditableChild( nsIDOMNode *aNode, PRBool *aOutIsFirst);
  nsresult IsLastEditableChild( nsIDOMNode *aNode, PRBool *aOutIsLast);
  nsresult GetFirstEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstChild);
  nsresult GetLastEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastChild);
  nsresult GetFirstEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstLeaf);
  nsresult GetLastEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastLeaf);

  nsresult GetDOMEventReceiver(nsIDOMEventReceiver **aEventReceiver);

// Data members
protected:

  TypeInState*                  mTypeInState;
  nsCOMPtr<nsIEditRules>        mRules;
  nsCOMPtr<nsIDOMEventListener> mKeyListenerP;
  nsCOMPtr<nsIDOMEventListener> mMouseListenerP;
  nsCOMPtr<nsIDOMEventListener> mTextListenerP;
  nsCOMPtr<nsIDOMEventListener> mCompositionListenerP;
  nsCOMPtr<nsIDOMEventListener> mDragListenerP;
  nsCOMPtr<nsIDOMEventListener> mFocusListenerP;
  PRBool 	mIsComposing;
  PRInt32 mMaxTextLength;
  
  nsCOMPtr<nsIAtom> mBoldAtom;
  nsCOMPtr<nsIAtom> mItalicAtom;
  nsCOMPtr<nsIAtom> mUnderlineAtom;
  nsCOMPtr<nsIAtom> mFontAtom;
  nsCOMPtr<nsIAtom> mLinkAtom;
  nsCOMPtr<nsIDOMNode> mCachedNode;
  
  PRBool mCachedBoldStyle;
  PRBool mCachedItalicStyle;
  PRBool mCachedUnderlineStyle;
  nsString mCachedFontName;

  // Used by GetFirstSelectedCell and GetNextSelectedCell
  PRInt32  mSelectedCellIndex;

public:
  static nsIAtom *gTypingTxnName;
  static nsIAtom *gIMETxnName;
  static nsIAtom *gDeleteTxnName;

// friends
friend class nsHTMLEditRules;
friend class nsTextEditRules;

};

#endif //nsHTMLEditor_h__

