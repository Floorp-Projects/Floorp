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
#include "nsIDOMHTMLHRElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLHRElementIID, NS_IDOMHTMLHRELEMENT_IID);

class nsHTMLHR : public nsIDOMHTMLHRElement,
                 public nsIScriptObjectOwner,
                 public nsIDOMEventReceiver,
                 public nsIHTMLContent
{
public:
  nsHTMLHR(nsIAtom* aTag);
  ~nsHTMLHR();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLHRElement
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetNoShade(PRBool* aNoShade);
  NS_IMETHOD SetNoShade(PRBool aNoShade);
  NS_IMETHOD GetSize(nsString& aSize);
  NS_IMETHOD SetSize(const nsString& aSize);
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
  nsHTMLGenericLeafContent mInner;
};

nsresult
NS_NewHTMLHR(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLHR(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLHR::nsHTMLHR(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLHR::~nsHTMLHR()
{
}

NS_IMPL_ADDREF(nsHTMLHR)

NS_IMPL_RELEASE(nsHTMLHR)

nsresult
nsHTMLHR::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLHRElementIID)) {
    nsIDOMHTMLHRElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLHR::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLHR* it = new nsHTMLHR(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLHR::GetAlign(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::align, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLHR::SetAlign(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::align, aValue, eSetAttrNotify_Reflow);
}

NS_IMETHODIMP
nsHTMLHR::GetNoShade(PRBool* aValue)
{
  nsHTMLValue val;
  nsresult rv = mInner.GetAttribute(nsHTMLAtoms::noshade, val);
  *aValue = NS_CONTENT_ATTR_NOT_THERE != rv;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLHR::SetNoShade(PRBool aValue)
{
  nsAutoString empty;
  if (aValue) {
    return mInner.SetAttr(nsHTMLAtoms::noshade, empty, eSetAttrNotify_Render);
  }
  else {
    mInner.UnsetAttribute(nsHTMLAtoms::noshade);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsHTMLHR::GetSize(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::size, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLHR::SetSize(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::size, aValue, eSetAttrNotify_Reflow);
}

NS_IMETHODIMP
nsHTMLHR::GetWidth(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::width, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLHR::SetWidth(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::width, aValue, eSetAttrNotify_Reflow);
}

static nsHTMLGenericContent::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

NS_IMETHODIMP
nsHTMLHR::StringToAttribute(nsIAtom* aAttribute,
                            const nsString& aValue,
                            nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::width) {
    nsHTMLGenericContent::ParseValueOrPercent(aValue, aResult,
                                              eHTMLUnit_Pixel);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    nsHTMLGenericContent::ParseValue(aValue, 1, 100, aResult, eHTMLUnit_Pixel);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::noshade) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (nsHTMLGenericContent::ParseEnumValue(aValue, kAlignTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLHR::AttributeToString(nsIAtom* aAttribute,
                            nsHTMLValue& aValue,
                            nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsHTMLGenericContent::EnumValueToString(aValue, kAlignTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLHR::MapAttributesInto(nsIStyleContext* aContext,
                            nsIPresContext* aPresContext)
{
  if (nsnull != mInner.mAttributes) {
    nsHTMLValue value;
    // align: enum
    GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      text->mTextAlign = value.GetIntValue();
    }

    // width: pixel, percent
    float p2t = aPresContext->GetPixelsToTwips();
    nsStylePosition* pos = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);
    GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mWidth.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mWidth.SetPercentValue(value.GetPercentValue());
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLHR::HandleDOMEvent(nsIPresContext& aPresContext,
                         nsEvent* aEvent,
                         nsIDOMEvent** aDOMEvent,
                         PRUint32 aFlags,
                         nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
