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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsXMLElement.h"
#include "nsIDocument.h"
#include "nsIAtom.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMScriptObjectFactory.h"

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"

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

static nsIAtom* kSimpleAtom;  // XXX these should get moved to nsXMLAtoms
static nsIAtom* kHrefAtom;
static nsIAtom* kShowAtom;
static nsIAtom* kTypeAtom;
static nsIAtom* kBaseAtom;
static PRUint32 kElementCount;

nsXMLElement::nsXMLElement(nsIAtom *aTag)
{
  NS_INIT_REFCNT();
  mInner.Init((nsIContent *)(nsIXMLContent *)this, aTag);
  mIsLink = PR_FALSE;
  mContentID = 0;

  if (0 == kElementCount++) {
    kSimpleAtom = NS_NewAtom("simple");
    kHrefAtom = NS_NewAtom("href");
    kShowAtom = NS_NewAtom("show");
    kTypeAtom = NS_NewAtom("type");
    kBaseAtom = NS_NewAtom("base");
  }
}
 
nsXMLElement::~nsXMLElement()
{
  if (0 == --kElementCount) {
    NS_RELEASE(kSimpleAtom);
    NS_RELEASE(kHrefAtom);
    NS_RELEASE(kShowAtom);
    NS_RELEASE(kTypeAtom);
    NS_RELEASE(kBaseAtom);
  }
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
  else if (aIID.Equals(NS_GET_IID(nsIBindableContent))) {
    nsIBindableContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsXMLElement)
NS_IMPL_RELEASE(nsXMLElement)

static inline nsresult MakeURI(const char *aSpec, nsIURI *aBase, nsIURI **aURI)
{
  nsresult rv;
  static NS_DEFINE_CID(ioServCID,NS_IOSERVICE_CID);
  NS_WITH_SERVICE(nsIIOService,service,ioServCID,&rv);
  if (NS_FAILED(rv))
    return rv;

  return service->NewURI(aSpec,aBase,aURI);
}

nsresult 
nsXMLElement::GetXMLBaseURI(nsIURI **aURI)
{
  NS_ABORT_IF_FALSE(aURI,"null ptr");
  if (!aURI)
    return NS_ERROR_NULL_POINTER;

  *aURI = nsnull;
  
  nsresult rv;

  nsAutoString base;
  nsCOMPtr<nsIContent> content = do_QueryInterface(NS_STATIC_CAST(nsIXMLContent*,this),&rv);
  while (NS_SUCCEEDED(rv) && content) {
    nsAutoString value;
    rv = content->GetAttribute(kNameSpaceID_XML,kBaseAtom,value);
    PRInt32 value_len;
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      // XXX Need to convert unicode to ???
      // XXX Need to URL-escape string
      PRInt32 colon = value.FindChar(':',PR_FALSE);
      PRInt32 slash = value.FindChar('/',PR_FALSE);
      if (colon > 0 && !( slash >= 0 && slash < colon)) {
        // Yay, we have absolute path!
        // The complex looking if above is to make sure that we do not erroneously
        // think a value of "./this:that" would have a scheme of "./that"

        nsCAutoString str(value);
      
        rv = MakeURI(str,nsnull,aURI);
        if (NS_FAILED(rv))
          break;

        if (!base.IsEmpty()) {
          str.AssignWithConversion(base.GetUnicode());
          rv = (*aURI)->SetRelativePath(str);
        }
        break;

      } else if ((value_len = value.Length()) > 0) {        
        if (base.Length() > 0) {
          if (base[0] == '/') {
            // Do nothing, we are waiting for a scheme starting value
          } else {
            // We do not want to add double / delimiters (although the user is free to do so)
            if (value[value_len - 1] != '/')
              value.AppendWithConversion('/');
            base.Insert(value, 0);
          }
        } else {
          if (value[value_len - 1] != '/')
            value.AppendWithConversion('/'); // Add delimiter/make sure we treat this as dir
          base = value;
        }
      }
    } // if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
    nsCOMPtr<nsIContent> parent;
    rv = content->GetParent(*getter_AddRefs(parent));
    content = parent;
  } // while

  if (NS_SUCCEEDED(rv)) {
    if (!*aURI && mInner.mDocument) {
      nsCOMPtr<nsIURI> docBase = dont_AddRef(mInner.mDocument->GetDocumentURL());
      if (base.IsEmpty()) {
        *aURI = docBase.get();    
        NS_IF_ADDREF(*aURI);  // nsCOMPtr releases this once
      } else {
        // XXX Need to convert unicode to ???
        // XXX Need to URL-escape string
        nsCAutoString str(base);
        rv = MakeURI(str,docBase,aURI);
      }
    }
  } else {
    NS_IF_RELEASE(*aURI);
  }

  return rv;
}

