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


class nsHTMLLinkElement : public nsGenericHTMLLeafElement,
                          public nsIDOMHTMLLinkElement,
                          public nsILink,
                          public nsIStyleSheetLinkingElement,
                          public nsIDOMLinkStyle
{
public:
  nsHTMLLinkElement();
  virtual ~nsHTMLLinkElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLLinkElement
  NS_DECL_IDOMHTMLLINKELEMENT

  // nsILink
  NS_IMETHOD    GetLinkState(nsLinkState &aState);
  NS_IMETHOD    SetLinkState(nsLinkState aState);
  NS_IMETHOD    GetHrefCString(char* &aBuf);

  // nsIStyleSheetLinkingElement  
  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aStyleSheet);
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aStyleSheet);

  // nsIDOMLinkStyle
  NS_DECL_IDOMLINKSTYLE

  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  nsIStyleSheet* mStyleSheet;

  // The cached visited state
  nsLinkState mLinkState;
};

nsresult
NS_NewHTMLLinkElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLLinkElement* it = new nsHTMLLinkElement();

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


nsHTMLLinkElement::nsHTMLLinkElement()
  : mLinkState(eLinkState_Unknown)
{
  mStyleSheet = nsnull;
  nsHTMLUtils::AddRef(); // for GetHrefCString
}

nsHTMLLinkElement::~nsHTMLLinkElement()
{
  NS_IF_RELEASE(mStyleSheet);
  nsHTMLUtils::Release(); // for GetHrefCString
}


NS_IMPL_ADDREF_INHERITED(nsHTMLLinkElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLLinkElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI3(nsHTMLLinkElement, nsGenericHTMLLeafElement,
                        nsIDOMHTMLLinkElement, nsIDOMLinkStyle,
                        nsIStyleSheetLinkingElement);


nsresult
nsHTMLLinkElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLLinkElement* it = new nsHTMLLinkElement();

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


NS_IMETHODIMP
nsHTMLLinkElement::GetDisabled(PRBool* aDisabled)
{
  nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(mStyleSheet));
  nsresult result = NS_OK;

  if (ss) {
    result = ss->GetDisabled(aDisabled);
  } else {
    *aDisabled = PR_FALSE;
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLLinkElement::SetDisabled(PRBool aDisabled)
{
  nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(mStyleSheet));
  nsresult result = NS_OK;

  if (ss) {
    result = ss->SetDisabled(aDisabled);
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

  return nsGenericHTMLLeafElement::SetAttribute(kNameSpaceID_HTML,
                                                nsHTMLAtoms::href, aValue,
                                                PR_TRUE);
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
nsHTMLLinkElement::HandleDOMEvent(nsIPresContext* aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus* aEventStatus)
{
  return HandleDOMEventForAnchors(this, aPresContext, aEvent, aDOMEvent,
                                  aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLLinkElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
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
      nsGenericHTMLLeafElement::GetAttribute(kNameSpaceID_HTML,
                                             nsHTMLAtoms::href, relURLSpec)) {
    // Clean up any leading or trailing whitespace
    relURLSpec.Trim(" \t\n\r");

    // Get base URL.
    nsCOMPtr<nsIURI> baseURL;
    GetBaseURL(*getter_AddRefs(baseURL));

    if (baseURL) {
      // Get absolute URL.
      NS_MakeAbsoluteURIWithCharset(&aBuf, relURLSpec, mDocument, baseURL,
                                    nsHTMLUtils::IOService,
                                    nsHTMLUtils::CharsetMgr);
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

