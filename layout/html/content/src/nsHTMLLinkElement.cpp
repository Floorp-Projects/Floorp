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
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMLinkStyle.h"
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
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsHTMLUtils.h"
#include "nsIURL.h"


class nsHTMLLinkElement : public nsIDOMHTMLLinkElement,
                          public nsIJSScriptObject,
                          public nsILink,
                          public nsIHTMLContent,
                          public nsIStyleSheetLinkingElement,
                          public nsIDOMLinkStyle
{
public:
  nsHTMLLinkElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLLinkElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLLinkElement
  NS_DECL_IDOMHTMLLINKELEMENT

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

  // nsILink
  NS_IMETHOD    GetLinkState(nsLinkState &aState);
  NS_IMETHOD    SetLinkState(nsLinkState aState);
  NS_IMETHOD    GetHrefCString(char* &aBuf);

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIStyleSheetLinkingElement  
  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aStyleSheet);
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aStyleSheet);

  // nsIDOMLinkStyle
  NS_DECL_IDOMLINKSTYLE

protected:
  nsGenericHTMLLeafElement mInner;
  nsIStyleSheet* mStyleSheet;

  // The cached visited state
  nsLinkState mLinkState;
};

nsresult
NS_NewHTMLLinkElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLLinkElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLLinkElement::nsHTMLLinkElement(nsINodeInfo *aNodeInfo)
  : mLinkState(eLinkState_Unknown)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  mStyleSheet = nsnull;
  nsHTMLUtils::AddRef(); // for GetHrefCString
}

nsHTMLLinkElement::~nsHTMLLinkElement()
{
  NS_IF_RELEASE(mStyleSheet);
  nsHTMLUtils::Release(); // for GetHrefCString
}

NS_IMPL_ADDREF(nsHTMLLinkElement)

NS_IMPL_RELEASE(nsHTMLLinkElement)

nsresult
nsHTMLLinkElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLLinkElement))) {
    nsIDOMHTMLLinkElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStyleSheetLinkingElement))) {
    nsIStyleSheetLinkingElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsILink))) {
    *aInstancePtr = (void*)(nsILink*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMLinkStyle))) {
    *aInstancePtr = (void*)(nsIDOMLinkStyle*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLLinkElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLLinkElement* it = new nsHTMLLinkElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLLinkElement::GetDisabled(PRBool* aDisabled)
{
  nsresult result = NS_OK;
  
  if (nsnull != mStyleSheet) {
    nsIDOMStyleSheet* ss;
    
    result = mStyleSheet->QueryInterface(NS_GET_IID(nsIDOMStyleSheet), (void**)&ss);
    if (NS_OK == result) {
      result = ss->GetDisabled(aDisabled);
      NS_RELEASE(ss);
    }
  }
  else {
    *aDisabled = PR_FALSE;
  }
  
  return result;
}

NS_IMETHODIMP 
nsHTMLLinkElement::SetDisabled(PRBool aDisabled)
{
  nsresult result = NS_OK;
  
  if (nsnull != mStyleSheet) {
    nsIDOMStyleSheet* ss;
    
    result = mStyleSheet->QueryInterface(NS_GET_IID(nsIDOMStyleSheet), (void**)&ss);
    if (NS_OK == result) {
      result = ss->SetDisabled(aDisabled);
      NS_RELEASE(ss);
    }
  }
  
  return result;
}

NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Charset, charset)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Media, media)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rel, rel)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rev, rev)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Target, target)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Type, type)

NS_IMETHODIMP
nsHTMLLinkElement::GetHref(nsAWritableString& aValue)
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
nsHTMLLinkElement::SetHref(const nsAReadableString& aValue)
{
  // Clobber our "cache", so we'll recompute it the next time
  // somebody asks for it.
  mLinkState = eLinkState_Unknown;

  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, aValue, PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLLinkElement::SetStyleSheet(nsIStyleSheet* aStyleSheet)
{
  NS_IF_RELEASE(mStyleSheet);

  mStyleSheet = aStyleSheet;
  NS_IF_ADDREF(mStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLLinkElement::GetStyleSheet(nsIStyleSheet*& aStyleSheet)
{
  aStyleSheet = mStyleSheet;
  NS_IF_ADDREF(aStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsAReadableString& aValue,
                              nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLLinkElement::AttributeToString(nsIAtom* aAttribute,
                              const nsHTMLValue& aValue,
                              nsAWritableString& aResult) const
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
nsHTMLLinkElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                            PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLLinkElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLLinkElement::HandleDOMEvent(nsIPresContext* aPresContext,
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
nsHTMLLinkElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}

NS_IMETHODIMP
nsHTMLLinkElement::GetLinkState(nsLinkState &aState)
{
  aState = mLinkState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::SetLinkState(nsLinkState aState)
{
  mLinkState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::GetHrefCString(char* &aBuf)
{
  // Get href= attribute (relative URL).
  nsAutoString relURLSpec;

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, relURLSpec)) {
    // Clean up any leading or trailing whitespace
    relURLSpec.Trim(" \t\n\r");

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
    // Absolute URL is empty because we have no HREF.
    aBuf = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::GetSheet(nsIDOMStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);
  *aSheet = 0;

  if (mStyleSheet)
    mStyleSheet->QueryInterface(NS_GET_IID(nsIDOMStyleSheet), (void **)aSheet);

  // Always return NS_OK to avoid throwing JS exceptions if mStyleSheet 
  // is not a nsIDOMStyleSheet
  return NS_OK;
}

