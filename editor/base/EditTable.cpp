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
PRBool nsHTMLEditor::IsOnlyCellInRow(nsCOMPtr<nsIDOMElement> &aCell, nsIDOMElement** aParentRow)
{
  if (!aParentRow) return NS_ERROR_NULL_POINTER;

  PRBool oneCellFound = PR_FALSE;
  nsresult res = GetElementOrParentByTagName("tr", aCell, aParentRow);
  if (NS_SUCCEEDED(res) && aParentRow)
  {
    PRBool done = PR_FALSE;
    do {
      nsCOMPtr<nsIDOMNode> resultNode;
      // Scan through all nodes starting at the row we are interested in,
      // and count the cells we run into.
      res = GetNextNode(*aParentRow, PR_TRUE, getter_AddRefs(resultNode));
      if (NS_SUCCEEDED(res) && resultNode)
      {
        //Test if it's a cell and parent is = aParentRow
        //Continue and count cells encountered until we leave the row
        nsCOMPtr<nsIDOMElement> parent;
        res = GetElementOrParentByTagName("tr", resultNode, getter_AddRefs(parent));
        if(NS_SUCCEEDED(res) && parent && (parent == *aParentRow))
        {
          if (oneCellFound)
          {
            // We found more than one cell, so we're done
            return PR_FALSE;
          } else {
            oneCellFound = PR_TRUE;
          }            
        } else {
          // We are outside of the row, so we're done
          done = PR_TRUE;
        }

      } else {
        // Should never happen!
        return PR_FALSE;      
      }
    } while(!done);
    // We should always find one cell - the one we started with!
    //NS_ASSERTION(oneCellFound, "Cell Not Found!");
  }
  return oneCellFound;
}

PRBool nsHTMLEditor::IsOnlyRowInTable(nsCOMPtr<nsIDOMElement> &aRow, nsCOMPtr<nsIDOMElement> &aTable)
{
  PRBool oneRowFound = PR_FALSE;
  PRBool done = PR_FALSE;

  do {
    nsCOMPtr<nsIDOMNode> resultNode;
    // Scan through all nodes starting at the row we are interested in
    nsresult res = GetNextNode(aTable, PR_TRUE, getter_AddRefs(resultNode));
    if (NS_SUCCEEDED(res))
    {
      //Test if it's a row and parent is same as table we are in
      nsCOMPtr<nsIDOMElement> parent;
      res = GetElementOrParentByTagName("table", resultNode, getter_AddRefs(parent));
      if(NS_SUCCEEDED(res) && parent && (parent == aTable))
      {
        if (oneRowFound)
        {
          // We found more than one row, so we're done
          return PR_FALSE;
        } else {
          oneRowFound = PR_TRUE;
        }            
      } else {
        // We are outside of the table, so we're done
        done = PR_TRUE;
      }
    } else {
      // Should never happen!
      return PR_FALSE;      
    }
  } while(!done);
  // We should always find one row - the one we started with!
    NS_ASSERTION(oneRowFound, "Row Not Found!");
  return oneRowFound;
}

// Table Editing methods

