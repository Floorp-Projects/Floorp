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

#include "IMECommitTxn.h"
#include "nsEditor.h"

nsIAtom *IMECommitTxn::gIMECommitTxnName = nsnull;

nsresult IMECommitTxn::ClassInit()
{
  if (nsnull==gIMECommitTxnName)
    gIMECommitTxnName = NS_NewAtom("NS_IMECommitTxn");
  return NS_OK;
}

IMECommitTxn::IMECommitTxn()
  : EditTxn()
{
}

IMECommitTxn::~IMECommitTxn()
{
}

NS_IMETHODIMP IMECommitTxn::Init(void)
{
	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::Do(void)
{
#ifdef DEBUG_tague
	printf("Do IME Commit");
#endif

	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::Undo(void)
{
#ifdef DEBUG_TAGUE
	printf("Undo IME Commit");
#endif
	
	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
#ifdef DEBUG_TAGUE
	printf("Merge IME Commit");
#endif

	*aDidMerge=PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::GetUndoString(nsString *aString)
{
  if (nsnull!=aString)
  {
    *aString="Remove IMECommit: ";
  }
  return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::GetRedoString(nsString *aString)
{
  if (nsnull!=aString)
  {
    *aString="Insert IMECommit: ";
  }
  return NS_OK;
}

/* ============= nsISupports implementation ====================== */

NS_IMETHODIMP
IMECommitTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(IMECommitTxn::GetCID())) {
    *aInstancePtr = (void*)(IMECommitTxn*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return (EditTxn::QueryInterface(aIID, aInstancePtr));
}


