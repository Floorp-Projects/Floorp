/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLFontElement.h"
#include "mozilla/dom/HTMLFontElementBinding.h"
#include "mozilla/MappedDeclarations.h"
#include "nsAttrValueInlines.h"
#include "nsMappedAttributes.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Font)

namespace mozilla {
namespace dom {

HTMLFontElement::~HTMLFontElement()
{
}

JSObject*
HTMLFontElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLFontElement_Binding::Wrap(aCx, this, aGivenProto);
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
                                       MappedDeclarations& aDecls)
{
  // face: string list
  if (!aDecls.PropertyIsSet(eCSSProperty_font_family)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::face);
    if (value && value->Type() == nsAttrValue::eString &&
        !value->IsEmptyString()) {
      aDecls.SetFontFamily(value->GetStringValue());
    }
  }
  // size: int
  if (!aDecls.PropertyIsSet(eCSSProperty_font_size)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::size);
    if (value && value->Type() == nsAttrValue::eInteger)
      aDecls.SetKeywordValue(eCSSProperty_font_size, value->GetIntegerValue());
  }
  if (!aDecls.PropertyIsSet(eCSSProperty_color)) {
    // color: color
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::color);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      aDecls.SetColorValue(eCSSProperty_color, color);
    }
  }
  if (aDecls.Document()->GetCompatibilityMode() == eCompatibility_NavQuirks) {
    // Make <a><font color="red">text</font></a> give the text a red underline
    // in quirks mode.  The NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL flag only
    // affects quirks mode rendering.
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::color);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      aDecls.SetTextDecorationColorOverride();
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
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
