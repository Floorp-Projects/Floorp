/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLIElement.h"
#include "mozilla/dom/HTMLLIElementBinding.h"

#include "mozilla/MappedDeclarations.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(LI)

namespace mozilla {
namespace dom {

HTMLLIElement::~HTMLLIElement() {}

NS_IMPL_ELEMENT_CLONE(HTMLLIElement)

// values that are handled case-insensitively
static const nsAttrValue::EnumTable kUnorderedListItemTypeTable[] = {
    {"disc", NS_STYLE_LIST_STYLE_DISC},
    {"circle", NS_STYLE_LIST_STYLE_CIRCLE},
    {"round", NS_STYLE_LIST_STYLE_CIRCLE},
    {"square", NS_STYLE_LIST_STYLE_SQUARE},
    {nullptr, 0}};

// values that are handled case-sensitively
static const nsAttrValue::EnumTable kOrderedListItemTypeTable[] = {
    {"A", NS_STYLE_LIST_STYLE_UPPER_ALPHA},
    {"a", NS_STYLE_LIST_STYLE_LOWER_ALPHA},
    {"I", NS_STYLE_LIST_STYLE_UPPER_ROMAN},
    {"i", NS_STYLE_LIST_STYLE_LOWER_ROMAN},
    {"1", NS_STYLE_LIST_STYLE_DECIMAL},
    {nullptr, 0}};

bool HTMLLIElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsIPrincipal* aMaybeScriptedPrincipal,
                                   nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, kOrderedListItemTypeTable, true) ||
             aResult.ParseEnumValue(aValue, kUnorderedListItemTypeTable, false);
    }
    if (aAttribute == nsGkAtoms::value) {
      return aResult.ParseIntValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLLIElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                          MappedDeclarations& aDecls) {
  if (!aDecls.PropertyIsSet(eCSSProperty_list_style_type)) {
    // type: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
    if (value && value->Type() == nsAttrValue::eEnum)
      aDecls.SetKeywordValue(eCSSProperty_list_style_type,
                             value->GetEnumValue());
  }

  // Map <li value=INTEGER> to 'counter-set: list-item INTEGER'.
  const nsAttrValue* attrVal = aAttributes->GetAttr(nsGkAtoms::value);
  if (attrVal && attrVal->Type() == nsAttrValue::eInteger) {
    if (!aDecls.PropertyIsSet(eCSSProperty_counter_set)) {
      aDecls.SetCounterSetListItem(attrVal->GetIntegerValue());
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
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

}  // namespace dom
}  // namespace mozilla
