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
#include "InsertTableTxn.h"
#include "InsertTableCellTxn.h"
#include "InsertTableColumnTxn.h"
#include "InsertTableRowTxn.h"
#include "DeleteTableTxn.h"
#include "DeleteTableCellTxn.h"
#include "DeleteTableColumnTxn.h"
#include "DeleteTableRowTxn.h"
#include "JoinTableCellsTxn.h"
// Include after above so Transaction types are defined first
#include "nsHTMLEditor.h"

static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);

// Table Editing methods -- for testing, hooked up to Tool Menu items
// Modeled after nsEditor::InsertText()
NS_IMETHODIMP nsHTMLEditor::InsertTable()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::CreateTxnForInsertTable(const nsIDOMElement *aTableNode, InsertTableTxn ** aTxn)
{
  if( aTableNode == nsnull || aTxn == nsnull )
  {
    return NS_ERROR_NULL_POINTER;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::InsertTableCell(PRInt32 aNumber, PRBool aAfter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::InsertTableColumn(PRInt32 aNumber, PRBool aAfter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::InsertTableRow(PRInt32 aNumber, PRBool aAfter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::DeleteTable()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::DeleteTableCell(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::DeleteTableColumn(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::DeleteTableRow(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::JoinTableCells(PRBool aCellToRight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::GetColIndexForCell(nsIDOMNode *aCellNode, PRInt32 &aCellIndex)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  aCellIndex=0; // initialize out param
  result = NS_ERROR_FAILURE;        // we return an error unless we get the index
  nsISupports *layoutObject=nsnull; // frames are not ref counted, so don't use an nsCOMPtr

  result = nsEditor::GetLayoutObject(aCellNode, &layoutObject);

  if ((NS_SUCCEEDED(result)) && (nsnull!=layoutObject))
  { // get the table cell interface from the frame
    nsITableCellLayout *cellLayoutObject=nsnull; // again, frames are not ref-counted
    result = layoutObject->QueryInterface(nsITableCellLayout::GetIID(), (void**)(&cellLayoutObject));
    if ((NS_SUCCEEDED(result)) && (nsnull!=cellLayoutObject))
    { // get the index
      result = cellLayoutObject->GetColIndex(aCellIndex);
    }
  }
  return result;
}

NS_IMETHODIMP nsHTMLEditor::GetRowIndexForCell(nsIDOMNode *aCellNode, PRInt32 &aCellIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsHTMLEditor::GetFirstCellInColumn(nsIDOMNode *aCurrentCellNode, nsIDOMNode* &aFirstCellNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsHTMLEditor::GetNextCellInColumn(nsIDOMNode *aCurrentCellNode, nsIDOMNode* &aNextCellNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsHTMLEditor::GetFirstCellInRow(nsIDOMNode *aCurrentCellNode, nsIDOMNode* &aCellNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsHTMLEditor::GetNextCellInRow(nsIDOMNode *aCurrentCellNode, nsIDOMNode* &aNextCellNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

