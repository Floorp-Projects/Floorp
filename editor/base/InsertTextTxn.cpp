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

#include "InsertTextTxn.h"
#include "editor.h"
#include "nsIDOMCharacterData.h"

static NS_DEFINE_IID(kInsertTextTxnIID, INSERTTEXTTXN_IID);

// note that aEditor is not refcounted
InsertTextTxn::InsertTextTxn(nsEditor *aEditor,
                             nsIDOMCharacterData *aElement,
                             PRUint32 aOffset,
                             const nsString& aStringToInsert)
  : EditTxn(aEditor)
{
  mElement = aElement;
  mOffset = aOffset;
  mStringToInsert = aStringToInsert;
}

nsresult InsertTextTxn::Do(void)
{
  return (mElement->InsertData(mOffset, mStringToInsert));
}

nsresult InsertTextTxn::Undo(void)
{
  PRUint32 length = mStringToInsert.Length();
  return (mElement->DeleteData(mOffset, length));
}

nsresult InsertTextTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

nsresult InsertTextTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  // set out param default value
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;

  if ((nsnull!=aDidMerge) && (nsnull!=aTransaction))
  {
    // if aTransaction isa InsertTextTxn, absorb it
    nsCOMPtr<InsertTextTxn> otherTxn;
    nsresult result = aTransaction->QueryInterface(kInsertTextTxnIID, getter_AddRefs(otherTxn));
    if (NS_SUCCEEDED(result) && (otherTxn))
    {
      nsString otherData;
      otherTxn->GetData(otherData);
      mStringToInsert += otherData;
    }
    *aDidMerge = PR_TRUE;
  }
  return NS_OK;
}

nsresult InsertTextTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult InsertTextTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Text: ";
    **aString += mStringToInsert;
  }
  return NS_OK;
}

nsresult InsertTextTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Text: ";
    **aString += mStringToInsert;
  }
  return NS_OK;
}

/* ============= nsISupports implementation ====================== */

nsresult
InsertTextTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kInsertTextTxnIID)) {
    *aInstancePtr = (void*)(InsertTextTxn*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return (EditTxn::QueryInterface(aIID, aInstancePtr));
}

/* ============ protected methods ================== */

nsresult InsertTextTxn::GetData(nsString& aResult)
{
  aResult = mStringToInsert;
  return NS_OK;
}
