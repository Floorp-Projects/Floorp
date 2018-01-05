/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLFontElement.h"
#include "mozilla/dom/HTMLFontElementBinding.h"
#include "mozilla/GenericSpecifiedValuesInlines.h"
#include "nsAttrValueInlines.h"
#include "nsMappedAttributes.h"
#include "nsContentUtils.h"
#include "nsCSSParser.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Font)

namespace mozilla {
namespace dom {

HTMLFontElement::~HTMLFontElement()
{
}

JSObject*
HTMLFontElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLFontElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ELEMENT_CLONE(HTMLFontElement)

bool
HTMLFontElement::ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::size) {
      int32_t size = nsContentUtils::ParseLegacyFontSize(aValue);
      if (size) {
        aResult.SetTo(size, &aValue);
        return true;
      }
      return false;
    }
    if (aAttribute == nsGkAtoms::color) {
      return aResult.ParseColor(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLFontElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                       GenericSpecifiedValues* aData)
{
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Font))) {
    // face: string list
    if (!aData->PropertyIsSet(eCSSProperty_font_family)) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::face);
      if (value && value->Type() == nsAttrValue::eString &&
          !value->IsEmptyString()) {
        aData->SetFontFamily(value->GetStringValue());
      }
    }
    // size: int
    if (!aData->PropertyIsSet(eCSSProperty_font_size)) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::size);
      if (value && value->Type() == nsAttrValue::eInteger)
        aData->SetKeywordValue(eCSSProperty_font_size, value->GetIntegerValue());
    }
  }
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Color))) {
    if (!aData->PropertyIsSet(eCSSProperty_color) &&
        !aData->ShouldIgnoreColors()) {
      // color: color
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::color);
      nscolor color;
      if (value && value->GetColorValue(color)) {
        aData->SetColorValue(eCSSProperty_color, color);
      }
    }
  }
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(TextReset)) &&
      aData->Document()->GetCompatibilityMode() == eCompatibility_NavQuirks) {
    // Make <a><font color="red">text</font></a> give the text a red underline
    // in quirks mode.  The NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL flag only
    // affects quirks mode rendering.
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::color);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      aData->SetTextDecorationColorOverride();
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLFontElement::IsAttributeMapped(const nsAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::face },
    { &nsGkAtoms::size },
    { &nsGkAtoms::color },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLFontElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
