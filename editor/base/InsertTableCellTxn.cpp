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

#include "InsertTableCellTxn.h"
#include "nsEditor.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMSelection.h"
#include "nsIPresShell.h"
#include "EditAggregateTxn.h"

static NS_DEFINE_IID(kInsertTableCellTxnIID,   INSERT_CELL_TXN_IID);
static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);

nsIAtom *InsertTableCellTxn::gInsertTableCellTxnName;

nsresult InsertTableCellTxn::ClassInit()
{
  if (nsnull==gInsertTableCellTxnName)
    gInsertTableCellTxnName = NS_NewAtom("NS_InsertTableCellTxn");
  return NS_OK;
}

InsertTableCellTxn::InsertTableCellTxn()
  : EditTxn()
{
}

InsertTableCellTxn::~InsertTableCellTxn()
{
}

NS_IMETHODIMP InsertTableCellTxn::Init(nsIDOMCharacterData *aElement,
                             PRUint32 aOffset,
                             nsIDOMNode *aNode,
                             nsIPresShell* aPresShell)
{
  mElement = do_QueryInterface(aElement);
  mOffset = aOffset;
  mNodeToInsert = aNode;
  mPresShell = aPresShell;
  return NS_OK;
}

NS_IMETHODIMP InsertTableCellTxn::Do(void)
{
  //nsresult res = mElement->InsertData(mOffset, mStringToInsert);
  // advance caret: This requires the presentation shell to get the selection.
  nsresult res = NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMSelection> selection;
  res = mPresShell->GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(res)) {
    res = selection->Collapse(mElement, mOffset+1 /*+mStringToInsert.Length()*/, SELECTION_NORMAL);
  }
  return res;
}

NS_IMETHODIMP InsertTableCellTxn::Undo(void)
{
  nsresult result = NS_ERROR_FAILURE;
#if 0
  PRUint32 length = mStringToInsert.Length();
  result = mElement->DeleteData(mOffset, length);
  if (NS_SUCCEEDED(result))
  { // set the selection to the insertion point where the string was removed
    nsCOMPtr<nsIDOMSelection> selection;
    result = mPresShell->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result)) {
      result = selection->Collapse(mElement, mOffset, SELECTION_NORMAL);
    }
  }
#endif
  return result;
}

#if 0
NS_IMETHODIMP InsertTableCellTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  // set out param default value
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  if ((nsnull!=aDidMerge) && (nsnull!=aTransaction))
  {
    // if aTransaction isa InsertTableCellTxn, and if the selection hasn't changed, 
    // then absorb it
    nsCOMPtr<InsertTableCellTxn> otherTxn(aTransaction);
    if (otherTxn)
    {
      if (PR_TRUE==IsSequentialInsert(otherTxn))
      {
        nsAutoString otherData;
        otherTxn->GetData(otherData);
        mStringToInsert += otherData;
        *aDidMerge = PR_TRUE;
      }
    }
    else
    { // the next InsertTableCellTxn might be inside an aggregate that we have special knowledge of
      nsCOMPtr<EditAggregateTxn> otherTxn(aTransaction);
      if (otherTxn)
      {
        nsCOMPtr<nsIAtom> txnName;
        otherTxn->GetName(getter_AddRefs(txnName));
        if (txnName==gInsertTableCellTxnName)
        { // yep, it's one of ours.  By definition, it must contain only
          // a single InsertTableCellTxn
          nsCOMPtr<EditTxn> childTxn;
          otherTxn->GetTxnAt(0, getter_AddRefs(childTxn));
          nsCOMPtr<InsertTableCellTxn> otherInsertTxn(childTxn);
          if (otherInsertTxn)
          {
            if (PR_TRUE==IsSequentialInsert(otherInsertTxn))
            {
              nsAutoString otherData;
              otherInsertTxn->GetData(otherData);
              mStringToInsert += otherData;
              *aDidMerge = PR_TRUE;
            }
          }
        }
      }
    }
  }
  return NS_OK;
}
#endif

NS_IMETHODIMP InsertTableCellTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP InsertTableCellTxn::GetUndoString(nsString *aString)
{
  if (nsnull!=aString)
  {
    *aString="Remove Table";
  }
  return NS_OK;
}

NS_IMETHODIMP InsertTableCellTxn::GetRedoString(nsString *aString)
{
  if (nsnull!=aString)
  {
    *aString="Insert Table";
  }
  return NS_OK;
}

/* ============= nsISupports implementation ====================== */

NS_IMETHODIMP
InsertTableCellTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kInsertTableCellTxnIID)) {
    *aInstancePtr = (void*)(InsertTableCellTxn*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return (EditTxn::QueryInterface(aIID, aInstancePtr));
}

