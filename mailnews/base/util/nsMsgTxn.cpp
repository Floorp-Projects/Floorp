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

NS_IMPL_THREADSAFE_ADDREF(nsMsgTxn)
NS_IMPL_THREADSAFE_RELEASE(nsMsgTxn)
NS_IMPL_THREADSAFE_QUERY_INTERFACE1(nsMsgTxn, nsITransaction)

// note that aEditor is not refcounted
nsMsgTxn::nsMsgTxn() 
{
  NS_INIT_REFCNT();
  m_txnType = 0;
  m_undoString.AssignWithConversion("Undo");
  m_redoString.AssignWithConversion("Redo");
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


NS_IMETHODIMP nsMsgTxn::GetMsgWindow(nsIMsgWindow **msgWindow)
{
    if (!msgWindow || !m_msgWindow)
        return NS_ERROR_NULL_POINTER;
    *msgWindow = m_msgWindow;
    NS_ADDREF (*msgWindow);
    return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::SetMsgWindow(nsIMsgWindow *msgWindow)
{
    m_msgWindow = msgWindow;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgTxn::SetUndoString(nsString *aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	m_undoString = *aString;
	return NS_OK;
}

NS_IMETHODIMP
nsMsgTxn::SetRedoString(nsString* aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	m_redoString = *aString;
	return NS_OK;
}

NS_IMETHODIMP
nsMsgTxn::GetUndoString(nsString* aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	*aString = m_undoString;
	return NS_OK;
}

NS_IMETHODIMP
nsMsgTxn::GetRedoString(nsString* aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	*aString = m_redoString;
	return NS_OK;
}


NS_IMETHODIMP 
nsMsgTxn::GetTransactionType(PRUint32 *txnType)
{
    if (!txnType) 
        return NS_ERROR_NULL_POINTER;
    *txnType = m_txnType;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgTxn::SetTransactionType(PRUint32 txnType)
{
    m_txnType = txnType;
    return NS_OK;
}

