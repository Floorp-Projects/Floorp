/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
  //TODO: HANDLE CASE OF aAfter = true
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMElement> table;
  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMNode> cellParent;
  PRInt32 cellOffset, startRow, startCol;
  nsresult res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRow, startCol);
  
  if (NS_SUCCEEDED(res))
  {
    nsAutoEditBatch beginBatching(this);

    PRInt32 rowCount, colCount, row;
    if (NS_FAILED(GetTableSize(table, rowCount, colCount)))
      return NS_ERROR_FAILURE;
    for ( row = 0; row < rowCount; row++)
    {
      nsCOMPtr<nsIDOMElement> curCell;
      PRInt32 curStartRow, curStartCol, rowSpan, colSpan;
      PRBool  curIsSelected;
      res = GetCellDataAt(table, row, startCol, *getter_AddRefs(curCell),
                          curStartRow, curStartCol, rowSpan, colSpan, curIsSelected);
      if (NS_SUCCEEDED(res) && curCell)
      {
        if (curStartCol < startCol)
        {
          // We have a cell spanning this location
          // Simply increase its rowspan to keep table rectangular
          nsString newColSpan;
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
    }
    SetCaretAfterTableEdit(table, startRow, startCol, ePreviousColumn);  
  }
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
  
  if (NS_SUCCEEDED(res))
  {
    nsAutoEditBatch beginBatching(this);
    //TODO: FINISH ME! 
    selection->ClearSelection();
  }
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

    // Save where we need to restore the selection
    nsCOMPtr<nsIDOMNode> tableParent;
    PRInt32 tableOffset;
    if(NS_FAILED(table->GetParentNode(getter_AddRefs(tableParent))) || !tableParent)
      return NS_ERROR_FAILURE;
    if(NS_FAILED(GetChildOffset(table, tableParent, tableOffset)))
      return NS_ERROR_FAILURE;

    res = DeleteNode(table);

    // Place selection just before the table
    selection->Collapse(tableParent, tableOffset);
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
  nsresult res = GetCellContext(selection, table, cell, cellParent, cellOffset, startRow, startCol);
  
  if (NS_SUCCEEDED(res))
  {
    nsAutoEditBatch beginBatching(this);

    selection->ClearSelection();
    PRInt32 i;
    for (i = 0; i < aNumber; i++)
    {
      //TODO: FINISH ME!
      if (NS_FAILED(DeleteNode(cell)))
        break;
    }
    SetCaretAfterTableEdit(table, startRow, startCol, ePreviousColumn);
  }
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
nsHTMLEditor::GetCellIndexes(nsIDOMElement *aCell, PRInt32 &aColIndex, PRInt32 &aRowIndex)
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

        while (NS_COMFALSE == iter->IsDone())
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
  if (NS_SUCCEEDED(res) && cell)
  {
    // Set the caret to just before the first child of the cell?
    // TODO: Should we really be placing the caret at the END
    //  of the cell content?
    selection->Collapse(cell, 0);
  }
    //TODO: SEARCH FOR NEAREST CELL TO PLACE CARET INTO
  return res;
}

