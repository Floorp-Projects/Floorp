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
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLTableRowElementIID, NS_IDOMHTMLTABLEROWELEMENT_IID);

class nsHTMLTableRow : public nsIDOMHTMLTableRowElement,
                       public nsIScriptObjectOwner,
                       public nsIDOMEventReceiver,
                       public nsIHTMLContent
{
public:
  nsHTMLTableRow(nsIAtom* aTag);
  ~nsHTMLTableRow();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTableRowElement
  NS_IMETHOD GetRowIndex(PRInt32* aRowIndex);
  NS_IMETHOD SetRowIndex(PRInt32 aRowIndex);
  NS_IMETHOD GetSectionRowIndex(PRInt32* aSectionRowIndex);
  NS_IMETHOD SetSectionRowIndex(PRInt32 aSectionRowIndex);
  NS_IMETHOD GetCells(nsIDOMHTMLCollection** aCells);
  NS_IMETHOD SetCells(nsIDOMHTMLCollection* aCells);
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetBgColor(nsString& aBgColor);
  NS_IMETHOD SetBgColor(const nsString& aBgColor);
  NS_IMETHOD GetCh(nsString& aCh);
  NS_IMETHOD SetCh(const nsString& aCh);
  NS_IMETHOD GetChOff(nsString& aChOff);
  NS_IMETHOD SetChOff(const nsString& aChOff);
  NS_IMETHOD GetVAlign(nsString& aVAlign);
  NS_IMETHOD SetVAlign(const nsString& aVAlign);
  NS_IMETHOD InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteCell(PRInt32 aIndex);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsHTMLGenericContainerContent mInner;
};

nsresult
NS_NewHTMLTableRow(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTableRow(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLTableRow::nsHTMLTableRow(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLTableRow::~nsHTMLTableRow()
{
}

NS_IMPL_ADDREF(nsHTMLTableRow)

NS_IMPL_RELEASE(nsHTMLTableRow)

nsresult
nsHTMLTableRow::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLTableRowElementIID)) {
    nsIDOMHTMLTableRowElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableRow::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLTableRow* it = new nsHTMLTableRow(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLTableRow::GetRowIndex(PRInt32* aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRow::SetRowIndex(PRInt32 aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRow::GetSectionRowIndex(PRInt32* aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRow::SetSectionRowIndex(PRInt32 aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRow::GetCells(nsIDOMHTMLCollection** aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRow::SetCells(nsIDOMHTMLCollection* aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRow::InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRow::DeleteCell(PRInt32 aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLTableRow, Align, align, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableRow, BgColor, bgcolor, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLTableRow, Ch, ch, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableRow, ChOff, choff, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableRow, VAlign, valign, eSetAttrNotify_Reflow)

NS_IMETHODIMP
nsHTMLTableRow::StringToAttribute(nsIAtom* aAttribute,
                                  const nsString& aValue,
                                  nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableRow::AttributeToString(nsIAtom* aAttribute,
                                  nsHTMLValue& aValue,
                                  nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLTableRow::MapAttributesInto(nsIStyleContext* aContext,
                                  nsIPresContext* aPresContext)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRow::HandleDOMEvent(nsIPresContext& aPresContext,
                               nsEvent* aEvent,
                               nsIDOMEvent** aDOMEvent,
                               PRUint32 aFlags,
                               nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
