/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLIElement.h"
#include "mozilla/dom/HTMLLIElementBinding.h"

#include "mozilla/MappedDeclarationsBuilder.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(LI)

namespace mozilla::dom {

HTMLLIElement::~HTMLLIElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLLIElement)

// https://html.spec.whatwg.org/#lists
const nsAttrValue::EnumTable HTMLLIElement::kULTypeTable[] = {
    {"none", ListStyle::None},
    {"disc", ListStyle::Disc},
    {"circle", ListStyle::Circle},
    {"square", ListStyle::Square},
    {nullptr, 0}};

// https://html.spec.whatwg.org/#lists
const nsAttrValue::EnumTable HTMLLIElement::kOLTypeTable[] = {
    {"A", ListStyle::UpperAlpha}, {"a", ListStyle::LowerAlpha},
    {"I", ListStyle::UpperRoman}, {"i", ListStyle::LowerRoman},
    {"1", ListStyle::Decimal},    {nullptr, 0}};

bool HTMLLIElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsIPrincipal* aMaybeScriptedPrincipal,
                                   nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, kOLTypeTable, true) ||
             aResult.ParseEnumValue(aValue, kULTypeTable, false);
    }
    if (aAttribute == nsGkAtoms::value) {
      return aResult.ParseIntValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLLIElement::MapAttributesIntoRule(MappedDeclarationsBuilder& aBuilder) {
  if (!aBuilder.PropertyIsSet(eCSSProperty_list_style_type)) {
    // type: enum
    const nsAttrValue* value = aBuilder.GetAttr(nsGkAtoms::type);
    if (value && value->Type() == nsAttrValue::eEnum) {
      aBuilder.SetKeywordValue(eCSSProperty_list_style_type,
                               value->GetEnumValue());
    }
  }

  // Map <li value=INTEGER> to 'counter-set: list-item INTEGER'.
  const nsAttrValue* attrVal = aBuilder.GetAttr(nsGkAtoms::value);
  if (attrVal && attrVal->Type() == nsAttrValue::eInteger) {
    if (!aBuilder.PropertyIsSet(eCSSProperty_counter_set)) {
      aBuilder.SetCounterSetListItem(attrVal->GetIntegerValue());
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aBuilder);
}

NS_IMETHODIMP_(bool)
HTMLLIElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {
      {nsGkAtoms::type},
      {nsGkAtoms::value},
      {nullptr},
  };

  static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLLIElement::GetAttributeMappingFunction() const {
  return &MapAttributesIntoRule;
}

JSObject* HTMLLIElement::WrapNode(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return HTMLLIElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
