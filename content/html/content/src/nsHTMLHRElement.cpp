/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLHRElement.h"
#include "nsIDOMNSHTMLHRElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"

class nsHTMLHRElement : public nsGenericHTMLElement,
                        public nsIDOMHTMLHRElement,
                        public nsIDOMNSHTMLHRElement
{
public:
  nsHTMLHRElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLHRElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLHRElement
  NS_DECL_NSIDOMHTMLHRELEMENT

  // nsIDOMNSHTMLHRElement
  NS_DECL_NSIDOMNSHTMLHRELEMENT

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(HR)


nsHTMLHRElement::nsHTMLHRElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLHRElement::~nsHTMLHRElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLHRElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLHRElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLHRElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLHRElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLHRElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLHRElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLHRElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(HR)


NS_IMPL_STRING_ATTR(nsHTMLHRElement, Align, align)
NS_IMPL_BOOL_ATTR(nsHTMLHRElement, NoShade, noshade)
NS_IMPL_STRING_ATTR(nsHTMLHRElement, Size, size)
NS_IMPL_STRING_ATTR(nsHTMLHRElement, Width, width)
NS_IMPL_STRING_ATTR(nsHTMLHRElement, Color, color)

static const nsHTMLValue::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

PRBool
nsHTMLHRElement::ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::width) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::size) {
    return aResult.ParseIntWithBounds(aValue, 1, 1000);
  }
  if (aAttribute == nsHTMLAtoms::align) {
    return aResult.ParseEnumValue(aValue, kAlignTable);
  }
  if (aAttribute == nsHTMLAtoms::color) {
    return aResult.ParseColor(aValue, GetOwnerDoc());
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLHRElement::AttributeToString(nsIAtom* aAttribute,
                                   const nsHTMLValue& aValue,
                                   nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      aValue.EnumValueToString(kAlignTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  PRBool noshade = PR_FALSE;

  const nsAttrValue* colorValue = aAttributes->GetAttr(nsHTMLAtoms::color);
  nscolor color;
  PRBool colorIsSet = colorValue && colorValue->GetColorValue(color);

  if (aData->mSID == eStyleStruct_Position ||
      aData->mSID == eStyleStruct_Border) {
    if (colorIsSet) {
      noshade = PR_TRUE;
    } else {
      noshade = !!aAttributes->GetAttr(nsHTMLAtoms::noshade);
    }
  }

  if (aData->mSID == eStyleStruct_Margin) {
    // align: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum) {
      // Map align attribute into auto side margins
      nsCSSRect& margin = aData->mMarginData->mMargin;
      switch (value->GetEnumValue()) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        if (margin.mLeft.GetUnit() == eCSSUnit_Null)
          margin.mLeft.SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (margin.mRight.GetUnit() == eCSSUnit_Null)
          margin.mRight.SetAutoValue();
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        if (margin.mLeft.GetUnit() == eCSSUnit_Null)
          margin.mLeft.SetAutoValue();
        if (margin.mRight.GetUnit() == eCSSUnit_Null)
          margin.mRight.SetFloatValue(0.0f, eCSSUnit_Pixel);
        break;
      case NS_STYLE_TEXT_ALIGN_CENTER:
        if (margin.mLeft.GetUnit() == eCSSUnit_Null)
          margin.mLeft.SetAutoValue();
        if (margin.mRight.GetUnit() == eCSSUnit_Null)
          margin.mRight.SetAutoValue();
        break;
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Position) {
    // width: integer, percent
    if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::width);
      if (value && value->Type() == nsAttrValue::eInteger) {
        aData->mPositionData->mWidth.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      } else if (value && value->Type() == nsAttrValue::ePercent) {
        aData->mPositionData->mWidth.SetPercentValue(value->GetPercentValue());
      }
    }

    if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
      // size: integer
      if (noshade) {
        // noshade case: size is set using the border
        aData->mPositionData->mHeight.SetAutoValue();
      } else {
        // normal case
        // the height includes the top and bottom borders that are initially 1px.
        // for size=1, html.css has a special case rule that makes this work by
        // removing all but the top border.
        const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::size);
        if (value && value->Type() == nsAttrValue::eInteger) {
          aData->mPositionData->mHeight.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
        } // else use default value from html.css
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Border && noshade) { // if not noshade, border styles are dealt with by html.css
    // size: integer
    // if a size is set, use half of it per side, otherwise, use 1px per side
    float sizePerSide;
    PRBool allSides = PR_TRUE;
    const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::size);
    if (value && value->Type() == nsAttrValue::eInteger) {
      sizePerSide = (float)value->GetIntegerValue() / 2.0f;
      if (sizePerSide < 1.0f) {
        // XXX When the pixel bug is fixed, all the special casing for
        // subpixel borders should be removed.
        // In the meantime, this makes http://www.microsoft.com/ look right.
        sizePerSide = 1.0f;
        allSides = PR_FALSE;
      }
    } else {
      sizePerSide = 1.0f; // default to a 2px high line
    }
    nsCSSRect& borderWidth = aData->mMarginData->mBorderWidth;
    if (borderWidth.mTop.GetUnit() == eCSSUnit_Null) {
      borderWidth.mTop.SetFloatValue(sizePerSide, eCSSUnit_Pixel);
    }
    if (allSides) {
      if (borderWidth.mRight.GetUnit() == eCSSUnit_Null) {
        borderWidth.mRight.SetFloatValue(sizePerSide, eCSSUnit_Pixel);
      }
      if (borderWidth.mBottom.GetUnit() == eCSSUnit_Null) {
        borderWidth.mBottom.SetFloatValue(sizePerSide, eCSSUnit_Pixel);
      }
      if (borderWidth.mLeft.GetUnit() == eCSSUnit_Null) {
        borderWidth.mLeft.SetFloatValue(sizePerSide, eCSSUnit_Pixel);
      }
    }

    // if a color is set, set the border-style to 'solid' so that the
    // 'color' property takes effect, otherwise, use '-moz-bg-solid'.
    // (we got the color attribute earlier)
    PRInt32 style = colorIsSet ? NS_STYLE_BORDER_STYLE_SOLID :
                                 NS_STYLE_BORDER_STYLE_BG_SOLID;

    nsCSSRect& borderStyle = aData->mMarginData->mBorderStyle;
    if (borderStyle.mTop.GetUnit() == eCSSUnit_Null) {
      borderStyle.mTop.SetIntValue(style, eCSSUnit_Enumerated);
    }
    if (allSides) {
      if (borderStyle.mRight.GetUnit() == eCSSUnit_Null) {
        borderStyle.mRight.SetIntValue(style, eCSSUnit_Enumerated);
      }
      if (borderStyle.mBottom.GetUnit() == eCSSUnit_Null) {
        borderStyle.mBottom.SetIntValue(style, eCSSUnit_Enumerated);
      }
      if (borderStyle.mLeft.GetUnit() == eCSSUnit_Null) {
        borderStyle.mLeft.SetIntValue(style, eCSSUnit_Enumerated);
      }

      // If it would be noticeable, set the border radius to
      // 100% on all corners
      nsCSSRect& borderRadius = aData->mMarginData->mBorderRadius;
      if (borderRadius.mTop.GetUnit() == eCSSUnit_Null) {
        borderRadius.mTop.SetPercentValue(1.0f);
      }
      if (borderRadius.mRight.GetUnit() == eCSSUnit_Null) {
        borderRadius.mRight.SetPercentValue(1.0f);
      }
      if (borderRadius.mBottom.GetUnit() == eCSSUnit_Null) {
        borderRadius.mBottom.SetPercentValue(1.0f);
      }
      if (borderRadius.mLeft.GetUnit() == eCSSUnit_Null) {
        borderRadius.mLeft.SetPercentValue(1.0f);
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Color) {
    // color: a color
    // (we got the color attribute earlier)
    if (colorIsSet &&
        aData->mColorData->mColor.GetUnit() == eCSSUnit_Null) {
      aData->mColorData->mColor.SetColorValue(color);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLHRElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsHTMLAtoms::align },
    { &nsHTMLAtoms::width },
    { &nsHTMLAtoms::size },
    { &nsHTMLAtoms::color },
    { &nsHTMLAtoms::noshade },
    { nsnull },
  };
  
  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}


NS_IMETHODIMP
nsHTMLHRElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}