NS_IMETHODIMP 
nsXMLElement::SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
			                     const nsString& aValue,
			                     PRBool aNotify)
{
  // XXX It sucks that we have to do a strcmp for
  // every attribute set. It might be a bit more expensive
  // to create an atom.
  if ((kNameSpaceID_XLink == aNameSpaceID) &&
      (kTypeAtom == aName)) { 
    if (aValue.EqualsWithConversion(kSimpleAtom, PR_FALSE)) {
      // NOTE: This really is a link according to the XLink spec,
      //       we do not need to check other attributes. If there
      //       is no href attribute, then this link is simply
      //       untraversible [XLink 3.2].
      // XXX If a parent of this element is already a simple link, then this
      //     must not create a link of its own, this is just a normal element
      //     inside the parent simple XLink element [XLink 3.2].
      mIsLink = PR_TRUE;
    } else {
      mIsLink = PR_FALSE;
    }
  }

  // XXX If the XLink actuate attribute is present and its value is
  //     onLoad, we should obey that too [XLink 3.6.2].

  return mInner.SetAttribute(aNameSpaceID, aName, aValue, aNotify);
}

NS_IMETHODIMP 
nsXMLElement::HandleDOMEvent(nsIPresContext* aPresContext,
			     nsEvent* aEvent,
			     nsIDOMEvent** aDOMEvent,
			     PRUint32 aFlags,
			     nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  NS_ENSURE_ARG(aPresContext);
  // Try script event handlers first
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                       aFlags, aEventStatus);

  if (mIsLink && (NS_OK == ret) && (nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE)) {
    switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(this, NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS);
          NS_RELEASE(stateManager);
        }
        *aEventStatus = nsEventStatus_eConsumeDoDefault;
      }
      break;

    case NS_MOUSE_LEFT_CLICK:
      {
        if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {
          nsAutoString show, href, target;
          nsIURI* baseURL = nsnull;
	        nsLinkVerb verb = eLinkVerb_Undefined;
          GetAttribute(kNameSpaceID_XLink, kHrefAtom, href);
          if (href.IsEmpty()) {
            *aEventStatus = nsEventStatus_eConsumeDoDefault; 
            break;
          }
          GetAttribute(kNameSpaceID_XLink, kShowAtom, show);
	        // XXX Should probably do this using atoms 
	        if (show.EqualsWithConversion("new")) {
	          verb = eLinkVerb_New;
	        }
          else if (show.EqualsWithConversion("replace")) {
            verb = eLinkVerb_Replace;
          }
	        else if (show.EqualsWithConversion("embed")) {
	          verb = eLinkVerb_Embed;
	        }
          
          GetXMLBaseURI(&baseURL);

          ret = mInner.TriggerLink(aPresContext, verb, baseURL, href, target, PR_TRUE);
          NS_IF_RELEASE(baseURL);
          *aEventStatus = nsEventStatus_eConsumeDoDefault; 
        }
      }
      break;

    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      // XXX Bring up a contextual menu provided by the application
      break;

    case NS_MOUSE_ENTER:
      {
        nsAutoString href, target;
        nsIURI* baseURL = nsnull;
        GetAttribute(kNameSpaceID_XLink, kHrefAtom, href);
        if (href.IsEmpty()) {
          *aEventStatus = nsEventStatus_eConsumeDoDefault; 
          break;
        }

        GetXMLBaseURI(&baseURL);

        ret = mInner.TriggerLink(aPresContext, eLinkVerb_Replace, baseURL, href, target, PR_FALSE);
        
        NS_IF_RELEASE(baseURL);
        *aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT:
      {
        nsAutoString empty;
        ret = mInner.TriggerLink(aPresContext, eLinkVerb_Replace, nsnull, empty, empty, PR_FALSE);
        *aEventStatus = nsEventStatus_eConsumeDoDefault; 
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
  mInner.CopyInnerTo((nsIContent *)(nsIXMLContent *)this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsXMLElement::SetBinding(nsIXBLBinding* aBinding) 
{
  mBinding = aBinding; // nsCOMPtr handles addrefing.
  return NS_OK;
}

NS_IMETHODIMP
nsXMLElement::GetBinding(nsIXBLBinding** aResult)
{
  *aResult = mBinding.get();
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLElement::GetBaseTag(nsIAtom** aResult)
{
  if (mBinding)
    return mBinding->GetBaseTag(aResult);
  else
    return NS_OK;
}