NS_IMETHODIMP
nsHTMLEditor::InsertTableCell(PRInt32 aNumber, PRBool aAfter)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRow, startCol;
  nsresult res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRow, startCol);
  
  if (NS_SUCCEEDED(res))
  {
    PRInt32 i;
    for (i = 0; i < aNumber; i++)
    {
      nsCOMPtr<nsIDOMElement> newCell;
      res = CreateElementWithDefaults("td", getter_AddRefs(newCell));
      if (NS_SUCCEEDED(res) && newCell)
      {
        if (aAfter) cellOffset++;
        res = InsertNode(newCell, cellParent, cellOffset);
      }
    }
    SetCaretAfterTableEdit(table, startRow, startCol, ePreviousColumn);  
  }
  return res;                                                         
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableColumn(PRInt32 aNumber, PRBool aAfter)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRow, startCol;
  nsresult res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRow, startCol);
  if (NS_FAILED(res)) return res;

  // Get more data for current cell (we need ROWSPAN)
  nsCOMPtr<nsIDOMElement> curCell;
  PRInt32 curStartRow, curStartCol, rowSpan, colSpan;
  PRBool  isSelected;
  GetCellDataAt(table, startRow, startCol, *getter_AddRefs(curCell),
                curStartRow, curStartCol, rowSpan, colSpan, isSelected);
  if (!curCell) return NS_ERROR_FAILURE;

  // Use column after current cell if requested
  if (aAfter)
    startCol += colSpan;
   
  PRInt32 rowCount, colCount, row;
  if (NS_FAILED(GetTableSize(table, rowCount, colCount)))
    return NS_ERROR_FAILURE;

  PRInt32 lastColumn = colCount - 1;

  nsAutoEditBatch beginBatching(this);

  for ( row = 0; row < rowCount; row++)
  {
    if (startCol < colCount)
    {
      // We are insert before an existing column
      GetCellDataAt(table, row, startCol, *getter_AddRefs(curCell),
                    curStartRow, curStartCol, rowSpan, colSpan, isSelected);
      // Don't fail entire process if we fail to find a cell
      //  (may fail just in particular rows with < adequate cells per row)
      if (curCell)
      {
        if (curStartCol < startCol)
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
      GetCellDataAt(table, row, lastColumn, *getter_AddRefs(curCell),
                    curStartRow, curStartCol, rowSpan, colSpan, isSelected);
      if( curCell)
      {
        selection->Collapse(curCell, 0);
        res = InsertTableCell(aNumber, PR_TRUE);
      }
    }
  }
  SetCaretAfterTableEdit(table, startRow, startCol, ePreviousColumn);  

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableRow(PRInt32 aNumber, PRBool aAfter)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRow, startCol;
  nsresult res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRow, startCol);
  if (NS_FAILED(res)) return res;

  // Get more data for current cell in row we are inserting at (we need COLSPAN)
  nsCOMPtr<nsIDOMElement> curCell;
  PRInt32 curStartRow, curStartCol, rowSpan, colSpan;
  PRBool  isSelected;
  res = GetCellDataAt(table, startRow, startCol, *getter_AddRefs(curCell),
                      curStartRow, curStartCol, rowSpan, colSpan, isSelected);
  if (NS_FAILED(res)) return res;
  if (!curCell) return NS_ERROR_FAILURE;
  
  // Use row after current cell if requested
  if (aAfter)
    startRow += rowSpan;

  nsCOMPtr<nsIDOMElement> parentRow;
  GetElementOrParentByTagName("tr", curCell, getter_AddRefs(parentRow));
  if (!parentRow) return NS_ERROR_NULL_POINTER;

  PRInt32 rowCount, colCount;
  if (NS_FAILED(GetTableSize(table, rowCount, colCount)))
    return NS_ERROR_FAILURE;

  // Get the parent and offset where we will insert new row(s)
  nsCOMPtr<nsIDOMNode> parentOfRow;
  PRInt32 newRowOffset;
  parentRow->GetParentNode(getter_AddRefs(parentOfRow));
  if (!parentOfRow) return NS_ERROR_NULL_POINTER;
  res = GetChildOffset(parentRow, parentOfRow, newRowOffset);
  if (NS_FAILED(res)) return res;
  if (!parentOfRow)   return NS_ERROR_NULL_POINTER;

  // offset to use for new row insert
  if (aAfter)
    newRowOffset += rowSpan;

  nsAutoEditBatch beginBatching(this);

  PRInt32 cellsInRow = 0;
  if (startRow < rowCount)
  {
    // We are inserting above an existing row
    // Get each cell in the insert row to adjust for COLSPAN effects while we
    //   count how many cells are needed
    PRInt32 colIndex = 0;
    // This returns NS_TABLELAYOUT_CELL_NOT_FOUND when we run past end of row,
    //   which passes the NS_SUCCEEDED macro
    while ( NS_OK == GetCellDataAt(table, newRowOffset, colIndex, *getter_AddRefs(curCell), 
                                   curStartRow, curStartCol, rowSpan, colSpan, isSelected) )
    {
      if (curCell)
      {
        if (curStartRow < startRow)
        {
          // We have a cell spanning this location
          // Simply increase its colspan to keep table rectangular
          nsAutoString newRowSpan;
          newRowSpan.Append(rowSpan+aNumber, 10);
          SetAttribute(curCell, "rowspan", newRowSpan);
        } else {
          // Count the number of cells we need to add to the new row
          cellsInRow++;
        }
      }
      // Next cell in row
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
  SetCaretAfterTableEdit(table, startRow, startCol, ePreviousRow);

  return res;
}

// This is an internal helper (not exposed in IDL)
NS_IMETHODIMP
nsHTMLEditor::DeleteTable(nsCOMPtr<nsIDOMElement> &table, nsCOMPtr<nsIDOMSelection> &selection)
{
  nsCOMPtr<nsIDOMNode> tableParent;
  PRInt32 tableOffset;
  if(NS_FAILED(table->GetParentNode(getter_AddRefs(tableParent))) || !tableParent)
    return NS_ERROR_FAILURE;

  // Save offset we need to restore the selection
  if(NS_FAILED(GetChildOffset(table, tableParent, tableOffset)))
    return NS_ERROR_FAILURE;

  nsresult res = DeleteNode(table);

  // Place selection just before the table
  selection->Collapse(tableParent, tableOffset);
  
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTable()
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRow, startCol;
  nsresult res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRow, startCol);
    
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
  PRInt32 cellOffset, startRow, startCol;
  nsresult res = NS_OK;

  nsAutoEditBatch beginBatching(this);

  for (PRInt32 i = 0; i < aNumber; i++)
  {
    res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRow, startCol);
  
    if (NS_SUCCEEDED(res) && cell)
    {
      // We clear the selection to avoid problems when nodes in the selection are deleted,
      // Be sure to set it correctly later (in SetCaretAfterTableEdit)!
      selection->ClearSelection();
      nsCOMPtr<nsIDOMElement> parentRow;
      
      if (IsOnlyCellInRow(cell, getter_AddRefs(parentRow)))
      {
        // We should delete the row instead,
        //  but first check if its the only row left
        //  so we can delete the entire table
        if (IsOnlyRowInTable(parentRow, table))
          return DeleteTable(table, selection);
        
        // Delete the row
        res = DeleteNode(parentRow);
      } else {
        res = DeleteNode(cell);
        // If we fail, don't try to delete any more cells???
        if (NS_FAILED(res)) break;
      }
      if NS_FAILED(SetCaretAfterTableEdit(table, startRow, startCol, eNoSearch))
      {
        break;
      }
    }
  }
  SetCaretAfterTableEdit(table, startRow, startCol, ePreviousColumn);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableColumn(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableRow(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  
    res = layoutObject->QueryInterface(nsITableCellLayout::GetIID(), (void**)(&cellLayoutObject));
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
    
    res = layoutObject->QueryInterface(nsITableLayout::GetIID(), 
                            (void**)(tableLayoutObject)); 
  }
  return res;
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
    if (NS_SUCCEEDED(res) && table)
      aTable = table;
    else
      return NS_ERROR_FAILURE;
  }
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayoutObject;
  res = GetTableLayoutObject(aTable, &tableLayoutObject);
  if ((NS_SUCCEEDED(res)) && (nsnull!=tableLayoutObject)) 
  {
    res = tableLayoutObject->GetTableSize(aRowCount, aColCount); 
  }
  return res;
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
    // Get the selected talbe or the table enclosing the selection anchor
    nsCOMPtr<nsIDOMElement> table;
    res = GetElementOrParentByTagName("table", nsnull, getter_AddRefs(table));
    if (NS_SUCCEEDED(res) && table)
      aTable = table;
    else
      return NS_ERROR_FAILURE;
  }
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayoutObject;
  res = GetTableLayoutObject(aTable, &tableLayoutObject);
  if ((NS_SUCCEEDED(res)) && (nsnull!=tableLayoutObject)) 
  {
    // Note that this returns NS_TABLELAYOUT_CELL_NOT_FOUND when
    //  the index(es) are out of bounds
    res = tableLayoutObject->GetCellDataAt(aRowIndex, aColIndex, aCell, 
                                              aStartRowIndex, aStartColIndex,
                                              aRowSpan, aColSpan, aIsSelected);
  } 
  return res;
}

