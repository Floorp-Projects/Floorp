/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef nsITableEditor_h__
#define nsITableEditor_h__

  // Wrapping includes can speed up compiles (see "Large Scale C++ Software Design")
#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif
#ifndef nsIDOMElement_h__
#include "nsIDOMElement.h"
	// for |nsIDOMElement| because an |nsCOMPtr<nsIDOMElement>&| is used, below
#endif
#ifndef nsIDOMRange_h__
#include "nsIDOMRange.h"
#endif


#define NS_ITABLEEDITOR_IID                   \
{ /* 4805e684-49b9-11d3-9ce4-ed60bd6cb5bc} */ \
0x4805e684, 0x49b9, 0x11d3,                   \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }

class nsITableEditor : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ITABLEEDITOR_IID; return iid; }

  /* ------------ Table editing Methods -------------- */

  /** Insert table methods
    * Insert relative to the selected cell or the 
    *  cell enclosing the selection anchor
    * The selection is collapsed and is left in the new cell
    *  at the same row,col location as the original anchor cell
    *
    * @param aNumber    Number of items to insert
    * @param aAfter     If TRUE, insert after the current cell,
    *                     else insert before current cell
    */
  NS_IMETHOD InsertTableCell(PRInt32 aNumber, PRBool aAfter)=0;
  NS_IMETHOD InsertTableColumn(PRInt32 aNumber, PRBool aAfter)=0;
  NS_IMETHOD InsertTableRow(PRInt32 aNumber, PRBool aAfter)=0;

  /** Delete table methods
    * Delete starting at the selected cell or the 
    *  cell (or table) enclosing the selection anchor
    * The selection is collapsed and is left in the 
    *  cell at the same row,col location as
    *  the previous selection anchor, if possible,
    *  else in the closest neigboring cell
    *
    * @param aNumber    Number of items to insert/delete
    */
  NS_IMETHOD DeleteTable()=0;

  /** Delete just the cell contents
    * This is what should happen when Delete key is used
    *   for selected cells, to minimize upsetting the table layout
    */
  NS_IMETHOD DeleteTableCellContents()=0;

  /** Delete cell elements as well as contents
    * @param aNumber   Number of contiguous cells, rows, or columns
    *
    * When there are more than 1 selected cells, aNumber is ignored.
    * For Delete Rows or Columns, the complete columns or rows are 
    *  determined by the selected cells. E.g., to delete 2 complete rows,
    *  user simply selects a cell in each, and they don't
    *  have to be contiguous.
    */
  NS_IMETHOD DeleteTableCell(PRInt32 aNumber)=0;
  NS_IMETHOD DeleteTableColumn(PRInt32 aNumber)=0;
  NS_IMETHOD DeleteTableRow(PRInt32 aNumber)=0;

  /** Table Selection methods
    * Selecting a row or column actually
    * selects all cells (not TR in the case of rows)
    */
  NS_IMETHOD SelectTableCell()=0;

  /** Select a rectangular block of cells:
    *  all cells falling within the row/column index of aStartCell
    *  to through the row/column index of the aEndCell
    *  aStartCell can be any location relative to aEndCell,
    *   as long as they are in the same table
    *  @param aStartCell  starting cell in block
    *  @param aEndCell    ending cell in block
    */
  NS_IMETHOD SelectBlockOfCells(nsIDOMElement *aStartCell, nsIDOMElement *aEndCell)=0;

  NS_IMETHOD SelectTableRow()=0;
  NS_IMETHOD SelectTableColumn()=0;
  NS_IMETHOD SelectTable()=0;
  NS_IMETHOD SelectAllTableCells()=0;

  /** Create a new TD or TH element, the opposite type of the supplied aSourceCell
    *   1. Copy all attributes from aSourceCell to the new cell
    *   2. Move all contents of aSourceCell to the new cell
    *   3. Replace aSourceCell in the table with the new cell
    *
    *  @param aSourceCell   The cell to be replaced
    *  @param aNewCell      The new cell that replaces aSourceCell
    */
  NS_IMETHOD SwitchTableCellHeaderType(nsIDOMElement *aSourceCell, nsIDOMElement **aNewCell)=0;

  /** Merges contents of all selected cells
    * for selected cells that are adjacent,
    * this will result in a larger cell with appropriate 
    * rowspan and colspan, and original cells are deleted
    * The resulting cell is in the location of the 
    *   cell at the upper-left corner of the adjacent
    *   block of selected cells
    *
    * @param aMergeNonContiguousContents:  
    *       If true: 
    *         Non-contiguous cells are not deleted,
    *         but their contents are still moved 
    *         to the upper-left cell
    *       If false: contiguous cells are ignored
    *
    * If there are no selected cells,
    *   and selection or caret is in a cell,
    *   that cell and the one to the right 
    *   are merged
    */
  NS_IMETHOD JoinTableCells(PRBool aMergeNonContiguousContents)=0;

  /** Split a cell that has rowspan and/or colspan > 0
    *   into cells such that all new cells have 
    *   rowspan = 1 and colspan = 1
    *  All of the contents are not touched --
    *   they will appear to be in the upper-left cell 
    */
  NS_IMETHOD SplitTableCell()=0;

  /** Scan through all rows and add cells as needed so 
    *   all locations in the cellmap are occupied.
    *   Used after inserting single cells or pasting
    *   a collection of cells that extend past the
    *   previous size of the table
    * If aTable is null, it uses table enclosing the selection anchor
    * This doesn't doesn't change the selection,
    *   thus it can be used to fixup all tables
    *   in a page independant of the selection
    */
  NS_IMETHOD NormalizeTable(nsIDOMElement *aTable)=0;

  /** Get the row an column index from the layout's cellmap
    * If aTable is null, it will try to find enclosing table of selection ancho
    * 
    */
  NS_IMETHOD GetCellIndexes(nsIDOMElement *aCell, PRInt32& aRowIndex, PRInt32& aColIndex)=0;

  /** Get the number of rows and columns in a table from the layout's cellmap
    * If aTable is null, it will try to find enclosing table of selection ancho
    * Note that all rows in table will not have this many because of 
    * ROWSPAN effects or if table is not "rectangular" (has short rows)
    */
  NS_IMETHOD GetTableSize(nsIDOMElement *aTable, PRInt32& aRowCount, PRInt32& aColCount)=0;

  /** Get a cell element at cellmap grid coordinates
    * A cell that spans across multiple cellmap locations will
    *   be returned multiple times, once for each location it occupies
    *
    * @param aTable                   A table in the document
    * @param aRowIndex, aColIndex     The 0-based cellmap indexes
    *
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an element is not found (passes NS_SUCCEEDED macro)
    *   You can scan for all cells in a row or column
    *   by iterating through the appropriate indexes
    *   until the returned aCell is null
    */
  NS_IMETHOD GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement **aCell)=0;

  /** Get a cell at cellmap grid coordinates and associated data
    * A cell that spans across multiple cellmap locations will
    *   be returned multiple times, once for each location it occupies
    * Examine the returned aStartRowIndex and aStartColIndex to see 
    *   if it is in the same layout column or layout row:
    *   A "layout row" is all cells sharing the same top edge
    *   A "layout column" is all cells sharing the same left edge
    *   This is important to determine what to do when inserting or deleting a column or row
    * 
    *  @param aTable                   A table in the document
    *  @param aRowIndex, aColIndex     The 0-based cellmap indexes
    * returns values:
    *  @param aCell                    The cell at this cellmap location
    *  @param aStartRowIndex           The row index where cell starts
    *  @param aStartColIndex           The col index where cell starts
    *  @param aRowSpan                 May be 0 (to span down entire table) or number of cells spanned
    *  @param aColSpan                 May be 0 (to span across entire table) or number of cells spanned
    *  @param aActualRowSpan           The actual number of cellmap locations (rows) spanned by the cell
    *  @param aActualColSpan           The actual number of cellmap locations (columns) spanned by the cell
    *  @param aIsSelected
    *  @param 
    *
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if a cell is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement **aCell,
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex,
                           PRInt32& aRowSpan, PRInt32& aColSpan, 
                           PRInt32& aActualRowSpan, PRInt32& aActualColSpan, 
                           PRBool& aIsSelected)=0;

  /** Get the first row element in a table
    *
    * @param aTableElement Any TABLE or child-of-table element in the document
    *
    * Returns:
    * @param aRowNode  The row found or null if there are no rows in table
    *              
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an row is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetFirstRow(nsIDOMElement* aTableElement, nsIDOMNode** aRowNode)=0;

  /** Get the next row element
    *
    * @param aCurrentRowNode The row to start search from
    *
    * Returns:
    * @param aRowNode   The row found or null if there isn't another row
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an row is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetNextRow(nsIDOMNode* aCurrentRowNode, nsIDOMNode** aRowNode)=0;
  
  /** Get the first cell element in a table row
    *
    * @param aRowNode  A table row node
    *
    * Returns:
    * @param aCellNode  The cell found or null if there are no cells in row
    *
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an row is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetFirstCellInRow(nsIDOMNode* aRowNode, nsIDOMNode** aCellNode)=0;

  /** Get the next cell node in a table row
    *
    * @param aCurrentCellNode The cell to start search from
    *
    * Returns:
    * @param aCellNode    The next cell found or null if there isn't another cell
    *                 
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an element is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetNextCellInRow(nsIDOMNode* aCurrentCellNode, nsIDOMNode** aCellNode)=0;

  /** Get the first cell element in a table row
    *
    * @param aRowNode  A table row node
    *
    * Returns:
    * @param aCellNode  The cell found or null if there are no cells in row
    *
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an row is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetLastCellInRow(nsIDOMNode* aRowNode, nsIDOMNode** aCellNode)=0;

  /** Preferred direction to search for neighboring cell
    * when trying to locate a cell to place caret in after
    * a table editing action. 
    * Used for aDirection param in SetSelectionAfterTableEdit
    */
  enum { 
    eNoSearch, 
    ePreviousColumn, 
    ePreviousRow 
  };
  /** Reset a selected cell or collapsed selection (the caret) after table editing
    *
    * @param aTable      A table in the document
    * @param aRow        The row ...
    * @param aCol        ... and column defining the cell
    *                    where we will try to place the caret
    * @param aSelected   If true, we select the whole cell instead of setting caret
    * @param aDirection  If cell at (aCol, aRow) is not found,
    *                    search for previous cell in the same
    *                    column (aPreviousColumn) or row (ePreviousRow)
    *                    or don't search for another cell (aNoSearch)
    *                    If no cell is found, caret is place just before table;
    *                    and if that fails, at beginning of document.
    *                    Thus we generally don't worry about the return value
    *                     and can use the nsSetSelectionAfterTableEdit stack-based 
    *                     object to insure we reset the caret in a table-editing method.
    */
  NS_IMETHOD SetSelectionAfterTableEdit(nsIDOMElement* aTable, PRInt32 aRow, PRInt32 aCol, 
                                        PRInt32 aDirection, PRBool aSelected)=0;

  /** Examine the current selection and find
    *   a selected TABLE, TD or TH, or TR element.
    *   or return the parent TD or TH if selection is inside a table cell
    *   Returns null if no table element is found.
    *
    * @param aTableElement      The table element (table, row, or cell) returned
    * @param aTagName           The tagname of returned element
    *                           Note that "td" will be returned if name is actually "th"
    * @param aSelectedCount     How many table elements were selected
    *                           This tells us if we have multiple cells selected
    *                             (0 if element is a parent cell of selection)
    *
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an element is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetSelectedOrParentTableElement(nsIDOMElement* &aTableElement, nsString& aTagName, PRInt32 &aSelectedCount)=0;

  /** Generally used after GetSelectedOrParentTableElement
    *   to test if selected cells are complete rows or columns
    * 
    * @param aElement           Any table or cell element or any element inside a table
    *                           Used to get enclosing table. 
    *                           If null, selection's anchorNode is used
    * 
    * @param aSelectionType     Returns:
    *                             0                        aCellElement was not a cell (returned result = NS_ERROR_FAILURE)
    *                             TABLESELECTION_CELL      There are 1 or more cells selected
    *                                                        but complete rows or columns are not selected
    *                             TABLESELECTION_ROW       All cells are in 1 or more rows
    *                                                       and in each row, all cells selected
    *                                                       Note: This is the value if all rows (thus all cells) are selected
    *                             TABLESELECTION_COLUMN    All cells are in 1 or more columns
    *                                                       and in each column, all cells are selected
    */
  NS_IMETHOD GetSelectedCellsType(nsIDOMElement *aElement, PRUint32 &aSelectionType)=0;

  /** Get first selected element from first selection range.
    * Assumes cell-selection model where each cell
    * is in a separate range (selection parent node is table row)
    * @param aCell     Selected cell or null if ranges don't contain cell selections
    * @param aRange    Optional: if not null, return the selection range 
    *                     associated with the cell
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an element is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetFirstSelectedCell(nsIDOMElement **aCell, nsIDOMRange **aRange)=0;
  
  /** Get first selected element from first selection range.
    * Assumes cell-selection model where each cell
    * is in a separate range (selection parent node is table row)
    * @param aCell       Selected cell or null if ranges don't contain cell selections
    * @param aRowIndex   Optional: if not null, return the row index of first cell
    * @param aColIndex   Optional: if not null, return the column index of first cell
    *
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an element is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetFirstSelectedCellInTable(nsIDOMElement **aCell, PRInt32 *aRowIndex, PRInt32 *aColIndex)=0;

  /** Get next selected cell element from first selection range.
    * Assumes cell-selection model where each cell
    * is in a separate range (selection parent node is table row)
    * Always call GetFirstSelectedCell() to initialize stored index of "next" cell
    * @param aCell     Selected cell or null if no more selected cells
    *                     or ranges don't contain cell selections
    * @param aRange    Optional: if not null, return the selection range 
    *                     associated with the cell
    */
  NS_IMETHOD GetNextSelectedCell(nsIDOMElement **aCell, nsIDOMRange **aRange)=0;
};

#endif // nsITableEditor_h__
