/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLHRElement.h"
#include "mozilla/dom/HTMLHRElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(HR)

namespace mozilla {
namespace dom {

HTMLHRElement::HTMLHRElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLHRElement::~HTMLHRElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLHRElement, nsGenericHTMLElement,
                            nsIDOMHTMLHRElement)

NS_IMPL_ELEMENT_CLONE(HTMLHRElement)


bool
HTMLHRElement::ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult)
{
  static const nsAttrValue::EnumTable kAlignTable[] = {
    { "left", NS_STYLE_TEXT_ALIGN_LEFT },
    { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
    { "center", NS_STYLE_TEXT_ALIGN_CENTER },
    { nullptr, 0 }
  };

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

void
HTMLHRElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                     GenericSpecifiedValues* aData)
{
  bool noshade = false;

  const nsAttrValue* colorValue = aAttributes->GetAttr(nsGkAtoms::color);
  nscolor color;
  bool colorIsSet = colorValue && colorValue->GetColorValue(color);

  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Position) |
                                      NS_STYLE_INHERIT_BIT(Border))) {
    if (colorIsSet) {
      noshade = true;
    } else {
      noshade = !!aAttributes->GetAttr(nsGkAtoms::noshade);
    }
  }

  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Margin))) {
    // align: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum) {
      // Map align attribute into auto side margins
      switch (value->GetEnumValue()) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        aData->SetPixelValueIfUnset(eCSSProperty_margin_left, 0.0f);
        aData->SetAutoValueIfUnset(eCSSProperty_margin_right);
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        aData->SetAutoValueIfUnset(eCSSProperty_margin_left);
        aData->SetPixelValueIfUnset(eCSSProperty_margin_right, 0.0f);
        break;
      case NS_STYLE_TEXT_ALIGN_CENTER:
        aData->SetAutoValueIfUnset(eCSSProperty_margin_left);
        aData->SetAutoValueIfUnset(eCSSProperty_margin_right);
        break;
      }
    }
  }
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Position))) {
    if (!aData->PropertyIsSet(eCSSProperty_height)) {
      // size: integer
      if (noshade) {
        // noshade case: size is set using the border
        aData->SetAutoValue(eCSSProperty_height);
      } else {
        // normal case
        // the height includes the top and bottom borders that are initially 1px.
        // for size=1, html.css has a special case rule that makes this work by
        // removing all but the top border.
        const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::size);
        if (value && value->Type() == nsAttrValue::eInteger) {
          aData->SetPixelValue(eCSSProperty_height, (float)value->GetIntegerValue());
        } // else use default value from html.css
      }
    }
  }

  // if not noshade, border styles are dealt with by html.css
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Border)) && noshade) {
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
    aData->SetPixelValueIfUnset(eCSSProperty_border_top_width, sizePerSide);
    if (allSides) {
      aData->SetPixelValueIfUnset(eCSSProperty_border_right_width, sizePerSide);
      aData->SetPixelValueIfUnset(eCSSProperty_border_bottom_width, sizePerSide);
      aData->SetPixelValueIfUnset(eCSSProperty_border_left_width, sizePerSide);
    }

    if (!aData->PropertyIsSet(eCSSProperty_border_top_style))
      aData->SetKeywordValue(eCSSProperty_border_top_style,
                             NS_STYLE_BORDER_STYLE_SOLID);
    if (allSides) {
      aData->SetKeywordValueIfUnset(eCSSProperty_border_right_style,
                                    NS_STYLE_BORDER_STYLE_SOLID);
      aData->SetKeywordValueIfUnset(eCSSProperty_border_bottom_style,
                                    NS_STYLE_BORDER_STYLE_SOLID);
      aData->SetKeywordValueIfUnset(eCSSProperty_border_left_style,
                                    NS_STYLE_BORDER_STYLE_SOLID);

      // If it would be noticeable, set the border radius to
      // 10000px on all corners; this triggers the clamping to make
      // circular ends.  This assumes the <hr> isn't larger than
      // that in *both* dimensions.
      for (const nsCSSPropertyID* props =
            nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_radius);
           *props != eCSSProperty_UNKNOWN; ++props) {
        aData->SetPixelValueIfUnset(*props, 10000.0f);
      }
    }
  }
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Color))) {
    // color: a color
    // (we got the color attribute earlier)
    if (colorIsSet &&
        aData->PresContext()->UseDocumentColors()) {
      aData->SetColorValueIfUnset(eCSSProperty_color, color);
    }
  }

  nsGenericHTMLElement::MapWidthAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLHRElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { &nsGkAtoms::width },
    { &nsGkAtoms::size },
    { &nsGkAtoms::color },
    { &nsGkAtoms::noshade },
    { nullptr },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLHRElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

JSObject*
HTMLHRElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLHRElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
