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
  SetTransactionDescriptionID( kTransactionID );
  /* log description initialized in parent constructor */
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
#ifdef DEBUG_IME
	printf("Do IME Commit");
#endif

	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::Undo(void)
{
#ifdef DEBUG_IME
	printf("Undo IME Commit");
#endif
	
	return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
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

NS_IMETHODIMP IMECommitTxn::Write(nsIOutputStream *aOutputStream)
{
  NS_ASSERTION(aOutputStream, "null ptr- aOutputStream");
  if(nsnull == aOutputStream)
	return NS_ERROR_NULL_POINTER;
  else 
    return NS_OK;
}

NS_IMETHODIMP IMECommitTxn::GetUndoString(nsString *aString)
{
  NS_ASSERTION(aString, "null ptr- aString");
  if(nsnull == aString) {
	return NS_ERROR_NULL_POINTER;
  } else {
    aString->AssignWithConversion("Remove IMECommit: ");
    return NS_OK;
  }
}

NS_IMETHODIMP IMECommitTxn::GetRedoString(nsString *aString)
{
  NS_ASSERTION(aString, "null ptr- aString");
  if(nsnull == aString) {
	return NS_ERROR_NULL_POINTER;
  } else {
    aString->AssignWithConversion("Insert IMECommit: ");
    return NS_OK;
  }
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


