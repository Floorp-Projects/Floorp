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
 */
#include "nsCOMPtr.h"
#include "nsHTMLUtils.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMNSHTMLAnchorElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsILink.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIURL.h"

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsNetUtil.h"

#include "nsCOMPtr.h"
#include "nsIFrameManager.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIHTMLAttributes.h"

// XXX suppress

// XXX either suppress is handled in the event code below OR we need a
// custom frame

static NS_DEFINE_IID(kIDOMHTMLAnchorElementIID, NS_IDOMHTMLANCHORELEMENT_IID);

class nsHTMLAnchorElement : public nsIDOMHTMLAnchorElement,
                            public nsIDOMNSHTMLAnchorElement,
                            public nsIJSScriptObject,
                            public nsILink,
                            public nsIHTMLContent
{
public:
  nsHTMLAnchorElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLAnchorElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLAnchorElement
  NS_IMETHOD GetAccessKey(nsString& aAccessKey);
  NS_IMETHOD SetAccessKey(const nsString& aAccessKey);
  NS_IMETHOD GetCharset(nsString& aCharset);
  NS_IMETHOD SetCharset(const nsString& aCharset);
  NS_IMETHOD GetCoords(nsString& aCoords);
  NS_IMETHOD SetCoords(const nsString& aCoords);
  NS_IMETHOD GetHref(nsString& aHref);
  NS_IMETHOD SetHref(const nsString& aHref);
  NS_IMETHOD GetHreflang(nsString& aHreflang);
  NS_IMETHOD SetHreflang(const nsString& aHreflang);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetRel(nsString& aRel);
  NS_IMETHOD SetRel(const nsString& aRel);
  NS_IMETHOD GetRev(nsString& aRev);
  NS_IMETHOD SetRev(const nsString& aRev);
  NS_IMETHOD GetShape(nsString& aShape);
  NS_IMETHOD SetShape(const nsString& aShape);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD GetTarget(nsString& aTarget);
  NS_IMETHOD SetTarget(const nsString& aTarget);
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD SetType(const nsString& aType);
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();

  // nsIDOMNSHTMLAnchorElement
  NS_IMETHOD    GetProtocol(nsString& aProtocol);
  NS_IMETHOD    GetHost(nsString& aHost);
  NS_IMETHOD    GetHostname(nsString& aHostname);
  NS_IMETHOD    GetPathname(nsString& aPathname);
  NS_IMETHOD    GetSearch(nsString& aSearch);
  NS_IMETHOD    GetPort(nsString& aPort);
  NS_IMETHOD    GetHash(nsString& aHash);
  NS_IMETHOD    GetText(nsString& aText);

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

  // nsILink
  NS_IMETHOD    GetLinkState(nsLinkState &aState);
  NS_IMETHOD    SetLinkState(nsLinkState aState);
  NS_IMETHOD    GetHrefCString(char* &aBuf);

  // nsIContent
  //NS_IMPL_ICONTENT_NO_FOCUS_USING_GENERIC(mInner)
  NS_IMPL_ICONTENT_NO_SETPARENT_NO_SETDOCUMENT_NO_FOCUS_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsresult RegUnRegAccessKey(PRBool aDoReg);
  nsGenericHTMLContainerElement mInner;

  // The cached visited state
  nsLinkState mLinkState;
};

nsresult
NS_NewHTMLAnchorElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLAnchorElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLAnchorElement::nsHTMLAnchorElement(nsINodeInfo *aNodeInfo)
  : mLinkState(eLinkState_Unknown)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  nsHTMLUtils::AddRef(); // for GetHrefCString
}

nsHTMLAnchorElement::~nsHTMLAnchorElement()
{
  nsHTMLUtils::Release(); // for GetHrefCString
}

NS_IMPL_ADDREF(nsHTMLAnchorElement)

NS_IMPL_RELEASE(nsHTMLAnchorElement)

nsresult
nsHTMLAnchorElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLAnchorElementIID)) {
    nsIDOMHTMLAnchorElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsIDOMNSHTMLAnchorElement))) {
    *aInstancePtr = (void*)(nsIDOMNSHTMLAnchorElement*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsILink))) {
    *aInstancePtr = (void*)(nsILink*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLAnchorElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLAnchorElement* it = new nsHTMLAnchorElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Charset, charset)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Coords, coords)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Rel, rel)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Rev, rev)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Shape, shape)
