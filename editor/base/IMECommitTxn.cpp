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
 */

#include "IMECommitTxn.h"
#include "nsEditor.h"

// #define DEBUG_IME
nsIAtom *IMECommitTxn::gIMECommitTxnName = nsnull;

nsresult IMECommitTxn::ClassInit()
{
  if (nsnull==gIMECommitTxnName)
    gIMECommitTxnName = NS_NewAtom("NS_IMECommitTxn");
  return NS_OK;
}

nsresult IMECommitTxn::ClassShutdown()
{
  NS_IF_RELEASE(gIMECommitTxnName);
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

NS_IMETHODIMP IMECommitTxn::DoTransaction(void)
{
#ifdef DEBUG_IME
	printf("Do IME Commit");
#endif

	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::UndoTransaction(void)
{
#ifdef DEBUG_IME
	printf("Undo IME Commit");
#endif
	
	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
#ifdef DEBUG_IME
	printf("Merge IME Commit");
#endif

	NS_ASSERTION(aDidMerge, "null ptr- aDidMerge");
	NS_ASSERTION(aTransaction, "null ptr- aTransaction");
	if((nsnull == aDidMerge) || (nsnull == aTransaction))
		return NS_ERROR_NULL_POINTER;
		
	*aDidMerge=PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::GetTxnDescription(nsAWritableString& aString)
{
  aString.Assign(NS_LITERAL_STRING("IMECommitTxn"));
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


