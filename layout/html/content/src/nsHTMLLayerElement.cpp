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
#include "nsIDOMHTMLElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"

#define _I32_MIN  (-2147483647 - 1) /* minimum signed 32 bit value */

static NS_DEFINE_IID(kIDOMHTMLLayerElementIID, NS_IDOMHTMLELEMENT_IID);

class nsHTMLLayerElement : public nsIDOMHTMLElement,/* XXX need layer api */
                    public nsIScriptObjectOwner,
                    public nsIDOMEventReceiver,
                    public nsIHTMLContent
{
public:
  nsHTMLLayerElement(nsIAtom* aTag);
  virtual ~nsHTMLLayerElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLLayerElement
  NS_IMETHOD GetCite(nsString& aCite);
  NS_IMETHOD SetCite(const nsString& aCite);

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
NS_NewHTMLLayerElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLLayerElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLLayerElement::nsHTMLLayerElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLLayerElement::~nsHTMLLayerElement()
{
}

NS_IMPL_ADDREF(nsHTMLLayerElement)

NS_IMPL_RELEASE(nsHTMLLayerElement)

nsresult
nsHTMLLayerElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
#if XXX
  if (aIID.Equals(kIDOMHTMLLayerElementIID)) {
    nsIDOMHTMLLayerElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
#endif
  return NS_NOINTERFACE;
}

nsresult
nsHTMLLayerElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLLayerElement* it = new nsHTMLLayerElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLLayerElement, Cite, cite)

static nsGenericHTMLElement::EnumTable kVisibilityTable[] = {
  {"hide", NS_STYLE_VISIBILITY_HIDDEN},
  {"visible", NS_STYLE_VISIBILITY_VISIBLE},
  {0}
};

NS_IMETHODIMP
nsHTMLLayerElement::StringToAttribute(nsIAtom*        aAttribute,
                                      const nsString& aValue,
                                      nsHTMLValue&    aResult)
{
  // XXX CLIP
  if (aAttribute == nsHTMLAtoms::src) {
    nsAutoString src(aValue);
    src.StripWhitespace();
    aResult.SetStringValue(src);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if ((aAttribute == nsHTMLAtoms::left) ||
           (aAttribute == nsHTMLAtoms::top)) {
    if (nsGenericHTMLElement::ParseValue(aValue, _I32_MIN, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if ((aAttribute == nsHTMLAtoms::width) ||
           (aAttribute == nsHTMLAtoms::height)) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::zindex) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::visibility) {
    if (nsGenericHTMLElement::ParseEnumValue(aValue, kVisibilityTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) {
    if (nsGenericHTMLElement::ParseColor(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::background) {
    nsAutoString url(aValue);
    url.StripWhitespace();
    aResult.SetStringValue(url);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  // ABOVE, BELOW, OnMouseOver, OnMouseOut, OnFocus, OnBlur, OnLoad
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLLayerElement::AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::visibility) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::EnumValueToString(aValue, kVisibilityTable, aResult);
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
  // Note: ua.css specifies that the 'position' is absolute
  if (nsnull != aAttributes) {
    nsHTMLValue      value;
    float            p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nsStylePosition* position = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);

    // Left
    aAttributes->GetAttribute(nsHTMLAtoms::left, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      position->mOffset.SetLeft(nsStyleCoord(twips));
    }

    // Top
    aAttributes->GetAttribute(nsHTMLAtoms::top, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      position->mOffset.SetTop(nsStyleCoord(twips));
    }

    // Width
    aAttributes->GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      position->mWidth.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      position->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // Height
    aAttributes->GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      position->mHeight.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      position->mHeight.SetPercentValue(value.GetPercentValue());
    }

    // Z-index
    aAttributes->GetAttribute(nsHTMLAtoms::zindex, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      position->mZIndex.SetIntValue(value.GetIntValue(), eStyleUnit_Integer);
    }

    // Visibility
    aAttributes->GetAttribute(nsHTMLAtoms::visibility, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      nsStyleDisplay* display = (nsStyleDisplay*)
        aContext->GetMutableStyleData(eStyleStruct_Display);

      display->mVisible = value.GetIntValue() == NS_STYLE_VISIBILITY_VISIBLE;
    }

    // Background and bgcolor
    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLLayerElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLLayerElement::HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLLayerElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}