NS_IMPL_INT_ATTR(nsHTMLAnchorElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Target, target)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Type, type)

NS_IMETHODIMP
nsHTMLAnchorElement::SetParent(nsIContent* aParent)
{
  return mInner.SetParent(aParent);
}

// This goes and gets the proper PresContext in order
// for it to get the EventStateManager so it can register 
// the access key
nsresult nsHTMLAnchorElement::RegUnRegAccessKey(PRBool aDoReg)
{
  // first check to see if it even has an acess key
  nsAutoString accessKey;
  PRInt32 nameSpaceID;
  GetNameSpaceID(nameSpaceID);
  nsresult rv = GetAttribute(nameSpaceID, nsHTMLAtoms::accesskey, accessKey);

  if (NS_CONTENT_ATTR_NOT_THERE != rv) {
    nsCOMPtr<nsIPresContext> presContext;
    nsGenericHTMLElement::GetPresContext(this, getter_AddRefs(presContext));

    // With a valid PresContext we can get the EVM 
    // and register the access key
    if (presContext) {
      nsCOMPtr<nsIEventStateManager> stateManager;
      if (NS_SUCCEEDED(presContext->GetEventStateManager(getter_AddRefs(stateManager)))) {
        if (aDoReg) {
          return stateManager->RegisterAccessKey(nsnull, (nsIContent*)this, (PRUint32)accessKey.First());
        } else {
          return stateManager->UnregisterAccessKey(nsnull, (nsIContent*)this, (PRUint32)accessKey.First());
        }
      }
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers)
{
  // The document gets set to null before it is destroyed,
  // so we unregister the the access key here (if it has one)
  // before setting it to null
  if (aDocument == nsnull) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult res = mInner.SetDocument(aDocument, aDeep, aCompileEventHandlers);

  // Register the access key here (if it has one) 
  // if the document isn't null
  if (aDocument != nsnull) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLAnchorElement::Blur()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::Focus()
{
  nsCOMPtr<nsIDocument> doc; // Strong
  nsresult rv = GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv)) { return rv; }
  if (!doc) { return NS_ERROR_NULL_POINTER; }

  PRInt32 numShells = doc->GetNumberOfShells();
  nsCOMPtr<nsIPresContext> context;
  for (PRInt32 i=0; i<numShells; i++) 
  {
    nsCOMPtr<nsIPresShell> shell = getter_AddRefs(doc->GetShellAt(i));
    if (!shell) { return NS_ERROR_NULL_POINTER; }

    rv = shell->GetPresContext(getter_AddRefs(context));
    if (NS_FAILED(rv)) { return rv; }
    if (!context) { return NS_ERROR_NULL_POINTER; }

    rv = SetFocus(context);
    if (NS_FAILED(rv)) { return rv; }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetFocus(nsIPresContext* aPresContext)
{

  // don't make the link grab the focus if there is no link handler
  nsILinkHandler* handler;
  nsresult rv = aPresContext->GetLinkHandler(&handler);
  if (NS_SUCCEEDED(rv) && (nsnull != handler)) {
    nsIEventStateManager *stateManager;
    if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
      stateManager->SetContentState(this, NS_EVENT_STATE_FOCUS);

      // Make sure the presentation is up-to-date
      if (mInner.mDocument) {
        mInner.mDocument->FlushPendingNotifications();
      }

      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
      if (presShell) {
        nsIFrame* frame = nsnull;
        presShell->GetPrimaryFrameFor(this, &frame);
        if (frame) {
          presShell->ScrollFrameIntoView(frame,
                                         NS_PRESSHELL_SCROLL_ANYWHERE,NS_PRESSHELL_SCROLL_ANYWHERE);
        }
      }

      NS_RELEASE(stateManager);
    }
    NS_RELEASE(handler);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::RemoveFocus(nsIPresContext* aPresContext)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::tabindex) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, 32767, aResult,
                                         eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::suppress) {
    if (aValue.EqualsIgnoreCase("true")) {
      aResult.SetEmptyValue();  // XXX? shouldn't just leave "true"
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLAnchorElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                                  nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

// XXX support suppress in here
NS_IMETHODIMP
nsHTMLAnchorElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEventForAnchors(this,
                                         aPresContext, 
                                         aEvent, 
                                         aDOMEvent, 
                                         aFlags, 
                                         aEventStatus);
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetHref(nsString& aValue)
{
  char *buf;
  nsresult rv = GetHrefCString(buf);
  if (NS_FAILED(rv)) return rv;
  if (buf) {
    aValue.AssignWithConversion(buf);
    nsCRT::free(buf);
  }
  // NS_IMPL_STRING_ATTR does nothing where we have (buf == null)
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetHref(const nsString& aValue)
{
  // Clobber our "cache", so we'll recompute it the next time
  // somebody asks for it.
  mLinkState = eLinkState_Unknown;

  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLAnchorElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetProtocol(nsString& aProtocol)
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
        aProtocol.AssignWithConversion(protocol);
        aProtocol.AppendWithConversion(":");
        nsCRT::free(protocol);
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetHost(nsString& aHost)
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
        aHost.AssignWithConversion(host);
        nsCRT::free(host);
        PRInt32 port;
        (void)url->GetPort(&port);
        if (-1 != port) {
          aHost.AppendWithConversion(":");
          aHost.AppendInt(port, 10);
        }
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetHostname(nsString& aHostname)
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
        aHostname.AssignWithConversion(host);
        nsCRT::free(host);
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetPathname(nsString& aPathname)
{
  nsAutoString href;
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  aPathname.Truncate();
  
  result = GetHref(href);
  if (NS_FAILED(result)) {
    return result;
  }

  result = NS_NewURI(getter_AddRefs(uri), href);
  if (NS_FAILED(result)) {
    return result;
  }

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (!url) {
    return NS_OK;
  }

  char* file;
  result = url->GetFilePath(&file);
  if (NS_FAILED(result)) {
    return result;
  }

  aPathname.AssignWithConversion(file);
  nsCRT::free(file);

  return result;
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetSearch(nsString& aSearch)
{
  nsAutoString href;
  nsIURI *uri;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURI(&uri, href);
    if (NS_OK == result) {
      char *search;
      nsIURL* url;
      result = uri->QueryInterface(NS_GET_IID(nsIURL), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->GetQuery(&search);
        NS_RELEASE(url);
      }
      if (result == NS_OK && (nsnull != search) && ('\0' != *search)) {
        aSearch.AssignWithConversion("?");
        aSearch.AppendWithConversion(search);
        nsCRT::free(search);
      }
      else {
        aSearch.SetLength(0);
      }
      NS_RELEASE(uri);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetPort(nsString& aPort)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      aPort.SetLength(0);
      PRInt32 port;
      (void)url->GetPort(&port);
      if (-1 != port) {
        aPort.AppendInt(port, 10);
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetHash(nsString& aHash)
{
  nsAutoString href;
  nsIURI *uri;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURI(&uri, href);

    if (NS_OK == result) {
      char *ref;
      nsIURL* url;
      result = uri->QueryInterface(NS_GET_IID(nsIURL), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->GetRef(&ref);
        NS_RELEASE(url);
      }
      if (result == NS_OK && (nsnull != ref) && ('\0' != *ref)) {
        aHash.AssignWithConversion("#");
        aHash.AppendWithConversion(ref);
        nsCRT::free(ref);
      }
      else {
        aHash.SetLength(0);
      }
      NS_RELEASE(uri);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetText(nsString& aText)
{
  aText.Truncate();

  nsCOMPtr<nsIDOMNode> child;

  mInner.GetFirstChild(getter_AddRefs(child));

  if (child) {
    child->GetNodeValue(aText);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetLinkState(nsLinkState &aState)
{
  aState = mLinkState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetLinkState(nsLinkState aState)
{
  mLinkState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetHrefCString(char* &aBuf)
{
  // Get href= attribute (relative URL).
  nsAutoString relURLSpec;

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, relURLSpec)) {
    // Get base URL.
    nsCOMPtr<nsIURI> baseURL;
    mInner.GetBaseURL(*getter_AddRefs(baseURL));

    if (baseURL) {
      // Get absolute URL.
      NS_MakeAbsoluteURIWithCharset(&aBuf, relURLSpec, mInner.mDocument, baseURL,
                                    nsHTMLUtils::IOService, nsHTMLUtils::CharsetMgr);
    }
    else {
      // Absolute URL is same as relative URL.
      aBuf = relURLSpec.ToNewUTF8String();
    }
  }
  else {
    // Absolute URL is null to say we have no HREF.
    aBuf = nsnull;
  }

  return NS_OK;
}
