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
#include "nsIAtom.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCellElement.h"
/*
#include "nsIDocument.h"
#include "nsIServiceManager.h"
#include "nsEditFactory.h"
#include "nsTextEditFactory.h"
#include "nsEditorCID.h"
#include "nsTransactionManagerCID.h"
#include "nsITransactionManager.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsICollection.h"
#include "nsIEnumerator.h"
#include "nsVoidArray.h"
#include "nsICaret.h"
*/

#include "nsITableCellLayout.h" // For efficient access to table cell
#include "nsITableLayout.h"     //  data owned by the table and cell frames

// transactions the editor knows how to build
#include "TransactionFactory.h"
#include "EditAggregateTxn.h"
#include "nsIDOMHTMLCollection.h"
#include "nsHTMLEditor.h"

static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);

// Table Editing methods

NS_IMETHODIMP
nsHTMLEditor::InsertTable()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableCell(PRInt32 aNumber, PRBool aAfter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableColumn(PRInt32 aNumber, PRBool aAfter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTableRow(PRInt32 aNumber, PRBool aAfter)
{
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
    return res;
  // Collapse the current selection
  selection->ClearSelection();
  nsCOMPtr<nsIDOMNode> anchorNode;
  res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
  if (NS_FAILED(res) || !anchorNode)
    return res;

  nsCOMPtr<nsIDOMElement> cell;
  nsCOMPtr<nsIDOMElement> row;
  nsCOMPtr<nsIDOMElement> table;
  res = NS_ERROR_FAILURE;
  //if(NS_SUCCEEDED(GetElementOrParentByTagName("td", anchorNode, getter_AddRefs(cell))


  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTable()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteTableCell(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
nsHTMLEditor::JoinTableCells(PRBool aCellToRight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsHTMLEditor::GetCellIndexes(nsIDOMElement *aCell, PRInt32 &aColIndex, PRInt32 &aRowIndex)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  aColIndex=0; // initialize out params
  aRowIndex=0;
  if (!aCell)
    return result;

  result = NS_ERROR_FAILURE;        // we return an error unless we get the index
  nsISupports *layoutObject=nsnull; // frames are not ref counted, so don't use an nsCOMPtr

  result = nsEditor::GetLayoutObject(aCell, &layoutObject);

  if ((NS_SUCCEEDED(result)) && (nsnull!=layoutObject))
  { // get the table cell interface from the frame
    nsITableCellLayout *cellLayoutObject=nsnull; // again, frames are not ref-counted
    result = layoutObject->QueryInterface(nsITableCellLayout::GetIID(), (void**)(&cellLayoutObject));
    if ((NS_SUCCEEDED(result)) && (nsnull!=cellLayoutObject))
    { // get the index
      result = cellLayoutObject->GetColIndex(aColIndex);
      if (NS_SUCCEEDED(result))
      {
        result = cellLayoutObject->GetRowIndex(aRowIndex);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLEditor::GetTableLayoutObject(nsIDOMElement* aTable, nsITableLayout **tableLayoutObject)
{
  *tableLayoutObject=nsnull;
  if (!aTable)
    return NS_ERROR_NOT_INITIALIZED;
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsISupports *layoutObject=nsnull;
  nsresult result = nsEditor::GetLayoutObject(aTable, &layoutObject); 
  if ((NS_SUCCEEDED(result)) && (nsnull!=layoutObject)) 
  { // get the table interface from the frame 
    
    result = layoutObject->QueryInterface(nsITableLayout::GetIID(), 
                            (void**)(tableLayoutObject)); 
  }
  return result;
}

/* Not scriptable: For convenience in C++ */
NS_IMETHODIMP
nsHTMLEditor::GetTableSize(nsIDOMElement *aTable, PRInt32& aRowCount, PRInt32& aColCount)
{
  aRowCount = 0;
  aColCount = 0;
  if (!aTable)
    return NS_ERROR_NOT_INITIALIZED;
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayoutObject;
  nsresult result = GetTableLayoutObject(aTable, &tableLayoutObject);
  if ((NS_SUCCEEDED(result)) && (nsnull!=tableLayoutObject)) 
  {
    result = tableLayoutObject->GetTableSize(aRowCount, aColCount); 
  }
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell, 
                            PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                            PRInt32& aRowSpan, PRInt32& aColSpan, PRBool& aIsSelected)
{
  aCell = nsnull;
  aStartRowIndex = 0;
  aStartColIndex = 0;
  aRowSpan = 0;
  aColSpan = 0;
  aIsSelected = PR_FALSE;

  if (!aTable)
    return NS_ERROR_NOT_INITIALIZED;
  
  // frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayoutObject;
  nsresult result = GetTableLayoutObject(aTable, &tableLayoutObject);
  if ((NS_SUCCEEDED(result)) && (nsnull!=tableLayoutObject)) 
  {
    // Note that this returns NS_TABLELAYOUT_CELL_NOT_FOUND when
    //  the index(es) are out of bounds
    result = tableLayoutObject->GetCellDataAt(aRowIndex, aColIndex, aCell, 
                                              aStartRowIndex, aStartColIndex,
                                              aRowSpan, aColSpan, aIsSelected);
  } 
  return result;
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

