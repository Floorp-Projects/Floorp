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
#include "nsIDOMHTMLFormElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);

class nsHTMLFormElement : public nsIDOMHTMLFormElement,
                   public nsIScriptObjectOwner,
                   public nsIDOMEventReceiver,
                   public nsIHTMLContent
{
public:
  nsHTMLFormElement(nsIAtom* aTag);
  ~nsHTMLFormElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLFormElement
  NS_IMETHOD GetElements(nsIDOMHTMLCollection** aElements);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD GetAcceptCharset(nsString& aAcceptCharset);
  NS_IMETHOD SetAcceptCharset(const nsString& aAcceptCharset);
  NS_IMETHOD GetAction(nsString& aAction);
  NS_IMETHOD SetAction(const nsString& aAction);
  NS_IMETHOD GetEnctype(nsString& aEnctype);
  NS_IMETHOD SetEnctype(const nsString& aEnctype);
  NS_IMETHOD GetMethod(nsString& aMethod);
  NS_IMETHOD SetMethod(const nsString& aMethod);
  NS_IMETHOD GetTarget(nsString& aTarget);
  NS_IMETHOD SetTarget(const nsString& aTarget);
  NS_IMETHOD Reset();
  NS_IMETHOD Submit();

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLLeafElement mInner;
};

nsresult
NS_NewHTMLFormElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLFormElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLFormElement::nsHTMLFormElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLFormElement::~nsHTMLFormElement()
{
}

NS_IMPL_ADDREF(nsHTMLFormElement)

NS_IMPL_RELEASE(nsHTMLFormElement)

nsresult
nsHTMLFormElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLFormElementIID)) {
    nsIDOMHTMLFormElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLFormElement::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLFormElement* it = new nsHTMLFormElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLFormElement::GetElements(nsIDOMHTMLCollection** aResult)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::GetName(nsString& aValue)
{
  return mInner.GetAttribute(nsHTMLAtoms::name, aValue);
}

NS_IMPL_STRING_ATTR(nsHTMLFormElement, AcceptCharset, acceptcharset, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Action, action, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Enctype, enctype, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Method, method, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Target, target, eSetAttrNotify_None)

NS_IMETHODIMP
nsHTMLFormElement::Reset()
{
  // XXX
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::Submit()
{
  // XXX
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLFormElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLFormElement::AttributeToString(nsIAtom* aAttribute,
                              nsHTMLValue& aValue,
                              nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  // XXX write me
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLFormElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}



NS_IMETHODIMP
nsHTMLFormElement::HandleDOMEvent(nsIPresContext& aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
