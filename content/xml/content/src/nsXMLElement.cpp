/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsXMLElement.h"
#include "nsIDocument.h"
#include "nsIAtom.h"
#include "nsIEventListenerManager.h"
#include "nsIHTMLAttributes.h"
#include "nsIDOMScriptObjectFactory.h"

//static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIXMLContentIID, NS_IXMLCONTENT_IID);

nsresult
NS_NewXMLElement(nsIXMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIXMLContent* it = new nsXMLElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIXMLContentIID, (void**) aInstancePtrResult);
}

nsXMLElement::nsXMLElement(nsIAtom *aTag)
{
  NS_INIT_REFCNT();
  mInner.Init((nsIContent *)(nsIXMLContent *)this, aTag);
  mNameSpace = nsnull;
  mNameSpaceId = gNameSpaceId_Unknown;
  mScriptObject = nsnull;
}
 
nsXMLElement::~nsXMLElement()
{
  NS_IF_RELEASE(mNameSpace);
}

NS_IMETHODIMP 
nsXMLElement::QueryInterface(REFNSIID aIID,
			     void** aInstancePtr)
{
  NS_IMPL_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this, nsIXMLContent)
  if (aIID.Equals(kIXMLContentIID)) {
    nsIXMLContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsXMLElement)
NS_IMPL_RELEASE(nsXMLElement)

NS_IMETHODIMP 
nsXMLElement::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
  nsresult res = NS_OK;

  // XXX Yuck! Reaching into the generic content object isn't good.
  nsDOMSlots *slots = mInner.GetDOMSlots();
  if (nsnull == slots->mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }

    nsAutoString tag;
    nsIContent* parent;

    mInner.GetTagName(tag);
    mInner.GetParent(parent);

    res = factory->NewScriptXMLElement(tag, aContext, 
				       (nsISupports *)(nsIDOMElement *)this,
				       parent, (void**)&slots->mScriptObject);
    NS_IF_RELEASE(parent);
    NS_RELEASE(factory);
    
    char tagBuf[50];
    tag.ToCString(tagBuf, sizeof(tagBuf));
    
    nsIDocument *document;
    mInner.GetDocument(document);
    if (nsnull != document) {
      aContext->AddNamedReference((void *)&slots->mScriptObject,
                                  slots->mScriptObject,
                                  tagBuf);
      NS_RELEASE(document);
    }
  }
  *aScriptObject = slots->mScriptObject;
  return res;
}

NS_IMETHODIMP 
nsXMLElement::SetScriptObject(void *aScriptObject)
{
  return mInner.SetScriptObject(aScriptObject);
}

NS_IMETHODIMP 
nsXMLElement::SetNameSpace(nsIAtom* aNameSpace)
{
  NS_IF_RELEASE(mNameSpace);

  mNameSpace = aNameSpace;

  NS_IF_ADDREF(mNameSpace);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLElement::GetNameSpace(nsIAtom*& aNameSpace)
{
  aNameSpace = mNameSpace;
  
  NS_IF_ADDREF(mNameSpace);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLElement::SetNameSpaceIdentifier(PRInt32 aNameSpaceId)
{
  mNameSpaceId = aNameSpaceId;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLElement::GetNameSpaceIdentifier(PRInt32& aNameSpaceId)
{
  aNameSpaceId = mNameSpaceId;
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLElement::HandleDOMEvent(nsIPresContext& aPresContext,
			     nsEvent* aEvent,
			     nsIDOMEvent** aDOMEvent,
			     PRUint32 aFlags,
			     nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsXMLElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsXMLElement* it = new nsXMLElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo((nsIContent *)(nsIXMLContent *)this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

