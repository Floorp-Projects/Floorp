/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
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

#include "EditAggregateTxn.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsVoidArray.h"

EditAggregateTxn::EditAggregateTxn()
  : EditTxn()
{
  // base class does this: NS_INIT_REFCNT();
  mChildren = new nsVoidArray();
}

EditAggregateTxn::~EditAggregateTxn()
{
  if (nsnull!=mChildren)
  {
    PRInt32 i;
    PRInt32 count = mChildren->Count();
    for (i=0; i<count; i++)
    {
      EditTxn *txn = (EditTxn*)(mChildren->ElementAt(i));
      NS_IF_RELEASE(txn);
    }
    delete mChildren;
  }
}

NS_IMETHODIMP EditAggregateTxn::Do(void)
{
  nsresult result=NS_OK;  // it's legal (but not very useful) to have an empty child list
  if (nsnull!=mChildren)
  {
    PRInt32 i;
    PRInt32 count = mChildren->Count();
    for (i=0; i<count; i++)
    {
      EditTxn *txn = (EditTxn*)(mChildren->ElementAt(i));
      result = txn->Do();
      if (NS_FAILED(result))
        break;
    }  
  }
  return result;
}

NS_IMETHODIMP EditAggregateTxn::Undo(void)
{
  nsresult result=NS_OK;  // it's legal (but not very useful) to have an empty child list
  if (nsnull!=mChildren)
  {
    PRInt32 i;
    PRInt32 count = mChildren->Count();
    // undo goes through children backwards
    for (i=count-1; i>=0; i--)
    {
      EditTxn *txn = (EditTxn*)(mChildren->ElementAt(i));
      result = txn->Undo();
      if (NS_FAILED(result))
        break;
    }  
  }
  return result;
}

NS_IMETHODIMP EditAggregateTxn::Redo(void)
{
  nsresult result=NS_OK;  // it's legal (but not very useful) to have an empty child list
  if (nsnull!=mChildren)
  {
    PRInt32 i;
    PRInt32 count = mChildren->Count();
    for (i=0; i<count; i++)
    {
      EditTxn *txn = (EditTxn*)(mChildren->ElementAt(i));
      result = txn->Redo();
      if (NS_FAILED(result))
        break;
    }  
  }
  return result;
}

NS_IMETHODIMP EditAggregateTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  nsresult result=NS_OK;  // it's legal (but not very useful) to have an empty child list
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  if (nsnull!=mChildren)
  {
    PRInt32 count = mChildren->Count();
    NS_ASSERTION(count>0, "bad count");
    if (0<count)
    {
      EditTxn *txn = (EditTxn*)(mChildren->ElementAt(count-1));
      result = txn->Merge(aDidMerge, aTransaction);
    }
  }
  return result;

}

NS_IMETHODIMP EditAggregateTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
    *aString=nsnull;
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
    *aString=nsnull;
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::AppendChild(EditTxn *aTxn)
{
  if ((nsnull!=mChildren) && (nsnull!=aTxn))
  {
    mChildren->AppendElement(aTxn);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP EditAggregateTxn::GetCount(PRInt32 *aCount)
{
  if (!aCount) {
    return NS_ERROR_NULL_POINTER;
  }
  *aCount=0;
  if (mChildren) {
    *aCount = mChildren->Count();
  }
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::GetTxnAt(PRInt32 aIndex, EditTxn **aTxn)
{
  if (!aTxn) {
    return NS_ERROR_NULL_POINTER;
  }
  if (!mChildren) {
    return NS_ERROR_UNEXPECTED;
  }
  const PRInt32 txnCount = mChildren->Count();
  if (0>aIndex || txnCount<=aIndex) {
    return NS_ERROR_UNEXPECTED;
  }
  *aTxn = (EditTxn *)(mChildren->ElementAt(aIndex));
  if (!*aTxn)
    return NS_ERROR_UNEXPECTED;
  NS_ADDREF(*aTxn);
  return NS_OK;
}


NS_IMETHODIMP EditAggregateTxn::SetName(nsIAtom *aName)
{
  mName = do_QueryInterface(aName);
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::GetName(nsIAtom **aName)
{
  if (aName)
  {
    if (mName)
    {
      *aName = mName;
      NS_ADDREF(*aName);
      return NS_OK;
    }
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP_(nsrefcnt) EditAggregateTxn::AddRef(void)
{
  return EditTxn::AddRef();
}

//NS_IMPL_RELEASE_INHERITED(Class, Super)
NS_IMETHODIMP_(nsrefcnt) EditAggregateTxn::Release(void)
{
  return EditTxn::Release();
}

//NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, AdditionalInterface)
NS_IMETHODIMP EditAggregateTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
 
  if (aIID.Equals(EditAggregateTxn::GetIID())) {
    *aInstancePtr = (nsISupports*)(EditAggregateTxn*)(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return EditTxn::QueryInterface(aIID, aInstancePtr);
}






