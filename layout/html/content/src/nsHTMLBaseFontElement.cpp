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
#include "nsIDOMHTMLBaseFontElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLBaseFontElementIID, NS_IDOMHTMLBASEFONTELEMENT_IID);

class nsHTMLBaseFontElement : public nsIDOMHTMLBaseFontElement,
                              public nsIScriptObjectOwner,
                              public nsIDOMEventReceiver,
                              public nsIHTMLContent
{
public:
  nsHTMLBaseFontElement(nsIAtom* aTag);
  virtual ~nsHTMLBaseFontElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLBaseElement
  NS_IMETHOD GetColor(nsString& aColor);
  NS_IMETHOD SetColor(const nsString& aColor);
  NS_IMETHOD GetFace(nsString& aFace);
  NS_IMETHOD SetFace(const nsString& aFace);
  NS_IMETHOD GetSize(nsString& aSize);
  NS_IMETHOD SetSize(const nsString& aSize);

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
NS_NewHTMLBaseFontElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLBaseFontElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLBaseFontElement::nsHTMLBaseFontElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLBaseFontElement::~nsHTMLBaseFontElement()
{
}

NS_IMPL_ADDREF(nsHTMLBaseFontElement)

NS_IMPL_RELEASE(nsHTMLBaseFontElement)

nsresult
nsHTMLBaseFontElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLBaseFontElementIID)) {
    nsIDOMHTMLBaseFontElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLBaseFontElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLBaseFontElement* it = new nsHTMLBaseFontElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLBaseFontElement, Color, color)
NS_IMPL_STRING_ATTR(nsHTMLBaseFontElement, Face, face)
NS_IMPL_STRING_ATTR(nsHTMLBaseFontElement, Size, size)

NS_IMETHODIMP
nsHTMLBaseFontElement::StringToAttribute(nsIAtom* aAttribute,
                                         const nsString& aValue,
                                         nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLBaseFontElement::AttributeToString(nsIAtom* aAttribute,
                                         const nsHTMLValue& aValue,
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
nsHTMLBaseFontElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}



NS_IMETHODIMP
nsHTMLBaseFontElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                      nsEvent* aEvent,
                                      nsIDOMEvent** aDOMEvent,
                                      PRUint32 aFlags,
                                      nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLBaseFontElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  if (nsHTMLAtoms::color == aAttribute ||
    nsHTMLAtoms::face == aAttribute ||
    nsHTMLAtoms::size == aAttribute) {
    *aHint = NS_STYLE_HINT_RECONSTRUCT_ALL;
  }
  else {
    nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  }
  return NS_OK;
}
