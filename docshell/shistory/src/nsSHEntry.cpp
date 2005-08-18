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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsISupportsUtils.h"
#include "nsIDOMDocument.h"
#include "nsSHEntry.h"
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

//*****************************************************************************
//***    nsSHEnumerator: Object Management
//*****************************************************************************
class nsSHEnumerator : public nsIEnumerator
{
public:

  NS_DECL_ISUPPORTS

  NS_DECL_NSIENUMERATOR
  NS_IMETHOD CurrentItem(nsISHEntry ** aItem);

private:
  friend class nsSHEntry;

  nsSHEnumerator(nsSHEntry *  aEntry);
  virtual ~nsSHEnumerator();

  PRInt32     mIndex;
  nsSHEntry * mSHEntry;
};



//*****************************************************************************
//***    nsSHEntry: Object Management
//*****************************************************************************

nsSHEntry::nsSHEntry() 
{

NS_INIT_REFCNT();
mURI = (nsnull);
mPostData = (nsnull);
mDocument= (nsnull);
mLayoutHistoryState= (nsnull);
mTitle = nsnull;

}


NS_IMETHODIMP
nsSHEntry::Create(nsIURI * aURI, const PRUnichar * aTitle, nsIDOMDocument * aDOMDocument,
			         nsIInputStream * aInputStream, nsISupports * aHistoryLayoutState)
{
    SetUri(aURI);
	SetTitle(aTitle);
	SetDocument(aDOMDocument);
	SetPostData(aInputStream);
	SetLayoutHistoryState(aHistoryLayoutState);
	return NS_OK;
	
}


nsSHEntry::~nsSHEntry() 
{
  NS_IF_RELEASE(mURI);
  NS_IF_RELEASE(mPostData);
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mLayoutHistoryState);
  if (mTitle)
	  delete mTitle;

  DestroyChildren();
}

void
nsSHEntry::DestroyChildren() {

  PRInt32 i, n;

  n = GetChildCount(&n);
  for (i = 0; i < n; i++) {
    nsISHEntry* child = (nsISHEntry *) mChildren.ElementAt(i);
    child->SetParent(nsnull);    // Weak reference to parent 
    delete child;
  }
  mChildren.Clear();
  

}

// nsSHEntry::nsISupports

NS_IMPL_ISUPPORTS2(nsSHEntry, nsISHEntry, nsISHContainer)

