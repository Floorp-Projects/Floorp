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
#include "nsIDOMHTMLAreaElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLAreaElementIID, NS_IDOMHTMLAREAELEMENT_IID);

class nsHTMLArea : public nsIDOMHTMLAreaElement,
		   public nsIScriptObjectOwner,
		   public nsIDOMEventReceiver,
		   public nsIHTMLContent
{
public:
  nsHTMLArea(nsIAtom* aTag);
  ~nsHTMLArea();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLAreaElement
  NS_IMETHOD GetAccessKey(nsString& aAccessKey);
  NS_IMETHOD SetAccessKey(const nsString& aAccessKey);
  NS_IMETHOD GetAlt(nsString& aAlt);
  NS_IMETHOD SetAlt(const nsString& aAlt);
  NS_IMETHOD GetCoords(nsString& aCoords);
  NS_IMETHOD SetCoords(const nsString& aCoords);
  NS_IMETHOD GetHref(nsString& aHref);
  NS_IMETHOD SetHref(const nsString& aHref);
  NS_IMETHOD GetNoHref(PRBool* aNoHref);
  NS_IMETHOD SetNoHref(PRBool aNoHref);
  NS_IMETHOD GetShape(nsString& aShape);
  NS_IMETHOD SetShape(const nsString& aShape);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD GetTarget(nsString& aTarget);
  NS_IMETHOD SetTarget(const nsString& aTarget);

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
NS_NewHTMLArea(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLArea(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLArea::nsHTMLArea(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLArea::~nsHTMLArea()
{
}

NS_IMPL_ADDREF(nsHTMLArea)

NS_IMPL_RELEASE(nsHTMLArea)

nsresult
nsHTMLArea::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLAreaElementIID)) {
    nsIDOMHTMLAreaElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLArea::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLArea* it = new nsHTMLArea(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLArea::GetAccessKey(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::accesskey, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::SetAccessKey(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::accesskey, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLArea::GetAlt(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::alt, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::SetAlt(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::alt, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLArea::GetCoords(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::coords, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::SetCoords(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::coords, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLArea::GetHref(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::href, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::SetHref(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::href, aValue, eSetAttrNotify_Render);
}

NS_IMETHODIMP
nsHTMLArea::GetNoHref(PRBool* aValue)
{
  nsHTMLValue val;
  *aValue = NS_CONTENT_ATTR_HAS_VALUE ==
    mInner.GetAttribute(nsHTMLAtoms::nohref, val);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::SetNoHref(PRBool aValue)
{
  nsAutoString empty;
  if (aValue) {
    return mInner.SetAttr(nsHTMLAtoms::nohref, empty, eSetAttrNotify_None);
  }
  else {
    mInner.UnsetAttribute(nsHTMLAtoms::nohref);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsHTMLArea::GetShape(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::shape, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::SetShape(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::shape, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLArea::GetTabIndex(PRInt32* aValue)
{
  nsHTMLValue value;
  *aValue = -1;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mInner.GetAttribute(nsHTMLAtoms::tabindex, value)) {
    if (value.GetUnit() == eHTMLUnit_Integer) {
      *aValue = value.GetIntValue();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::SetTabIndex(PRInt32 aValue)
{
  nsHTMLValue value(aValue, eHTMLUnit_Integer);
  return mInner.SetAttr(nsHTMLAtoms::tabindex, value, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLArea::GetTarget(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::target, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::SetTarget(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::target, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLArea::StringToAttribute(nsIAtom* aAttribute,
                            const nsString& aValue,
                            nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLArea::AttributeToString(nsIAtom* aAttribute,
                            nsHTMLValue& aValue,
                            nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLArea::MapAttributesInto(nsIStyleContext* aContext,
			      nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLArea::HandleDOMEvent(nsIPresContext& aPresContext,
			   nsEvent* aEvent,
			   nsIDOMEvent** aDOMEvent,
			   PRUint32 aFlags,
			   nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
