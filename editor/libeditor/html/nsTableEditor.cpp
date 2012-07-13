/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "mozilla/Assertions.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsEditProperty.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIEditor.h"
#include "nsIFrame.h"
#include "nsIHTMLEditor.h"
#include "nsINode.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"  // For nsISelectionPrivate::TABLESELECTION_ defines
#include "nsISupportsUtils.h"
#include "nsITableCellLayout.h" // For efficient access to table cell
#include "nsITableEditor.h"
#include "nsITableLayout.h"     //  data owned by the table and cell frames
#include "nsLayoutErrors.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nscore.h"
#include "prtypes.h"

using namespace mozilla;

/***************************************************************************
 * stack based helper class for restoring selection after table edit
 */
class NS_STACK_CLASS nsSetSelectionAfterTableEdit
{
  private:
    nsCOMPtr<nsITableEditor> mEd;
    nsCOMPtr<nsIDOMElement> mTable;
    PRInt32 mCol, mRow, mDirection, mSelected;
  public:
    nsSetSelectionAfterTableEdit(nsITableEditor *aEd, nsIDOMElement* aTable, 
                                 PRInt32 aRow, PRInt32 aCol, PRInt32 aDirection, 
                                 bool aSelected) : 
        mEd(do_QueryInterface(aEd))
    { 
      mTable = aTable; 
      mRow = aRow; 
      mCol = aCol; 
      mDirection = aDirection;
      mSelected = aSelected;
    } 
    
    ~nsSetSelectionAfterTableEdit() 
    { 
      if (mEd)
        mEd->SetSelectionAfterTableEdit(mTable, mRow, mCol, mDirection, mSelected);
    }
    // This is needed to abort the caret reset in the destructor
    //  when one method yields control to another
    void CancelSetCaret() {mEd = nsnull; mTable = nsnull;}
};

// Stack-class to turn on/off selection batching for table selection
class NS_STACK_CLASS nsSelectionBatcherForTable
{
private:
  nsCOMPtr<nsISelectionPrivate> mSelection;
public:
  nsSelectionBatcherForTable(nsISelection *aSelection)
  {
    nsCOMPtr<nsISelection> sel(aSelection);
    mSelection = do_QueryInterface(sel);
    if (mSelection)  mSelection->StartBatchChanges();
  }
  virtual ~nsSelectionBatcherForTable() 
  { 
    if (mSelection) mSelection->EndBatchChanges();
  }
};

// Table Editing helper utilities (not exposed in IDL)

NS_IMETHODIMP
nsHTMLEditor::InsertCell(nsIDOMElement *aCell, PRInt32 aRowSpan, PRInt32 aColSpan, 
                         bool aAfter, bool aIsHeader, nsIDOMElement **aNewCell)
{
  NS_ENSURE_TRUE(aCell, NS_ERROR_NULL_POINTER);
  if (aNewCell) *aNewCell = nsnull;

  // And the parent and offsets needed to do an insert
  nsCOMPtr<nsIDOMNode> cellParent;
  nsresult res = aCell->GetParentNode(getter_AddRefs(cellParent));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(cellParent, NS_ERROR_NULL_POINTER);

  PRInt32 cellOffset = GetChildOffset(aCell, cellParent);

  nsCOMPtr<nsIDOMElement> newCell;
  if (aIsHeader)
    res = CreateElementWithDefaults(NS_LITERAL_STRING("th"), getter_AddRefs(newCell));
  else
    res = CreateElementWithDefaults(NS_LITERAL_STRING("td"), getter_AddRefs(newCell));
    
  if(NS_FAILED(res)) return res;
  if(!newCell) return NS_ERROR_FAILURE;

  //Optional: return new cell created
  if (aNewCell)
  {
    *aNewCell = newCell.get();
    NS_ADDREF(*aNewCell);
  }

  if( aRowSpan > 1)
  {
    // Note: Do NOT use editor transaction for this
    nsAutoString newRowSpan;
    newRowSpan.AppendInt(aRowSpan, 10);
    newCell->SetAttribute(NS_LITERAL_STRING("rowspan"), newRowSpan);
  }
  if( aColSpan > 1)
  {
    // Note: Do NOT use editor transaction for this
    nsAutoString newColSpan;
    newColSpan.AppendInt(aColSpan, 10);
    newCell->SetAttribute(NS_LITERAL_STRING("colspan"), newColSpan);
  }
  if(aAfter) cellOffset++;

  //Don't let Rules System change the selection
  nsAutoTxnsConserveSelection dontChangeSelection(this);
  return InsertNode(newCell, cellParent, cellOffset);
}

NS_IMETHODIMP nsHTMLEditor::SetColSpan(nsIDOMElement *aCell, PRInt32 aColSpan)
{
  NS_ENSURE_TRUE(aCell, NS_ERROR_NULL_POINTER);
  nsAutoString newSpan;
  newSpan.AppendInt(aColSpan, 10);
  return SetAttribute(aCell, NS_LITERAL_STRING("colspan"), newSpan);
}

NS_IMETHODIMP nsHTMLEditor::SetRowSpan(nsIDOMElement *aCell, PRInt32 aRowSpan)
{
  NS_ENSURE_TRUE(aCell, NS_ERROR_NULL_POINTER);
  nsAutoString newSpan;
  newSpan.AppendInt(aRowSpan, 10);
  return SetAttribute(aCell, NS_LITERAL_STRING("rowspan"), newSpan);
}

/****************************************************************/

// Table Editing interface methods

