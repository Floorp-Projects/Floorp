/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTableCaptionElement.h"

#include "mozilla/MappedDeclarations.h"
#include "nsAttrValueInlines.h"
#include "nsMappedAttributes.h"
#include "nsStyleConsts.h"
#include "mozilla/dom/HTMLTableCaptionElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableCaption)

namespace mozilla {
namespace dom {

HTMLTableCaptionElement::~HTMLTableCaptionElement() = default;

JSObject* HTMLTableCaptionElement::WrapNode(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return HTMLTableCaptionElement_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ELEMENT_CLONE(HTMLTableCaptionElement)

static const nsAttrValue::EnumTable kCaptionAlignTable[] = {
    {"left", NS_STYLE_CAPTION_SIDE_LEFT},
    {"right", NS_STYLE_CAPTION_SIDE_RIGHT},
    {"top", NS_STYLE_CAPTION_SIDE_TOP},
    {"bottom", NS_STYLE_CAPTION_SIDE_BOTTOM},
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
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  if (!aDecls.PropertyIsSet(eCSSProperty_caption_side)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum)
      aDecls.SetKeywordValue(eCSSProperty_caption_side, value->GetEnumValue());
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
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

}  // namespace dom
}  // namespace mozilla
