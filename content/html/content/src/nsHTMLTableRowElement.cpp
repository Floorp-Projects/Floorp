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
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLTableRowElementIID, NS_IDOMHTMLTABLEROWELEMENT_IID);

class nsHTMLTableRowElement : public nsIDOMHTMLTableRowElement,
                              public nsIScriptObjectOwner,
                              public nsIDOMEventReceiver,
                              public nsIHTMLContent
{
public:
  nsHTMLTableRowElement(nsIAtom* aTag);
  ~nsHTMLTableRowElement();

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
  nsGenericHTMLContainerElement mInner;
};

nsresult
NS_NewHTMLTableRowElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTableRowElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLTableRowElement::nsHTMLTableRowElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLTableRowElement::~nsHTMLTableRowElement()
{
}

NS_IMPL_ADDREF(nsHTMLTableRowElement)

NS_IMPL_RELEASE(nsHTMLTableRowElement)

nsresult
nsHTMLTableRowElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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
nsHTMLTableRowElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLTableRowElement* it = new nsHTMLTableRowElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetRowIndex(PRInt32* aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::SetRowIndex(PRInt32 aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetSectionRowIndex(PRInt32* aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::SetSectionRowIndex(PRInt32 aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetCells(nsIDOMHTMLCollection** aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::SetCells(nsIDOMHTMLCollection* aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::DeleteCell(PRInt32 aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, Ch, ch)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, ChOff, choff)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, VAlign, valign)


NS_IMETHODIMP
nsHTMLTableRowElement::StringToAttribute(nsIAtom* aAttribute,
                                  const nsString& aValue,
                                  nsHTMLValue& aResult)
{
  /* ignore these attributes, stored simply as strings
     ch
   */
  /* attributes that resolve to integers with default=0*/
  if (aAttribute == nsHTMLAtoms::choff) {
    nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  /* attributes that resolve to integers or percents */
  else if (aAttribute == nsHTMLAtoms::height) {
    nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  /* attributes that resolve to integers or percents or proportions */
  else if (aAttribute == nsHTMLAtoms::width) {
    nsGenericHTMLElement::ParseValueOrPercentOrProportional(aValue, aResult, eHTMLUnit_Pixel);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  /* other attributes */
  else if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::ParseTableHAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::background) {
    nsAutoString href(aValue);
    href.StripWhitespace();
    aResult.SetStringValue(href);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) {
    nsGenericHTMLElement::ParseColor(aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (nsGenericHTMLElement::ParseTableVAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableRowElement::AttributeToString(nsIAtom* aAttribute,
                                  nsHTMLValue& aValue,
                                  nsString& aResult) const
{
  /* ignore these attributes, stored already as strings
     ch
   */
  /* ignore attributes that are of standard types
     choff, height, width, background, bgcolor
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::TableHAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (nsGenericHTMLElement::TableVAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  if (nsnull!=aAttributes)
  {
    nsHTMLValue value;
    nsHTMLValue widthValue;
    nsStyleText* textStyle = nsnull;

    // align: enum
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) 
    {
      textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mTextAlign = value.GetIntValue();
    }
  
    // valign: enum
    aAttributes->GetAttribute(nsHTMLAtoms::valign, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) 
    {
      if (nsnull==textStyle)
        textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mVerticalAlign.SetIntValue(value.GetIntValue(), eStyleUnit_Enumerated);
    }

    // height: pixel
    aAttributes->GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      float p2t;
      aPresContext->GetScaledPixelsToTwips(p2t);
      nsStylePosition* pos = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mHeight.SetCoordValue(twips);
    }
    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
    nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
  }
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableRowElement::HandleDOMEvent(nsIPresContext& aPresContext,
                               nsEvent* aEvent,
                               nsIDOMEvent** aDOMEvent,
                               PRUint32 aFlags,
                               nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetStyleHintForAttributeChange(
    const nsIContent * aNode,
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  if (PR_TRUE == nsGenericHTMLElement::SetStyleHintForCommonAttributes(aNode, 
    aAttribute, aHint)) {
    // Do nothing
  }
  else {
    *aHint = NS_STYLE_HINT_REFLOW;
  }
  return NS_OK;
}