/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLDivElement.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleConsts.h"
#include "mozilla/dom/HTMLDivElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Div)

namespace mozilla::dom {

HTMLDivElement::~HTMLDivElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLDivElement)

JSObject* HTMLDivElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return dom::HTMLDivElement_Binding::Wrap(aCx, this, aGivenProto);
}

bool HTMLDivElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsIPrincipal* aMaybeScriptedPrincipal,
                                    nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::align) {
    return ParseDivAlignValue(aValue, aResult);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLDivElement::MapAttributesIntoRule(
    MappedDeclarationsBuilder& aBuilder) {
  MapDivAlignAttributeInto(aBuilder);
  MapCommonAttributesInto(aBuilder);
}

NS_IMETHODIMP_(bool)
HTMLDivElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry* const map[] = {sDivAlignAttributeMap,
                                                    sCommonAttributeMap};
  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLDivElement::GetAttributeMappingFunction() const {
  return &MapAttributesIntoRule;
}

}  // namespace mozilla::dom
