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
#include "nsIDOMHTMLMapElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "GenericElementCollection.h"

static NS_DEFINE_IID(kIDOMHTMLMapElementIID, NS_IDOMHTMLMAPELEMENT_IID);

class nsHTMLMapElement : public nsIDOMHTMLMapElement,
                         public nsIScriptObjectOwner,
                         public nsIDOMEventReceiver,
                         public nsIHTMLContent
{
public:
  nsHTMLMapElement(nsIAtom* aTag);
  ~nsHTMLMapElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLMapElement
  NS_IMETHOD GetAreas(nsIDOMHTMLCollection** aAreas);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
  GenericElementCollection* mAreas;
};

nsresult
NS_NewHTMLMapElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLMapElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLMapElement::nsHTMLMapElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mAreas = nsnull;
}

nsHTMLMapElement::~nsHTMLMapElement()
{
  if (nsnull != mAreas) {
    mAreas->ParentDestroyed();
    NS_RELEASE(mAreas);
  }
}

NS_IMPL_ADDREF(nsHTMLMapElement)

NS_IMPL_RELEASE(nsHTMLMapElement)

nsresult
nsHTMLMapElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLMapElementIID)) {
    nsIDOMHTMLMapElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLMapElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLMapElement* it = new nsHTMLMapElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLMapElement::GetAreas(nsIDOMHTMLCollection** aAreas)
{
  if (nsnull == aAreas) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull == mAreas) {
    mAreas = new GenericElementCollection(NS_STATIC_CAST(nsIContent*, this),
                                          nsHTMLAtoms::area);
    if (nsnull == mAreas) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mAreas);
  }
  *aAreas = NS_STATIC_CAST(nsIDOMHTMLCollection*, mAreas);
  NS_ADDREF(mAreas);
  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLMapElement, Name, name)

NS_IMETHODIMP
nsHTMLMapElement::StringToAttribute(nsIAtom* aAttribute,
                                    const nsString& aValue,
                                    nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLMapElement::AttributeToString(nsIAtom* aAttribute,
                                    const nsHTMLValue& aValue,
                                    nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLMapElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLMapElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                 nsEvent* aEvent,
                                 nsIDOMEvent** aDOMEvent,
                                 PRUint32 aFlags,
                                 nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLMapElement::GetStyleHintForAttributeChange(
  const nsIAtom* aAttribute,
  PRInt32 *aHint) const
{
  nsGenericHTMLElement::SetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}
