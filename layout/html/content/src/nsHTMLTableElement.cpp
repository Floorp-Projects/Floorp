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
#include "nsIDOMHTMLTableElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLTableElementIID, NS_IDOMHTMLTABLEELEMENT_IID);

class nsHTMLTableElement : public nsIDOMHTMLTableElement,
                    public nsIScriptObjectOwner,
                    public nsIDOMEventReceiver,
                    public nsIHTMLContent
{
public:
  nsHTMLTableElement(nsIAtom* aTag);
  ~nsHTMLTableElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTableElement
  NS_IMETHOD GetCaption(nsIDOMHTMLTableCaptionElement** aCaption);
  NS_IMETHOD SetCaption(nsIDOMHTMLTableCaptionElement* aCaption);
  NS_IMETHOD GetTHead(nsIDOMHTMLTableSectionElement** aTHead);
  NS_IMETHOD SetTHead(nsIDOMHTMLTableSectionElement* aTHead);
  NS_IMETHOD GetTFoot(nsIDOMHTMLTableSectionElement** aTFoot);
  NS_IMETHOD SetTFoot(nsIDOMHTMLTableSectionElement* aTFoot);
  NS_IMETHOD GetRows(nsIDOMHTMLCollection** aRows);
  NS_IMETHOD SetRows(nsIDOMHTMLCollection* aRows);
  NS_IMETHOD GetTBodies(nsIDOMHTMLCollection** aTBodies);
  NS_IMETHOD SetTBodies(nsIDOMHTMLCollection* aTBodies);
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetBgColor(nsString& aBgColor);
  NS_IMETHOD SetBgColor(const nsString& aBgColor);
  NS_IMETHOD GetBorder(nsString& aBorder);
  NS_IMETHOD SetBorder(const nsString& aBorder);
  NS_IMETHOD GetCellPadding(nsString& aCellPadding);
  NS_IMETHOD SetCellPadding(const nsString& aCellPadding);
  NS_IMETHOD GetCellSpacing(nsString& aCellSpacing);
  NS_IMETHOD SetCellSpacing(const nsString& aCellSpacing);
  NS_IMETHOD GetFrame(nsString& aFrame);
  NS_IMETHOD SetFrame(const nsString& aFrame);
  NS_IMETHOD GetRules(nsString& aRules);
  NS_IMETHOD SetRules(const nsString& aRules);
  NS_IMETHOD GetSummary(nsString& aSummary);
  NS_IMETHOD SetSummary(const nsString& aSummary);
  NS_IMETHOD GetWidth(nsString& aWidth);
  NS_IMETHOD SetWidth(const nsString& aWidth);
  NS_IMETHOD CreateTHead(nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteTHead();
  NS_IMETHOD CreateTFoot(nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteTFoot();
  NS_IMETHOD CreateCaption(nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteCaption();
  NS_IMETHOD InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteRow(PRInt32 aIndex);

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
NS_NewHTMLTableElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTableElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLTableElement::nsHTMLTableElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLTableElement::~nsHTMLTableElement()
{
}

NS_IMPL_ADDREF(nsHTMLTableElement)

NS_IMPL_RELEASE(nsHTMLTableElement)

nsresult
nsHTMLTableElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLTableElementIID)) {
    nsIDOMHTMLTableElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableElement::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLTableElement* it = new nsHTMLTableElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLTableElement, Align, align, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, BgColor, bgcolor, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Border, border, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, CellPadding, cellpadding, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, CellSpacing, cellspacing, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Frame, frame, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Rules, rules, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Summary, summary, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Width, width, eSetAttrNotify_None)

NS_IMETHODIMP
nsHTMLTableElement::GetCaption(nsIDOMHTMLTableCaptionElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::SetCaption(nsIDOMHTMLTableCaptionElement* aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::GetTHead(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::SetTHead(nsIDOMHTMLTableSectionElement* aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::GetTFoot(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::SetTFoot(nsIDOMHTMLTableSectionElement* aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::GetRows(nsIDOMHTMLCollection** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::SetRows(nsIDOMHTMLCollection* aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::GetTBodies(nsIDOMHTMLCollection** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::SetTBodies(nsIDOMHTMLCollection* aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::CreateTHead(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTHead()
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::CreateTFoot(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTFoot()
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::CreateCaption(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteCaption()
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteRow(PRInt32 aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::StringToAttribute(nsIAtom* aAttribute,
                               const nsString& aValue,
                               nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableElement::AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLTableElement::MapAttributesInto(nsIStyleContext* aContext,
                               nsIPresContext* aPresContext)
{
  // XXX write me
  return mInner.MapAttributesInto(aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLTableElement::HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
