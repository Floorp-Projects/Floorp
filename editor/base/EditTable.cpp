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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsIDOMDocument.h"
#include "nsEditor.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDOMSelection.h"
#include "nsLayoutCID.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIAtom.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsITableCellLayout.h" // For efficient access to table cell
#include "nsITableLayout.h"     //  data owned by the table and cell frames
#include "nsHTMLEditor.h"

#include "nsEditorUtils.h"

static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);

/***************************************************************************
 * stack based helper class for restoring selection after table edit
 */
class nsSetCaretAfterTableEdit
{
  private:
    nsCOMPtr<nsITableEditor> mEd;
    nsCOMPtr<nsIDOMElement> mTable;
    PRInt32 mCol, mRow, mDirection;
  public:
    nsSetCaretAfterTableEdit(nsITableEditor *aEd, nsIDOMElement* aTable, 
                             PRInt32 aRow, PRInt32 aCol, PRInt32 aDirection) : 
        mEd(do_QueryInterface(aEd))
    { 
      mTable = aTable; 
      mRow = aRow; 
      mCol = aCol; 
      mDirection = aDirection;
    } 
    
    ~nsSetCaretAfterTableEdit() 
    { 
      if (mEd)
        mEd->SetCaretAfterTableEdit(mTable, mRow, mCol, mDirection);
    }
    // This is needed to abort the caret reset in the destructor
    //  when one method yields control to another
    void CancelSetCaret() {mEd = nsnull; mTable = nsnull;}
};

NS_IMETHODIMP
nsHTMLEditor::InsertCell(nsIDOMElement *aCell, PRInt32 aRowSpan, PRInt32 aColSpan, PRBool aAfter)
{
  if (!aCell) return NS_ERROR_NULL_POINTER;

  // And the parent and offsets needed to do an insert
  nsCOMPtr<nsIDOMNode> cellParent;
  nsresult res = aCell->GetParentNode(getter_AddRefs(cellParent));
  if( NS_SUCCEEDED(res) && cellParent)
  {
    PRInt32 cellOffset;
    res = GetChildOffset(aCell, cellParent, cellOffset);
    if( NS_SUCCEEDED(res))
    {
      nsCOMPtr<nsIDOMElement> newCell;
      res = CreateElementWithDefaults("td", getter_AddRefs(newCell));
      if (NS_SUCCEEDED(res) && newCell)
      {
        if( aRowSpan > 1)
        {
          nsAutoString newRowSpan(aRowSpan);
          //newRowSpan.Append(aRowSpan, 10);
          // Note: Do NOT use editor txt for this
          newCell->SetAttribute("rowspan", newRowSpan);
        }
        if( aColSpan > 1)
        {
          nsAutoString newColSpan(aColSpan);
          // Note: Do NOT use editor txt for this
          newCell->SetAttribute("colspan", newColSpan);
        }
        if(aAfter) cellOffset++;
        res = InsertNode(newCell, cellParent, cellOffset);
      }
    }
  }
  return res;
}

/****************************************************************/

// Table Editing interface methods

