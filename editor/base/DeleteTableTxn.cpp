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

#include "DeleteTableTxn.h"
#include "nsEditor.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMSelection.h"
#include "nsIPresShell.h"
#include "EditAggregateTxn.h"

static NS_DEFINE_IID(kDeleteTableTxnIID,   DELETE_TABLE_TXN_IID);
static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);

nsIAtom *DeleteTableTxn::gDeleteTableTxnName;

nsresult DeleteTableTxn::ClassInit()
{
  if (nsnull==gDeleteTableTxnName)
    gDeleteTableTxnName = NS_NewAtom("NS_DeleteTableTxn");
  return NS_OK;
}

DeleteTableTxn::DeleteTableTxn()
  : EditTxn()
{
}

DeleteTableTxn::~DeleteTableTxn()
{
}

NS_IMETHODIMP DeleteTableTxn::Init(nsIDOMCharacterData *aElement,
                             nsIDOMNode *aNode,
                             nsIPresShell* aPresShell)
{
  mElement = do_QueryInterface(aElement);
  mNodeToInsert = aNode;
  mPresShell = aPresShell;
  return NS_OK;
}

NS_IMETHODIMP DeleteTableTxn::Do(void)
{
  //nsresult res = mElement->InsertData(mOffset, mStringToInsert);
  // advance caret: This requires the presentation shell to get the selection.
  nsresult res = NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMSelection> selection;
  res = mPresShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(res)) {
    res = selection->Collapse(mElement, 0 /*mOffset+1*/ /*+mStringToInsert.Length()*/);
  }
  return res;
}

NS_IMETHODIMP DeleteTableTxn::Undo(void)
{
  nsresult result = NS_ERROR_FAILURE;
#if 0
  PRUint32 length = mStringToInsert.Length();
  result = mElement->DeleteData(mOffset, length);
  if (NS_SUCCEEDED(result))
  { // set the selection to the insertion point where the string was removed
    nsCOMPtr<nsIDOMSelection> selection;
    result = mPresShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_SUCCEEDED(result)) {
      result = selection->Collapse(mElement, mOffset);
    }
  }
#endif
  return result;
}

NS_IMETHODIMP DeleteTableTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP DeleteTableTxn::GetUndoString(nsString *aString)
{
  if (nsnull!=aString)
  {
    *aString="Remove Table";
  }
  return NS_OK;
}

NS_IMETHODIMP DeleteTableTxn::GetRedoString(nsString *aString)
{
  if (nsnull!=aString)
  {
    *aString="Insert Table";
  }
  return NS_OK;
}

/* ============= nsISupports implementation ====================== */

NS_IMETHODIMP
DeleteTableTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kDeleteTableTxnIID)) {
    *aInstancePtr = (void*)(DeleteTableTxn*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return (EditTxn::QueryInterface(aIID, aInstancePtr));
}

