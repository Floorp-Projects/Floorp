/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsReadableUtils.h"
#include "nsIDocument.h"

class nsHTMLAreaElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLAreaElement,
                          public nsIDOMNSHTMLAreaElement,
                          public nsILink
{
public:
  nsHTMLAreaElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLAreaElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLAreaElement
  NS_DECL_NSIDOMHTMLAREAELEMENT

  // nsIDOMNSHTMLAreaElement
  NS_DECL_NSIDOMNSHTMLAREAELEMENT

  // nsILink
  NS_IMETHOD GetLinkState(nsLinkState &aState);
  NS_IMETHOD SetLinkState(nsLinkState aState);
  NS_IMETHOD GetHrefURI(nsIURI** aURI);

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);
  virtual void SetFocus(nsIPresContext* aPresContext);
  virtual void RemoveFocus(nsIPresContext* aPresContext);

  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

protected:
  // The cached visited state
  nsLinkState mLinkState;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Area)


nsHTMLAreaElement::nsHTMLAreaElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mLinkState(eLinkState_Unknown)
{
}

nsHTMLAreaElement::~nsHTMLAreaElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLAreaElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLAreaElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLAreaElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLAreaElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsILink)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLAreaElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Area)


NS_IMPL_STRING_ATTR(nsHTMLAreaElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLAreaElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLAreaElement, Coords, coords)
NS_IMPL_URI_ATTR(nsHTMLAreaElement, Href, href)
NS_IMPL_BOOL_ATTR(nsHTMLAreaElement, NoHref, nohref)
NS_IMPL_STRING_ATTR(nsHTMLAreaElement, Shape, shape)
NS_IMPL_INT_ATTR(nsHTMLAreaElement, TabIndex, tabindex)

NS_IMETHODIMP
nsHTMLAreaElement::GetTarget(nsAString& aValue)
{
  aValue.Truncate();

  nsresult rv;
  rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, aValue);
  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    GetBaseTarget(aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaElement::SetTarget(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::target, aValue, PR_TRUE);
}

PRBool
nsHTMLAreaElement::ParseAttribute(nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::tabindex) {
    return aResult.ParseIntWithBounds(aValue, 0, 32767);
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

nsresult
nsHTMLAreaElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent,
                                  nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus)
{
  return HandleDOMEventForAnchors(aPresContext, aEvent, aDOMEvent, 
                                  aFlags, aEventStatus);
}

void
nsHTMLAreaElement::SetFocus(nsIPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  aPresContext->EventStateManager()->SetContentState(this,
                                                     NS_EVENT_STATE_FOCUS);
    
  // Make sure the presentation is up-to-date    
  if (mDocument) {
    mDocument->FlushPendingNotifications(Flush_Layout);
  }

  nsIPresShell *presShell = aPresContext->GetPresShell();

  if (presShell) {
    nsIFrame* frame = nsnull;
    presShell->GetPrimaryFrameFor(this, &frame);
    if (frame) {
      presShell->ScrollFrameIntoView(frame, NS_PRESSHELL_SCROLL_ANYWHERE,
                                     NS_PRESSHELL_SCROLL_ANYWHERE);
    }
  }
}

void
nsHTMLAreaElement::RemoveFocus(nsIPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  aPresContext->EventStateManager()->SetContentState(nsnull,
                                                     NS_EVENT_STATE_FOCUS);
}

void
nsHTMLAreaElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                               PRBool aCompileEventHandlers)
{
  PRBool documentChanging = (aDocument != mDocument);
  
  // Unregister the access key for the old document.
  if (documentChanging && mDocument) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsGenericHTMLElement::SetDocument(aDocument, aDeep, aCompileEventHandlers);

  // Register the access key for the new document.
  if (documentChanging && mDocument) {
    RegUnRegAccessKey(PR_TRUE);
  }
}

nsresult
nsHTMLAreaElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
{
  if (aName == nsHTMLAtoms::accesskey && aNameSpaceID == kNameSpaceID_None) {
    RegUnRegAccessKey(PR_FALSE);
  }

  if (aName == nsHTMLAtoms::href && aNameSpaceID == kNameSpaceID_None) {
    SetLinkState(eLinkState_Unknown);
  }

  nsresult rv =
    nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue, aNotify);

  if (aName == nsHTMLAtoms::accesskey && aNameSpaceID == kNameSpaceID_None &&
      !aValue.IsEmpty()) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

nsresult
nsHTMLAreaElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::accesskey &&
      aNameSpaceID == kNameSpaceID_None) {
    RegUnRegAccessKey(PR_FALSE);
  }

  return nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetProtocol(nsAString& aProtocol)
{
  nsAutoString href;
  
  nsresult rv = GetHref(href);
  if (NS_FAILED(rv))
    return rv;

  // XXX this should really use GetHrefURI and not do so much string stuff
  return GetProtocolFromHrefString(href, aProtocol,
                                   nsGenericHTMLElement::GetOwnerDocument());
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
nsHTMLAreaElement::GetHrefURI(nsIURI** aURI)
{
  return GetHrefURIForAnchors(aURI);
}
