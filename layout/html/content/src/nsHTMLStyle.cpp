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
#include "nsIDOMHTMLStyleElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

// XXX no SRC attribute

static NS_DEFINE_IID(kIDOMHTMLStyleElementIID, NS_IDOMHTMLSTYLEELEMENT_IID);

class nsHTMLStyle : public nsIDOMHTMLStyleElement,
                    public nsIScriptObjectOwner,
                    public nsIDOMEventReceiver,
                    public nsIHTMLContent
{
public:
  nsHTMLStyle(nsIAtom* aTag);
  ~nsHTMLStyle();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLStyleElement
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetMedia(nsString& aMedia);
  NS_IMETHOD SetMedia(const nsString& aMedia);
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
NS_NewHTMLStyle(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLStyle(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLStyle::nsHTMLStyle(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLStyle::~nsHTMLStyle()
{
}

NS_IMPL_ADDREF(nsHTMLStyle)

NS_IMPL_RELEASE(nsHTMLStyle)

nsresult
nsHTMLStyle::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLStyleElementIID)) {
    nsIDOMHTMLStyleElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLStyle::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLStyle* it = new nsHTMLStyle(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLStyle::GetDisabled(PRBool* aValue)
{
  nsHTMLValue val;
  *aValue = NS_CONTENT_ATTR_HAS_VALUE ==
    mInner.GetAttribute(nsHTMLAtoms::disabled, val);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyle::SetDisabled(PRBool aValue)
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
nsHTMLStyle::GetMedia(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::media, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyle::SetMedia(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::media, aValue, eSetAttrNotify_Restart);
}

NS_IMETHODIMP
nsHTMLStyle::GetType(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::type, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyle::SetType(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::type, aValue, eSetAttrNotify_Restart);
}

NS_IMETHODIMP
nsHTMLStyle::StringToAttribute(nsIAtom* aAttribute,
                               const nsString& aValue,
                               nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLStyle::AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLStyle::MapAttributesInto(nsIStyleContext* aContext,
                               nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyle::HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
