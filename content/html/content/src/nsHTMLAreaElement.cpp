/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMNSHTMLAreaElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsILink.h"
#include "nsIPresShell.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsHTMLUtils.h"
#include "nsReadableUtils.h"
#include "nsIDocument.h"

class nsHTMLAreaElement : public nsGenericHTMLLeafElement,
                          public nsIDOMHTMLAreaElement,
                          public nsIDOMNSHTMLAreaElement,
                          public nsILink
{
public:
  nsHTMLAreaElement();
  virtual ~nsHTMLAreaElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLAreaElement
  NS_DECL_NSIDOMHTMLAREAELEMENT

  // nsIDOMNSHTMLAreaElement
  NS_DECL_NSIDOMNSHTMLAREAELEMENT

  // nsILink
  NS_IMETHOD GetLinkState(nsLinkState &aState);
  NS_IMETHOD SetLinkState(nsLinkState aState);
  NS_IMETHOD GetHrefCString(char* &aBuf);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAString& aValue,
                     PRBool aNotify);
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue,
                     PRBool aNotify);
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                       PRBool aNotify);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  // The cached visited state
  nsLinkState mLinkState;
};

nsresult
NS_NewHTMLAreaElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLAreaElement* it = new nsHTMLAreaElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = it;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLAreaElement::nsHTMLAreaElement()
  : mLinkState(eLinkState_Unknown)
{
  nsHTMLUtils::AddRef(); // for GetHrefCString
}

nsHTMLAreaElement::~nsHTMLAreaElement()
{
  nsHTMLUtils::Release(); // for GetHrefCString
}

NS_IMPL_ADDREF_INHERITED(nsHTMLAreaElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLAreaElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLAreaElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLAreaElement,
                                    nsGenericHTMLLeafElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsILink)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLAreaElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLAreaElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLAreaElement* it = new nsHTMLAreaElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = it;

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLAreaElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLAreaElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLAreaElement, Coords, coords)
NS_IMPL_BOOL_ATTR(nsHTMLAreaElement, NoHref, nohref)
NS_IMPL_STRING_ATTR(nsHTMLAreaElement, Shape, shape)
NS_IMPL_INT_ATTR(nsHTMLAreaElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLAreaElement, Target, target)


