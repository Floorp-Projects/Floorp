/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTableCaptionElement.h"

#include "mozilla/MappedDeclarationsBuilder.h"
#include "nsAttrValueInlines.h"
#include "nsStyleConsts.h"
#include "mozilla/dom/HTMLTableCaptionElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableCaption)

namespace mozilla::dom {

HTMLTableCaptionElement::~HTMLTableCaptionElement() = default;

JSObject* HTMLTableCaptionElement::WrapNode(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return HTMLTableCaptionElement_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ELEMENT_CLONE(HTMLTableCaptionElement)

static const nsAttrValue::EnumTable kCaptionAlignTable[] = {
    {"top", StyleCaptionSide::Top},
    {"bottom", StyleCaptionSide::Bottom},
    {nullptr, 0}};

bool HTMLTableCaptionElement::ParseAttribute(
    int32_t aNamespaceID, nsAtom* aAttribute, const nsAString& aValue,
    nsIPrincipal* aMaybeScriptedPrincipal, nsAttrValue& aResult) {
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return aResult.ParseEnumValue(aValue, kCaptionAlignTable, false);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLTableCaptionElement::MapAttributesIntoRule(
    MappedDeclarationsBuilder& aBuilder) {
  if (!aBuilder.PropertyIsSet(eCSSProperty_caption_side)) {
    const nsAttrValue* value = aBuilder.GetAttr(nsGkAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum) {
      aBuilder.SetKeywordValue(eCSSProperty_caption_side,
                               value->GetEnumValue());
    }
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aBuilder);
}

NS_IMETHODIMP_(bool)
HTMLTableCaptionElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {{nsGkAtoms::align},
                                                    {nullptr}};

  static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLTableCaptionElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

}  // namespace mozilla::dom
