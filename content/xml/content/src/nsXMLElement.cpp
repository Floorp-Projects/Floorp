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

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsINameSpaceManager.h"

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
  mNameSpacePrefix = nsnull;
  mNameSpaceID = kNameSpaceID_None;
  mScriptObject = nsnull;
  mIsLink = PR_FALSE;
}
 
nsXMLElement::~nsXMLElement()
{
  NS_IF_RELEASE(mNameSpacePrefix);
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
nsXMLElement::SetNameSpacePrefix(nsIAtom* aNameSpacePrefix)
{
  NS_IF_RELEASE(mNameSpacePrefix);

  mNameSpacePrefix = aNameSpacePrefix;

  NS_IF_ADDREF(mNameSpacePrefix);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLElement::GetNameSpacePrefix(nsIAtom*& aNameSpacePrefix) const
{
  aNameSpacePrefix = mNameSpacePrefix;
  
  NS_IF_ADDREF(mNameSpacePrefix);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLElement::SetNameSpaceID(PRInt32 aNameSpaceID)
{
  mNameSpaceID = aNameSpaceID;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLElement::GetNameSpaceID(PRInt32& aNameSpaceID) const
{
  aNameSpaceID = mNameSpaceID;
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLElement::SetAttribute(const nsString& aName, 
			   const nsString& aValue,
			   PRBool aNotify)
{
  // XXX It sucks that we have to do a couple of strcmps for
  // every attribute set. It might be a bit more expensive
  // to create an atom.
  // XXX this is wrong anyway, the attributes need to use the 
  // namespace ID, not the prefix
  if (aName.Equals("xml:link") && (aValue.Equals("simple"))) {
    mIsLink = PR_TRUE;
  }

  return mInner.SetAttribute(aName, aValue, aNotify);
}

NS_IMETHODIMP 
nsXMLElement::HandleDOMEvent(nsIPresContext& aPresContext,
			     nsEvent* aEvent,
			     nsIDOMEvent** aDOMEvent,
			     PRUint32 aFlags,
			     nsEventStatus& aEventStatus)
{
  // Try script event handlers first
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                       aFlags, aEventStatus);

  if (mIsLink && (NS_OK == ret) && (nsEventStatus_eIgnore == aEventStatus)) {
    switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          stateManager->SetActiveLink(this);
          NS_RELEASE(stateManager);
        }
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      {
        nsIEventStateManager *stateManager;
        nsIContent *activeLink;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          stateManager->GetActiveLink(&activeLink);
          NS_RELEASE(stateManager);
        }

        if (activeLink == this) {
          if (nsEventStatus_eConsumeNoDefault != aEventStatus) {
            nsAutoString show, href, base, target;
	          nsLinkVerb verb = eLinkVerb_Replace;
	          base.Truncate();
	          target.Truncate();
            GetAttribute(nsString("href"), href);
            GetAttribute(nsString("show"), show);
	          // XXX Should probably do this using atoms 
	          if (show.Equals("new")) {
	            verb = eLinkVerb_New;
	          }
	          else if (show.Equals("embed")) {
	            verb = eLinkVerb_Embed;
	          }
            mInner.TriggerLink(aPresContext, verb, base, href, target, PR_TRUE);
            aEventStatus = nsEventStatus_eConsumeNoDefault; 
          }
        }
      }
      break;

    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      // XXX Bring up a contextual menu provided by the application
      break;

    case NS_MOUSE_ENTER:
      //mouse enter doesn't work yet.  Use move until then.
      {
        nsAutoString base, href, target;
	base.Truncate();
	target.Truncate();
        GetAttribute(nsString("href"), href);
        mInner.TriggerLink(aPresContext, eLinkVerb_Replace, base, href, target, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT:
      {
        nsAutoString empty;
        mInner.TriggerLink(aPresContext, eLinkVerb_Replace, empty, empty, empty, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

    default:
      break;
    }
  }
  return ret;
}

NS_IMETHODIMP
nsXMLElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsXMLElement* it = new nsXMLElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo((nsIContent *)(nsIXMLContent *)this, &it->mInner);
  it->mNameSpacePrefix = mNameSpacePrefix;
  NS_IF_ADDREF(mNameSpacePrefix);
  it->mNameSpaceID = mNameSpaceID;
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