NS_IMETHODIMP
nsHTMLAreaElement::StringToAttribute(nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::nohref) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (aResult.ParseIntWithBounds(aValue, eHTMLUnit_Integer, 0)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLAreaElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent,
                                  nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus)
{
  return HandleDOMEventForAnchors(this, aPresContext, aEvent, aDOMEvent, 
                                  aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  nsCOMPtr<nsIEventStateManager> esm;
  aPresContext->GetEventStateManager(getter_AddRefs(esm));
  if (esm) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
    
    // Make sure the presentation is up-to-date    
    if (mDocument) {
      mDocument->FlushPendingNotifications();
    }

    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));

    if (presShell) {
      nsIFrame* frame = nsnull;
      presShell->GetPrimaryFrameFor(this, &frame);
      if (frame) {
        presShell->ScrollFrameIntoView(frame, NS_PRESSHELL_SCROLL_ANYWHERE,
                                       NS_PRESSHELL_SCROLL_ANYWHERE);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaElement::RemoveFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  nsIEventStateManager* esm;
  if (NS_OK == aPresContext->GetEventStateManager(&esm)) {
    esm->SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
    NS_RELEASE(esm);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaElement::GetHref(nsAString& aValue)
{
  char *buf;
  nsresult rv = GetHrefCString(buf);
  if (NS_FAILED(rv)) return rv;
  if (buf) {
    aValue.Assign(NS_ConvertASCIItoUCS2(buf));
    nsCRT::free(buf);
  }
  // NS_IMPL_STRING_ATTR does nothing where we have (buf == null)
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaElement::SetHref(const nsAString& aValue)
{
  // Clobber our "cache", so we'll recompute it the next time somebody
  // asks for it.
  mLinkState = eLinkState_Unknown;

  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::href, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                               PRBool aCompileEventHandlers)
{
  // Unregister the access key for the old document.
  if (mDocument) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult rv = nsGenericHTMLElement::SetDocument(aDocument, aDeep,
                                                  aCompileEventHandlers);

  // Register the access key for the new document.
  if (mDocument) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLAreaElement::SetAttr(nsINodeInfo* aNodeInfo,
                           const nsAString& aValue,
                           PRBool aNotify)
{
  return nsGenericHTMLElement::SetAttr(aNodeInfo, aValue, aNotify);
}


NS_IMETHODIMP
nsHTMLAreaElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           const nsAString& aValue,
                           PRBool aNotify)
{
  if (aName == nsHTMLAtoms::accesskey && aNameSpaceID == kNameSpaceID_None) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult rv =
    nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aValue, aNotify);

  if (aName == nsHTMLAtoms::accesskey && aNameSpaceID == kNameSpaceID_None &&
      !aValue.IsEmpty()) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLAreaElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::accesskey &&
      aNameSpaceID == kNameSpaceID_None) {
    RegUnRegAccessKey(PR_FALSE);
  }

  return nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLAreaElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif

NS_IMETHODIMP    
nsHTMLAreaElement::GetProtocol(nsAString& aProtocol)
{
  nsAutoString href;
  
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocument> doc;
  mNodeInfo->GetDocument(*getter_AddRefs(doc));

  return GetProtocolFromHrefString(href, aProtocol, doc);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetProtocol(const nsAString& aProtocol)
{
  nsAutoString href, new_href;
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;
  
  rv = SetProtocolInHrefString(href, aProtocol, new_href);
  if (NS_FAILED(rv))
    // Ignore failures to be compatible with NS4
    return NS_OK;

  return SetHref(new_href);
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetHost(nsAString& aHost)
{
  nsAutoString href;
  
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  return GetHostFromHrefString(href, aHost);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetHost(const nsAString& aHost)
{
  nsAutoString href, new_href;
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  rv = SetHostInHrefString(href, aHost, new_href);
  if (NS_FAILED(rv))
    // Ignore failures to be compatible with NS4
    return NS_OK;

  return SetHref(new_href);
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetHostname(nsAString& aHostname)
{
  nsAutoString href;
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  return GetHostnameFromHrefString(href, aHostname);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetHostname(const nsAString& aHostname)
{
  nsAutoString href, new_href;
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  rv = SetHostnameInHrefString(href, aHostname, new_href);
  if (NS_FAILED(rv))
    // Ignore failures to be compatible with NS4
    return NS_OK;
  
  return SetHref(new_href);
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetPathname(nsAString& aPathname)
{
  nsAutoString href;
 
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  return GetPathnameFromHrefString(href, aPathname);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetPathname(const nsAString& aPathname)
{
  nsAutoString href, new_href;
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  rv = SetPathnameInHrefString(href, aPathname, new_href);
  if (NS_FAILED(rv))
    // Ignore failures to be compatible with NS4
    return NS_OK;

  return SetHref(new_href);
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetSearch(nsAString& aSearch)
{
  nsAutoString href;

  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  return GetSearchFromHrefString(href, aSearch);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetSearch(const nsAString& aSearch)
{
  nsAutoString href, new_href;
  nsresult rv = GetHref(href);

  if (NS_FAILED(rv))
    return rv;

  rv = SetSearchInHrefString(href, aSearch, new_href);
  if (NS_FAILED(rv))
    // Ignore failures to be compatible with NS4
    return NS_OK;

  return SetHref(new_href);
}

NS_IMETHODIMP
nsHTMLAreaElement::GetPort(nsAString& aPort)
{
  nsAutoString href;
  
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  return GetPortFromHrefString(href, aPort);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetPort(const nsAString& aPort)
{
  nsAutoString href, new_href;
  nsresult rv = GetHref(href);

  if (NS_FAILED(rv))
    return rv;

  rv = SetPortInHrefString(href, aPort, new_href);
  if (NS_FAILED(rv))
    // Ignore failures to be compatible with NS4
    return NS_OK;
  
  return SetHref(new_href);
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetHash(nsAString& aHash)
{
  nsAutoString href;

  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  return GetHashFromHrefString(href, aHash);
}

NS_IMETHODIMP
nsHTMLAreaElement::SetHash(const nsAString& aHash)
{
  nsAutoString href, new_href;
  nsresult rv = GetHref(href);

  if (NS_FAILED(rv))
    return rv;

  rv = SetHashInHrefString(href, aHash, new_href);
  if (NS_FAILED(rv))
    // Ignore failures to be compatible with NS4
    return NS_OK;

  return SetHref(new_href);
}

NS_IMETHODIMP
nsHTMLAreaElement::GetLinkState(nsLinkState &aState)
{
  aState = mLinkState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaElement::SetLinkState(nsLinkState aState)
{
  mLinkState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaElement::GetHrefCString(char* &aBuf)
{
  // Get href= attribute (relative URL).
  nsAutoString relURLSpec;

  if (GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, relURLSpec) ==
      NS_CONTENT_ATTR_HAS_VALUE) {
    // Clean up any leading or trailing whitespace
    relURLSpec.Trim(" \t\n\r");

    // Get base URL.
    nsCOMPtr<nsIURI> baseURL;
    GetBaseURL(*getter_AddRefs(baseURL));

    if (baseURL) {
      // Get absolute URL.
      nsCAutoString buf;
      NS_MakeAbsoluteURIWithCharset(buf, relURLSpec, mDocument, baseURL,
                                    nsHTMLUtils::IOService, nsHTMLUtils::CharsetMgr);
      aBuf = ToNewCString(buf);
    }
    else {
      // Absolute URL is same as relative URL.
      aBuf = ToNewUTF8String(relURLSpec);
    }
  }
  else {
    // Absolute URL is empty because we have no HREF.
    aBuf = nsnull;
  }

  return NS_OK;
}
