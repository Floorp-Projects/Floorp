/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLPreElement.h"
#include "mozilla/dom/HTMLPreElementBinding.h"

#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Pre)
DOMCI_NODE_DATA(HTMLPreElement, mozilla::dom::HTMLPreElement)

namespace mozilla {
namespace dom {

HTMLPreElement::~HTMLPreElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLPreElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLPreElement, Element)

// QueryInterface implementation for HTMLPreElement
NS_INTERFACE_TABLE_HEAD(HTMLPreElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLPreElement, nsIDOMHTMLPreElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLPreElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLPreElement)

NS_IMPL_ELEMENT_CLONE(HTMLPreElement)

NS_IMPL_INT_ATTR(HTMLPreElement, Width, width)

bool
HTMLPreElement::ParseAttribute(int32_t aNamespaceID,
                               nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::cols) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    nsCSSValue* width = aData->ValueForWidth();
    if (width->GetUnit() == eCSSUnit_Null) {
      // width: int (html4 attribute == nav4 cols)
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (!value || value->Type() != nsAttrValue::eInteger) {
        // cols: int (nav4 attribute)
        value = aAttributes->GetAttr(nsGkAtoms::cols);
      }

      if (value && value->Type() == nsAttrValue::eInteger)
        width->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Char);
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Text)) {
    nsCSSValue* whiteSpace = aData->ValueForWhiteSpace();
    if (whiteSpace->GetUnit() == eCSSUnit_Null) {
      // wrap: empty
      if (aAttributes->GetAttr(nsGkAtoms::wrap))
        whiteSpace->SetIntValue(NS_STYLE_WHITESPACE_PRE_WRAP, eCSSUnit_Enumerated);

      // width: int (html4 attribute == nav4 cols)
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (!value || value->Type() != nsAttrValue::eInteger) {
        // cols: int (nav4 attribute)
        value = aAttributes->GetAttr(nsGkAtoms::cols);
      }

      if (value && value->Type() == nsAttrValue::eInteger) {
        // Force wrap property on since we want to wrap at a width
        // boundary not just a newline.
        whiteSpace->SetIntValue(NS_STYLE_WHITESPACE_PRE_WRAP, eCSSUnit_Enumerated);
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLPreElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::wrap },
    { &nsGkAtoms::cols },
    { &nsGkAtoms::width },
    { nullptr },
  };
  
  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLPreElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

JSObject*
HTMLPreElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return HTMLPreElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

} // namespace dom
} // namespace mozilla
