/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLIElement.h"
#include "mozilla/dom/HTMLLIElementBinding.h"

#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(LI)
DOMCI_NODE_DATA(HTMLLIElement, mozilla::dom::HTMLLIElement)

namespace mozilla {
namespace dom {

HTMLLIElement::~HTMLLIElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLLIElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLLIElement, Element)


// QueryInterface implementation for nsHTMLLIElement
NS_INTERFACE_TABLE_HEAD(HTMLLIElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLLIElement, nsIDOMHTMLLIElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLLIElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLLIElement)

NS_IMPL_ELEMENT_CLONE(HTMLLIElement)

NS_IMPL_STRING_ATTR(HTMLLIElement, Type, type)
NS_IMPL_INT_ATTR(HTMLLIElement, Value, value)

// values that are handled case-insensitively
static const nsAttrValue::EnumTable kUnorderedListItemTypeTable[] = {
  { "disc", NS_STYLE_LIST_STYLE_DISC },
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { 0 }
};

// values that are handled case-sensitively
static const nsAttrValue::EnumTable kOrderedListItemTypeTable[] = {
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { "1", NS_STYLE_LIST_STYLE_DECIMAL },
  { 0 }
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

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(List)) {
    nsCSSValue* listStyleType = aData->ValueForListStyleType();
    if (listStyleType->GetUnit() == eCSSUnit_Null) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
      if (value && value->Type() == nsAttrValue::eEnum)
        listStyleType->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
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
HTMLLIElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return HTMLLIElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

} // namespace dom
} // namespace mozilla