NS_IMETHODIMP
nsSHEntry::GetUri(nsIURI **aUri)
{
  NS_ENSURE_ARG_POINTER(aUri);

  *aUri = mURI;
  NS_IF_ADDREF(mURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetUri(nsIURI * aUri)
{

  NS_IF_RELEASE(mURI);
  mURI =  aUri;
  NS_IF_ADDREF(mURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetDocument(nsIDOMDocument * aDocument)
{
  NS_IF_RELEASE(mDocument);
  mDocument = aDocument;
  NS_IF_ADDREF(mDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetDocument(nsIDOMDocument ** aResult)
{
	NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mDocument;
    NS_IF_ADDREF(mDocument);
    return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetTitle(PRUnichar** aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  if (mTitle)
    *aTitle = mTitle->ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetTitle(const PRUnichar* aTitle)
{

  if (mTitle)
    delete mTitle;
  if (aTitle)
    mTitle =  new nsString(aTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetPostData(nsIInputStream ** aResult)
{
	NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mPostData;
    NS_IF_ADDREF(mPostData);
    return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetPostData(nsIInputStream * aPostData)
{
  NS_IF_RELEASE(mPostData);
  mPostData = aPostData;
  NS_IF_ADDREF(aPostData);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLayoutHistoryState(nsISupports ** aResult)
{
	NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mLayoutHistoryState;
    NS_IF_ADDREF(mLayoutHistoryState);
    return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLayoutHistoryState(nsISupports * aState)
{
  NS_IF_RELEASE(mLayoutHistoryState);
  mLayoutHistoryState = aState;
  NS_IF_ADDREF(aState);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetParent(nsISHEntry ** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mParent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


NS_IMETHODIMP
nsSHEntry::SetParent(nsISHEntry * aParent)
{
	/* parent not Addrefed on purpose to avoid cyclic reference
	 * Null parent is OK
	 */
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP 
nsSHEntry::GetChildCount(PRInt32 * aCount)
{
	NS_ENSURE_ARG_POINTER(aCount);
    *aCount = mChildren.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AddChild(nsISHEntry * aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

	NS_ENSURE_SUCCESS(aChild->SetParent(this), NS_ERROR_FAILURE);
	mChildren.AppendElement((void *)aChild);
	NS_ADDREF(aChild);

    return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::RemoveChild(nsISHEntry * aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);
	PRBool childRemoved = mChildren.RemoveElement((void *)aChild);
	if (childRemoved) {
	  aChild->SetParent(nsnull);
	  NS_RELEASE(aChild);
	}
    return NS_OK;
}


NS_IMETHODIMP
nsSHEntry::GetChildEnumerator(nsIEnumerator** aChildEnumerator)
{
	nsresult status = NS_OK;

    NS_ENSURE_ARG_POINTER(aChildEnumerator);
	nsSHEnumerator * iterator = new nsSHEnumerator(this);
	if (iterator && !!NS_SUCCEEDED(status = CallQueryInterface(iterator, aChildEnumerator)))
      delete iterator;
    return status;
}


//*****************************************************************************
//***    nsSHEnumerator: Object Management
//*****************************************************************************

nsSHEnumerator::nsSHEnumerator(nsSHEntry * aEntry):mIndex(0)
{
  NS_INIT_REFCNT();
  mSHEntry = aEntry;
}

nsSHEnumerator::~nsSHEnumerator()
{
}


NS_IMETHODIMP
nsSHEnumerator::Next()
{
  mIndex++;
  PRUint32 cnt=0;
  cnt = mSHEntry->mChildren.Count();
  if (mIndex < (PRInt32)cnt)
    return NS_OK;
  return NS_ERROR_FAILURE;
}


#if 0
NS_IMETHODIMP
nsSHEnumerator::Prev()
{
  mIndex--;
  if (mIndex >= 0 )
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSHEnumerator::Last()
{ 
  PRUint32 cnt;
  nsresult rv = mSHEntry->mChildren.Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  mIndex = (PRInt32)cnt-1;
  return NS_OK;
}
#endif  /* 0 */

NS_IMETHODIMP
nsSHEnumerator::First()
{
  mIndex = 0;
  return NS_OK;
}


NS_IMETHODIMP 
nsSHEnumerator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  PRUint32 cnt= mSHEntry->mChildren.Count();
  if (mIndex >=0 && mIndex < (PRInt32)cnt){
    *aItem = (nsISupports *)mSHEntry->mChildren.ElementAt(mIndex);
	NS_IF_ADDREF(*aItem);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS1(nsSHEnumerator, nsIEnumerator)


NS_IMETHODIMP 
nsSHEnumerator::CurrentItem(nsISHEntry **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  PRUint32 cnt = mSHEntry->mChildren.Count();
  if (mIndex >=0 && mIndex < (PRInt32)cnt){
    nsCOMPtr<nsISupports> indexIsupports =  (nsISHEntry *) mSHEntry->mChildren.ElementAt(mIndex);
    return indexIsupports->QueryInterface(NS_GET_IID(nsISHEntry),(void **)aItem);
  }
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsSHEnumerator::IsDone()
{
  PRUint32 cnt;
  cnt = mSHEntry->mChildren.Count();
  if (mIndex >= 0 && mIndex < (PRInt32)cnt ) { 
    return NS_ENUMERATOR_FALSE;
  }
  return NS_OK;
}


#if 0
NS_IMETHODIMP
nsRangeListIterator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIEnumerator))) {
    *aInstancePtr = NS_STATIC_CAST(nsIEnumerator*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIBidirectionalEnumerator))) {
    *aInstancePtr = NS_STATIC_CAST(nsIBidirectionalEnumerator*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return mDomSelection->QueryInterface(aIID, aInstancePtr);
}
#endif

////////////END nsSHEnumerator methods

NS_IMETHODIMP
NS_NewSHEntry(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aOuter == nsnull, "no aggregation");
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsresult rv = NS_OK;

  nsSHEntry* result = new nsSHEntry();
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
