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
#include "nsIDOMHTMLLinkElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLLinkElementIID, NS_IDOMHTMLLINKELEMENT_IID);

class nsHTMLLink : public nsIDOMHTMLLinkElement,
                   public nsIScriptObjectOwner,
                   public nsIDOMEventReceiver,
                   public nsIHTMLContent
{
public:
  nsHTMLLink(nsIAtom* aTag);
  ~nsHTMLLink();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLLinkElement
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetCharset(nsString& aCharset);
  NS_IMETHOD SetCharset(const nsString& aCharset);
  NS_IMETHOD GetHref(nsString& aHref);
  NS_IMETHOD SetHref(const nsString& aHref);
  NS_IMETHOD GetHreflang(nsString& aHreflang);
  NS_IMETHOD SetHreflang(const nsString& aHreflang);
  NS_IMETHOD GetMedia(nsString& aMedia);
  NS_IMETHOD SetMedia(const nsString& aMedia);
  NS_IMETHOD GetRel(nsString& aRel);
  NS_IMETHOD SetRel(const nsString& aRel);
  NS_IMETHOD GetRev(nsString& aRev);
  NS_IMETHOD SetRev(const nsString& aRev);
  NS_IMETHOD GetTarget(nsString& aTarget);
  NS_IMETHOD SetTarget(const nsString& aTarget);
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD SetType(const nsString& aType);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsHTMLGenericLeafContent mInner;
};

nsresult
NS_NewHTMLLink(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLLink(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLLink::nsHTMLLink(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLLink::~nsHTMLLink()
{
}

NS_IMPL_ADDREF(nsHTMLLink)

NS_IMPL_RELEASE(nsHTMLLink)

nsresult
nsHTMLLink::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLLinkElementIID)) {
    nsIDOMHTMLLinkElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLLink::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLLink* it = new nsHTMLLink(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLLink::GetCharset(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetCharset(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::charset, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLLink::GetDisabled(PRBool* aValue)
{
  nsHTMLValue val;
  *aValue = NS_CONTENT_ATTR_HAS_VALUE ==
    mInner.GetAttribute(nsHTMLAtoms::disabled, val);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetDisabled(PRBool aValue)
{
  nsAutoString empty;
  if (aValue) {
    return mInner.SetAttr(nsHTMLAtoms::disabled, empty, eSetAttrNotify_Render);
  }
  else {
    mInner.UnsetAttribute(nsHTMLAtoms::disabled);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsHTMLLink::GetHref(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::href, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetHref(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::href, aValue, eSetAttrNotify_Render);
}

NS_IMETHODIMP
nsHTMLLink::GetHreflang(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::hreflang, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetHreflang(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::hreflang, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLLink::GetMedia(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::media, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetMedia(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::media, aValue, eSetAttrNotify_Restart);
}

NS_IMETHODIMP
nsHTMLLink::GetRel(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::rel, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetRel(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::rel, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLLink::GetRev(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::rev, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetRev(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::rev, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLLink::GetTarget(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::target, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetTarget(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::target, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLLink::GetType(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::type, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::SetType(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::type, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLLink::StringToAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLLink::AttributeToString(nsIAtom* aAttribute,
                              nsHTMLValue& aValue,
                              nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLLink::MapAttributesInto(nsIStyleContext* aContext,
                              nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLink::HandleDOMEvent(nsIPresContext& aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
