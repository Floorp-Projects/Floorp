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
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLTableCellElementIID, NS_IDOMHTMLTABLECELLELEMENT_IID);

class nsHTMLTableCellElement : public nsIDOMHTMLTableCellElement,
                        public nsIScriptObjectOwner,
                        public nsIDOMEventReceiver,
                        public nsIHTMLContent
{
public:
  nsHTMLTableCellElement(nsIAtom* aTag);
  ~nsHTMLTableCellElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTableCellElement
  NS_IMETHOD GetCellIndex(PRInt32* aCellIndex);
  NS_IMETHOD SetCellIndex(PRInt32 aCellIndex);
  NS_IMETHOD GetAbbr(nsString& aAbbr);
  NS_IMETHOD SetAbbr(const nsString& aAbbr);
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetAxis(nsString& aAxis);
  NS_IMETHOD SetAxis(const nsString& aAxis);
  NS_IMETHOD GetBgColor(nsString& aBgColor);
  NS_IMETHOD SetBgColor(const nsString& aBgColor);
  NS_IMETHOD GetCh(nsString& aCh);
  NS_IMETHOD SetCh(const nsString& aCh);
  NS_IMETHOD GetChOff(nsString& aChOff);
  NS_IMETHOD SetChOff(const nsString& aChOff);
  NS_IMETHOD GetColSpan(PRInt32* aColSpan);
  NS_IMETHOD SetColSpan(PRInt32 aColSpan);
  NS_IMETHOD GetHeaders(nsString& aHeaders);
  NS_IMETHOD SetHeaders(const nsString& aHeaders);
  NS_IMETHOD GetHeight(nsString& aHeight);
  NS_IMETHOD SetHeight(const nsString& aHeight);
  NS_IMETHOD GetNoWrap(PRBool* aNoWrap);
  NS_IMETHOD SetNoWrap(PRBool aNoWrap);
  NS_IMETHOD GetRowSpan(PRInt32* aRowSpan);
  NS_IMETHOD SetRowSpan(PRInt32 aRowSpan);
  NS_IMETHOD GetScope(nsString& aScope);
  NS_IMETHOD SetScope(const nsString& aScope);
  NS_IMETHOD GetVAlign(nsString& aVAlign);
  NS_IMETHOD SetVAlign(const nsString& aVAlign);
  NS_IMETHOD GetWidth(nsString& aWidth);
  NS_IMETHOD SetWidth(const nsString& aWidth);

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
};

nsresult
NS_NewHTMLTableCellElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTableCellElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLTableCellElement::nsHTMLTableCellElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLTableCellElement::~nsHTMLTableCellElement()
{
}

NS_IMPL_ADDREF(nsHTMLTableCellElement)

NS_IMPL_RELEASE(nsHTMLTableCellElement)

nsresult
nsHTMLTableCellElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLTableCellElementIID)) {
    nsIDOMHTMLTableCellElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableCellElement::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLTableCellElement* it = new nsHTMLTableCellElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLTableCellElement::GetCellIndex(PRInt32* aCellIndex)
{
  *aCellIndex = 0;/* XXX */
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableCellElement::SetCellIndex(PRInt32 aCellIndex)
{
  // XXX write me
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Abbr, abbr, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Align, align, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Axis, axis, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, BgColor, bgcolor, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Ch, ch, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, ChOff, choff, eSetAttrNotify_Reflow)
NS_IMPL_INT_ATTR(nsHTMLTableCellElement, ColSpan, colspan, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Headers, headers, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Height, height, eSetAttrNotify_Reflow)
NS_IMPL_BOOL_ATTR(nsHTMLTableCellElement, NoWrap, nowrap, eSetAttrNotify_Reflow)
NS_IMPL_INT_ATTR(nsHTMLTableCellElement, RowSpan, rowspan, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Scope, scope, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, VAlign, valign, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Width, width, eSetAttrNotify_Reflow)

NS_IMETHODIMP
nsHTMLTableCellElement::StringToAttribute(nsIAtom* aAttribute,
                                   const nsString& aValue,
                                   nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableCellElement::AttributeToString(nsIAtom* aAttribute,
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
nsHTMLTableCellElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableCellElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                nsEvent* aEvent,
                                nsIDOMEvent** aDOMEvent,
                                PRUint32 aFlags,
                                nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
