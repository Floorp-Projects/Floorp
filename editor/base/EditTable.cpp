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

#include "nsITableCellLayout.h"   // for temp method GetColIndexForCell, to be removed

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
nsHTMLEditor::GetRowIndex(nsIDOMElement *aCell, PRInt32 &aRowIndex)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  aRowIndex=0;
  result = NS_ERROR_FAILURE;        // we return an error unless we get the index
  nsISupports *layoutObject=nsnull; // frames are not ref counted, so don't use an nsCOMPtr

  result = nsEditor::GetLayoutObject(aCell, &layoutObject);

  if ((NS_SUCCEEDED(result)) && (nsnull!=layoutObject))
  { // get the table cell interface from the frame
    nsITableCellLayout *cellLayoutObject=nsnull; // again, frames are not ref-counted
    result = layoutObject->QueryInterface(nsITableCellLayout::GetIID(), (void**)(&cellLayoutObject));
    if ((NS_SUCCEEDED(result)) && (nsnull!=cellLayoutObject))
    { // get the index
      result = cellLayoutObject->GetRowIndex(aRowIndex);
    }
  }
  return result;
}


NS_IMETHODIMP 
nsHTMLEditor::GetColumnIndex(nsIDOMElement *aCell, PRInt32 &aColIndex)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  aColIndex=0; // initialize out params
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
    }
  }
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::GetColumnCellCount(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32& aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsHTMLEditor::GetRowCellCount(nsIDOMElement* aTable, PRInt32 aColIndex, PRInt32& aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsHTMLEditor::GetMaxColumnCellCount(nsIDOMElement* aTable, PRInt32& aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsHTMLEditor::GetMaxRowCellCount(nsIDOMElement* aTable, PRInt32& aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//TODO: This should be implemented by layout for efficiency
NS_IMETHODIMP 
nsHTMLEditor::GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsHTMLEditor::GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell, 
                            PRInt32& aStartRowIndex, PRInt32& aStartColIndex, PRInt32& aRowSpan, PRInt32& aColSpan)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