// When all you want is the cell
NS_IMETHODIMP 
nsHTMLEditor::GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell)
{
  PRInt32 aStartRowIndex, aStartColIndex, aRowSpan, aColSpan;
  PRBool  aIsSelected;
  return GetCellDataAt(aTable, aRowIndex, aColIndex, aCell, 
                       aStartRowIndex, aStartColIndex, aRowSpan, aColSpan, aIsSelected);
}

NS_IMETHODIMP
nsHTMLEditor::GetCellContext(nsCOMPtr<nsIDOMSelection> &aSelection,
                             nsCOMPtr<nsIDOMElement> &aTable, nsCOMPtr<nsIDOMElement> &aCell, 
                             nsCOMPtr<nsIDOMNode> &aCellParent, PRInt32& aCellOffset, 
                             PRInt32& aRow, PRInt32& aCol)
{
  nsresult res = nsEditor::GetSelection(getter_AddRefs(aSelection));
  if (NS_FAILED(res) || !aSelection)
    return res;

  // Find the first selected cell
  res = GetFirstSelectedCell(aCell);
  if (NS_FAILED(res))
    return res;

  if (!aCell)
  {
    //If a cell wasn't selected, then assume the selection is INSIDE 
    //  and use anchor node to search up to the containing cell
    nsCOMPtr<nsIDOMNode> anchorNode;
    if(NS_FAILED(aSelection->GetAnchorNode(getter_AddRefs(anchorNode))) || !anchorNode)
      return NS_ERROR_FAILURE;

    // Get the cell enclosing the selection anchor
    if(NS_FAILED(GetElementOrParentByTagName("td", anchorNode, getter_AddRefs(aCell))) || !aCell)
      return NS_ERROR_FAILURE;
  }
  // Get containing table and the immediate parent of the cell
  if(NS_FAILED(GetElementOrParentByTagName("table", aCell, getter_AddRefs(aTable))) || !aTable)
    return NS_ERROR_FAILURE;
  if(NS_FAILED(aCell->GetParentNode(getter_AddRefs(aCellParent))) || !aCellParent)
    return NS_ERROR_FAILURE;

  // Get current cell location so we can put caret back there when done
  res = GetCellIndexes(aCell, aRow, aCol);
  if(NS_FAILED(res))
    return res;

  // And the parent and offsets needed to do an insert
  return GetChildOffset(aCell, aCellParent, aCellOffset);
}

