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
#include "nsIWebShell.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIDocShell.h"
#include "nsIScriptSecurityManager.h"
#include "nsIRefreshURI.h"
#include "nsStyleConsts.h"


nsresult
NS_NewXMLElement(nsIXMLContent** aInstancePtrResult, nsINodeInfo *aNodeInfo)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIXMLContent* it = new nsXMLElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIXMLContent), (void**) aInstancePtrResult);
}

static nsIAtom* kSimpleAtom;  // XXX these should get moved to nsXMLAtoms
static nsIAtom* kHrefAtom;
static nsIAtom* kShowAtom;
static nsIAtom* kTypeAtom;
static nsIAtom* kBaseAtom;
static nsIAtom* kActuateAtom;
static nsIAtom* kOnLoadAtom;
static nsIAtom* kEmbedAtom;
static PRUint32 kElementCount;

nsXMLElement::nsXMLElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();

  mInner.Init(this, aNodeInfo);
  mIsLink = PR_FALSE;
  mContentID = 0;

  if (0 == kElementCount++) {
    kSimpleAtom = NS_NewAtom("simple");
    kHrefAtom = NS_NewAtom("href");
    kShowAtom = NS_NewAtom("show");
    kTypeAtom = NS_NewAtom("type");
    kBaseAtom = NS_NewAtom("base");
    kActuateAtom = NS_NewAtom("actuate");
    kOnLoadAtom = NS_NewAtom("onLoad");
    kEmbedAtom = NS_NewAtom("embed");
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
    NS_RELEASE(kActuateAtom);
    NS_RELEASE(kOnLoadAtom);
    NS_RELEASE(kEmbedAtom);
  }
}