NS_IMETHODIMP
nsHTMLEditor::InsertTableCell(PRInt32 aNumber, bool aAfter)
{
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> curCell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRowIndex, startColIndex;
  nsresult res = GetCellContext(nsnull,
                                getter_AddRefs(table), 
                                getter_AddRefs(curCell), 
                                getter_AddRefs(cellParent), &cellOffset,
                                &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if no cell found
  NS_ENSURE_TRUE(curCell, NS_EDITOR_ELEMENT_NOT_FOUND);

  // Get more data for current cell in row we are inserting at (we need COLSPAN)
  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  res = GetCellDataAt(table, startRowIndex, startColIndex,
                      getter_AddRefs(curCell),
                      &curStartRowIndex, &curStartColIndex, &rowSpan, &colSpan,
                      &actualRowSpan, &actualColSpan, &isSelected);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(curCell, NS_ERROR_FAILURE);
  PRInt32 newCellIndex = aAfter ? (startColIndex+colSpan) : startColIndex;
  //We control selection resetting after the insert...
  nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, newCellIndex, ePreviousColumn, false);
  //...so suppress Rules System selection munging
  nsAutoTxnsConserveSelection dontChangeSelection(this);

  PRInt32 i;
  for (i = 0; i < aNumber; i++)
  {
    nsCOMPtr<nsIDOMElement> newCell;
    res = CreateElementWithDefaults(NS_LITERAL_STRING("td"), getter_AddRefs(newCell));
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
nsHTMLEditor::GetFirstRow(nsIDOMElement* aTableElement, nsIDOMNode** aRowNode)
{
  NS_ENSURE_TRUE(aRowNode, NS_ERROR_NULL_POINTER);

  *aRowNode = nsnull;  

  NS_ENSURE_TRUE(aTableElement, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> tableElement;
  nsresult res = GetElementOrParentByTagName(NS_LITERAL_STRING("table"), aTableElement, getter_AddRefs(tableElement));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(tableElement, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> tableChild;
  res = tableElement->GetFirstChild(getter_AddRefs(tableChild));
  NS_ENSURE_SUCCESS(res, res);

  while (tableChild)
  {
    nsCOMPtr<nsIContent> content = do_QueryInterface(tableChild);
    if (content)
    {
      nsIAtom *atom = content->Tag();

      if (atom == nsEditProperty::tr)
      {
        // Found a row directly under <table>
        *aRowNode = tableChild;
        NS_ADDREF(*aRowNode);
        return NS_OK;
      }
      // Look for row in one of the row container elements      
      if (atom == nsEditProperty::tbody ||
          atom == nsEditProperty::thead ||
          atom == nsEditProperty::tfoot)
      {
        nsCOMPtr<nsIDOMNode> rowNode;
        res = tableChild->GetFirstChild(getter_AddRefs(rowNode));
        NS_ENSURE_SUCCESS(res, res);
        
        // We can encounter textnodes here -- must find a row
        while (rowNode && !nsHTMLEditUtils::IsTableRow(rowNode))
        {
          nsCOMPtr<nsIDOMNode> nextNode;
          res = rowNode->GetNextSibling(getter_AddRefs(nextNode));
          NS_ENSURE_SUCCESS(res, res);

          rowNode = nextNode;
        }
        if(rowNode)
        {
          *aRowNode = rowNode.get();
          NS_ADDREF(*aRowNode);
          return NS_OK;
        }
      }
    }
    // Here if table child was a CAPTION or COLGROUP
    //  or child of a row parent wasn't a row (bad HTML?),
    //  or first child was a textnode
    // Look in next table child
    nsCOMPtr<nsIDOMNode> nextChild;
    res = tableChild->GetNextSibling(getter_AddRefs(nextChild));
    NS_ENSURE_SUCCESS(res, res);

    tableChild = nextChild;
  };
  // If here, row was not found
  return NS_EDITOR_ELEMENT_NOT_FOUND;
}

NS_IMETHODIMP 
nsHTMLEditor::GetNextRow(nsIDOMNode* aCurrentRowNode, nsIDOMNode **aRowNode)
{
  NS_ENSURE_TRUE(aRowNode, NS_ERROR_NULL_POINTER);

  *aRowNode = nsnull;  

  NS_ENSURE_TRUE(aCurrentRowNode, NS_ERROR_NULL_POINTER);

  if (!nsHTMLEditUtils::IsTableRow(aCurrentRowNode))
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMNode> nextRow;
  nsresult res = aCurrentRowNode->GetNextSibling(getter_AddRefs(nextRow));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMNode> nextNode;

  // Skip over any textnodes here
  while (nextRow && !nsHTMLEditUtils::IsTableRow(nextRow))
  {
    res = nextRow->GetNextSibling(getter_AddRefs(nextNode));
    NS_ENSURE_SUCCESS(res, res);
  
    nextRow = nextNode;
  }
  if(nextRow)
  {
    *aRowNode = nextRow.get();
    NS_ADDREF(*aRowNode);
    return NS_OK;
  }

  // No row found, search for rows in other table sections
  nsCOMPtr<nsIDOMNode> rowParent;
  res = aCurrentRowNode->GetParentNode(getter_AddRefs(rowParent));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(rowParent, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> parentSibling;
  res = rowParent->GetNextSibling(getter_AddRefs(parentSibling));
  NS_ENSURE_SUCCESS(res, res);

  while (parentSibling)
  {
    res = parentSibling->GetFirstChild(getter_AddRefs(nextRow));
    NS_ENSURE_SUCCESS(res, res);
  
    // We can encounter textnodes here -- must find a row
    while (nextRow && !nsHTMLEditUtils::IsTableRow(nextRow))
    {
      res = nextRow->GetNextSibling(getter_AddRefs(nextNode));
      NS_ENSURE_SUCCESS(res, res);

      nextRow = nextNode;
    }
    if(nextRow)
    {
      *aRowNode = nextRow.get();
      NS_ADDREF(*aRowNode);
      return NS_OK;
    }
#ifdef DEBUG_cmanske
    printf("GetNextRow: firstChild of row's parent's sibling is not a TR!\n");
#endif
    // We arrive here only if a table section has no children 
    //  or first child of section is not a row (bad HTML or more "_moz_text" nodes!)
    // So look for another section sibling
    res = parentSibling->GetNextSibling(getter_AddRefs(nextNode));
    NS_ENSURE_SUCCESS(res, res);

    parentSibling = nextNode;
  }
  // If here, row was not found
  return NS_EDITOR_ELEMENT_NOT_FOUND;
}

NS_IMETHODIMP 
nsHTMLEditor::GetLastCellInRow(nsIDOMNode* aRowNode, nsIDOMNode** aCellNode)
{
  NS_ENSURE_TRUE(aCellNode, NS_ERROR_NULL_POINTER);

  *aCellNode = nsnull;

  NS_ENSURE_TRUE(aRowNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> rowChild;
  nsresult res = aRowNode->GetLastChild(getter_AddRefs(rowChild));
  NS_ENSURE_SUCCESS(res, res);

  while (rowChild && !nsHTMLEditUtils::IsTableCell(rowChild))
  {
    // Skip over textnodes
    nsCOMPtr<nsIDOMNode> previousChild;
    res = rowChild->GetPreviousSibling(getter_AddRefs(previousChild));
    NS_ENSURE_SUCCESS(res, res);

    rowChild = previousChild;
  };
  if (rowChild)
  {
    *aCellNode = rowChild.get();
    NS_ADDREF(*aCellNode);
    return NS_OK;
  }
  // If here, cell was not found
  return NS_EDITOR_ELEMENT_NOT_FOUND;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableColumn(PRInt32 aNumber, bool aAfter)
{
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> curCell;
  PRInt32 startRowIndex, startColIndex;
  nsresult res = GetCellContext(getter_AddRefs(selection),
                                getter_AddRefs(table), 
                                getter_AddRefs(curCell), 
                                nsnull, nsnull,
                                &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if no cell found
  NS_ENSURE_TRUE(curCell, NS_EDITOR_ELEMENT_NOT_FOUND);

  // Get more data for current cell (we need ROWSPAN)
  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  res = GetCellDataAt(table, startRowIndex, startColIndex,
                      getter_AddRefs(curCell),
                      &curStartRowIndex, &curStartColIndex,
                      &rowSpan, &colSpan, 
                      &actualRowSpan, &actualColSpan, &isSelected);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(curCell, NS_ERROR_FAILURE);

  nsAutoEditBatch beginBatching(this);
  // Prevent auto insertion of BR in new cell until we're done
  nsAutoRules beginRulesSniffing(this, kOpInsertNode, nsIEditor::eNext);

  // Use column after current cell if requested
  if (aAfter)
  {
    startColIndex += actualColSpan;
    //Detect when user is adding after a COLSPAN=0 case
    // Assume they want to stop the "0" behavior and
    // really add a new column. Thus we set the 
    // colspan to its true value
    if (colSpan == 0)
      SetColSpan(curCell, actualColSpan);
  }
   
  PRInt32 rowCount, colCount, rowIndex;
  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  //We reset caret in destructor...
  nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousRow, false);
  //.. so suppress Rules System selection munging
  nsAutoTxnsConserveSelection dontChangeSelection(this);

  // If we are inserting after all existing columns
  // Make sure table is "well formed"
  //  before appending new column
  if (startColIndex >= colCount)
    NormalizeTable(table);

  nsCOMPtr<nsIDOMNode> rowNode;
  for ( rowIndex = 0; rowIndex < rowCount; rowIndex++)
  {
#ifdef DEBUG_cmanske
    if (rowIndex == rowCount-1)
      printf(" ***InsertTableColumn: Inserting cell at last row: %d\n", rowIndex);
#endif

    if (startColIndex < colCount)
    {
      // We are inserting before an existing column
      res = GetCellDataAt(table, rowIndex, startColIndex,
                          getter_AddRefs(curCell),
                          &curStartRowIndex, &curStartColIndex,
                          &rowSpan, &colSpan, 
                          &actualRowSpan, &actualColSpan, &isSelected);
      NS_ENSURE_SUCCESS(res, res);

      // Don't fail entire process if we fail to find a cell
      //  (may fail just in particular rows with < adequate cells per row)
      if (curCell)
      {
        if (curStartColIndex < startColIndex)
        {
          // We have a cell spanning this location
          // Simply increase its colspan to keep table rectangular
          // Note: we do nothing if colsSpan=0,
          //  since it should automatically span the new column
          if (colSpan > 0)
            SetColSpan(curCell, colSpan+aNumber);
        } else {
          // Simply set selection to the current cell 
          //  so we can let InsertTableCell() do the work
          // Insert a new cell before current one
          selection->Collapse(curCell, 0);
          res = InsertTableCell(aNumber, false);
        }
      }
    } else {
      // Get current row and append new cells after last cell in row
      if(rowIndex == 0)
        res = GetFirstRow(table.get(), getter_AddRefs(rowNode));
      else
      {
        nsCOMPtr<nsIDOMNode> nextRow;
        res = GetNextRow(rowNode.get(), getter_AddRefs(nextRow));
        rowNode = nextRow;
      }
      NS_ENSURE_SUCCESS(res, res);

      if (rowNode)
      {
        nsCOMPtr<nsIDOMNode> lastCell;
        res = GetLastCellInRow(rowNode, getter_AddRefs(lastCell));
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_TRUE(lastCell, NS_ERROR_FAILURE);

        curCell = do_QueryInterface(lastCell);
        if (curCell)
        {
          // Simply add same number of cells to each row
          // Although tempted to check cell indexes for curCell,
          //  the effects of COLSPAN>1 in some cells makes this futile!
          // We must use NormalizeTable first to assure
          //  that there are cells in each cellmap location
          selection->Collapse(curCell, 0);
          res = InsertTableCell(aNumber, true);
        }
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableRow(PRInt32 aNumber, bool aAfter)
{
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> curCell;
  
  PRInt32 startRowIndex, startColIndex;
  nsresult res = GetCellContext(nsnull,
                                getter_AddRefs(table), 
                                getter_AddRefs(curCell), 
                                nsnull, nsnull, 
                                &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if no cell found
  NS_ENSURE_TRUE(curCell, NS_EDITOR_ELEMENT_NOT_FOUND);

  // Get more data for current cell in row we are inserting at (we need COLSPAN)
  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  res = GetCellDataAt(table, startRowIndex, startColIndex,
                      getter_AddRefs(curCell),
                      &curStartRowIndex, &curStartColIndex,
                      &rowSpan, &colSpan, 
                      &actualRowSpan, &actualColSpan, &isSelected);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(curCell, NS_ERROR_FAILURE);
  
  PRInt32 rowCount, colCount;
  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  nsAutoEditBatch beginBatching(this);
  // Prevent auto insertion of BR in new cell until we're done
  nsAutoRules beginRulesSniffing(this, kOpInsertNode, nsIEditor::eNext);

  if (aAfter)
  {
    // Use row after current cell
    startRowIndex += actualRowSpan;

    //Detect when user is adding after a ROWSPAN=0 case
    // Assume they want to stop the "0" behavior and
    // really add a new row. Thus we set the 
    // rowspan to its true value
    if (rowSpan == 0)
      SetRowSpan(curCell, actualRowSpan);
  }

  //We control selection resetting after the insert...
  nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn, false);
  //...so suppress Rules System selection munging
  nsAutoTxnsConserveSelection dontChangeSelection(this);

  nsCOMPtr<nsIDOMElement> cellForRowParent;
  PRInt32 cellsInRow = 0;
  if (startRowIndex < rowCount)
  {
    // We are inserting above an existing row
    // Get each cell in the insert row to adjust for COLSPAN effects while we
    //   count how many cells are needed
    PRInt32 colIndex = 0;
    // This returns NS_TABLELAYOUT_CELL_NOT_FOUND when we run past end of row,
    //   which passes the NS_SUCCEEDED macro
    while ( NS_OK == GetCellDataAt(table, startRowIndex, colIndex,
                                   getter_AddRefs(curCell), 
                                   &curStartRowIndex, &curStartColIndex,
                                   &rowSpan, &colSpan, 
                                   &actualRowSpan, &actualColSpan,
                                   &isSelected) )
    {
      if (curCell)
      {
        if (curStartRowIndex < startRowIndex)
        {
          // We have a cell spanning this location
          // Simply increase its rowspan
          //Note that if rowSpan == 0, we do nothing,
          //  since that cell should automatically extend into the new row
          if (rowSpan > 0)
            SetRowSpan(curCell, rowSpan+aNumber);
        } else {
          // We have a cell in the insert row

          // Count the number of cells we need to add to the new row
          cellsInRow += actualColSpan;

          // Save cell we will use below
          if (!cellForRowParent)
            cellForRowParent = curCell;
        }
        // Next cell in row
        colIndex += actualColSpan;
      }
      else
        colIndex++;
    }
  } else {
    // We are adding a new row after all others
    // If it weren't for colspan=0 effect, 
    // we could simply use colCount for number of new cells...
    cellsInRow = colCount;
    
    // ...but we must compensate for all cells with rowSpan = 0 in the last row
    PRInt32 lastRow = rowCount-1;
    PRInt32 tempColIndex = 0;
    while ( NS_OK == GetCellDataAt(table, lastRow, tempColIndex,
                                   getter_AddRefs(curCell), 
                                   &curStartRowIndex, &curStartColIndex,
                                   &rowSpan, &colSpan, 
                                   &actualRowSpan, &actualColSpan,
                                   &isSelected) )
    {
      if (rowSpan == 0)
        cellsInRow -= actualColSpan;
      
      tempColIndex += actualColSpan;

      // Save cell from the last row that we will use below
      if (!cellForRowParent && curStartRowIndex == lastRow)
        cellForRowParent = curCell;
    }
  }

  if (cellsInRow > 0)
  {
    // The row parent and offset where we will insert new row
    nsCOMPtr<nsIDOMNode> parentOfRow;
    PRInt32 newRowOffset;

    NS_NAMED_LITERAL_STRING(trStr, "tr");
    if (cellForRowParent)
    {
      nsCOMPtr<nsIDOMElement> parentRow;
      res = GetElementOrParentByTagName(trStr, cellForRowParent, getter_AddRefs(parentRow));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(parentRow, NS_ERROR_NULL_POINTER);

      parentRow->GetParentNode(getter_AddRefs(parentOfRow));
      NS_ENSURE_TRUE(parentOfRow, NS_ERROR_NULL_POINTER);

      newRowOffset = GetChildOffset(parentRow, parentOfRow);
      
      // Adjust for when adding past the end 
      if (aAfter && startRowIndex >= rowCount)
        newRowOffset++;
    }
    else
      return NS_ERROR_FAILURE;

    for (PRInt32 row = 0; row < aNumber; row++)
    {
      // Create a new row
      nsCOMPtr<nsIDOMElement> newRow;
      res = CreateElementWithDefaults(trStr, getter_AddRefs(newRow));
      if (NS_SUCCEEDED(res))
      {
        NS_ENSURE_TRUE(newRow, NS_ERROR_FAILURE);
      
        for (PRInt32 i = 0; i < cellsInRow; i++)
        {
          nsCOMPtr<nsIDOMElement> newCell;
          res = CreateElementWithDefaults(NS_LITERAL_STRING("td"), getter_AddRefs(newCell));
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_TRUE(newCell, NS_ERROR_FAILURE);

          // Don't use transaction system yet! (not until entire row is inserted)
          nsCOMPtr<nsIDOMNode>resultNode;
          res = newRow->AppendChild(newCell, getter_AddRefs(resultNode));
          NS_ENSURE_SUCCESS(res, res);
        }
        // Use transaction system to insert the entire row+cells
        // (Note that rows are inserted at same childoffset each time)
        res = InsertNode(newRow, parentOfRow, newRowOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  return res;
}

// Editor helper only
// XXX Code changed for bug 217717 and now we don't need aSelection param
//     TODO: Remove aSelection param
NS_IMETHODIMP
nsHTMLEditor::DeleteTable2(nsIDOMElement *aTable, nsISelection *aSelection)
{
  NS_ENSURE_TRUE(aTable, NS_ERROR_NULL_POINTER);

  // Select the table
  nsresult res = ClearSelection();
  if (NS_SUCCEEDED(res))
    res = AppendNodeToSelectionAsRange(aTable);
  NS_ENSURE_SUCCESS(res, res);

  return DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTable()
{
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsresult res = GetCellContext(getter_AddRefs(selection),
                                getter_AddRefs(table), 
                                nsnull, nsnull, nsnull, nsnull, nsnull);
    
  NS_ENSURE_SUCCESS(res, res);

  nsAutoEditBatch beginBatching(this);
  return DeleteTable2(table, selection);
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableCell(PRInt32 aNumber)
{
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex;


  nsresult res = GetCellContext(getter_AddRefs(selection),
                         getter_AddRefs(table), 
                         getter_AddRefs(cell), 
                         nsnull, nsnull,
                         &startRowIndex, &startColIndex);

  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if we didn't find a table or cell
  NS_ENSURE_TRUE(table && cell, NS_EDITOR_ELEMENT_NOT_FOUND);

  nsAutoEditBatch beginBatching(this);
  // Prevent rules testing until we're done
  nsAutoRules beginRulesSniffing(this, kOpDeleteNode, nsIEditor::eNext);

  nsCOMPtr<nsIDOMElement> firstCell;
  nsCOMPtr<nsIDOMRange> range;
  res = GetFirstSelectedCell(getter_AddRefs(range), getter_AddRefs(firstCell));
  NS_ENSURE_SUCCESS(res, res);

  PRInt32 rangeCount;
  res = selection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);

  if (firstCell && rangeCount > 1)
  {
    // When > 1 selected cell,
    //  ignore aNumber and use selected cells
    cell = firstCell;

    PRInt32 rowCount, colCount;
    res = GetTableSize(table, &rowCount, &colCount);
    NS_ENSURE_SUCCESS(res, res);

    // Get indexes -- may be different than original cell
    res = GetCellIndexes(cell, &startRowIndex, &startColIndex);
    NS_ENSURE_SUCCESS(res, res);

    // The setCaret object will call SetSelectionAfterTableEdit in its destructor
    nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn, false);
    nsAutoTxnsConserveSelection dontChangeSelection(this);

    bool    checkToDeleteRow = true;
    bool    checkToDeleteColumn = true;
    while (cell)
    {
      bool deleteRow = false;
      bool deleteCol = false;

      if (checkToDeleteRow)
      {
        // Optimize to delete an entire row
        // Clear so we don't repeat AllCellsInRowSelected within the same row
        checkToDeleteRow = false;

        deleteRow = AllCellsInRowSelected(table, startRowIndex, colCount);
        if (deleteRow)
        {
          // First, find the next cell in a different row
          //   to continue after we delete this row
          PRInt32 nextRow = startRowIndex;
          while (nextRow == startRowIndex)
          {
            res = GetNextSelectedCell(nsnull, getter_AddRefs(cell));
            NS_ENSURE_SUCCESS(res, res);
            if (!cell) break;
            res = GetCellIndexes(cell, &nextRow, &startColIndex);
            NS_ENSURE_SUCCESS(res, res);
          }
          // Delete entire row
          res = DeleteRow(table, startRowIndex);          
          NS_ENSURE_SUCCESS(res, res);

          if (cell)
          {
            // For the next cell: Subtract 1 for row we deleted
            startRowIndex = nextRow - 1;
            // Set true since we know we will look at a new row next
            checkToDeleteRow = true;
          }
        }
      }
      if (!deleteRow)
      {
        if (checkToDeleteColumn)
        {
          // Optimize to delete an entire column
          // Clear this so we don't repeat AllCellsInColSelected within the same Col
          checkToDeleteColumn = false;

          deleteCol = AllCellsInColumnSelected(table, startColIndex, colCount);
          if (deleteCol)
          {
            // First, find the next cell in a different column
            //   to continue after we delete this column
            PRInt32 nextCol = startColIndex;
            while (nextCol == startColIndex)
            {
              res = GetNextSelectedCell(nsnull, getter_AddRefs(cell));
              NS_ENSURE_SUCCESS(res, res);
              if (!cell) break;
              res = GetCellIndexes(cell, &startRowIndex, &nextCol);
              NS_ENSURE_SUCCESS(res, res);
            }
            // Delete entire Col
            res = DeleteColumn(table, startColIndex);          
            NS_ENSURE_SUCCESS(res, res);
            if (cell) 
            {
              // For the next cell, subtract 1 for col. deleted
              startColIndex = nextCol - 1;
              // Set true since we know we will look at a new column next
              checkToDeleteColumn = true;
            }
          }
        }
        if (!deleteCol)
        {
          // First get the next cell to delete
          nsCOMPtr<nsIDOMElement> nextCell;
          res = GetNextSelectedCell(getter_AddRefs(range), getter_AddRefs(nextCell));
          NS_ENSURE_SUCCESS(res, res);

          // Then delete the cell
          res = DeleteNode(cell);
          NS_ENSURE_SUCCESS(res, res);
          
          // The next cell to delete
          cell = nextCell;
          if (cell)
          {
            res = GetCellIndexes(cell, &startRowIndex, &startColIndex);
            NS_ENSURE_SUCCESS(res, res);
          }
        }
      }
    }
  }
  else for (PRInt32 i = 0; i < aNumber; i++)
  {
    res = GetCellContext(getter_AddRefs(selection),
                         getter_AddRefs(table), 
                         getter_AddRefs(cell), 
                         nsnull, nsnull,
                         &startRowIndex, &startColIndex);
    NS_ENSURE_SUCCESS(res, res);
    // Don't fail if no cell found
    NS_ENSURE_TRUE(cell, NS_EDITOR_ELEMENT_NOT_FOUND);

    if (1 == GetNumberOfCellsInRow(table, startRowIndex))
    {
      nsCOMPtr<nsIDOMElement> parentRow;
      res = GetElementOrParentByTagName(NS_LITERAL_STRING("tr"), cell, getter_AddRefs(parentRow));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(parentRow, NS_ERROR_NULL_POINTER);

      // We should delete the row instead,
      //  but first check if its the only row left
      //  so we can delete the entire table
      PRInt32 rowCount, colCount;
      res = GetTableSize(table, &rowCount, &colCount);
      NS_ENSURE_SUCCESS(res, res);
      
      if (rowCount == 1)
        return DeleteTable2(table, selection);
    
      // We need to call DeleteTableRow to handle cells with rowspan 
      res = DeleteTableRow(1);
      NS_ENSURE_SUCCESS(res, res);
    } 
    else
    {
      // More than 1 cell in the row

      // The setCaret object will call SetSelectionAfterTableEdit in its destructor
      nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn, false);
      nsAutoTxnsConserveSelection dontChangeSelection(this);

      res = DeleteNode(cell);
      // If we fail, don't try to delete any more cells???
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableCellContents()
{
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex;
  nsresult res;
  res = GetCellContext(getter_AddRefs(selection),
                       getter_AddRefs(table), 
                       getter_AddRefs(cell), 
                       nsnull, nsnull,
                       &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if no cell found
  NS_ENSURE_TRUE(cell, NS_EDITOR_ELEMENT_NOT_FOUND);


  nsAutoEditBatch beginBatching(this);
  // Prevent rules testing until we're done
  nsAutoRules beginRulesSniffing(this, kOpDeleteNode, nsIEditor::eNext);
  //Don't let Rules System change the selection
  nsAutoTxnsConserveSelection dontChangeSelection(this);


  nsCOMPtr<nsIDOMElement> firstCell;
  nsCOMPtr<nsIDOMRange> range;
  res = GetFirstSelectedCell(getter_AddRefs(range), getter_AddRefs(firstCell));
  NS_ENSURE_SUCCESS(res, res);


  if (firstCell)
  {
    cell = firstCell;
    res = GetCellIndexes(cell, &startRowIndex, &startColIndex);
    NS_ENSURE_SUCCESS(res, res);
  }

  nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn, false);

  while (cell)
  {
    DeleteCellContents(cell);
    if (firstCell)
    {
      // We doing a selected cells, so do all of them
      res = GetNextSelectedCell(nsnull, getter_AddRefs(cell));
      NS_ENSURE_SUCCESS(res, res);
    }
    else
      cell = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteCellContents(nsIDOMElement *aCell)
{
  NS_ENSURE_TRUE(aCell, NS_ERROR_NULL_POINTER);

  // Prevent rules testing until we're done
  nsAutoRules beginRulesSniffing(this, kOpDeleteNode, nsIEditor::eNext);

  nsCOMPtr<nsIDOMNode> child;
  bool hasChild;
  aCell->HasChildNodes(&hasChild);

  while (hasChild)
  {
    aCell->GetLastChild(getter_AddRefs(child));
    nsresult res = DeleteNode(child);
    NS_ENSURE_SUCCESS(res, res);
    aCell->HasChildNodes(&hasChild);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableColumn(PRInt32 aNumber)
{
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex, rowCount, colCount;
  nsresult res = GetCellContext(getter_AddRefs(selection),
                                getter_AddRefs(table), 
                                getter_AddRefs(cell), 
                                nsnull, nsnull,
                                &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if no cell found
  NS_ENSURE_TRUE(table && cell, NS_EDITOR_ELEMENT_NOT_FOUND);

  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  // Shortcut the case of deleting all columns in table
  if(startColIndex == 0 && aNumber >= colCount)
    return DeleteTable2(table, selection);

  // Check for counts too high
  aNumber = NS_MIN(aNumber,(colCount-startColIndex));

  nsAutoEditBatch beginBatching(this);
  // Prevent rules testing until we're done
  nsAutoRules beginRulesSniffing(this, kOpDeleteNode, nsIEditor::eNext);

  // Test if deletion is controlled by selected cells
  nsCOMPtr<nsIDOMElement> firstCell;
  nsCOMPtr<nsIDOMRange> range;
  res = GetFirstSelectedCell(getter_AddRefs(range), getter_AddRefs(firstCell));
  NS_ENSURE_SUCCESS(res, res);

  PRInt32 rangeCount;
  res = selection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);

  if (firstCell && rangeCount > 1)
  {
    // Fetch indexes again - may be different for selected cells
    res = GetCellIndexes(firstCell, &startRowIndex, &startColIndex);
    NS_ENSURE_SUCCESS(res, res);
  }
  //We control selection resetting after the insert...
  nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousRow, false);

  if (firstCell && rangeCount > 1)
  {
    // Use selected cells to determine what rows to delete
    cell = firstCell;

    while (cell)
    {
      if (cell != firstCell)
      {
        res = GetCellIndexes(cell, &startRowIndex, &startColIndex);
        NS_ENSURE_SUCCESS(res, res);
      }
      // Find the next cell in a different column
      // to continue after we delete this column
      PRInt32 nextCol = startColIndex;
      while (nextCol == startColIndex)
      {
        res = GetNextSelectedCell(getter_AddRefs(range), getter_AddRefs(cell));
        NS_ENSURE_SUCCESS(res, res);
        if (!cell) break;
        res = GetCellIndexes(cell, &startRowIndex, &nextCol);
        NS_ENSURE_SUCCESS(res, res);
      }
      res = DeleteColumn(table, startColIndex);          
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  else for (PRInt32 i = 0; i < aNumber; i++)
  {
    res = DeleteColumn(table, startColIndex);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteColumn(nsIDOMElement *aTable, PRInt32 aColIndex)
{
  NS_ENSURE_TRUE(aTable, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMElement> cellInDeleteCol;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  PRInt32 rowIndex = 0;
  nsresult res = NS_OK;
   
  do {
    res = GetCellDataAt(aTable, rowIndex, aColIndex, getter_AddRefs(cell),
                        &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                        &actualRowSpan, &actualColSpan, &isSelected);

    NS_ENSURE_SUCCESS(res, res);

    if (cell)
    {
      // Find cells that don't start in column we are deleting
      if (startColIndex < aColIndex || colSpan > 1 || colSpan == 0)
      {
        // We have a cell spanning this location
        // Decrease its colspan to keep table rectangular,
        // but if colSpan=0, it will adjust automatically
        if (colSpan > 0)
        {
          NS_ASSERTION((colSpan > 1),"Bad COLSPAN in DeleteTableColumn");
          SetColSpan(cell, colSpan-1);
        }
        if (startColIndex == aColIndex)
        {
          // Cell is in column to be deleted, but must have colspan > 1,
          // so delete contents of cell instead of cell itself
          // (We must have reset colspan above)
          DeleteCellContents(cell);
        }
        // To next cell in column
        rowIndex += actualRowSpan;
      } 
      else 
      {
        // Delete the cell
        if (1 == GetNumberOfCellsInRow(aTable, rowIndex))
        {
          // Only 1 cell in row - delete the row
          nsCOMPtr<nsIDOMElement> parentRow;
          res = GetElementOrParentByTagName(NS_LITERAL_STRING("tr"), cell, getter_AddRefs(parentRow));
          NS_ENSURE_SUCCESS(res, res);
          if(!parentRow) return NS_ERROR_NULL_POINTER;

          //  But first check if its the only row left
          //  so we can delete the entire table
          //  (This should never happen but it's the safe thing to do)
          PRInt32 rowCount, colCount;
          res = GetTableSize(aTable, &rowCount, &colCount);
          NS_ENSURE_SUCCESS(res, res);

          if (rowCount == 1)
          {
            nsCOMPtr<nsISelection> selection;
            res = GetSelection(getter_AddRefs(selection));
            NS_ENSURE_SUCCESS(res, res);
            NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
            return DeleteTable2(aTable, selection);
          }
    
          // Delete the row by placing caret in cell we were to delete
          // We need to call DeleteTableRow to handle cells with rowspan 
          res = DeleteRow(aTable, startRowIndex);
          NS_ENSURE_SUCCESS(res, res);

          // Note that we don't incremenet rowIndex
          // since a row was deleted and "next" 
          // row now has current rowIndex
        } 
        else 
        {
          // A more "normal" deletion
          res = DeleteNode(cell);
          NS_ENSURE_SUCCESS(res, res);

          //Skip over any rows spanned by this cell
          rowIndex += actualRowSpan;
        }
      }
    }
  } while (cell);    

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableRow(PRInt32 aNumber)
{
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex;
  PRInt32 rowCount, colCount;
  nsresult res =  GetCellContext(getter_AddRefs(selection),
                                 getter_AddRefs(table), 
                                 getter_AddRefs(cell), 
                                 nsnull, nsnull,
                                 &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if no cell found
  NS_ENSURE_TRUE(cell, NS_EDITOR_ELEMENT_NOT_FOUND);

  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  // Shortcut the case of deleting all rows in table
  if(startRowIndex == 0 && aNumber >= rowCount)
    return DeleteTable2(table, selection);

  nsAutoEditBatch beginBatching(this);
  // Prevent rules testing until we're done
  nsAutoRules beginRulesSniffing(this, kOpDeleteNode, nsIEditor::eNext);

  nsCOMPtr<nsIDOMElement> firstCell;
  nsCOMPtr<nsIDOMRange> range;
  res = GetFirstSelectedCell(getter_AddRefs(range), getter_AddRefs(firstCell));
  NS_ENSURE_SUCCESS(res, res);

  PRInt32 rangeCount;
  res = selection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);

  if (firstCell && rangeCount > 1)
  {
    // Fetch indexes again - may be different for selected cells
    res = GetCellIndexes(firstCell, &startRowIndex, &startColIndex);
    NS_ENSURE_SUCCESS(res, res);
  }

  //We control selection resetting after the insert...
  nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousRow, false);
  // Don't change selection during deletions
  nsAutoTxnsConserveSelection dontChangeSelection(this);

  if (firstCell && rangeCount > 1)
  {
    // Use selected cells to determine what rows to delete
    cell = firstCell;

    while (cell)
    {
      if (cell != firstCell)
      {
        res = GetCellIndexes(cell, &startRowIndex, &startColIndex);
        NS_ENSURE_SUCCESS(res, res);
      }
      // Find the next cell in a different row
      // to continue after we delete this row
      PRInt32 nextRow = startRowIndex;
      while (nextRow == startRowIndex)
      {
        res = GetNextSelectedCell(getter_AddRefs(range), getter_AddRefs(cell));
        NS_ENSURE_SUCCESS(res, res);
        if (!cell) break;
        res = GetCellIndexes(cell, &nextRow, &startColIndex);
        NS_ENSURE_SUCCESS(res, res);
      }
      // Delete entire row
      res = DeleteRow(table, startRowIndex);          
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  else
  {
    // Check for counts too high
    aNumber = NS_MIN(aNumber,(rowCount-startRowIndex));

    for (PRInt32 i = 0; i < aNumber; i++)
    {
      res = DeleteRow(table, startRowIndex);
      // If failed in current row, try the next
      if (NS_FAILED(res))
        startRowIndex++;
    
      // Check if there's a cell in the "next" row
      res = GetCellAt(table, startRowIndex, startColIndex, getter_AddRefs(cell));
      NS_ENSURE_SUCCESS(res, res);
      if(!cell)
        break;
    }
  }
  return NS_OK;
}

// Helper that doesn't batch or change the selection
NS_IMETHODIMP
nsHTMLEditor::DeleteRow(nsIDOMElement *aTable, PRInt32 aRowIndex)
{
  NS_ENSURE_TRUE(aTable, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMElement> cellInDeleteRow;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  PRInt32 colIndex = 0;
  nsresult res = NS_OK;
   
  // Prevent rules testing until we're done
  nsAutoRules beginRulesSniffing(this, kOpDeleteNode, nsIEditor::eNext);

  // The list of cells we will change rowspan in
  //  and the new rowspan values for each
  nsTArray<nsIDOMElement*> spanCellList;
  nsTArray<PRInt32> newSpanList;

  // Scan through cells in row to do rowspan adjustments
  // Note that after we delete row, startRowIndex will point to the
  //   cells in the next row to be deleted
  do {
    res = GetCellDataAt(aTable, aRowIndex, colIndex, getter_AddRefs(cell),
                        &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                        &actualRowSpan, &actualColSpan, &isSelected);
  
    // We don't fail if we don't find a cell, so this must be real bad
    if(NS_FAILED(res)) return res;

    // Compensate for cells that don't start or extend below the row we are deleting
    if (cell)
    {
      if (startRowIndex < aRowIndex)
      {
        // Cell starts in row above us
        // Decrease its rowspan to keep table rectangular
        //  but we don't need to do this if rowspan=0,
        //  since it will automatically adjust
        if (rowSpan > 0)
        {
          // Build list of cells to change rowspan
          // We can't do it now since it upsets cell map,
          //  so we will do it after deleting the row
          spanCellList.AppendElement(cell);
          newSpanList.AppendElement(NS_MAX((aRowIndex - startRowIndex), actualRowSpan-1));
        }
      }
      else 
      {
        if (rowSpan > 1)
        {
          //Cell spans below row to delete,
          //  so we must insert new cells to keep rows below even
          // Note that we test "rowSpan" so we don't do this if rowSpan = 0 (automatic readjustment)
          res = SplitCellIntoRows(aTable, startRowIndex, startColIndex,
                                  aRowIndex - startRowIndex + 1, // The row above the row to insert new cell into
                                  actualRowSpan - 1, nsnull);    // Span remaining below
          NS_ENSURE_SUCCESS(res, res);
        }
        if (!cellInDeleteRow)
          cellInDeleteRow = cell; // Reference cell to find row to delete
      }
      // Skip over other columns spanned by this cell
      colIndex += actualColSpan;
    }
  } while (cell);

  // Things are messed up if we didn't find a cell in the row!
  NS_ENSURE_TRUE(cellInDeleteRow, NS_ERROR_FAILURE);

  // Delete the entire row
  nsCOMPtr<nsIDOMElement> parentRow;
  res = GetElementOrParentByTagName(NS_LITERAL_STRING("tr"), cellInDeleteRow, getter_AddRefs(parentRow));
  NS_ENSURE_SUCCESS(res, res);

  if (parentRow)
  {
    res = DeleteNode(parentRow);
    NS_ENSURE_SUCCESS(res, res);
  }

  // Now we can set new rowspans for cells stored above  
  for (PRUint32 i = 0, n = spanCellList.Length(); i < n; i++)
  {
    nsIDOMElement *cellPtr = spanCellList[i];
    if (cellPtr)
    {
      res = SetRowSpan(cellPtr, newSpanList[i]);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsHTMLEditor::SelectTable()
{
  nsCOMPtr<nsIDOMElement> table;
  nsresult res = NS_ERROR_FAILURE;
  res = GetElementOrParentByTagName(NS_LITERAL_STRING("table"), nsnull, getter_AddRefs(table));
  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if we didn't find a table
  NS_ENSURE_TRUE(table, NS_OK);

  res = ClearSelection();
  if (NS_SUCCEEDED(res))
    res = AppendNodeToSelectionAsRange(table);

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SelectTableCell()
{
  nsCOMPtr<nsIDOMElement> cell;
  nsresult res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nsnull, getter_AddRefs(cell));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(cell, NS_EDITOR_ELEMENT_NOT_FOUND);

  res = ClearSelection();
  if (NS_SUCCEEDED(res))
    res = AppendNodeToSelectionAsRange(cell);

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SelectBlockOfCells(nsIDOMElement *aStartCell, nsIDOMElement *aEndCell)
{
  NS_ENSURE_TRUE(aStartCell && aEndCell, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  NS_NAMED_LITERAL_STRING(tableStr, "table");
  nsCOMPtr<nsIDOMElement> table;
  res = GetElementOrParentByTagName(tableStr, aStartCell, getter_AddRefs(table));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMElement> endTable;
  res = GetElementOrParentByTagName(tableStr, aEndCell, getter_AddRefs(endTable));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(endTable, NS_ERROR_FAILURE);
  
  // We can only select a block if within the same table,
  //  so do nothing if not within one table
  if (table != endTable) return NS_OK;

  PRInt32 startRowIndex, startColIndex, endRowIndex, endColIndex;

  // Get starting and ending cells' location in the cellmap
  res = GetCellIndexes(aStartCell, &startRowIndex, &startColIndex);
  if(NS_FAILED(res)) return res;

  res = GetCellIndexes(aEndCell, &endRowIndex, &endColIndex);
  if(NS_FAILED(res)) return res;

  // Suppress nsISelectionListener notification
  //  until all selection changes are finished
  nsSelectionBatcherForTable selectionBatcher(selection);

  // Examine all cell nodes in current selection and 
  //  remove those outside the new block cell region
  PRInt32 minColumn = NS_MIN(startColIndex, endColIndex);
  PRInt32 minRow    = NS_MIN(startRowIndex, endRowIndex);
  PRInt32 maxColumn   = NS_MAX(startColIndex, endColIndex);
  PRInt32 maxRow      = NS_MAX(startRowIndex, endRowIndex);

  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 currentRowIndex, currentColIndex;
  nsCOMPtr<nsIDOMRange> range;
  res = GetFirstSelectedCell(getter_AddRefs(range), getter_AddRefs(cell));
  NS_ENSURE_SUCCESS(res, res);
  if (res == NS_EDITOR_ELEMENT_NOT_FOUND) return NS_OK;

  while (cell)
  {
    res = GetCellIndexes(cell, &currentRowIndex, &currentColIndex);
    NS_ENSURE_SUCCESS(res, res);

    if (currentRowIndex < maxRow || currentRowIndex > maxRow || 
        currentColIndex < maxColumn || currentColIndex > maxColumn)
    {
      selection->RemoveRange(range);
      // Since we've removed the range, decrement pointer to next range
      mSelectedCellIndex--;
    }    
    res = GetNextSelectedCell(getter_AddRefs(range), getter_AddRefs(cell));
    NS_ENSURE_SUCCESS(res, res);
  }

  PRInt32 rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  for (PRInt32 row = minRow; row <= maxRow; row++)
  {
    for(PRInt32 col = minColumn; col <= maxColumn; col += NS_MAX(actualColSpan, 1))
    {
      res = GetCellDataAt(table, row, col, getter_AddRefs(cell),
                          &currentRowIndex, &currentColIndex,
                          &rowSpan, &colSpan, 
                          &actualRowSpan, &actualColSpan, &isSelected);
      if (NS_FAILED(res)) break;
      // Skip cells that already selected or are spanned from previous locations
      if (!isSelected && cell && row == currentRowIndex && col == currentColIndex)
      {
        res = AppendNodeToSelectionAsRange(cell);
        if (NS_FAILED(res)) break;
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SelectAllTableCells()
{
  nsCOMPtr<nsIDOMElement> cell;
  nsresult res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nsnull, getter_AddRefs(cell));
  NS_ENSURE_SUCCESS(res, res);
  
  // Don't fail if we didn't find a cell
  NS_ENSURE_TRUE(cell, NS_EDITOR_ELEMENT_NOT_FOUND);

  nsCOMPtr<nsIDOMElement> startCell = cell;
  
  // Get parent table
  nsCOMPtr<nsIDOMElement> table;
  res = GetElementOrParentByTagName(NS_LITERAL_STRING("table"), cell, getter_AddRefs(table));
  NS_ENSURE_SUCCESS(res, res);
  if(!table) return NS_ERROR_NULL_POINTER;

  PRInt32 rowCount, colCount;
  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsISelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  // Suppress nsISelectionListener notification
  //  until all selection changes are finished
  nsSelectionBatcherForTable selectionBatcher(selection);

  // It is now safe to clear the selection
  // BE SURE TO RESET IT BEFORE LEAVING!
  res = ClearSelection();

  // Select all cells in the same column as current cell
  bool cellSelected = false;
  PRInt32 rowSpan, colSpan, actualRowSpan, actualColSpan, currentRowIndex, currentColIndex;
  bool    isSelected;
  for(PRInt32 row = 0; row < rowCount; row++)
  {
    for(PRInt32 col = 0; col < colCount; col += NS_MAX(actualColSpan, 1))
    {
      res = GetCellDataAt(table, row, col, getter_AddRefs(cell),
                          &currentRowIndex, &currentColIndex,
                          &rowSpan, &colSpan, 
                          &actualRowSpan, &actualColSpan, &isSelected);
      if (NS_FAILED(res)) break;
      // Skip cells that are spanned from previous rows or columns
      if (cell && row == currentRowIndex && col == currentColIndex)
      {
        res =  AppendNodeToSelectionAsRange(cell);
        if (NS_FAILED(res)) break;
        cellSelected = true;
      }
    }
  }
  // Safety code to select starting cell if nothing else was selected
  if (!cellSelected)
  {
    return AppendNodeToSelectionAsRange(startCell);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SelectTableRow()
{
  nsCOMPtr<nsIDOMElement> cell;
  nsresult res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nsnull, getter_AddRefs(cell));
  NS_ENSURE_SUCCESS(res, res);
  
  // Don't fail if we didn't find a cell
  NS_ENSURE_TRUE(cell, NS_EDITOR_ELEMENT_NOT_FOUND);
  nsCOMPtr<nsIDOMElement> startCell = cell;

  // Get table and location of cell:
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  PRInt32 startRowIndex, startColIndex;

  res = GetCellContext(getter_AddRefs(selection),
                       getter_AddRefs(table), 
                       getter_AddRefs(cell),
                       nsnull, nsnull,
                       &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);
  
  PRInt32 rowCount, colCount;
  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  //Note: At this point, we could get first and last cells in row,
  //  then call SelectBlockOfCells, but that would take just
  //  a little less code, so the following is more efficient

  // Suppress nsISelectionListener notification
  //  until all selection changes are finished
  nsSelectionBatcherForTable selectionBatcher(selection);

  // It is now safe to clear the selection
  // BE SURE TO RESET IT BEFORE LEAVING!
  res = ClearSelection();

  // Select all cells in the same row as current cell
  bool cellSelected = false;
  PRInt32 rowSpan, colSpan, actualRowSpan, actualColSpan, currentRowIndex, currentColIndex;
  bool    isSelected;
  for(PRInt32 col = 0; col < colCount; col += NS_MAX(actualColSpan, 1))
  {
    res = GetCellDataAt(table, startRowIndex, col, getter_AddRefs(cell),
                        &currentRowIndex, &currentColIndex, &rowSpan, &colSpan, 
                        &actualRowSpan, &actualColSpan, &isSelected);
    if (NS_FAILED(res)) break;
    // Skip cells that are spanned from previous rows or columns
    if (cell && currentRowIndex == startRowIndex && currentColIndex == col)
    {
      res = AppendNodeToSelectionAsRange(cell);
      if (NS_FAILED(res)) break;
      cellSelected = true;
    }
  }
  // Safety code to select starting cell if nothing else was selected
  if (!cellSelected)
  {
    return AppendNodeToSelectionAsRange(startCell);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SelectTableColumn()
{
  nsCOMPtr<nsIDOMElement> cell;
  nsresult res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nsnull, getter_AddRefs(cell));
  NS_ENSURE_SUCCESS(res, res);
  
  // Don't fail if we didn't find a cell
  NS_ENSURE_TRUE(cell, NS_EDITOR_ELEMENT_NOT_FOUND);

  nsCOMPtr<nsIDOMElement> startCell = cell;
  
  // Get location of cell:
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  PRInt32 startRowIndex, startColIndex;

  res = GetCellContext(getter_AddRefs(selection),
                       getter_AddRefs(table), 
                       getter_AddRefs(cell),
                       nsnull, nsnull,
                       &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  PRInt32 rowCount, colCount;
  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  // Suppress nsISelectionListener notification
  //  until all selection changes are finished
  nsSelectionBatcherForTable selectionBatcher(selection);

  // It is now safe to clear the selection
  // BE SURE TO RESET IT BEFORE LEAVING!
  res = ClearSelection();

  // Select all cells in the same column as current cell
  bool cellSelected = false;
  PRInt32 rowSpan, colSpan, actualRowSpan, actualColSpan, currentRowIndex, currentColIndex;
  bool    isSelected;
  for(PRInt32 row = 0; row < rowCount; row += NS_MAX(actualRowSpan, 1))
  {
    res = GetCellDataAt(table, row, startColIndex, getter_AddRefs(cell),
                        &currentRowIndex, &currentColIndex, &rowSpan, &colSpan, 
                        &actualRowSpan, &actualColSpan, &isSelected);
    if (NS_FAILED(res)) break;
    // Skip cells that are spanned from previous rows or columns
    if (cell && currentRowIndex == row && currentColIndex == startColIndex)
    {
      res = AppendNodeToSelectionAsRange(cell);
      if (NS_FAILED(res)) break;
      cellSelected = true;
    }
  }
  // Safety code to select starting cell if nothing else was selected
  if (!cellSelected)
  {
    return AppendNodeToSelectionAsRange(startCell);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SplitTableCell()
{
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex, actualRowSpan, actualColSpan;
  // Get cell, table, etc. at selection anchor node
  nsresult res = GetCellContext(nsnull,
                                getter_AddRefs(table), 
                                getter_AddRefs(cell),
                                nsnull, nsnull,
                                &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  if(!table || !cell) return NS_EDITOR_ELEMENT_NOT_FOUND;

  // We need rowspan and colspan data
  res = GetCellSpansAt(table, startRowIndex, startColIndex, actualRowSpan, actualColSpan);
  NS_ENSURE_SUCCESS(res, res);

  // Must have some span to split
  if (actualRowSpan <= 1 && actualColSpan <= 1)
    return NS_OK;
  
  nsAutoEditBatch beginBatching(this);
  // Prevent auto insertion of BR in new cell until we're done
  nsAutoRules beginRulesSniffing(this, kOpInsertNode, nsIEditor::eNext);

  // We reset selection  
  nsSetSelectionAfterTableEdit setCaret(this, table, startRowIndex, startColIndex, ePreviousColumn, false);
  //...so suppress Rules System selection munging
  nsAutoTxnsConserveSelection dontChangeSelection(this);

  nsCOMPtr<nsIDOMElement> newCell;
  PRInt32 rowIndex = startRowIndex;
  PRInt32 rowSpanBelow, colSpanAfter;

  // Split up cell row-wise first into rowspan=1 above, and the rest below,
  //  whittling away at the cell below until no more extra span
  for (rowSpanBelow = actualRowSpan-1; rowSpanBelow >= 0; rowSpanBelow--)
  {
    // We really split row-wise only if we had rowspan > 1
    if (rowSpanBelow > 0)
    {
      res = SplitCellIntoRows(table, rowIndex, startColIndex, 1, rowSpanBelow, getter_AddRefs(newCell));
      NS_ENSURE_SUCCESS(res, res);
      CopyCellBackgroundColor(newCell, cell);
    }
    PRInt32 colIndex = startColIndex;
    // Now split the cell with rowspan = 1 into cells if it has colSpan > 1
    for (colSpanAfter = actualColSpan-1; colSpanAfter > 0; colSpanAfter--)
    {
      res = SplitCellIntoColumns(table, rowIndex, colIndex, 1, colSpanAfter, getter_AddRefs(newCell));
      NS_ENSURE_SUCCESS(res, res);
      CopyCellBackgroundColor(newCell, cell);
      colIndex++;
    }
    // Point to the new cell and repeat
    rowIndex++;
  }
  return res;
}

nsresult
nsHTMLEditor::CopyCellBackgroundColor(nsIDOMElement *destCell, nsIDOMElement *sourceCell)
{
  NS_ENSURE_TRUE(destCell && sourceCell, NS_ERROR_NULL_POINTER);

  // Copy backgournd color to new cell
  NS_NAMED_LITERAL_STRING(bgcolor, "bgcolor");
  nsAutoString color;
  bool isSet;
  nsresult res = GetAttributeValue(sourceCell, bgcolor, color, &isSet);

  if (NS_SUCCEEDED(res) && isSet)
    res = SetAttribute(destCell, bgcolor, color);

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SplitCellIntoColumns(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32 aColIndex,
                                   PRInt32 aColSpanLeft, PRInt32 aColSpanRight,
                                   nsIDOMElement **aNewCell)
{
  NS_ENSURE_TRUE(aTable, NS_ERROR_NULL_POINTER);
  if (aNewCell) *aNewCell = nsnull;

  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  nsresult res = GetCellDataAt(aTable, aRowIndex, aColIndex, getter_AddRefs(cell),
                               &startRowIndex, &startColIndex,
                               &rowSpan, &colSpan, 
                               &actualRowSpan, &actualColSpan, &isSelected);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(cell, NS_ERROR_NULL_POINTER);
  
  // We can't split!
  if (actualColSpan <= 1 || (aColSpanLeft + aColSpanRight) > actualColSpan)
    return NS_OK;

  // Reduce colspan of cell to split
  res = SetColSpan(cell, aColSpanLeft);
  NS_ENSURE_SUCCESS(res, res);
  
  // Insert new cell after using the remaining span
  //  and always get the new cell so we can copy the background color;
  nsCOMPtr<nsIDOMElement> newCell;
  res = InsertCell(cell, actualRowSpan, aColSpanRight, true, false, getter_AddRefs(newCell));
  NS_ENSURE_SUCCESS(res, res);
  if (newCell)
  {
    if (aNewCell)
    {
      *aNewCell = newCell.get();
      NS_ADDREF(*aNewCell);
    }
    res = CopyCellBackgroundColor(newCell, cell);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SplitCellIntoRows(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32 aColIndex,
                                PRInt32 aRowSpanAbove, PRInt32 aRowSpanBelow, 
                                nsIDOMElement **aNewCell)
{
  NS_ENSURE_TRUE(aTable, NS_ERROR_NULL_POINTER);
  if (aNewCell) *aNewCell = nsnull;

  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  nsresult res = GetCellDataAt(aTable, aRowIndex, aColIndex, getter_AddRefs(cell),
                               &startRowIndex, &startColIndex,
                               &rowSpan, &colSpan, 
                               &actualRowSpan, &actualColSpan, &isSelected);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(cell, NS_ERROR_NULL_POINTER);
  
  // We can't split!
  if (actualRowSpan <= 1 || (aRowSpanAbove + aRowSpanBelow) > actualRowSpan)
    return NS_OK;

  PRInt32 rowCount, colCount;
  res = GetTableSize(aTable, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMElement> cell2;
  nsCOMPtr<nsIDOMElement> lastCellFound;
  PRInt32 startRowIndex2, startColIndex2, rowSpan2, colSpan2, actualRowSpan2, actualColSpan2;
  bool    isSelected2;
  PRInt32 colIndex = 0;
  bool insertAfter = (startColIndex > 0);
  // This is the row we will insert new cell into
  PRInt32 rowBelowIndex = startRowIndex+aRowSpanAbove;
  
  // Find a cell to insert before or after
  do 
  {
    // Search for a cell to insert before
    res = GetCellDataAt(aTable, rowBelowIndex, 
                        colIndex, getter_AddRefs(cell2),
                        &startRowIndex2, &startColIndex2, &rowSpan2, &colSpan2, 
                        &actualRowSpan2, &actualColSpan2, &isSelected2);
    // If we fail here, it could be because row has bad rowspan values,
    //   such as all cells having rowspan > 1 (Call FixRowSpan first!)
    if (NS_FAILED(res) || !cell) return NS_ERROR_FAILURE;

    // Skip over cells spanned from above (like the one we are splitting!)
    if (cell2 && startRowIndex2 == rowBelowIndex)
    {
      if (insertAfter)
      {
        // New cell isn't first in row,
        // so stop after we find the cell just before new cell's column
        if ((startColIndex2 + actualColSpan2) == startColIndex)
          break;

        // If cell found is AFTER desired new cell colum,
        //  we have multiple cells with rowspan > 1 that
        //  prevented us from finding a cell to insert after...
        if (startColIndex2 > startColIndex)
        {
          // ... so instead insert before the cell we found
          insertAfter = false;
          break;
        }
      }
      else
      {
        break; // Inserting before, so stop at first cell in row we want to insert into
      }
      lastCellFound = cell2;
    }
    // Skip to next available cellmap location
    colIndex += NS_MAX(actualColSpan2, 1);

    // Done when past end of total number of columns
    if (colIndex > colCount)
        break;

  } while(true);

  if (!cell2 && lastCellFound)
  {
    // Edge case where we didn't find a cell to insert after
    //  or before because column(s) before desired column 
    //  and all columns after it are spanned from above. 
    //  We can insert after the last cell we found 
    cell2 = lastCellFound;
    insertAfter = true; // Should always be true, but let's be sure
  }

  // Reduce rowspan of cell to split
  res = SetRowSpan(cell, aRowSpanAbove);
  NS_ENSURE_SUCCESS(res, res);


  // Insert new cell after using the remaining span
  //  and always get the new cell so we can copy the background color;
  nsCOMPtr<nsIDOMElement> newCell;
  res = InsertCell(cell2, aRowSpanBelow, actualColSpan, insertAfter, false, getter_AddRefs(newCell));
  NS_ENSURE_SUCCESS(res, res);
  if (newCell)
  {
    if (aNewCell)
    {
      *aNewCell = newCell.get();
      NS_ADDREF(*aNewCell);
    }
    res = CopyCellBackgroundColor(newCell, cell2);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SwitchTableCellHeaderType(nsIDOMElement *aSourceCell, nsIDOMElement **aNewCell)
{
  NS_ENSURE_TRUE(aSourceCell, NS_ERROR_NULL_POINTER);

  nsAutoEditBatch beginBatching(this);
  // Prevent auto insertion of BR in new cell created by ReplaceContainer
  nsAutoRules beginRulesSniffing(this, kOpInsertNode, nsIEditor::eNext);

  nsCOMPtr<nsIDOMNode> newNode;

  // Save current selection to restore when done
  // This is needed so ReplaceContainer can monitor selection
  //  when replacing nodes
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  nsAutoSelectionReset selectionResetter(selection, this);

  // Set to the opposite of current type
  nsCOMPtr<nsIAtom> atom = nsEditor::GetTag(aSourceCell);
  nsString newCellType( (atom == nsEditProperty::td) ? NS_LITERAL_STRING("th") : NS_LITERAL_STRING("td"));

  // This creates new node, moves children, copies attributes (true)
  //   and manages the selection!
  res = ReplaceContainer(aSourceCell, address_of(newNode), newCellType, nsnull, nsnull, true);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(newNode, NS_ERROR_FAILURE);

  // Return the new cell
  if (aNewCell)
  {
    nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(newNode);
    *aNewCell = newElement.get();
    NS_ADDREF(*aNewCell);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditor::JoinTableCells(bool aMergeNonContiguousContents)
{
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> targetCell;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  nsCOMPtr<nsIDOMElement> cell2;
  PRInt32 startRowIndex2, startColIndex2, rowSpan2, colSpan2, actualRowSpan2, actualColSpan2;
  bool    isSelected2;

  // Get cell, table, etc. at selection anchor node
  nsresult res = GetCellContext(nsnull,
                                getter_AddRefs(table), 
                                getter_AddRefs(targetCell),
                                nsnull, nsnull,
                                &startRowIndex, &startColIndex);
  NS_ENSURE_SUCCESS(res, res);
  if(!table || !targetCell) return NS_EDITOR_ELEMENT_NOT_FOUND;

  nsAutoEditBatch beginBatching(this);
  //Don't let Rules System change the selection
  nsAutoTxnsConserveSelection dontChangeSelection(this);

  // Note: We dont' use nsSetSelectionAfterTableEdit here so the selection
  //  is retained after joining. This leaves the target cell selected
  //  as well as the "non-contiguous" cells, so user can see what happened.

  nsCOMPtr<nsIDOMElement> firstCell;
  PRInt32 firstRowIndex, firstColIndex;
  res = GetFirstSelectedCellInTable(&firstRowIndex, &firstColIndex, getter_AddRefs(firstCell));
  NS_ENSURE_SUCCESS(res, res);
  
  bool joinSelectedCells = false;
  if (firstCell)
  {
    nsCOMPtr<nsIDOMElement> secondCell;
    res = GetNextSelectedCell(nsnull, getter_AddRefs(secondCell));
    NS_ENSURE_SUCCESS(res, res);

    // If only one cell is selected, join with cell to the right
    joinSelectedCells = (secondCell != nsnull);
  }

  if (joinSelectedCells)
  {
    // We have selected cells: Join just contiguous cells
    //  and just merge contents if not contiguous

    PRInt32 rowCount, colCount;
    res = GetTableSize(table, &rowCount, &colCount);
    NS_ENSURE_SUCCESS(res, res);

    // Get spans for cell we will merge into
    PRInt32 firstRowSpan, firstColSpan;
    res = GetCellSpansAt( table, firstRowIndex, firstColIndex, firstRowSpan, firstColSpan);
    NS_ENSURE_SUCCESS(res, res);

    // This defines the last indexes along the "edges"
    //  of the contiguous block of cells, telling us
    //  that we can join adjacent cells to the block
    // Start with same as the first values,
    //  then expand as we find adjacent selected cells
    PRInt32 lastRowIndex = firstRowIndex;
    PRInt32 lastColIndex = firstColIndex;
    PRInt32 rowIndex, colIndex;

    // First pass: Determine boundaries of contiguous rectangular block 
    //  that we will join into one cell,
    //  favoring adjacent cells in the same row
    for (rowIndex = firstRowIndex; rowIndex <= lastRowIndex; rowIndex++)
    {
      PRInt32 currentRowCount = rowCount;
      // Be sure each row doesn't have rowspan errors
      res = FixBadRowSpan(table, rowIndex, rowCount);
      NS_ENSURE_SUCCESS(res, res);
      // Adjust rowcount by number of rows we removed
      lastRowIndex -= (currentRowCount-rowCount);

      bool cellFoundInRow = false;
      bool lastRowIsSet = false;
      PRInt32 lastColInRow = 0;
      PRInt32 firstColInRow = firstColIndex;
      for (colIndex = firstColIndex; colIndex < colCount; colIndex += NS_MAX(actualColSpan2, 1))
      {
        res = GetCellDataAt(table, rowIndex, colIndex, getter_AddRefs(cell2),
                            &startRowIndex2, &startColIndex2,
                            &rowSpan2, &colSpan2, 
                            &actualRowSpan2, &actualColSpan2, &isSelected2);
        NS_ENSURE_SUCCESS(res, res);

        if (isSelected2)
        {
          if (!cellFoundInRow)
            // We've just found the first selected cell in this row
            firstColInRow = colIndex;

          if (rowIndex > firstRowIndex && firstColInRow != firstColIndex)
          {
            // We're in at least the second row,
            // but left boundary is "ragged" (not the same as 1st row's start)
            //Let's just end block on previous row
            // and keep previous lastColIndex
            //TODO: We could try to find the Maximum firstColInRow
            //      so our block can still extend down more rows?
            lastRowIndex = NS_MAX(0,rowIndex - 1);
            lastRowIsSet = true;
            break;
          }
          // Save max selected column in this row, including extra colspan
          lastColInRow = colIndex + (actualColSpan2-1);
          cellFoundInRow = true;
        }
        else if (cellFoundInRow)
        {
          // No cell or not selected, but at least one cell in row was found
          
          if (rowIndex > (firstRowIndex+1) && colIndex <= lastColIndex)
          {
            // Cell is in a column less than current right border in 
            //  the third or higher selected row, so stop block at the previous row
            lastRowIndex = NS_MAX(0,rowIndex - 1);
            lastRowIsSet = true;
          }
          // We're done with this row
          break;
        }
      } // End of column loop

      // Done with this row 
      if (cellFoundInRow) 
      {
        if (rowIndex == firstRowIndex)
        {
          // First row always initializes the right boundary
          lastColIndex = lastColInRow;
        }

        // If we didn't determine last row above...
        if (!lastRowIsSet)
        {
          if (colIndex < lastColIndex)
          {
            // (don't think we ever get here?)
            // Cell is in a column less than current right boundary,
            //  so stop block at the previous row
            lastRowIndex = NS_MAX(0,rowIndex - 1);
          }
          else
          {
            // Go on to examine next row
            lastRowIndex = rowIndex+1;
          }
        }
        // Use the minimum col we found so far for right boundary
        lastColIndex = NS_MIN(lastColIndex, lastColInRow);
      }
      else
      {
        // No selected cells in this row -- stop at row above
        //  and leave last column at its previous value
        lastRowIndex = NS_MAX(0,rowIndex - 1);
      }
    }
  
    // The list of cells we will delete after joining
    nsTArray<nsIDOMElement*> deleteList;

    // 2nd pass: Do the joining and merging
    for (rowIndex = 0; rowIndex < rowCount; rowIndex++)
    {
      for (colIndex = 0; colIndex < colCount; colIndex += NS_MAX(actualColSpan2, 1))
      {
        res = GetCellDataAt(table, rowIndex, colIndex, getter_AddRefs(cell2),
                            &startRowIndex2, &startColIndex2,
                            &rowSpan2, &colSpan2, 
                            &actualRowSpan2, &actualColSpan2, &isSelected2);
        NS_ENSURE_SUCCESS(res, res);

        // If this is 0, we are past last cell in row, so exit the loop
        if (actualColSpan2 == 0)
          break;

        // Merge only selected cells (skip cell we're merging into, of course)
        if (isSelected2 && cell2 != firstCell)
        {
          if (rowIndex >= firstRowIndex && rowIndex <= lastRowIndex && 
              colIndex >= firstColIndex && colIndex <= lastColIndex)
          {
            // We are within the join region
            // Problem: It is very tricky to delete cells as we merge,
            //  since that will upset the cellmap
            //  Instead, build a list of cells to delete and do it later
            NS_ASSERTION(startRowIndex2 == rowIndex, "JoinTableCells: StartRowIndex is in row above");

            if (actualColSpan2 > 1)
            {
              //Check if cell "hangs" off the boundary because of colspan > 1
              //  Use split methods to chop off excess
              PRInt32 extraColSpan = (startColIndex2 + actualColSpan2) - (lastColIndex+1);
              if ( extraColSpan > 0)
              {
                res = SplitCellIntoColumns(table, startRowIndex2, startColIndex2, 
                                           actualColSpan2-extraColSpan, extraColSpan, nsnull);
                NS_ENSURE_SUCCESS(res, res);
              }
            }

            res = MergeCells(firstCell, cell2, false);
            NS_ENSURE_SUCCESS(res, res);
            
            // Add cell to list to delete
            deleteList.AppendElement(cell2.get());
          }
          else if (aMergeNonContiguousContents)
          {
            // Cell is outside join region -- just merge the contents
            res = MergeCells(firstCell, cell2, false);
            NS_ENSURE_SUCCESS(res, res);
          }
        }
      }
    }

    // All cell contents are merged. Delete the empty cells we accumulated
    // Prevent rules testing until we're done
    nsAutoRules beginRulesSniffing(this, kOpDeleteNode, nsIEditor::eNext);

    for (PRUint32 i = 0, n = deleteList.Length(); i < n; i++)
    {
      nsIDOMElement *elementPtr = deleteList[i];
      if (elementPtr)
      {
        nsCOMPtr<nsIDOMNode> node = do_QueryInterface(elementPtr);
        res = DeleteNode(node);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    // Cleanup selection: remove ranges where cells were deleted
    nsCOMPtr<nsISelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

    PRInt32 rangeCount;
    res = selection->GetRangeCount(&rangeCount);
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsIDOMRange> range;
    PRInt32 i;
    for (i = 0; i < rangeCount; i++)
    {
      res = selection->GetRangeAt(i, getter_AddRefs(range));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDOMElement> deletedCell;
      res = GetCellFromRange(range, getter_AddRefs(deletedCell));
      if (!deletedCell)
      {
        selection->RemoveRange(range);
        rangeCount--;
        i--;
      }
    }

    // Set spans for the cell everthing merged into
    res = SetRowSpan(firstCell, lastRowIndex-firstRowIndex+1);
    NS_ENSURE_SUCCESS(res, res);
    res = SetColSpan(firstCell, lastColIndex-firstColIndex+1);
    NS_ENSURE_SUCCESS(res, res);
    
    
    // Fixup disturbances in table layout
    NormalizeTable(table);
  }
  else
  {
    // Joining with cell to the right -- get rowspan and colspan data of target cell
    res = GetCellDataAt(table, startRowIndex, startColIndex, getter_AddRefs(targetCell),
                        &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                        &actualRowSpan, &actualColSpan, &isSelected);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(targetCell, NS_ERROR_NULL_POINTER);

    // Get data for cell to the right
    res = GetCellDataAt(table, startRowIndex, startColIndex+actualColSpan, getter_AddRefs(cell2),
                        &startRowIndex2, &startColIndex2, &rowSpan2, &colSpan2, 
                        &actualRowSpan2, &actualColSpan2, &isSelected2);
    NS_ENSURE_SUCCESS(res, res);
    if(!cell2) return NS_OK; // Don't fail if there's no cell

    // sanity check
    NS_ASSERTION((startRowIndex >= startRowIndex2),"JoinCells: startRowIndex < startRowIndex2");

    // Figure out span of merged cell starting from target's starting row
    // to handle case of merged cell starting in a row above
    PRInt32 spanAboveMergedCell = startRowIndex - startRowIndex2;
    PRInt32 effectiveRowSpan2 = actualRowSpan2 - spanAboveMergedCell;

    if (effectiveRowSpan2 > actualRowSpan)
    {
      // Cell to the right spans into row below target
      // Split off portion below target cell's bottom-most row
      res = SplitCellIntoRows(table, startRowIndex2, startColIndex2,
                              spanAboveMergedCell+actualRowSpan, 
                              effectiveRowSpan2-actualRowSpan, nsnull);
      NS_ENSURE_SUCCESS(res, res);
    }

    // Move contents from cell to the right
    // Delete the cell now only if it starts in the same row
    //   and has enough row "height"
    res = MergeCells(targetCell, cell2, 
                     (startRowIndex2 == startRowIndex) && 
                     (effectiveRowSpan2 >= actualRowSpan));
    NS_ENSURE_SUCCESS(res, res);

    if (effectiveRowSpan2 < actualRowSpan)
    {
      // Merged cell is "shorter" 
      // (there are cells(s) below it that are row-spanned by target cell)
      // We could try splitting those cells, but that's REAL messy,
      //  so the safest thing to do is NOT really join the cells
      return NS_OK;
    }

    if( spanAboveMergedCell > 0 )
    {
      // Cell we merged started in a row above the target cell
      // Reduce rowspan to give room where target cell will extend its colspan
      res = SetRowSpan(cell2, spanAboveMergedCell);
      NS_ENSURE_SUCCESS(res, res);
    }

    // Reset target cell's colspan to encompass cell to the right
    res = SetColSpan(targetCell, actualColSpan+actualColSpan2);
    NS_ENSURE_SUCCESS(res, res);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::MergeCells(nsCOMPtr<nsIDOMElement> aTargetCell, 
                         nsCOMPtr<nsIDOMElement> aCellToMerge,
                         bool aDeleteCellToMerge)
{
  nsCOMPtr<dom::Element> targetCell = do_QueryInterface(aTargetCell);
  nsCOMPtr<dom::Element> cellToMerge = do_QueryInterface(aCellToMerge);
  NS_ENSURE_TRUE(targetCell && cellToMerge, NS_ERROR_NULL_POINTER);

  // Prevent rules testing until we're done
  nsAutoRules beginRulesSniffing(this, kOpDeleteNode, nsIEditor::eNext);

  // Don't need to merge if cell is empty
  if (!IsEmptyCell(cellToMerge)) {
    // Get index of last child in target cell
    // If we fail or don't have children, 
    //  we insert at index 0
    PRInt32 insertIndex = 0;

    // Start inserting just after last child
    PRUint32 len = targetCell->GetChildCount();
    if (len == 1 && IsEmptyCell(targetCell)) {
      // Delete the empty node
      nsIContent* cellChild = targetCell->GetFirstChild();
      nsresult res = DeleteNode(cellChild->AsDOMNode());
      NS_ENSURE_SUCCESS(res, res);
      insertIndex = 0;
    } else {
      insertIndex = (PRInt32)len;
    }

    // Move the contents
    while (cellToMerge->HasChildren()) {
      nsCOMPtr<nsIDOMNode> cellChild = cellToMerge->GetLastChild()->AsDOMNode();
      nsresult res = DeleteNode(cellChild);
      NS_ENSURE_SUCCESS(res, res);

      res = InsertNode(cellChild, aTargetCell, insertIndex);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // Delete cells whose contents were moved
  if (aDeleteCellToMerge)
    return DeleteNode(aCellToMerge);

  return NS_OK;
}


NS_IMETHODIMP 
nsHTMLEditor::FixBadRowSpan(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32& aNewRowCount)
{
  NS_ENSURE_TRUE(aTable, NS_ERROR_NULL_POINTER);

  PRInt32 rowCount, colCount;
  nsresult res = GetTableSize(aTable, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMElement>cell;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;

  PRInt32 minRowSpan = -1;
  PRInt32 colIndex;
  
  for( colIndex = 0; colIndex < colCount; colIndex += NS_MAX(actualColSpan, 1))
  {
    res = GetCellDataAt(aTable, aRowIndex, colIndex, getter_AddRefs(cell),
                        &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                        &actualRowSpan, &actualColSpan, &isSelected);
    // NOTE: This is a *real* failure. 
    // GetCellDataAt passes if cell is missing from cellmap
    if(NS_FAILED(res)) return res;
    if (!cell) break;
    if(rowSpan > 0 && 
       startRowIndex == aRowIndex &&
       (rowSpan < minRowSpan || minRowSpan == -1))
    {
      minRowSpan = rowSpan;
    }
    NS_ASSERTION((actualColSpan > 0),"ActualColSpan = 0 in FixBadRowSpan");
  }
  if(minRowSpan > 1)
  {
    // The amount to reduce everyone's rowspan
    // so at least one cell has rowspan = 1
    PRInt32 rowsReduced = minRowSpan - 1;
    for(colIndex = 0; colIndex < colCount; colIndex += NS_MAX(actualColSpan, 1))
    {
      res = GetCellDataAt(aTable, aRowIndex, colIndex, getter_AddRefs(cell),
                          &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                          &actualRowSpan, &actualColSpan, &isSelected);
      if(NS_FAILED(res)) return res;
      // Fixup rowspans only for cells starting in current row
      if(cell && rowSpan > 0 &&
         startRowIndex == aRowIndex && 
         startColIndex ==  colIndex )
      {
        res = SetRowSpan(cell, rowSpan-rowsReduced);
        if(NS_FAILED(res)) return res;
      }
      NS_ASSERTION((actualColSpan > 0),"ActualColSpan = 0 in FixBadRowSpan");
    }
  }
  return GetTableSize(aTable, &aNewRowCount, &colCount);
}

NS_IMETHODIMP 
nsHTMLEditor::FixBadColSpan(nsIDOMElement *aTable, PRInt32 aColIndex, PRInt32& aNewColCount)
{
  NS_ENSURE_TRUE(aTable, NS_ERROR_NULL_POINTER);

  PRInt32 rowCount, colCount;
  nsresult res = GetTableSize(aTable, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;

  PRInt32 minColSpan = -1;
  PRInt32 rowIndex;
  
  for( rowIndex = 0; rowIndex < rowCount; rowIndex += NS_MAX(actualRowSpan, 1))
  {
    res = GetCellDataAt(aTable, rowIndex, aColIndex, getter_AddRefs(cell),
                        &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                        &actualRowSpan, &actualColSpan, &isSelected);
    // NOTE: This is a *real* failure. 
    // GetCellDataAt passes if cell is missing from cellmap
    if(NS_FAILED(res)) return res;
    if (!cell) break;
    if(colSpan > 0 && 
       startColIndex == aColIndex &&
       (colSpan < minColSpan || minColSpan == -1))
    {
      minColSpan = colSpan;
    }
    NS_ASSERTION((actualRowSpan > 0),"ActualRowSpan = 0 in FixBadColSpan");
  }
  if(minColSpan > 1)
  {
    // The amount to reduce everyone's colspan
    // so at least one cell has colspan = 1
    PRInt32 colsReduced = minColSpan - 1;
    for(rowIndex = 0; rowIndex < rowCount; rowIndex += NS_MAX(actualRowSpan, 1))
    {
      res = GetCellDataAt(aTable, rowIndex, aColIndex, getter_AddRefs(cell),
                          &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                          &actualRowSpan, &actualColSpan, &isSelected);
      if(NS_FAILED(res)) return res;
      // Fixup colspans only for cells starting in current column
      if(cell && colSpan > 0 &&
         startColIndex == aColIndex && 
         startRowIndex ==  rowIndex )
      {
        res = SetColSpan(cell, colSpan-colsReduced);
        if(NS_FAILED(res)) return res;
      }
      NS_ASSERTION((actualRowSpan > 0),"ActualRowSpan = 0 in FixBadColSpan");
    }
  }
  return GetTableSize(aTable, &rowCount, &aNewColCount);
}

NS_IMETHODIMP 
nsHTMLEditor::NormalizeTable(nsIDOMElement *aTable)
{
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMElement> table;
  res = GetElementOrParentByTagName(NS_LITERAL_STRING("table"), aTable, getter_AddRefs(table));
  NS_ENSURE_SUCCESS(res, res);
  // Don't fail if we didn't find a table
  NS_ENSURE_TRUE(table, NS_OK);

  PRInt32 rowCount, colCount, rowIndex, colIndex;
  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  // Save current selection
  nsAutoSelectionReset selectionResetter(selection, this);

  nsAutoEditBatch beginBatching(this);
  // Prevent auto insertion of BR in new cell until we're done
  nsAutoRules beginRulesSniffing(this, kOpInsertNode, nsIEditor::eNext);

  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;

  // Scan all cells in each row to detect bad rowspan values
  for(rowIndex = 0; rowIndex < rowCount; rowIndex++)
  {
    res = FixBadRowSpan(table, rowIndex, rowCount);
    NS_ENSURE_SUCCESS(res, res);
  }
  // and same for colspans
  for(colIndex = 0; colIndex < colCount; colIndex++)
  {
    res = FixBadColSpan(table, colIndex, colCount);
    NS_ENSURE_SUCCESS(res, res);
  }

  // Fill in missing cellmap locations with empty cells
  for(rowIndex = 0; rowIndex < rowCount; rowIndex++)
  {
    nsCOMPtr<nsIDOMElement> previousCellInRow;

    for(colIndex = 0; colIndex < colCount; colIndex++)
    {
      res = GetCellDataAt(table, rowIndex, colIndex, getter_AddRefs(cell),
                          &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                          &actualRowSpan, &actualColSpan, &isSelected);
      // NOTE: This is a *real* failure. 
      // GetCellDataAt passes if cell is missing from cellmap
      if(NS_FAILED(res)) return res;
      if (!cell)
      {
        //We are missing a cell at a cellmap location
#ifdef DEBUG
        printf("NormalizeTable found missing cell at row=%d, col=%d\n", rowIndex, colIndex);
#endif
        // Add a cell after the previous Cell in the current row
        if(previousCellInRow)
        {
          // Insert a new cell after (true), and return the new cell to us
          res = InsertCell(previousCellInRow, 1, 1, true, false, getter_AddRefs(cell));
          NS_ENSURE_SUCCESS(res, res);

          // Set this so we use returned new "cell" to set previousCellInRow below
          if(cell)
            startRowIndex = rowIndex;   
        } else {
          // We don't have any cells in this row -- We are really messed up!
#ifdef DEBUG
          printf("NormalizeTable found no cells in row=%d, col=%d\n", rowIndex, colIndex);
#endif
          return NS_ERROR_FAILURE;
        }
      }
      // Save the last cell found in the same row we are scanning
      if(startRowIndex == rowIndex)
      {
        previousCellInRow = cell;
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetCellIndexes(nsIDOMElement *aCell,
                             PRInt32 *aRowIndex, PRInt32 *aColIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aColIndex=0; // initialize out params
  NS_ENSURE_ARG_POINTER(aColIndex);
  *aRowIndex=0;
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (!aCell)
  {
    // Get the selected cell or the cell enclosing the selection anchor
    nsCOMPtr<nsIDOMElement> cell;
    res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nsnull, getter_AddRefs(cell));
    if (NS_SUCCEEDED(res) && cell)
      aCell = cell;
    else
      return NS_ERROR_FAILURE;
  }

  NS_ENSURE_TRUE(mDocWeak, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIContent> nodeAsContent( do_QueryInterface(aCell) );
  NS_ENSURE_TRUE(nodeAsContent, NS_ERROR_FAILURE);
  // frames are not ref counted, so don't use an nsCOMPtr
  nsIFrame *layoutObject = nodeAsContent->GetPrimaryFrame();
  NS_ENSURE_TRUE(layoutObject, NS_ERROR_FAILURE);

  nsITableCellLayout *cellLayoutObject = do_QueryFrame(layoutObject);
  NS_ENSURE_TRUE(cellLayoutObject, NS_ERROR_FAILURE);
  return cellLayoutObject->GetCellIndexes(*aRowIndex, *aColIndex);
}

NS_IMETHODIMP
nsHTMLEditor::GetTableLayoutObject(nsIDOMElement* aTable, nsITableLayout **tableLayoutObject)
{
  *tableLayoutObject = nsnull;
  NS_ENSURE_TRUE(aTable, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mDocWeak, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIContent> nodeAsContent( do_QueryInterface(aTable) );
  NS_ENSURE_TRUE(nodeAsContent, NS_ERROR_FAILURE);
  // frames are not ref counted, so don't use an nsCOMPtr
  nsIFrame *layoutObject = nodeAsContent->GetPrimaryFrame();
  NS_ENSURE_TRUE(layoutObject, NS_ERROR_FAILURE);

  *tableLayoutObject = do_QueryFrame(layoutObject);
  return *tableLayoutObject ? NS_OK : NS_NOINTERFACE;
}

//Return actual number of cells (a cell with colspan > 1 counts as just 1)
PRInt32 nsHTMLEditor::GetNumberOfCellsInRow(nsIDOMElement* aTable, PRInt32 rowIndex)
{
  PRInt32 cellCount = 0;
  nsCOMPtr<nsIDOMElement> cell;
  PRInt32 colIndex = 0;
  nsresult res;
  do {
    PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
    bool    isSelected;
    res = GetCellDataAt(aTable, rowIndex, colIndex, getter_AddRefs(cell),
                        &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                        &actualRowSpan, &actualColSpan, &isSelected);
    NS_ENSURE_SUCCESS(res, 0);
    if (cell)
    {
      // Only count cells that start in row we are working with
      if (startRowIndex == rowIndex)
        cellCount++;
      
      //Next possible location for a cell
      colIndex += actualColSpan;
    }
    else
      colIndex++;

  } while (cell);

  return cellCount;
}

/* Not scriptable: For convenience in C++ 
   Use GetTableRowCount and GetTableColumnCount from JavaScript
*/
NS_IMETHODIMP
nsHTMLEditor::GetTableSize(nsIDOMElement *aTable,
                           PRInt32* aRowCount, PRInt32* aColCount)
{
  NS_ENSURE_ARG_POINTER(aRowCount);
  NS_ENSURE_ARG_POINTER(aColCount);
  nsresult res;
  *aRowCount = 0;
  *aColCount = 0;
  nsCOMPtr<nsIDOMElement> table;
  // Get the selected talbe or the table enclosing the selection anchor
  res = GetElementOrParentByTagName(NS_LITERAL_STRING("table"), aTable, getter_AddRefs(table));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayoutObject;
  res = GetTableLayoutObject(table.get(), &tableLayoutObject);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(tableLayoutObject, NS_ERROR_FAILURE);

  return tableLayoutObject->GetTableSize(*aRowCount, *aColCount); 
}

NS_IMETHODIMP 
nsHTMLEditor::GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex,
                            PRInt32 aColIndex, nsIDOMElement **aCell, 
                            PRInt32* aStartRowIndex, PRInt32* aStartColIndex, 
                            PRInt32* aRowSpan, PRInt32* aColSpan, 
                            PRInt32* aActualRowSpan, PRInt32* aActualColSpan, 
                            bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aStartRowIndex);
  NS_ENSURE_ARG_POINTER(aStartColIndex);
  NS_ENSURE_ARG_POINTER(aRowSpan);
  NS_ENSURE_ARG_POINTER(aColSpan);
  NS_ENSURE_ARG_POINTER(aActualRowSpan);
  NS_ENSURE_ARG_POINTER(aActualColSpan);
  NS_ENSURE_ARG_POINTER(aIsSelected);
  NS_ENSURE_TRUE(aCell, NS_ERROR_NULL_POINTER);

  nsresult res=NS_ERROR_FAILURE;
  *aStartRowIndex = 0;
  *aStartColIndex = 0;
  *aRowSpan = 0;
  *aColSpan = 0;
  *aActualRowSpan = 0;
  *aActualColSpan = 0;
  *aIsSelected = false;

  *aCell = nsnull;

  if (!aTable)
  {
    // Get the selected table or the table enclosing the selection anchor
    nsCOMPtr<nsIDOMElement> table;
    res = GetElementOrParentByTagName(NS_LITERAL_STRING("table"), nsnull, getter_AddRefs(table));
    NS_ENSURE_SUCCESS(res, res);
    if (table)
      aTable = table;
    else
      return NS_ERROR_FAILURE;
  }
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayoutObject;
  res = GetTableLayoutObject(aTable, &tableLayoutObject);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(tableLayoutObject, NS_ERROR_FAILURE);

  // Note that this returns NS_TABLELAYOUT_CELL_NOT_FOUND when
  //  the index(es) are out of bounds
  nsCOMPtr<nsIDOMElement> cell;
  res = tableLayoutObject->GetCellDataAt(aRowIndex, aColIndex,
                                         *getter_AddRefs(cell), 
                                         *aStartRowIndex, *aStartColIndex,
                                         *aRowSpan, *aColSpan, 
                                         *aActualRowSpan, *aActualColSpan, 
                                         *aIsSelected);
  if (cell)
  {
    *aCell = cell.get();
    NS_ADDREF(*aCell);
  }
  // Convert to editor's generic "not found" return value
  if (res == NS_TABLELAYOUT_CELL_NOT_FOUND) res = NS_EDITOR_ELEMENT_NOT_FOUND;
  return res;
}

// When all you want is the cell
NS_IMETHODIMP 
nsHTMLEditor::GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement **aCell)
{
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;
  return GetCellDataAt(aTable, aRowIndex, aColIndex, aCell, 
                       &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                       &actualRowSpan, &actualColSpan, &isSelected);
}

// When all you want are the rowspan and colspan (not exposed in nsITableEditor)
NS_IMETHODIMP
nsHTMLEditor::GetCellSpansAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, 
                             PRInt32& aActualRowSpan, PRInt32& aActualColSpan)
{
  nsCOMPtr<nsIDOMElement> cell;    
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan;
  bool    isSelected;
  return GetCellDataAt(aTable, aRowIndex, aColIndex, getter_AddRefs(cell), 
                       &startRowIndex, &startColIndex, &rowSpan, &colSpan, 
                       &aActualRowSpan, &aActualColSpan, &isSelected);
}

NS_IMETHODIMP
nsHTMLEditor::GetCellContext(nsISelection **aSelection,
                             nsIDOMElement   **aTable,
                             nsIDOMElement   **aCell,
                             nsIDOMNode      **aCellParent, PRInt32 *aCellOffset,
                             PRInt32 *aRowIndex, PRInt32 *aColIndex)
{
  // Initialize return pointers
  if (aSelection) *aSelection = nsnull;
  if (aTable) *aTable = nsnull;
  if (aCell) *aCell = nsnull;
  if (aCellParent) *aCellParent = nsnull;
  if (aCellOffset) *aCellOffset = 0;
  if (aRowIndex) *aRowIndex = 0;
  if (aColIndex) *aColIndex = 0;

  nsCOMPtr <nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  if (aSelection)
  {
    *aSelection = selection.get();
    NS_ADDREF(*aSelection);
  }
  nsCOMPtr <nsIDOMElement> table;
  nsCOMPtr <nsIDOMElement> cell;

  // Caller may supply the cell...
  if (aCell && *aCell)
    cell = *aCell;

  // ...but if not supplied,
  //    get cell if it's the child of selection anchor node,
  //    or get the enclosing by a cell
  if (!cell)
  {
    // Find a selected or enclosing table element
    nsCOMPtr<nsIDOMElement> cellOrTableElement;
    PRInt32 selectedCount;
    nsAutoString tagName;
    res = GetSelectedOrParentTableElement(tagName, &selectedCount,
                                          getter_AddRefs(cellOrTableElement));
    NS_ENSURE_SUCCESS(res, res);
    if (tagName.EqualsLiteral("table"))
    {
      // We have a selected table, not a cell
      if (aTable)
      {
        *aTable = cellOrTableElement.get();
        NS_ADDREF(*aTable);
      }
      return NS_OK;
    }
    if (!tagName.EqualsLiteral("td"))
      return NS_EDITOR_ELEMENT_NOT_FOUND;

    // We found a cell
    cell = cellOrTableElement;
  }
  if (aCell)
  {
    *aCell = cell.get();
    NS_ADDREF(*aCell);
  }

  // Get containing table
  res = GetElementOrParentByTagName(NS_LITERAL_STRING("table"), cell, getter_AddRefs(table));
  NS_ENSURE_SUCCESS(res, res);
  // Cell must be in a table, so fail if not found
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);
  if (aTable)
  {
    *aTable = table.get();
    NS_ADDREF(*aTable);
  }

  // Get the rest of the related data only if requested
  if (aRowIndex || aColIndex)
  {
    PRInt32 rowIndex, colIndex;
    // Get current cell location so we can put caret back there when done
    res = GetCellIndexes(cell, &rowIndex, &colIndex);
    if(NS_FAILED(res)) return res;
    if (aRowIndex) *aRowIndex = rowIndex;
    if (aColIndex) *aColIndex = colIndex;
  }
  if (aCellParent)
  {
    nsCOMPtr <nsIDOMNode> cellParent;
    // Get the immediate parent of the cell
    res = cell->GetParentNode(getter_AddRefs(cellParent));
    NS_ENSURE_SUCCESS(res, res);
    // Cell has to have a parent, so fail if not found
    NS_ENSURE_TRUE(cellParent, NS_ERROR_FAILURE);

    *aCellParent = cellParent.get();
    NS_ADDREF(*aCellParent);

    if (aCellOffset) {
      *aCellOffset = GetChildOffset(cell, cellParent);
    }
  }

  return res;
}

nsresult 
nsHTMLEditor::GetCellFromRange(nsIDOMRange *aRange, nsIDOMElement **aCell)
{
  // Note: this might return a node that is outside of the range.
  // Use carefully.
  NS_ENSURE_TRUE(aRange && aCell, NS_ERROR_NULL_POINTER);

  *aCell = nsnull;

  nsCOMPtr<nsIDOMNode> startParent;
  nsresult res = aRange->GetStartContainer(getter_AddRefs(startParent));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(startParent, NS_ERROR_FAILURE);

  PRInt32 startOffset;
  res = aRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMNode> childNode = GetChildAt(startParent, startOffset);
  // This means selection is probably at a text node (or end of doc?)
  if (!childNode) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> endParent;
  res = aRange->GetEndContainer(getter_AddRefs(endParent));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(startParent, NS_ERROR_FAILURE);

  PRInt32 endOffset;
  res = aRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  // If a cell is deleted, the range is collapse
  //   (startOffset == endOffset)
  //   so tell caller the cell wasn't found
  if (startParent == endParent && 
      endOffset == startOffset+1 &&
      nsHTMLEditUtils::IsTableCell(childNode))
  {
    // Should we also test if frame is selected? (Use GetCellDataAt())
    // (Let's not for now -- more efficient)
    nsCOMPtr<nsIDOMElement> cellElement = do_QueryInterface(childNode);
    *aCell = cellElement.get();
    NS_ADDREF(*aCell);
    return NS_OK;
  }
  return NS_EDITOR_ELEMENT_NOT_FOUND;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFirstSelectedCell(nsIDOMRange **aRange, nsIDOMElement **aCell)
{
  NS_ENSURE_TRUE(aCell, NS_ERROR_NULL_POINTER);
  *aCell = nsnull;
  if (aRange) *aRange = nsnull;

  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  mSelectedCellIndex = 0;

  res = GetCellFromRange(range, aCell);
  // Failure here probably means selection is in a text node,
  //  so there's no selected cell
  if (NS_FAILED(res)) {
    return NS_EDITOR_ELEMENT_NOT_FOUND;
  }
  // No cell means range was collapsed (cell was deleted)
  if (!*aCell) {
    return NS_EDITOR_ELEMENT_NOT_FOUND;
  }

  if (aRange)
  {
    *aRange = range.get();
    NS_ADDREF(*aRange);
  }

  // Setup for next cell
  mSelectedCellIndex = 1;

  return res;  
}

NS_IMETHODIMP
nsHTMLEditor::GetNextSelectedCell(nsIDOMRange **aRange, nsIDOMElement **aCell)
{
  NS_ENSURE_TRUE(aCell, NS_ERROR_NULL_POINTER);
  *aCell = nsnull;
  if (aRange) *aRange = nsnull;

  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  PRInt32 rangeCount;
  res = selection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);

  // Don't even try if index exceeds range count
  if (mSelectedCellIndex >= rangeCount) 
    return NS_EDITOR_ELEMENT_NOT_FOUND;

  // Scan through ranges to find next valid selected cell
  nsCOMPtr<nsIDOMRange> range;
  for (; mSelectedCellIndex < rangeCount; mSelectedCellIndex++)
  {
    res = selection->GetRangeAt(mSelectedCellIndex, getter_AddRefs(range));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

    res = GetCellFromRange(range, aCell);
    // Failure here means the range doesn't contain a cell
    NS_ENSURE_SUCCESS(res, NS_EDITOR_ELEMENT_NOT_FOUND);
    
    // We found a selected cell
    if (*aCell) break;
#ifdef DEBUG_cmanske
    else
      printf("GetNextSelectedCell: Collapsed range found\n");
#endif

    // If we didn't find a cell, continue to next range in selection
  }
  // No cell means all remaining ranges were collapsed (cells were deleted)
  NS_ENSURE_TRUE(*aCell, NS_EDITOR_ELEMENT_NOT_FOUND);

  if (aRange)
  {
    *aRange = range.get();
    NS_ADDREF(*aRange);
  }

  // Setup for next cell
  mSelectedCellIndex++;

  return res;  
}

NS_IMETHODIMP 
nsHTMLEditor::GetFirstSelectedCellInTable(PRInt32 *aRowIndex, PRInt32 *aColIndex, nsIDOMElement **aCell)
{
  NS_ENSURE_TRUE(aCell, NS_ERROR_NULL_POINTER);
  *aCell = nsnull;
  if (aRowIndex)
    *aRowIndex = 0;
  if (aColIndex)
    *aColIndex = 0;

  nsCOMPtr<nsIDOMElement> cell;
  nsresult res = GetFirstSelectedCell(nsnull, getter_AddRefs(cell));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(cell, NS_EDITOR_ELEMENT_NOT_FOUND);

  *aCell = cell.get();
  NS_ADDREF(*aCell);

  // Also return the row and/or column if requested
  if (aRowIndex || aColIndex)
  {
    PRInt32 startRowIndex, startColIndex;
    res = GetCellIndexes(cell, &startRowIndex, &startColIndex);
    if(NS_FAILED(res)) return res;

    if (aRowIndex)
      *aRowIndex = startRowIndex;

    if (aColIndex)
      *aColIndex = startColIndex;
  }

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetSelectionAfterTableEdit(nsIDOMElement* aTable, PRInt32 aRow, PRInt32 aCol, 
                                     PRInt32 aDirection, bool aSelected)
{
  NS_ENSURE_TRUE(aTable, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  
  if (!selection)
  {
#ifdef DEBUG_cmanske
    printf("Selection not found after table manipulation!\n");
#endif
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMElement> cell;
  bool done = false;
  do {
    res = GetCellAt(aTable, aRow, aCol, getter_AddRefs(cell));
    if (NS_SUCCEEDED(res))
    {
      if (cell)
      {
        if (aSelected)
        {
          // Reselect the cell
          return SelectElement(cell);
        }
        else
        {
          // Set the caret to deepest first child
          //   but don't go into nested tables
          // TODO: Should we really be placing the caret at the END
          //  of the cell content?
          return CollapseSelectionToDeepestNonTableFirstChild(selection, cell);
        }
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
                done = true;
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
                done = true;
            }
            else
              aRow--;
            break;
          default:
            done = true;
        }
      }
    }
    else
      break;
  } while (!done);

  // We didn't find a cell
  // Set selection to just before the table
  nsCOMPtr<nsIDOMNode> tableParent;
  res = aTable->GetParentNode(getter_AddRefs(tableParent));
  if(NS_SUCCEEDED(res) && tableParent)
  {
    PRInt32 tableOffset = GetChildOffset(aTable, tableParent);
    return selection->Collapse(tableParent, tableOffset);
  }
  // Last resort: Set selection to start of doc
  // (it's very bad to not have a valid selection!)
  return SetSelectionAtDocumentStart(selection);
}

NS_IMETHODIMP 
nsHTMLEditor::GetSelectedOrParentTableElement(nsAString& aTagName,
                                              PRInt32 *aSelectedCount,
                                              nsIDOMElement** aTableElement)
{
  NS_ENSURE_ARG_POINTER(aTableElement);
  NS_ENSURE_ARG_POINTER(aSelectedCount);
  *aTableElement = nsnull;
  aTagName.Truncate();
  *aSelectedCount = 0;

  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  // Try to get the first selected cell
  nsCOMPtr<nsIDOMElement> tableOrCellElement;
  res = GetFirstSelectedCell(nsnull, getter_AddRefs(tableOrCellElement));
  NS_ENSURE_SUCCESS(res, res);

  NS_NAMED_LITERAL_STRING(tdName, "td");

  if (tableOrCellElement)
  {
      // Each cell is in its own selection range,
      //  so count signals multiple-cell selection
      res = selection->GetRangeCount(aSelectedCount);
      NS_ENSURE_SUCCESS(res, res);
      aTagName = tdName;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> anchorNode;
    res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
    if(NS_FAILED(res)) return res;
    NS_ENSURE_TRUE(anchorNode, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNode> selectedNode;

    // Get child of anchor node, if exists
    bool hasChildren;
    anchorNode->HasChildNodes(&hasChildren);

    if (hasChildren)
    {
      PRInt32 anchorOffset;
      res = selection->GetAnchorOffset(&anchorOffset);
      NS_ENSURE_SUCCESS(res, res);
      selectedNode = GetChildAt(anchorNode, anchorOffset);
      if (!selectedNode)
      {
        selectedNode = anchorNode;
        // If anchor doesn't have a child, we can't be selecting a table element,
        //  so don't do the following:
      }
      else
      {
        nsCOMPtr<nsIAtom> atom = nsEditor::GetTag(selectedNode);

        if (atom == nsEditProperty::td)
        {
          tableOrCellElement = do_QueryInterface(selectedNode);
          aTagName = tdName;
          // Each cell is in its own selection range,
          //  so count signals multiple-cell selection
          res = selection->GetRangeCount(aSelectedCount);
          NS_ENSURE_SUCCESS(res, res);
        }
        else if (atom == nsEditProperty::table)
        {
          tableOrCellElement = do_QueryInterface(selectedNode);
          aTagName.AssignLiteral("table");
          *aSelectedCount = 1;
        }
        else if (atom == nsEditProperty::tr)
        {
          tableOrCellElement = do_QueryInterface(selectedNode);
          aTagName.AssignLiteral("tr");
          *aSelectedCount = 1;
        }
      }
    }
    if (!tableOrCellElement)
    {
      // Didn't find a table element -- find a cell parent
      res = GetElementOrParentByTagName(tdName, anchorNode, getter_AddRefs(tableOrCellElement));
      if(NS_FAILED(res)) return res;
      if (tableOrCellElement)
        aTagName = tdName;
    }
  }
  if (tableOrCellElement)
  {
    *aTableElement = tableOrCellElement.get();
    NS_ADDREF(*aTableElement);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetSelectedCellsType(nsIDOMElement *aElement, PRUint32 *aSelectionType)
{
  NS_ENSURE_ARG_POINTER(aSelectionType);
  *aSelectionType = 0;

  // Be sure we have a table element 
  //  (if aElement is null, this uses selection's anchor node)
  nsCOMPtr<nsIDOMElement> table;

  nsresult res = GetElementOrParentByTagName(NS_LITERAL_STRING("table"), aElement, getter_AddRefs(table));
  NS_ENSURE_SUCCESS(res, res);

  PRInt32 rowCount, colCount;
  res = GetTableSize(table, &rowCount, &colCount);
  NS_ENSURE_SUCCESS(res, res);

  // Traverse all selected cells 
  nsCOMPtr<nsIDOMElement> selectedCell;
  res = GetFirstSelectedCell(nsnull, getter_AddRefs(selectedCell));
  NS_ENSURE_SUCCESS(res, res);
  if (res == NS_EDITOR_ELEMENT_NOT_FOUND) return NS_OK;
  
  // We have at least one selected cell, so set return value
  *aSelectionType = nsISelectionPrivate::TABLESELECTION_CELL;

  // Store indexes of each row/col to avoid duplication of searches
  nsTArray<PRInt32> indexArray;

  bool allCellsInRowAreSelected = false;
  bool allCellsInColAreSelected = false;
  while (NS_SUCCEEDED(res) && selectedCell)
  {
    // Get the cell's location in the cellmap
    PRInt32 startRowIndex, startColIndex;
    res = GetCellIndexes(selectedCell, &startRowIndex, &startColIndex);
    if(NS_FAILED(res)) return res;
    
    if (!indexArray.Contains(startColIndex))
    {
      indexArray.AppendElement(startColIndex);
      allCellsInRowAreSelected = AllCellsInRowSelected(table, startRowIndex, colCount);
      // We're done as soon as we fail for any row
      if (!allCellsInRowAreSelected) break;
    }
    res = GetNextSelectedCell(nsnull, getter_AddRefs(selectedCell));
  }

  if (allCellsInRowAreSelected)
  {
    *aSelectionType = nsISelectionPrivate::TABLESELECTION_ROW;
    return NS_OK;
  }
  // Test for columns

  // Empty the indexArray
  indexArray.Clear();

  // Start at first cell again
  res = GetFirstSelectedCell(nsnull, getter_AddRefs(selectedCell));
  while (NS_SUCCEEDED(res) && selectedCell)
  {
    // Get the cell's location in the cellmap
    PRInt32 startRowIndex, startColIndex;
    res = GetCellIndexes(selectedCell, &startRowIndex, &startColIndex);
    if(NS_FAILED(res)) return res;
  
    if (!indexArray.Contains(startRowIndex))
    {
      indexArray.AppendElement(startColIndex);
      allCellsInColAreSelected = AllCellsInColumnSelected(table, startColIndex, rowCount);
      // We're done as soon as we fail for any column
      if (!allCellsInRowAreSelected) break;
    }
    res = GetNextSelectedCell(nsnull, getter_AddRefs(selectedCell));
  }
  if (allCellsInColAreSelected)
    *aSelectionType = nsISelectionPrivate::TABLESELECTION_COLUMN;

  return NS_OK;
}

bool 
nsHTMLEditor::AllCellsInRowSelected(nsIDOMElement *aTable, PRInt32 aRowIndex, PRInt32 aNumberOfColumns)
{
  NS_ENSURE_TRUE(aTable, false);

  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;

  for( PRInt32 col = 0; col < aNumberOfColumns; col += NS_MAX(actualColSpan, 1))
  {
    nsCOMPtr<nsIDOMElement> cell;    
    nsresult res = GetCellDataAt(aTable, aRowIndex, col, getter_AddRefs(cell),
                                 &curStartRowIndex, &curStartColIndex,
                                 &rowSpan, &colSpan,
                                 &actualRowSpan, &actualColSpan, &isSelected);
 
    NS_ENSURE_SUCCESS(res, false);
    // If no cell, we may have a "ragged" right edge,
    //   so return TRUE only if we already found a cell in the row
    NS_ENSURE_TRUE(cell, (col > 0) ? true : false);

    // Return as soon as a non-selected cell is found
    NS_ENSURE_TRUE(isSelected, false);

    NS_ASSERTION((actualColSpan > 0),"ActualColSpan = 0 in AllCellsInRowSelected");
  }
  return true;
}

bool 
nsHTMLEditor::AllCellsInColumnSelected(nsIDOMElement *aTable, PRInt32 aColIndex, PRInt32 aNumberOfRows)
{
  NS_ENSURE_TRUE(aTable, false);

  PRInt32 curStartRowIndex, curStartColIndex, rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool    isSelected;

  for( PRInt32 row = 0; row < aNumberOfRows; row += NS_MAX(actualRowSpan, 1))
  {
    nsCOMPtr<nsIDOMElement> cell;    
    nsresult res = GetCellDataAt(aTable, row, aColIndex, getter_AddRefs(cell),
                                 &curStartRowIndex, &curStartColIndex,
                                 &rowSpan, &colSpan,
                                 &actualRowSpan, &actualColSpan, &isSelected);
    
    NS_ENSURE_SUCCESS(res, false);
    // If no cell, we must have a "ragged" right edge on the last column
    //   so return TRUE only if we already found a cell in the row
    NS_ENSURE_TRUE(cell, (row > 0) ? true : false);

    // Return as soon as a non-selected cell is found
    NS_ENSURE_TRUE(isSelected, false);
  }
  return true;
}

bool 
nsHTMLEditor::IsEmptyCell(dom::Element* aCell)
{
  MOZ_ASSERT(aCell);

  // Check if target only contains empty text node or <br>
  nsCOMPtr<nsINode> cellChild = aCell->GetFirstChild();
  if (!cellChild) {
    return false;
  }

  nsCOMPtr<nsINode> nextChild = cellChild->GetNextSibling();
  if (nextChild) {
    return false;
  }

  // We insert a single break into a cell by default
  //   to have some place to locate a cursor -- it is dispensable
  if (cellChild->IsElement() && cellChild->AsElement()->IsHTML(nsGkAtoms::br)) {
    return true;
  }

  bool isEmpty;
  // Or check if no real content
  nsresult rv = IsEmptyNode(cellChild, &isEmpty, false, false);
  NS_ENSURE_SUCCESS(rv, false);
  return isEmpty;
}