NS_IMETHODIMP 
nsHTMLEditor::GetFirstSelectedCell(nsCOMPtr<nsIDOMElement> &aCell)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
    return res;

  // The most straight forward way of finding a selected cell
  //   is to use the selection iterator
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_SUCCEEDED(res) && enumerator)
  {
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if ((NS_SUCCEEDED(res)) && currentItem)
    {
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      nsCOMPtr<nsIContentIterator> iter;
      res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                  nsIContentIterator::GetIID(), 
                                                  getter_AddRefs(iter));
      if ((NS_SUCCEEDED(res)) && iter)
      {
        iter->Init(range);
        // loop through the content iterator for each content node
        nsCOMPtr<nsIContent> content;

        while (NS_ENUMERATOR_FALSE == iter->IsDone())
        {
          res = iter->CurrentNode(getter_AddRefs(content));
          // Not likely!
          if (NS_FAILED(res))
            return NS_ERROR_FAILURE;

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
    }
  } else {
    printf("Could not create enumerator for GetCellContext\n");
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetCaretAfterTableEdit(nsIDOMElement* aTable, PRInt32 aCol, PRInt32 aRow, SetCaretSearchDirection aDirection)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;
  if (!aTable)
    return res;

  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (!NS_SUCCEEDED(res) || !selection)
  {
#ifdef DEBUG_cmanske
    printf("Selection not found after table manipulation!\n");
#endif
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMElement> cell;
  res = GetCellAt(aTable, aCol, aRow, *getter_AddRefs(cell));
  nsCOMPtr<nsIDOMNode> cellNode = do_QueryInterface(cell);

  if (NS_SUCCEEDED(res))
  {
    if (cell)
    {
      // Set the caret to just before the first child of the cell?
      // TODO: Should we really be placing the caret at the END
      //  of the cell content?
      selection->Collapse(cell, 0);
    } else {
      res = NS_ERROR_FAILURE;
    }
  }
    //TODO: SEARCH FOR NEAREST CELL TO PLACE CARET INTO
  return res;
}

