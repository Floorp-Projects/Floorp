/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998, 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsMsgTxn.h"
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionIID, NS_ITRANSACTION_IID);

NS_IMPL_ADDREF(nsMsgTxn)
NS_IMPL_RELEASE(nsMsgTxn)

// note that aEditor is not refcounted
nsMsgTxn::nsMsgTxn()
{
  NS_INIT_REFCNT();
}

nsMsgTxn::~nsMsgTxn()
{
}

NS_IMETHODIMP nsMsgTxn::Do(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  else
	  return NS_ERROR_NULL_POINTER;
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgTxn::GetUndoString(nsString *aString)
{
  if (nsnull!=aString)
    *aString="Undo";
  else
	  return NS_ERROR_NULL_POINTER;
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::GetRedoString(nsString *aString)
{
  if (nsnull!=aString)
    *aString="Redo";
  else
	  return NS_ERROR_NULL_POINTER;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITransactionIID)) {
    *aInstancePtr = (void*)(nsITransaction*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}