NS_IMETHODIMP
nsHTMLEditor::InsertTableCell(PRInt32 aNumber, PRBool aAfter)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> curCell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;
  nsresult res = GetCellContext(selection, table, curCell, cellParent, cellOffset, startRowIndex, startColIndex);
  if (NS_FAILED(res)) return res;

  // Get more data for current cell in row we are inserting at (we need COLSPAN)
  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan;
  PRBool  isSelected;
  res = GetCellDataAt(table, startRowIndex, startColIndex, *getter_AddRefs(curCell),
                      curStartRowIndex, curStartColIndex, rowSpan, colSpan, isSelected);
  if (NS_FAILED(res)) return res;
  if (!curCell) return NS_ERROR_FAILURE;
  PRInt32 newCellIndex = aAfter ? (startColIndex+colSpan) : startColIndex;
  nsSetCaretAfterTableEdit setCaret(this, table, startRowIndex, newCellIndex, ePreviousColumn);

  PRInt32 i;
  for (i = 0; i < aNumber; i++)
  {
    nsCOMPtr<nsIDOMElement> newCell;
    res = CreateElementWithDefaults("td", getter_AddRefs(newCell));
    if (NS_SUCCEEDED(res) && newCell)
    {
      if (aAfter) cellOffset++;
      res = InsertNode(newCell, cellParent, cellOffset);
      if(NS_FAILED(res)) break;
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableColumn(PRInt32 aNumber, PRBool aAfter)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> curCell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;
  nsresult res = GetCellContext(selection, table, curCell, cellParent, cellOffset, startRowIndex, startColIndex);
  if (NS_FAILED(res)) return res;

  // Get more data for current cell (we need ROWSPAN)
  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan;
  PRBool  isSelected;
  res = GetCellDataAt(table, startRowIndex, startColIndex, *getter_AddRefs(curCell),
                      curStartRowIndex, curStartColIndex, rowSpan, colSpan, isSelected);
  if (NS_FAILED(res)) return res;
  if (!curCell) return NS_ERROR_FAILURE;

  // Use column after current cell if requested
  if (aAfter)
    startColIndex += colSpan;
   
  PRInt32 rowCount, colCount, row;
  res = GetTableSize(table, rowCount, colCount);
  if (NS_FAILED(res)) return res;

  PRInt32 lastColumn = colCount - 1;

  nsAutoEditBatch beginBatching(this);
  nsSetCaretAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousRow);

  for ( row = 0; row < rowCount; row++)
  {
    if (startColIndex < colCount)
    {
      // We are inserting before an existing column
      res = GetCellDataAt(table, row, startColIndex, *getter_AddRefs(curCell),
                          curStartRowIndex, curStartColIndex, rowSpan, colSpan, isSelected);
      if (NS_FAILED(res)) return res;

      // Don't fail entire process if we fail to find a cell
      //  (may fail just in particular rows with < adequate cells per row)
      if (curCell)
      {
        if (curStartColIndex < startColIndex)
        {
          // We have a cell spanning this location
          // Simply increase its colspan to keep table rectangular
          nsAutoString newColSpan;
          newColSpan.Append(colSpan+aNumber, 10);
          SetAttribute(curCell, "colspan", newColSpan);
        } else {
          // Simply set selection to the current cell 
          //  so we can let InsertTableCell() do the work
          // Insert a new cell before current one
          selection->Collapse(curCell, 0);
          res = InsertTableCell(aNumber, PR_FALSE);
        }
      }
    } else {
      //TODO: A better strategy is to get the row at index "row"
      //   and append a cell to the end of it.
      // Need to write "GetRowAt()" to do this

      // Get last cell in row and insert new cell after it
      res = GetCellDataAt(table, row, lastColumn, *getter_AddRefs(curCell),
                          curStartRowIndex, curStartColIndex, rowSpan, colSpan, isSelected);
      if (NS_FAILED(res)) return res;
      if (curCell)
      {
        selection->Collapse(curCell, 0);
        res = InsertTableCell(aNumber, PR_TRUE);
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableRow(PRInt32 aNumber, PRBool aAfter)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> curCell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;
  nsresult res = GetCellContext(selection, table, curCell, cellParent, cellOffset, startRowIndex, startColIndex);
  if (NS_FAILED(res)) return res;

  // Get more data for current cell in row we are inserting at (we need COLSPAN)
  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan;
  PRBool  isSelected;
  res = GetCellDataAt(table, startRowIndex, startColIndex, *getter_AddRefs(curCell),
                      curStartRowIndex, curStartColIndex, rowSpan, colSpan, isSelected);
  if (NS_FAILED(res)) return res;
  if (!curCell) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMElement> parentRow;
  GetElementOrParentByTagName("tr", curCell, getter_AddRefs(parentRow));
  if (!parentRow) return NS_ERROR_NULL_POINTER;

  PRInt32 rowCount, colCount;
  res = GetTableSize(table, rowCount, colCount);
  if (NS_FAILED(res)) return res;

  // Get the parent and offset where we will insert new row(s)
  nsCOMPtr<nsIDOMNode> parentOfRow;
  PRInt32 newRowOffset;
  parentRow->GetParentNode(getter_AddRefs(parentOfRow));
  if (!parentOfRow) return NS_ERROR_NULL_POINTER;
  res = GetChildOffset(parentRow, parentOfRow, newRowOffset);
  if (NS_FAILED(res)) return res;
  if (!parentOfRow)   return NS_ERROR_NULL_POINTER;

  if (aAfter)
  {
    // Use row after current cell
    startRowIndex += rowSpan;
    // offset to use for new row insert
    newRowOffset += rowSpan;
  }

  nsAutoEditBatch beginBatching(this);
  nsSetCaretAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn);

  PRInt32 cellsInRow = 0;
  if (startRowIndex < rowCount)
  {
    // We are inserting above an existing row
    // Get each cell in the insert row to adjust for COLSPAN effects while we
    //   count how many cells are needed
    PRInt32 colIndex = 0;
    // This returns NS_TABLELAYOUT_CELL_NOT_FOUND when we run past end of row,
    //   which passes the NS_SUCCEEDED macro
    while ( NS_OK == GetCellDataAt(table, newRowOffset, colIndex, *getter_AddRefs(curCell), 
                                   curStartRowIndex, curStartColIndex, rowSpan, colSpan, isSelected) )
    {
      if (curCell)
      {
        if (curStartRowIndex < startRowIndex)
        {
          // We have a cell spanning this location
          // Simply increase its colspan to keep table rectangular
          nsAutoString newRowSpan;
          newRowSpan.Append(rowSpan+aNumber, 10);
          SetAttribute(curCell, "rowspan", newRowSpan);
        } else {
          // Count the number of cells we need to add to the new row
          cellsInRow += colSpan;
        }
        // Next cell in row
        colIndex += colSpan;
      }
      else
        colIndex++;
    }
  } else {
    // We are adding after existing rows, 
    //  so use max number of cells in a row
    cellsInRow = colCount;
  }

  if (cellsInRow > 0)
  {
    for (PRInt32 row = 0; row < aNumber; row++)
    {
      // Create a new row
      nsCOMPtr<nsIDOMElement> newRow;
      res = CreateElementWithDefaults("tr", getter_AddRefs(newRow));
      if (NS_SUCCEEDED(res))
      {
        if (!newRow) return NS_ERROR_FAILURE;
      
        for (PRInt32 i = 0; i < cellsInRow; i++)
        {
          nsCOMPtr<nsIDOMElement> newCell;
          res = CreateElementWithDefaults("td", getter_AddRefs(newCell));
          if (NS_FAILED(res)) return res;
          if (!newCell) return NS_ERROR_FAILURE;

          // Don't use transaction system yet! (not until entire row is inserted)
          nsCOMPtr<nsIDOMNode>resultNode;
          res = newRow->AppendChild(newCell, getter_AddRefs(resultNode));
          if (NS_FAILED(res)) return res;
        }
        // Use transaction system to insert the entire row+cells
        // (Note that rows are inserted at same childoffset each time)
        res = nsEditor::InsertNode(newRow, parentOfRow, newRowOffset);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  return res;
}

// This is an internal helper (not exposed in IDL)
NS_IMETHODIMP
nsHTMLEditor::DeleteTable(nsCOMPtr<nsIDOMElement> &aTable, nsCOMPtr<nsIDOMSelection> &aSelection)
{
  nsCOMPtr<nsIDOMNode> tableParent;
  PRInt32 tableOffset;
  if(NS_FAILED(aTable->GetParentNode(getter_AddRefs(tableParent))) || !tableParent)
    return NS_ERROR_FAILURE;

  // Save offset we need to restore the selection
  if(NS_FAILED(GetChildOffset(aTable, tableParent, tableOffset)))
    return NS_ERROR_FAILURE;

  nsresult res = DeleteNode(aTable);

  // Place selection just before the table
  aSelection->Collapse(tableParent, tableOffset);
  
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTable()
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;
  nsresult res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRowIndex, startColIndex);
    
  if (NS_SUCCEEDED(res))
  {
    nsAutoEditBatch beginBatching(this);
    res = DeleteTable(table, selection);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableCell(PRInt32 aNumber)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;

  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_FAILURE;

  nsAutoEditBatch beginBatching(this);

  for (PRInt32 i = 0; i < aNumber; i++)
  {
    res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRowIndex, startColIndex);
    if (NS_FAILED(res)) return res;
    if (!cell) return NS_ERROR_NULL_POINTER;

    if (1 == GetNumberOfCellsInRow(table, startRowIndex))
    {
      nsCOMPtr<nsIDOMElement> parentRow;
      res = GetElementOrParentByTagName("tr", cell, getter_AddRefs(parentRow));
      if (!parentRow)
      {
        res = NS_ERROR_NULL_POINTER;
        break;
      }
      if (NS_FAILED(res)) return res;
      // We should delete the row instead,
      //  but first check if its the only row left
      //  so we can delete the entire table
      PRInt32 rowCount, colCount;
      res = GetTableSize(table, rowCount, colCount);
      if (NS_FAILED(res)) return res;
      
      if (rowCount == 1)
        return DeleteTable(table, selection);
    
      // Delete the row by placing caret in cell we were to delete
      // We need to call DeleteTableRow to handle cells with rowspan 
      selection->Collapse(cell,0);
      res = DeleteTableRow(1);
      if (NS_FAILED(res)) return res;
    } 
    else
    {
      // More than 1 cell in the row

      // We clear the selection to avoid problems when nodes in the selection are deleted,
      // The setCaret object will call SetCaretAfterTableEdit in it's destructor
      selection->ClearSelection();
      nsSetCaretAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn);

      res = DeleteNode(cell);
      // If we fail, don't try to delete any more cells???
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableCellContents()
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;
  nsresult res = NS_OK;

  res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRowIndex, startColIndex);
  if (NS_FAILED(res)) return res;
  if (!cell) return NS_ERROR_NULL_POINTER;


  // We clear the selection to avoid problems when nodes in the selection are deleted,
  selection->ClearSelection();
  nsSetCaretAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn);

  nsAutoEditBatch beginBatching(this);

  nsCOMPtr<nsIDOMNodeList> nodeList;
  res = cell->GetChildNodes(getter_AddRefs(nodeList));
  if (NS_FAILED(res)) return res;

  if (!nodeList) return NS_ERROR_FAILURE;
  PRUint32 nodeListLength; 
  res = nodeList->GetLength(&nodeListLength);
  if (NS_FAILED(res)) return res;

  for (PRUint32 i = 0; i < nodeListLength; i++)
  {
    
    nsCOMPtr<nsIDOMNode> child;
    res = cell->GetLastChild(getter_AddRefs(child));
    if (NS_FAILED(res)) return res;
    res = DeleteNode(child);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableColumn(PRInt32 aNumber)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;
  PRInt32 rowCount, colCount;
  nsresult res = NS_OK;

  res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRowIndex, startColIndex);
  if (NS_FAILED(res)) return res;
  if(!cell) return NS_ERROR_NULL_POINTER;

  res = GetTableSize(table, rowCount, colCount);
  if (NS_FAILED(res)) return res;

  // Shortcut the case of deleting all columns in table
  if(startColIndex == 0 && aNumber >= colCount)
    return DeleteTable(table, selection);

  nsAutoEditBatch beginBatching(this);

  // Check for counts too high
  aNumber = PR_MIN(aNumber,(colCount-startColIndex));

  selection->ClearSelection();
  nsSetCaretAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn);

  // Scan through cells in row to do rowspan adjustments
  nsCOMPtr<nsIDOMElement> curCell;
  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan;
  PRBool  isSelected;
  PRInt32 rowIndex = 0;

  for (PRInt32 i = 0; i < aNumber; i++)
  {
    do {
      res = GetCellDataAt(table, rowIndex, startColIndex, *getter_AddRefs(curCell),
                          curStartRowIndex, curStartColIndex, rowSpan, colSpan, isSelected);

      if (NS_FAILED(res)) return res;
      NS_ASSERTION((colSpan > 0),"COLSPAN = 0 in DeleteTableColumn");
      NS_ASSERTION((rowSpan > 0),"ROWSPAN = 0 in DeleteTableColumn");

      if (curCell)
      {
        // Find cells that don't start in column we are deleting
        if (curStartColIndex < startColIndex || colSpan > 1)
        {
          NS_ASSERTION((colSpan > 1),"Bad COLSPAN in DeleteTableColumn");
          // We have a cell spanning this location
          // Decrease its colspan to keep table rectangular

          nsAutoString newColSpan;
          //This calculates final span for all columns deleted,
          //  but we are iterating aNumber times through here so
          //  just decrement current value by 1
          //PRInt32 minSpan = startColIndex - curStartColIndex;
          //PRInt32 newSpan = PR_MAX(minSpan, colSpan - aNumber);
          //newColSpan.Append(newSpan, 10);
          newColSpan.Append(colSpan-1, 10);
          SetAttribute(curCell, "colspan", newColSpan);

          // Delete contents of cell instead of cell itself
          if (curStartColIndex == startColIndex)
          {
            selection->Collapse(curCell,0);
            DeleteTableCellContents();
          }
          // To next cell in row
          rowIndex++;
                  
        } else {
          // Delete the cell
          if (1 == GetNumberOfCellsInRow(table, rowIndex))
          {
            nsCOMPtr<nsIDOMElement> parentRow;
            res = GetElementOrParentByTagName("tr", cell, getter_AddRefs(parentRow));
            if (NS_FAILED(res)) return res;
            if(!parentRow) return NS_ERROR_NULL_POINTER;

            // We should delete the row instead,
            //  but first check if its the only row left
            //  so we can delete the entire table
            //  (This should never happen but it's the safe thing to do)
            PRInt32 rowCount, colCount;
            res = GetTableSize(table, rowCount, colCount);
            if (NS_FAILED(res)) return res;

            if (rowCount == 1)
              return DeleteTable(table, selection);
      
            // Delete the row by placing caret in cell we were to delete
            // We need to call DeleteTableRow to handle cells with rowspan 
            selection->Collapse(cell,0);
            res = DeleteTableRow(1);
            if (NS_FAILED(res)) return res;

            // Note that we don't incremenet rowIndex
            // since a row was deleted and "next" 
            // row now has current rowIndex
          } else {
            res = DeleteNode(curCell);
            if (NS_FAILED(res)) return res;

            //Skipover any rows spanned by this cell
            rowIndex +=rowSpan;
          }
        }
      } else {
        // No curCell, we've reached end of row
        rowIndex++;
      }

    } while (curCell);    
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableRow(PRInt32 aNumber)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;
  PRInt32 rowCount, colCount;
  nsresult res = NS_OK;

  res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRowIndex, startColIndex);
  if (NS_FAILED(res)) return res;
  if(!cell) return NS_ERROR_NULL_POINTER;

  res = GetTableSize(table, rowCount, colCount);
  if (NS_FAILED(res)) return res;

  // Shortcut the case of deleting all rows in table
  if(startRowIndex == 0 && aNumber >= rowCount)
    return DeleteTable(table, selection);

  nsAutoEditBatch beginBatching(this);

  // Check for counts too high
  aNumber = PR_MIN(aNumber,(rowCount-startRowIndex));

  // We clear the selection to avoid problems when nodes in the selection are deleted,
  // Be sure to set it correctly later (in SetCaretAfterTableEdit)!
  selection->ClearSelection();
  
  // Scan through cells in row to do rowspan adjustments
  nsCOMPtr<nsIDOMElement> curCell;
  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan;
  PRBool  isSelected;
  PRInt32 colIndex = 0;
  do {
    res = GetCellDataAt(table, startRowIndex, colIndex, *getter_AddRefs(curCell),
                        curStartRowIndex, curStartColIndex, rowSpan, colSpan, isSelected);

    NS_ASSERTION((colSpan > 0),"COLSPAN = 0 in DeleteTableRow");
    NS_ASSERTION((rowSpan > 0),"ROWSPAN = 0 in DeleteTableRow");

    // Find cells that don't start in row we are deleting
    if (NS_SUCCEEDED(res) && curCell)
    {
      if (curStartRowIndex < startRowIndex)
      {
        // We have a cell spanning this location
        // Decrease its rowspan to keep table rectangular
        nsAutoString newRowSpan;
        PRInt32 minSpan = startRowIndex - curStartRowIndex;
        PRInt32 newSpan = PR_MAX(minSpan, rowSpan - aNumber);
        newRowSpan.Append(newSpan, 10);
        SetAttribute(curCell, "rowspan", newRowSpan);
      }
      // Skip over locations spanned by this cell
      colIndex += colSpan;
    }
    else
      colIndex++;

  } while (curCell);    

  for (PRInt32 i = 0; i < aNumber; i++)
  {
    //TODO: To minimize effect of deleting cells that have rowspan > 1:
    //      Scan for rowspan > 1 and insert extra emtpy cells in 
    //      appropriate rows to take place of spanned regions.
    //      (Hard part is finding appropriate neighbor cell before/after in correct row)

    // Delete the row
    nsCOMPtr<nsIDOMElement> parentRow;
    res = GetElementOrParentByTagName("tr", cell, getter_AddRefs(parentRow));
    if (NS_SUCCEEDED(res) && parentRow)
      res = DeleteNode(parentRow);
    if (NS_FAILED(res))
      startRowIndex++;

    res = GetCellAt(table, startRowIndex, startColIndex, *getter_AddRefs(cell));
    if(!cell)
      break;
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::JoinTableCells()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsHTMLEditor::NormalizeTable(nsIDOMElement *aTable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsHTMLEditor::GetCellIndexes(nsIDOMElement *aCell, PRInt32 &aRowIndex, PRInt32 &aColIndex)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  aColIndex=0; // initialize out params
  aRowIndex=0;
  if (!aCell)
  {
    // Get the selected cell or the cell enclosing the selection anchor
    nsCOMPtr<nsIDOMElement> cell;
    res = GetElementOrParentByTagName("td", nsnull, getter_AddRefs(cell));
    if (NS_SUCCEEDED(res) && cell)
      aCell = cell;
    else
      return NS_ERROR_FAILURE;
  }

  res = NS_ERROR_FAILURE;        // we return an error unless we get the index
  nsISupports *layoutObject=nsnull; // frames are not ref counted, so don't use an nsCOMPtr

  res = nsHTMLEditor::GetLayoutObject(aCell, &layoutObject);

  if ((NS_SUCCEEDED(res)) && (nsnull!=layoutObject))
  { // get the table cell interface from the frame
    nsITableCellLayout *cellLayoutObject=nsnull; // again, frames are not ref-counted
  
    res = layoutObject->QueryInterface(NS_GET_IID(nsITableCellLayout), (void**)(&cellLayoutObject));
    if ((NS_SUCCEEDED(res)) && (nsnull!=cellLayoutObject))
    {
      res = cellLayoutObject->GetCellIndexes(aRowIndex, aColIndex);
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetTableLayoutObject(nsIDOMElement* aTable, nsITableLayout **tableLayoutObject)
{
  *tableLayoutObject=nsnull;
  if (!aTable)
    return NS_ERROR_NOT_INITIALIZED;
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsISupports *layoutObject=nsnull;
  nsresult res = nsHTMLEditor::GetLayoutObject(aTable, &layoutObject); 
  if ((NS_SUCCEEDED(res)) && (nsnull!=layoutObject)) 
  { // get the table interface from the frame 
    
    res = layoutObject->QueryInterface(NS_GET_IID(nsITableLayout), 
                            (void**)(tableLayoutObject)); 
  }
  return res;
}

//Return actual number of cells (a cell with colspan > 1 counts as just 1)
PRBool nsHTMLEditor::GetNumberOfCellsInRow(nsIDOMElement* aTable, PRInt32 rowIndex)
{
  PRInt32 cellCount = 0;
  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 colIndex = 0;
  nsresult res;
  do {
    PRInt32 startRowIndex, startColIndex, rowSpan, colSpan;
    PRBool  isSelected;
    res = GetCellDataAt(aTable, rowIndex, colIndex, *getter_AddRefs(cell),
                        startRowIndex, startColIndex, rowSpan, colSpan, isSelected);
    if (NS_FAILED(res)) return res;
    if (cell)
    {
      // Only count cells that start in row we are working with
      if (startRowIndex == rowIndex)
        cellCount++;
      
      //Next possible location for a cell
      colIndex += colSpan;
    }
    else
      colIndex++;

  } while (cell);

  return cellCount;
}

/* Not scriptable: For convenience in C++ */
NS_IMETHODIMP
nsHTMLEditor::GetTableSize(nsIDOMElement *aTable, PRInt32& aRowCount, PRInt32& aColCount)
{
  nsresult res=NS_ERROR_FAILURE;
  aRowCount = 0;
  aColCount = 0;
  if (!aTable)
  {
    // Get the selected talbe or the table enclosing the selection anchor
    nsCOMPtr<nsIDOMElement> table;
    res = GetElementOrParentByTagName("table", nsnull, getter_AddRefs(table));
    if (NS_FAILED(res)) return res;
    if (table)
      aTable = table;
    else
      return NS_ERROR_FAILURE;
  }
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayoutObject;
  res = GetTableLayoutObject(aTable, &tableLayoutObject);
  if (NS_FAILED(res)) return res;
  if (!tableLayoutObject)
    return NS_ERROR_FAILURE;

  return tableLayoutObject->GetTableSize(aRowCount, aColCount); 
}

NS_IMETHODIMP 
nsHTMLEditor::GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell, 
                            PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                            PRInt32& aRowSpan, PRInt32& aColSpan, PRBool& aIsSelected)
{
  nsresult res=NS_ERROR_FAILURE;
  aCell = nsnull;
  aStartRowIndex = 0;
  aStartColIndex = 0;
  aRowSpan = 0;
  aColSpan = 0;
  aIsSelected = PR_FALSE;

  if (!aTable)
  {
    // Get the selected table or the table enclosing the selection anchor
    nsCOMPtr<nsIDOMElement> table;
    res = GetElementOrParentByTagName("table", nsnull, getter_AddRefs(table));
    if (NS_FAILED(res)) return res;
    if (table)
      aTable = table;
    else
      return NS_ERROR_FAILURE;
  }
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayoutObject;
  res = GetTableLayoutObject(aTable, &tableLayoutObject);
  if (NS_FAILED(res)) return res;
  if (!tableLayoutObject) return NS_ERROR_FAILURE;

  // Note that this returns NS_TABLELAYOUT_CELL_NOT_FOUND when
  //  the index(es) are out of bounds
  return tableLayoutObject->GetCellDataAt(aRowIndex, aColIndex, aCell, 
                                          aStartRowIndex, aStartColIndex,
                                          aRowSpan, aColSpan, aIsSelected);
}

// When all you want is the cell
NS_IMETHODIMP 
nsHTMLEditor::GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell)
{
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan;
  PRBool  isSelected;
  return GetCellDataAt(aTable, aRowIndex, aColIndex, aCell, 
                       startRowIndex, startColIndex, rowSpan, colSpan, isSelected);
}

NS_IMETHODIMP
nsHTMLEditor::GetCellContext(nsCOMPtr<nsIDOMSelection> &aSelection,
                             nsCOMPtr<nsIDOMElement> &aTable, nsCOMPtr<nsIDOMElement> &aCell, 
                             nsCOMPtr<nsIDOMNode> &aCellParent, PRInt32& aCellOffset, 
                             PRInt32& aRow, PRInt32& aCol)
{
  nsresult res = nsEditor::GetSelection(getter_AddRefs(aSelection));
  if (NS_FAILED(res)) return res;
  if (!aSelection) return NS_ERROR_FAILURE;

  // Find the first selected cell
  res = GetFirstSelectedCell(aCell);
  if (!aCell)
  {
    //If a cell wasn't selected, then assume the selection is INSIDE 
    //  and use anchor node to search up to the containing cell
    nsCOMPtr<nsIDOMNode> anchorNode;

    res = aSelection->GetAnchorNode(getter_AddRefs(anchorNode));
    if (NS_FAILED(res)) return res;
    if (!anchorNode)    return NS_ERROR_FAILURE;

    // Get the cell enclosing the selection anchor
    res = GetElementOrParentByTagName("td", anchorNode, getter_AddRefs(aCell));
    if (NS_FAILED(res)) return res;
    if (!aCell)         return NS_ERROR_FAILURE;
  }
  // Get containing table and the immediate parent of the cell
  res = GetElementOrParentByTagName("table", aCell, getter_AddRefs(aTable));
  if (NS_FAILED(res)) return res;
  if (!aTable)        return NS_ERROR_FAILURE;

  res = aCell->GetParentNode(getter_AddRefs(aCellParent));
  if (NS_FAILED(res)) return res;
  if (!aCellParent)   return NS_ERROR_FAILURE;

  // Get current cell location so we can put caret back there when done
  res = GetCellIndexes(aCell, aRow, aCol);
  if(NS_FAILED(res)) return res;

  // And the parent and offsets needed to do an insert
  return GetChildOffset(aCell, aCellParent, aCellOffset);
}

NS_IMETHODIMP 
nsHTMLEditor::GetFirstSelectedCell(nsCOMPtr<nsIDOMElement> &aCell)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_FAILURE;

  // The most straight forward way of finding a selected cell
  //   is to use the selection iterator
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  
  if (NS_FAILED(res)) return res;
  if (!enumerator)    return NS_ERROR_FAILURE;
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if ((NS_SUCCEEDED(res)) && currentItem)
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    nsCOMPtr<nsIContentIterator> iter;
    res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                NS_GET_IID(nsIContentIterator), 
                                                getter_AddRefs(iter));
    if (NS_FAILED(res)) return res;
    if (!iter)          return NS_ERROR_FAILURE;

    iter->Init(range);
    // loop through the content iterator for each content node
    nsCOMPtr<nsIContent> content;

    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      res = iter->CurrentNode(getter_AddRefs(content));
      // Not likely!
      if (NS_FAILED(res)) return NS_ERROR_FAILURE;

      nsCOMPtr<nsIAtom> atom;
      content->GetTag(*getter_AddRefs(atom));
      if (atom.get() == nsIEditProperty::td ||
          atom.get() == nsIEditProperty::th )
      {
        // We found a cell   
        aCell = do_QueryInterface(content);
        break;
      }
      iter->Next();
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetCaretAfterTableEdit(nsIDOMElement* aTable, PRInt32 aRow, PRInt32 aCol, PRInt32 aDirection)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;
  if (!aTable) return res;

  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  if (!selection)
  {
#ifdef DEBUG_cmanske
    printf("Selection not found after table manipulation!\n");
#endif
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMElement> cell;
  PRBool done = PR_FALSE;
  do {
    res = GetCellAt(aTable, aRow, aCol, *getter_AddRefs(cell));
    nsCOMPtr<nsIDOMNode> cellNode = do_QueryInterface(cell);
    if (NS_SUCCEEDED(res))
    {
      if (cell)
      {
        // Set the caret to just before the first child of the cell?
        // TODO: Should we really be placing the caret at the END
        //  of the cell content?
        selection->Collapse(cell, 0);
        return NS_OK;
      } else {
        // Setup index to find another cell in the 
        //   direction requested, but move in
        //   other direction if already at beginning of row or column
        switch (aDirection)
        {
          case ePreviousColumn:
            if (aCol == 0)
            {
              if (aRow > 0)
                aRow--;
              else
                done = PR_TRUE;
            }
            else
              aCol--;
            break;
          case ePreviousRow:
            if (aRow == 0)
            {
              if (aCol > 0)
                aCol--;
              else
                done = PR_TRUE;
            }
            else
              aRow--;
            break;
          default:
            done = PR_TRUE;
        }
      }
    }
    else
      break;
  } while (!done);

  // We didn't find a cell
  // Set selection to just before the table
  nsCOMPtr<nsIDOMNode> tableParent;
  PRInt32 tableOffset;
  res = aTable->GetParentNode(getter_AddRefs(tableParent));
  if(NS_SUCCEEDED(res) && tableParent)
  {
    if(NS_SUCCEEDED(GetChildOffset(aTable, tableParent, tableOffset)))
      return selection->Collapse(tableParent, tableOffset);

    // Still failing! Set selection to start of doc
	  nsCOMPtr<nsIDOMElement> bodyElement;
	  res = GetBodyElement(getter_AddRefs(bodyElement));  
	  if (NS_SUCCEEDED(res))
    {
  	  if (!bodyElement) return NS_ERROR_NULL_POINTER;
      res = selection->Collapse(bodyElement,0);
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetSelectedOrParentTableElement(nsCOMPtr<nsIDOMElement> &aTableElement, nsString& aTagName, PRBool &aIsSelected)
{
  aTableElement = nsnull;
  aTagName = "";
  aIsSelected = PR_FALSE;

  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_FAILURE;

  nsAutoString table("table");
  nsAutoString tr("tr");
  nsAutoString td("td");

  // Find the first selected cell
  // TODO: Handle multiple cells selected!
  nsCOMPtr<nsIDOMElement> firstCell;  
  res = GetFirstSelectedCell(aTableElement);
  if(NS_FAILED(res)) return res;
  if (aTableElement)
  {
    aTagName = td;
    aIsSelected = PR_TRUE;
    return NS_OK;
  }

  // See if table or row is selected
  res = GetSelectedElement(table, getter_AddRefs(aTableElement));
  if(NS_FAILED(res)) return res;
  if (aTableElement)
  {
    aTagName = table;
    aIsSelected = PR_TRUE;
    return NS_OK;
  }
  res = GetSelectedElement(tr, getter_AddRefs(aTableElement));
  if(NS_FAILED(res)) return res;
  if (aTableElement)
  {
    aTagName = tr;
    aIsSelected = PR_TRUE;
    return NS_OK;
  }

  // Look for a table cell parent
  nsCOMPtr<nsIDOMNode> anchorNode;
  res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
  if (NS_FAILED(res)) return res;
  if (!anchorNode)    return NS_ERROR_FAILURE;

  res = GetElementOrParentByTagName(td, anchorNode, getter_AddRefs(aTableElement));
  if(NS_FAILED(res)) return res;
  if (aTableElement)
    aTagName = td;

  return res;
}
