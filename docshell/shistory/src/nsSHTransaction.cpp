/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
 */

#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsIDOMDocument.h"
#include "nsSHTransaction.h"
#include "nsIGenericFactory.h"


#ifdef XXX_NS_DEBUG       // XXX: we'll need a logging facility for debugging
#define WEB_TRACE(_bit,_args)            \
   PR_BEGIN_MACRO                         \
     if (WEB_LOG_TEST(gLogModule,_bit)) { \
       PR_LogPrint _args;                 \
     }                                    \
   PR_END_MACRO
#else
#define WEB_TRACE(_bit,_args)
#endif

NS_IMPL_ISUPPORTS1(nsSHTransaction, nsISHTransaction)

nsSHTransaction::nsSHTransaction() : mPersist(PR_TRUE), mParent(nsnull), 
   mChild(nsnull), mLRVList(nsnull), mSHEntry(nsnull)
{
NS_INIT_REFCNT();
}


nsSHTransaction::~nsSHTransaction()
{

  NS_IF_RELEASE(mSHEntry);
  mParent = nsnull; //Weak reference to parent transaction
  NS_IF_RELEASE(mChild);
  NS_IF_RELEASE(mLRVList);
  
}

NS_IMETHODIMP
nsSHTransaction::Create(nsISHEntry * aSHEntry, nsISHTransaction * aParent)
{
     SetSHEntry(aSHEntry);
	 if (aParent) {
		 // This will correctly set the parent child pointers 		
        ((nsSHTransaction *)aParent)->SetChild(this);
	 }
	 else
		 SetParent(nsnull);
	 return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::GetSHEntry(nsISHEntry ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
	*aResult = mSHEntry;
	NS_IF_ADDREF(mSHEntry);
	return NS_OK;
}


nsresult
nsSHTransaction::SetSHEntry(nsISHEntry * aSHEntry)
{
	NS_IF_RELEASE(mSHEntry);
	mSHEntry = aSHEntry;
	NS_IF_ADDREF(mSHEntry);
	return NS_OK;
}


NS_IMETHODIMP
nsSHTransaction::GetChild(nsISHTransaction * * aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);
   *aResult = mChild;
   NS_IF_ADDREF(mChild);
   return NS_OK;
}


nsresult
nsSHTransaction::SetChild(nsISHTransaction * aChild)
{
	if (mChild) {
		// There is already a child. Move the child to the LRV list
		NS_IF_RELEASE(mLRVList);
		mLRVList = mChild;
		NS_ADDREF(mLRVList);		
		//SetLRVList(mChild);
	}

   NS_ENSURE_SUCCESS(((nsSHTransaction *)aChild)->SetParent(this), NS_ERROR_FAILURE);
   NS_IF_RELEASE(mChild);
   mChild = aChild;
   NS_IF_ADDREF(aChild);
   return NS_OK;
}



nsresult
nsSHTransaction::SetParent(nsISHTransaction * aParent)
{
	/* This is weak reference to parent. Do not Addref it */
     mParent = aParent;
	 return NS_OK;
}

#if 0
NS_IMETHODIMP
nsSHTransaction::SetLRVList(nsISHTransaction * aLRVList) {
	
   NS_IF_RELEASE(mLRVList);
   mLRVList = aLRVList;
   NS_IF_ADDREF(mLRVList);
   return NS_OK;
   
}
#endif  /* 0 */

nsresult
nsSHTransaction::GetParent(nsISHTransaction ** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);
   *aResult  = mParent;
   NS_IF_ADDREF(*aResult);
   return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::GetLrvList(nsISHTransaction ** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);
   *aResult = mLRVList;
   NS_IF_ADDREF(mLRVList);
   return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::SetPersist(PRBool aPersist)
{
   mPersist = aPersist;
   return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::GetPersist(PRBool* aPersist)
{
   NS_ENSURE_ARG_POINTER(aPersist);

   *aPersist = mPersist;
   return NS_OK;
}


NS_IMETHODIMP
NS_NewSHTransaction(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aOuter == nsnull, "no aggregation");
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsresult rv = NS_OK;

  nsSHTransaction* result = new nsSHTransaction();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;


  rv = result->QueryInterface(aIID, aResult);

  if (NS_FAILED(rv)) {
    delete result;
    *aResult = nsnull;
    return rv;
  }

  return rv;
}
