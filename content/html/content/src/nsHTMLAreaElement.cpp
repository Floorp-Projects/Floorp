/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsHTMLUtils.h"


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
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

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

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
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

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

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
                                     const nsAReadableString& aValue,
                                     nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::nohref) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
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
  nsIEventStateManager* esm;
  if (NS_OK == aPresContext->GetEventStateManager(&esm)) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
    NS_RELEASE(esm);
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
nsHTMLAreaElement::GetHref(nsAWritableString& aValue)
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
nsHTMLAreaElement::SetHref(const nsAReadableString& aValue)
{
  // Clobber our "cache", so we'll recompute it the next time somebody
  // asks for it.
  mLinkState = eLinkState_Unknown;

  return NS_STATIC_CAST(nsIContent *, this)->SetAttr(kNameSpaceID_HTML,
                                                     nsHTMLAtoms::href,
                                                     aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLAreaElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetProtocol(nsAWritableString& aProtocol)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char* protocol;
      result = url->GetScheme(&protocol);
      if (result == NS_OK) {
        aProtocol.Assign(NS_ConvertASCIItoUCS2(protocol));
        aProtocol.Append(PRUnichar(':'));
        nsCRT::free(protocol);
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetHost(nsAWritableString& aHost)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char* host;
      result = url->GetHost(&host);
      if (result == NS_OK) {
        aHost.Assign(NS_ConvertASCIItoUCS2(host));
        nsCRT::free(host);
        PRInt32 port;
        (void)url->GetPort(&port);
        if (-1 != port) {
          aHost.Append(PRUnichar(':'));
          nsAutoString portStr;
          portStr.AppendInt(port, 10);
          aHost.Append(portStr);
        }
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetHostname(nsAWritableString& aHostname)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char* host;
      result = url->GetHost(&host);
      if (result == NS_OK) {
        aHostname.Assign(NS_ConvertASCIItoUCS2(host));
        nsCRT::free(host);
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetPathname(nsAWritableString& aPathname)
{
  nsAutoString href;
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  aPathname.Truncate();
 
  result = GetHref(href);
  NS_ENSURE_SUCCESS(result, result);

  result = NS_NewURI(getter_AddRefs(uri), href);
  NS_ENSURE_SUCCESS(result, result);

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  NS_ENSURE_TRUE(url, NS_OK);

  char* file;
  result = url->GetFilePath(&file);
  NS_ENSURE_SUCCESS(result, result);

  aPathname.Assign(NS_ConvertASCIItoUCS2(file));
  nsCRT::free(file);

  return result;
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetSearch(nsAWritableString& aSearch)
{
  nsAutoString href;
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_SUCCEEDED(result)) {
    result = NS_NewURI(getter_AddRefs(uri), href);
    if (NS_SUCCEEDED(result)) {
      char *search;
      nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

      if (url) {
        result = url->GetEscapedQuery(&search);
      }

      if (NS_SUCCEEDED(result) && search && *search) {
        aSearch.Assign(PRUnichar('?'));
        aSearch.Append(NS_ConvertASCIItoUCS2(search));
        nsCRT::free(search);
      }
      else {
        aSearch.SetLength(0);
      }
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetPort(nsAWritableString& aPort)
{
  nsAutoString href;
  nsCOMPtr<nsIURI> url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_SUCCEEDED(result)) {
    result = NS_NewURI(getter_AddRefs(url), href);
    if (NS_SUCCEEDED(result)) {
      aPort.SetLength(0);
      PRInt32 port;
      url->GetPort(&port);
      if (-1 != port) {
        nsAutoString portStr;
        portStr.AppendInt(port, 10);
        aPort.Append(portStr);
      }
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAreaElement::GetHash(nsAWritableString& aHash)
{
  nsAutoString href;
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURI(getter_AddRefs(uri), href);

    if (NS_SUCCEEDED(result)) {
      char *ref;
      nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
      if (url) {
        result = url->GetRef(&ref);
      }

      if (NS_SUCCEEDED(result) && ref && *ref) {
        aHash.Assign(PRUnichar('#'));
        aHash.Append(NS_ConvertASCIItoUCS2(ref));
        nsCRT::free(ref);
      }
      else {
        aHash.SetLength(0);
      }
    }
  }

  return result;
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

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      NS_STATIC_CAST(nsIContent *, this)->GetAttr(kNameSpaceID_HTML,
                                                  nsHTMLAtoms::href,
                                                  relURLSpec)) {
    // Clean up any leading or trailing whitespace
    relURLSpec.Trim(" \t\n\r");

    // Get base URL.
    nsCOMPtr<nsIURI> baseURL;
    GetBaseURL(*getter_AddRefs(baseURL));

    if (baseURL) {
      // Get absolute URL.
      NS_MakeAbsoluteURIWithCharset(&aBuf, relURLSpec, mDocument, baseURL,
                                    nsHTMLUtils::IOService, nsHTMLUtils::CharsetMgr);
    }
    else {
      // Absolute URL is same as relative URL.
      aBuf = relURLSpec.ToNewUTF8String();
    }
  }
  else {
    // Absolute URL is empty because we have no HREF.
    aBuf = nsnull;
  }

  return NS_OK;
}
