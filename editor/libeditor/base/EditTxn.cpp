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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionIID, NS_ITRANSACTION_IID);

NS_IMPL_ADDREF(EditTxn)
NS_IMPL_RELEASE(EditTxn)

// note that aEditor is not refcounted
EditTxn::EditTxn()
  : mTransactionID(-1)
{
  NS_INIT_REFCNT();
}

EditTxn::~EditTxn()
{
}

NS_IMETHODIMP EditTxn::Do(void)
{
  return NS_OK;
}

NS_IMETHODIMP EditTxn::Undo(void)
{
  return NS_OK;
}

NS_IMETHODIMP EditTxn::Redo(void)
{
  return Do();
}

NS_IMETHODIMP EditTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP EditTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  return NS_OK;
}

NS_IMETHODIMP EditTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP EditTxn::GetUndoString(nsString *aString)
{
  if (nsnull!=aString)
    aString->SetLength(0);
  return NS_OK;
}

NS_IMETHODIMP EditTxn::GetRedoString(nsString *aString)
{
  if (nsnull!=aString)
    aString->SetLength(0);
  return NS_OK;
}

NS_IMETHODIMP EditTxn::GetLogDescription(PRUnichar * *aString)
{
  if (nsnull!=aString)
    *aString = mLogDescription.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP EditTxn::SetLogDescription(const PRUnichar *aString)
{
  mLogDescription = (PRUnichar *)aString;
  return NS_OK;
}

NS_IMETHODIMP EditTxn::GetTransactionDescriptionID(int *aID)
{
  if (nsnull!=aID)
    *aID = mTransactionID;
  return NS_OK;
}

NS_IMETHODIMP EditTxn::SetTransactionDescriptionID(int aID)
{
  mTransactionID = aID;
  return NS_OK;
}

NS_IMETHODIMP
EditTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsITransaction *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtr = (void*)(nsISupports*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITransactionIID)) {
    *aInstancePtr = (void*)(nsITransaction*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsITransactionDescription))) {
    *aInstancePtr = (void*)(nsITransactionDescription*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}



