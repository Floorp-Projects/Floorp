/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLHRElement.h"
#include "mozilla/dom/HTMLHRElementBinding.h"

#include "nsCSSProps.h"
#include "nsStyleConsts.h"
#include "mozilla/MappedDeclarations.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(HR)

namespace mozilla::dom {

HTMLHRElement::HTMLHRElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLHRElement::~HTMLHRElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLHRElement)

bool HTMLHRElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsIPrincipal* aMaybeScriptedPrincipal,
                                   nsAttrValue& aResult) {
  static const nsAttrValue::EnumTable kAlignTable[] = {
      {"left", StyleTextAlign::Left},
      {"right", StyleTextAlign::Right},
      {"center", StyleTextAlign::Center},
      {nullptr, 0}};

  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseHTMLDimension(aValue);
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
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLHRElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                          MappedDeclarations& aDecls) {
  bool noshade = false;

  const nsAttrValue* colorValue = aAttributes->GetAttr(nsGkAtoms::color);
  nscolor color;
  bool colorIsSet = colorValue && colorValue->GetColorValue(color);

  if (colorIsSet) {
    noshade = true;
  } else {
    noshade = !!aAttributes->GetAttr(nsGkAtoms::noshade);
  }

  // align: enum
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
  if (value && value->Type() == nsAttrValue::eEnum) {
    // Map align attribute into auto side margins
    switch (StyleTextAlign(value->GetEnumValue())) {
      case StyleTextAlign::Left:
        aDecls.SetPixelValueIfUnset(eCSSProperty_margin_left, 0.0f);
        aDecls.SetAutoValueIfUnset(eCSSProperty_margin_right);
        break;
      case StyleTextAlign::Right:
        aDecls.SetAutoValueIfUnset(eCSSProperty_margin_left);
        aDecls.SetPixelValueIfUnset(eCSSProperty_margin_right, 0.0f);
        break;
      case StyleTextAlign::Center:
        aDecls.SetAutoValueIfUnset(eCSSProperty_margin_left);
        aDecls.SetAutoValueIfUnset(eCSSProperty_margin_right);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown <hr align> value");
        break;
    }
  }
  if (!aDecls.PropertyIsSet(eCSSProperty_height)) {
    // size: integer
    if (noshade) {
      // noshade case: size is set using the border
      aDecls.SetAutoValue(eCSSProperty_height);
    } else {
      // normal case
      // the height includes the top and bottom borders that are initially 1px.
      // for size=1, html.css has a special case rule that makes this work by
      // removing all but the top border.
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::size);
      if (value && value->Type() == nsAttrValue::eInteger) {
        aDecls.SetPixelValue(eCSSProperty_height,
                             (float)value->GetIntegerValue());
      }  // else use default value from html.css
    }
  }

  // if not noshade, border styles are dealt with by html.css
  if (noshade) {
    // size: integer
    // if a size is set, use half of it per side, otherwise, use 1px per side
    float sizePerSide;
    bool allSides = true;
    value = aAttributes->GetAttr(nsGkAtoms::size);
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
      sizePerSide = 1.0f;  // default to a 2px high line
    }
    aDecls.SetPixelValueIfUnset(eCSSProperty_border_top_width, sizePerSide);
    if (allSides) {
      aDecls.SetPixelValueIfUnset(eCSSProperty_border_right_width, sizePerSide);
      aDecls.SetPixelValueIfUnset(eCSSProperty_border_bottom_width,
                                  sizePerSide);
      aDecls.SetPixelValueIfUnset(eCSSProperty_border_left_width, sizePerSide);
    }

    if (!aDecls.PropertyIsSet(eCSSProperty_border_top_style))
      aDecls.SetKeywordValue(eCSSProperty_border_top_style,
                             StyleBorderStyle::Solid);
    if (allSides) {
      aDecls.SetKeywordValueIfUnset(eCSSProperty_border_right_style,
                                    StyleBorderStyle::Solid);
      aDecls.SetKeywordValueIfUnset(eCSSProperty_border_bottom_style,
                                    StyleBorderStyle::Solid);
      aDecls.SetKeywordValueIfUnset(eCSSProperty_border_left_style,
                                    StyleBorderStyle::Solid);

      // If it would be noticeable, set the border radius to
      // 10000px on all corners; this triggers the clamping to make
      // circular ends.  This assumes the <hr> isn't larger than
      // that in *both* dimensions.
      for (const nsCSSPropertyID* props =
               nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_radius);
           *props != eCSSProperty_UNKNOWN; ++props) {
        aDecls.SetPixelValueIfUnset(*props, 10000.0f);
      }
    }
  }
  // color: a color
  // (we got the color attribute earlier)
  if (colorIsSet) {
    aDecls.SetColorValueIfUnset(eCSSProperty_color, color);
  }

  nsGenericHTMLElement::MapWidthAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLHRElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {
      {nsGkAtoms::align}, {nsGkAtoms::width},   {nsGkAtoms::size},
      {nsGkAtoms::color}, {nsGkAtoms::noshade}, {nullptr},
  };

  static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLHRElement::GetAttributeMappingFunction() const {
  return &MapAttributesIntoRule;
}

JSObject* HTMLHRElement::WrapNode(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return HTMLHRElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
