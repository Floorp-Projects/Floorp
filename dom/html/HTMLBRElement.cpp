/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/HTMLBRElementBinding.h"
#include "mozilla/GenericSpecifiedValuesInlines.h"
#include "nsAttrValueInlines.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"


NS_IMPL_NS_NEW_HTML_ELEMENT(BR)

namespace mozilla {
namespace dom {

HTMLBRElement::HTMLBRElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLBRElement::~HTMLBRElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLBRElement)

static const nsAttrValue::EnumTable kClearTable[] = {
  { "left", StyleClear::Left },
  { "right", StyleClear::Right },
  { "all", StyleClear::Both },
  { "both", StyleClear::Both },
  { nullptr, 0 }
};

bool
HTMLBRElement::ParseAttribute(int32_t aNamespaceID,
                              nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::clear && aNamespaceID == kNameSpaceID_None) {
    return aResult.ParseEnumValue(aValue, kClearTable, false);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLBRElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                     GenericSpecifiedValues* aData)
{
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Display))) {
    if (!aData->PropertyIsSet(eCSSProperty_clear)) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::clear);
      if (value && value->Type() == nsAttrValue::eEnum)
        aData->SetKeywordValue(eCSSProperty_clear, value->GetEnumValue());
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLBRElement::IsAttributeMapped(const nsAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::clear },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLBRElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

JSObject*
HTMLBRElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLBRElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
