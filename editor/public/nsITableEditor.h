/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL")=0; you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifndef nsITableEditor_h__
#define nsITableEditor_h__


#define NS_ITABLEEDITOR_IID                   \
{ /* 4805e684-49b9-11d3-9ce4-ed60bd6cb5bc} */ \
0x4805e684, 0x49b9, 0x11d3,                   \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }

class nsIDOMElement;

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
  NS_IMETHOD DeleteTableCell(PRInt32 aNumber)=0;
  NS_IMETHOD DeleteTableColumn(PRInt32 aNumber)=0;
  NS_IMETHOD DeleteTableRow(PRInt32 aNumber)=0;

  /** Join the contents of the selected cells into one cell,
    *   expanding that cells ROWSPAN and COLSPAN to take up
    *   the same number of cellmap locations as before.
    *   Cells whose contents were moved are deleted.
    *   If there's one cell selected or caret is in one cell,
    *     it is joined with the cell to the right, if it exists
    */
  NS_IMETHOD JoinTableCells()=0;


  /** Scan through all rows and add cells as needed so 
    *   all locations in the cellmap are occupied.
    *   Used after inserting single cells or pasting
    *   a collection of cells that extend past the
    *   previous size of the table
    * If aTable is null, it uses table enclosing the selection anchor
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
    * Note that this returns NS_TABLELAYOUT_CELL_NOT_FOUND 
    *   when a cell is not found at the given indexes,
    *   but this passes the NS_SUCCEEDED() test,
    *   so you can scan for all cells in a row or column
    *   by iterating through the appropriate indexes
    *   until the returned aCell is null
    */
  NS_IMETHOD GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell)=0;

  /** Get a cell at cellmap grid coordinates and associated data
    * A cell that spans across multiple cellmap locations will
    *   be returned multiple times, once for each location it occupies
    * Examine the returned aStartRowIndex and aStartColIndex to see 
    *   if it is in the same layout column or layout row:
    *   A "layout row" is all cells sharing the same top edge
    *   A "layout column" is all cells sharing the same left edge
    *   This is important to determine what to do when inserting or deleting a column or row
    * 
    * @param aTable                   A table in the document
    * @param aRowIndex, aColIndex     The 0-based cellmap indexes
    *
    * Note that this returns NS_TABLELAYOUT_CELL_NOT_FOUND
    *   when a cell is not found at the given indexes  (see note for GetCellAt())
    */
  NS_IMETHOD GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell,
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex,
                           PRInt32& aRowSpan, PRInt32& aColSpan, PRBool& aIsSelected)=0;


};


#endif // nsITableEditor_h__
