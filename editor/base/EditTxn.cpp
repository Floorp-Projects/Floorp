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
{
}

nsresult EditTxn::Do(void)
{
  return NS_OK;
}

nsresult EditTxn::Undo(void)
{
  return NS_OK;
}

nsresult EditTxn::Redo(void)
{
  return Do();
}

nsresult EditTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

nsresult EditTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  return NS_OK;
}

nsresult EditTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult EditTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
    *aString=nsnull;
  return NS_OK;
}

nsresult EditTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
    *aString=nsnull;
  return NS_OK;
}

nsresult
EditTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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



