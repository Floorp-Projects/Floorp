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

#include "nsDocumentFragment.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMScriptObjectFactory.h"

static NS_DEFINE_IID(kIDOMDocumentFragmentIID, NS_IDOMDOCUMENTFRAGMENT_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
		       nsIDocument* aOwnerDocument)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsDocumentFragment* it = new nsDocumentFragment(aOwnerDocument);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIDOMDocumentFragmentIID, 
			    (void**) aInstancePtrResult);
}

nsDocumentFragment::nsDocumentFragment(nsIDocument* aOwnerDocument)
{
  NS_INIT_REFCNT();
  mInner.Init(this, nsnull);
  mScriptObject = nsnull;
  mOwnerDocument = aOwnerDocument;
  NS_IF_ADDREF(mOwnerDocument);
}

nsDocumentFragment::~nsDocumentFragment()
{
  NS_IF_RELEASE(mOwnerDocument);
}
 
NS_IMPL_ADDREF(nsDocumentFragment)
NS_IMPL_RELEASE(nsDocumentFragment)

nsresult
nsDocumentFragment::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMDocumentFragment* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMDocumentFragmentIID)) {
    nsIDOMDocumentFragment* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNodeIID)) {
    nsIDOMDocumentFragment* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIContentIID)) {
    nsIContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeName(nsString& aNodeName)
{
  aNodeName.SetString("#document-fragment");
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeValue(nsString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::SetNodeValue(const nsString& aNodeValue)
{
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::DOCUMENT_FRAGMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  if (nsnull != mOwnerDocument) {
    return mOwnerDocument->QueryInterface(kIDOMDocumentIID, (void **)aOwnerDocument);
  }
  else {
    *aOwnerDocument = nsnull;
    return NS_OK;
  }
}


NS_IMETHODIMP    
nsDocumentFragment::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDocumentFragment* it;
  it = new nsDocumentFragment(mOwnerDocument);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
//XXX  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP 
nsDocumentFragment::GetScriptObject(nsIScriptContext* aContext, 
				    void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = mInner.GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }
    
    res = factory->NewScriptDocumentFragment(aContext, 
					     (nsISupports*)(nsIDOMDocumentFragment*)this, 
					     mOwnerDocument,
					     (void**)&mScriptObject);
    NS_RELEASE(factory);
    
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP 
nsDocumentFragment::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

