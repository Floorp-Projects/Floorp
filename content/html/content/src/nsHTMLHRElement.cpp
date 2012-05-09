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

#include "mozilla/Util.h"

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLHRElement.h"

#include "nsIDOMEventTarget.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsCSSProps.h"

using namespace mozilla;

class nsHTMLHRElement : public nsGenericHTMLElement,
                        public nsIDOMHTMLHRElement
{
public:
  nsHTMLHRElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLHRElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLHRElement
  NS_DECL_NSIDOMHTMLHRELEMENT

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }
};


NS_IMPL_NS_NEW_HTML_ELEMENT(HR)


nsHTMLHRElement::nsHTMLHRElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLHRElement::~nsHTMLHRElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLHRElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLHRElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLHRElement, nsHTMLHRElement)

// QueryInterface implementation for nsHTMLHRElement
NS_INTERFACE_TABLE_HEAD(nsHTMLHRElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLHRElement,
                                   nsIDOMHTMLHRElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLHRElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLHRElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLHRElement)


NS_IMPL_STRING_ATTR(nsHTMLHRElement, Align, align)
NS_IMPL_BOOL_ATTR(nsHTMLHRElement, NoShade, noshade)
NS_IMPL_STRING_ATTR(nsHTMLHRElement, Size, size)
NS_IMPL_STRING_ATTR(nsHTMLHRElement, Width, width)
NS_IMPL_STRING_ATTR(nsHTMLHRElement, Color, color)

static const nsAttrValue::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