NS_IMETHODIMP 
nsXMLElement::QueryInterface(REFNSIID aIID,
                             void** aInstancePtr)
{
  NS_IMPL_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this, nsIXMLContent)
  if (aIID.Equals(NS_GET_IID(nsIXMLContent))) {
    nsIXMLContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIStyledContent::GetIID())) {
    nsIStyledContent* tmp = this;
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
      PRInt32 colon = value.FindChar(':',PR_FALSE);
      PRInt32 slash = value.FindChar('/',PR_FALSE);
      if (colon > 0 && !( slash >= 0 && slash < colon)) {
        // Yay, we have absolute path!
        // The complex looking if above is to make sure that we do not erroneously
        // think a value of "./this:that" would have a scheme of "./that"

        // XXX URL escape?
        nsCAutoString str; str.AssignWithConversion(value);
      
        rv = MakeURI(str,nsnull,aURI);
        if (NS_FAILED(rv))
          break;

        if (!base.IsEmpty()) {
          // XXX URL escape?
          str.AssignWithConversion(base.GetUnicode());
          nsXPIDLCString resolvedStr;
          rv = (*aURI)->Resolve(str, getter_Copies(resolvedStr));
          if (NS_FAILED(rv)) break;
          rv = (*aURI)->SetSpec(resolvedStr);
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
        // XXX URL escape?
        nsCAutoString str; str.AssignWithConversion(base);
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
                           const nsAReadableString& aValue,
                           PRBool aNotify)
{
  nsresult rv;
  nsCOMPtr<nsINodeInfoManager> nimgr;
  rv = mInner.mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINodeInfo> ni;
  rv = nimgr->GetNodeInfo(aName, nsnull, aNameSpaceID,
                          *getter_AddRefs(ni));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetAttribute(ni, aValue, aNotify);
}

NS_IMETHODIMP 
nsXMLElement::SetAttribute(nsINodeInfo *aNodeInfo, const nsAReadableString& aValue,
                           PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  if (aNodeInfo->Equals(kTypeAtom, kNameSpaceID_XLink)) { 
    const PRUnichar* simpleStr;
    kSimpleAtom->GetUnicode(&simpleStr);
    if (aValue.Equals(simpleStr)) {
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

    // We will check for actuate="onLoad" in MaybeTriggerAutoLink
  }

  return mInner.SetAttribute(aNodeInfo, aValue, aNotify);
}

static nsresult WebShellToPresContext(nsIWebShell *aShell, nsIPresContext **aPresContext)
{
  *aPresContext = nsnull;

  nsresult rv;
  nsCOMPtr<nsIDocShell> ds = do_QueryInterface(aShell,&rv);
  if (NS_FAILED(rv))
    return rv;

  return ds->GetPresContext(aPresContext);
}


static nsresult CheckLoadURI(nsIURI *aBaseURI, const nsAReadableString& aURI, nsIURI **aAbsURI)
{
  // XXX URL escape?
  nsCAutoString str; str.Assign(NS_ConvertUCS2toUTF8(aURI));

  *aAbsURI = nsnull;

  nsresult rv;
  rv = MakeURI(str,aBaseURI,aAbsURI);
  if (NS_SUCCEEDED(rv)) {
    NS_WITH_SERVICE(nsIScriptSecurityManager,
                    securityManager, 
                    NS_SCRIPTSECURITYMANAGER_PROGID,
                    &rv);
    if (NS_SUCCEEDED(rv)) {
      rv= securityManager->CheckLoadURI(aBaseURI,
                                         *aAbsURI,
                                         PR_TRUE);
    }
  }

  if (NS_FAILED(rv)) {
    NS_IF_RELEASE(*aAbsURI);
  }

  return rv;
}

static inline nsresult SpecialAutoLoadReturn(nsresult aRv, nsLinkVerb aVerb)
{
  if (NS_SUCCEEDED(aRv)) {
    switch(aVerb) {
      case eLinkVerb_Embed:
        aRv = NS_XML_AUTOLINK_EMBED;
        break;
      case eLinkVerb_New:
        aRv = NS_XML_AUTOLINK_NEW;
        break;
      case eLinkVerb_Replace:
        aRv = NS_XML_AUTOLINK_REPLACE;
        break;
      default:
        aRv = NS_XML_AUTOLINK_UNDEFINED;
        break;
    }
  }
  return aRv;
}

NS_IMETHODIMP
nsXMLElement::MaybeTriggerAutoLink(nsIWebShell *aShell)
{
  NS_ABORT_IF_FALSE(aShell,"null ptr");
  if (!aShell)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;

  if (mIsLink) {
    do {
      // actuate="onLoad" ?
      nsAutoString value;
      rv = GetAttribute(kNameSpaceID_XLink,kActuateAtom,value);
      if (rv == NS_CONTENT_ATTR_HAS_VALUE &&
          value.EqualsAtom(kOnLoadAtom,PR_FALSE)) {

        // show= ?
        nsLinkVerb verb = eLinkVerb_Undefined;
        rv = GetAttribute(kNameSpaceID_XLink, kShowAtom, value);
        if (NS_FAILED(rv))
          break;
        // XXX Should probably do this using atoms 
        if (value.EqualsWithConversion("new")) {
          verb = eLinkVerb_New;
        } else if (value.EqualsWithConversion("replace")) {
          // We want to actually stop processing the current document now.
          // We do this by returning the correct value so that the one
          // that called us knows to stop processing.
          verb = eLinkVerb_Replace;
        } else if (value.EqualsWithConversion("embed")) {
          // XXX TODO
          break;
        }

        // base
        nsCOMPtr<nsIURI> base;
        rv = GetXMLBaseURI(getter_AddRefs(base));
        if (NS_FAILED(rv))
          break;

        // href= ?
        rv = GetAttribute(kNameSpaceID_XLink,kHrefAtom,value);
        if (rv == NS_CONTENT_ATTR_HAS_VALUE && !value.IsEmpty()) {
          nsCOMPtr<nsIURI> uri;
          rv = CheckLoadURI(base,value,getter_AddRefs(uri));
          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIPresContext> pc;
            rv = WebShellToPresContext(aShell,getter_AddRefs(pc));
            if (NS_SUCCEEDED(rv)) {
              rv = mInner.TriggerLink(pc, verb, base, value, nsAutoString(), PR_TRUE);

              return SpecialAutoLoadReturn(rv,verb);
            }
          }
        } // href
      }
    } while (0);
  }

  return rv;
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

    case NS_MOUSE_ENTER_SYNTH:
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
    case NS_MOUSE_EXIT_SYNTH:
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
  nsXMLElement* it = new nsXMLElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo((nsIContent *)(nsIXMLContent *)this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

// nsIStyledContent implementation

NS_IMETHODIMP
nsXMLElement::GetID(nsIAtom*& aResult) const
{
  nsresult rv;  
  nsCOMPtr<nsIAtom> atom;
  rv = mInner.mNodeInfo->GetIDAttributeAtom(getter_AddRefs(atom));
  
  aResult = nsnull;
  if (NS_SUCCEEDED(rv) && atom) {
    nsAutoString value;
    rv = GetAttribute(kNameSpaceID_Unknown, atom, value);
    if (NS_SUCCEEDED(rv))
      aResult = NS_NewAtom(value);
  }

  return rv;
}

NS_IMETHODIMP
nsXMLElement::GetClasses(nsVoidArray& aArray) const
{
  aArray.Clear();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLElement::HasClass(nsIAtom* aClass) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLElement::GetContentStyleRules(nsISupportsArray* aRules)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLElement::GetInlineStyleRules(nsISupportsArray* aRules)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                       PRInt32& aHint) const
{
  aHint = NS_STYLE_HINT_CONTENT;  // by default, never map attributes to style
  return NS_OK;
}
