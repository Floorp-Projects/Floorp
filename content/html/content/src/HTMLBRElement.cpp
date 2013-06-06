/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/HTMLBRElementBinding.h"
#include "mozilla/Util.h"

#include "nsAttrValueInlines.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"


NS_IMPL_NS_NEW_HTML_ELEMENT(BR)

namespace mozilla {
namespace dom {

HTMLBRElement::HTMLBRElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetIsDOMBinding();
}

HTMLBRElement::~HTMLBRElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLBRElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLBRElement, Element)


// QueryInterface implementation for HTMLBRElement
NS_INTERFACE_TABLE_HEAD(HTMLBRElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
  NS_INTERFACE_TABLE_INHERITED1(HTMLBRElement, nsIDOMHTMLBRElement)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_ELEMENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(HTMLBRElement)


NS_IMPL_STRING_ATTR(HTMLBRElement, Clear, clear)

static const nsAttrValue::EnumTable kClearTable[] = {
  { "left", NS_STYLE_CLEAR_LEFT },
  { "right", NS_STYLE_CLEAR_RIGHT },
  { "all", NS_STYLE_CLEAR_LEFT_AND_RIGHT },
  { "both", NS_STYLE_CLEAR_LEFT_AND_RIGHT },
  { 0 }
};

bool
HTMLBRElement::ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::clear && aNamespaceID == kNameSpaceID_None) {
    return aResult.ParseEnumValue(aValue, kClearTable, false);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Display)) {
    nsCSSValue* clear = aData->ValueForClear();
    if (clear->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::clear);
      if (value && value->Type() == nsAttrValue::eEnum)
        clear->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLBRElement::IsAttributeMapped(const nsIAtom* aAttribute) const
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
HTMLBRElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLBRElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