bool
nsHTMLHRElement::ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::size) {
      return aResult.ParseIntWithBounds(aValue, 1, 1000);
    }
    if (aAttribute == nsGkAtoms::align) {
      return aResult.ParseEnumValue(aValue, kAlignTable, false);
    }
    if (aAttribute == nsGkAtoms::color) {
      return aResult.ParseColor(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  bool noshade = false;

  const nsAttrValue* colorValue = aAttributes->GetAttr(nsGkAtoms::color);
  nscolor color;
  bool colorIsSet = colorValue && colorValue->GetColorValue(color);

  if (aData->mSIDs & (NS_STYLE_INHERIT_BIT(Position) |
                      NS_STYLE_INHERIT_BIT(Border))) {
    if (colorIsSet) {
      noshade = true;
    } else {
      noshade = !!aAttributes->GetAttr(nsGkAtoms::noshade);
    }
  }

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Margin)) {
    // align: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum) {
      // Map align attribute into auto side margins
      nsCSSValue* marginLeft = aData->ValueForMarginLeftValue();
      nsCSSValue* marginRight = aData->ValueForMarginRightValue();
      switch (value->GetEnumValue()) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetAutoValue();
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetAutoValue();
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetFloatValue(0.0f, eCSSUnit_Pixel);
        break;
      case NS_STYLE_TEXT_ALIGN_CENTER:
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetAutoValue();
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetAutoValue();
        break;
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    // width: integer, percent
    nsCSSValue* width = aData->ValueForWidth();
    if (width->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (value && value->Type() == nsAttrValue::eInteger) {
        width->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      } else if (value && value->Type() == nsAttrValue::ePercent) {
        width->SetPercentValue(value->GetPercentValue());
      }
    }

    nsCSSValue* height = aData->ValueForHeight();
    if (height->GetUnit() == eCSSUnit_Null) {
      // size: integer
      if (noshade) {
        // noshade case: size is set using the border
        height->SetAutoValue();
      } else {
        // normal case
        // the height includes the top and bottom borders that are initially 1px.
        // for size=1, html.css has a special case rule that makes this work by
        // removing all but the top border.
        const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::size);
        if (value && value->Type() == nsAttrValue::eInteger) {
          height->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
        } // else use default value from html.css
      }
    }
  }
  if ((aData->mSIDs & NS_STYLE_INHERIT_BIT(Border)) && noshade) { // if not noshade, border styles are dealt with by html.css
    // size: integer
    // if a size is set, use half of it per side, otherwise, use 1px per side
    float sizePerSide;
    bool allSides = true;
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::size);
    if (value && value->Type() == nsAttrValue::eInteger) {
      sizePerSide = (float)value->GetIntegerValue() / 2.0f;
      if (sizePerSide < 1.0f) {
        // XXX When the pixel bug is fixed, all the special casing for
        // subpixel borders should be removed.
        // In the meantime, this makes http://www.microsoft.com/ look right.
        sizePerSide = 1.0f;
        allSides = false;
      }
    } else {
      sizePerSide = 1.0f; // default to a 2px high line
    }
    nsCSSValue* borderTopWidth = aData->ValueForBorderTopWidth();
    if (borderTopWidth->GetUnit() == eCSSUnit_Null) {
      borderTopWidth->SetFloatValue(sizePerSide, eCSSUnit_Pixel);
    }
    if (allSides) {
      nsCSSValue* borderRightWidth = aData->ValueForBorderRightWidthValue();
      if (borderRightWidth->GetUnit() == eCSSUnit_Null) {
        borderRightWidth->SetFloatValue(sizePerSide, eCSSUnit_Pixel);
      }
      nsCSSValue* borderBottomWidth = aData->ValueForBorderBottomWidth();
      if (borderBottomWidth->GetUnit() == eCSSUnit_Null) {
        borderBottomWidth->SetFloatValue(sizePerSide, eCSSUnit_Pixel);
      }
      nsCSSValue* borderLeftWidth = aData->ValueForBorderLeftWidthValue();
      if (borderLeftWidth->GetUnit() == eCSSUnit_Null) {
        borderLeftWidth->SetFloatValue(sizePerSide, eCSSUnit_Pixel);
      }
    }

    nsCSSValue* borderTopStyle = aData->ValueForBorderTopStyle();
    if (borderTopStyle->GetUnit() == eCSSUnit_Null) {
      borderTopStyle->SetIntValue(NS_STYLE_BORDER_STYLE_SOLID,
                                  eCSSUnit_Enumerated);
    }
    if (allSides) {
      nsCSSValue* borderRightStyle = aData->ValueForBorderRightStyleValue();
      if (borderRightStyle->GetUnit() == eCSSUnit_Null) {
        borderRightStyle->SetIntValue(NS_STYLE_BORDER_STYLE_SOLID,
                                      eCSSUnit_Enumerated);
      }
      nsCSSValue* borderBottomStyle = aData->ValueForBorderBottomStyle();
      if (borderBottomStyle->GetUnit() == eCSSUnit_Null) {
        borderBottomStyle->SetIntValue(NS_STYLE_BORDER_STYLE_SOLID,
                                       eCSSUnit_Enumerated);
      }
      nsCSSValue* borderLeftStyle = aData->ValueForBorderLeftStyleValue();
      if (borderLeftStyle->GetUnit() == eCSSUnit_Null) {
        borderLeftStyle->SetIntValue(NS_STYLE_BORDER_STYLE_SOLID,
                                     eCSSUnit_Enumerated);
      }

      // If it would be noticeable, set the border radius to
      // 10000px on all corners; this triggers the clamping to make
      // circular ends.  This assumes the <hr> isn't larger than
      // that in *both* dimensions.
      for (const nsCSSProperty* props =
            nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_radius);
           *props != eCSSProperty_UNKNOWN; ++props) {
        nsCSSValue* dimen = aData->ValueFor(*props);
        if (dimen->GetUnit() == eCSSUnit_Null) {
          dimen->SetFloatValue(10000.0f, eCSSUnit_Pixel);
        }
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    // color: a color
    // (we got the color attribute earlier)
    nsCSSValue* colorValue = aData->ValueForColor();
    if (colorIsSet &&
        colorValue->GetUnit() == eCSSUnit_Null &&
        aData->mPresContext->UseDocumentColors()) {
      colorValue->SetColorValue(color);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLHRElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { &nsGkAtoms::width },
    { &nsGkAtoms::size },
    { &nsGkAtoms::color },
    { &nsGkAtoms::noshade },
    { nsnull },
  };
  
  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
nsHTMLHRElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}
