/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLIElement.h"
#include "mozilla/dom/HTMLLIElementBinding.h"

#include "mozilla/GenericSpecifiedValuesInlines.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(LI)

namespace mozilla {
namespace dom {

HTMLLIElement::~HTMLLIElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED0(HTMLLIElement, nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLLIElement)

// values that are handled case-insensitively
static const nsAttrValue::EnumTable kUnorderedListItemTypeTable[] = {
  { "disc", NS_STYLE_LIST_STYLE_DISC },
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { nullptr, 0 }
};

// values that are handled case-sensitively
static const nsAttrValue::EnumTable kOrderedListItemTypeTable[] = {
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { "1", NS_STYLE_LIST_STYLE_DECIMAL },
  { nullptr, 0 }
};

bool
HTMLLIElement::ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, kOrderedListItemTypeTable,
                                    true) ||
             aResult.ParseEnumValue(aValue, kUnorderedListItemTypeTable, false);
    }
    if (aAttribute == nsGkAtoms::value) {
      return aResult.ParseIntValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLLIElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                     GenericSpecifiedValues* aData)
{
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(List))) {
    if (!aData->PropertyIsSet(eCSSProperty_list_style_type)) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
      if (value && value->Type() == nsAttrValue::eEnum)
        aData->SetKeywordValue(eCSSProperty_list_style_type, value->GetEnumValue());
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLLIElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::type },
    { nullptr },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLLIElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

JSObject*
HTMLLIElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLLIElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
