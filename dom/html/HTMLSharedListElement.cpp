/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSharedListElement.h"
#include "mozilla/dom/HTMLDListElementBinding.h"
#include "mozilla/dom/HTMLOListElementBinding.h"
#include "mozilla/dom/HTMLUListElementBinding.h"

#include "mozilla/MappedDeclarations.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(SharedList)

namespace mozilla {
namespace dom {

HTMLSharedListElement::~HTMLSharedListElement()
{
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLSharedListElement,
                                               nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLSharedListElement)


// Shared with nsHTMLSharedElement.cpp
nsAttrValue::EnumTable kListTypeTable[] = {
  { "none", NS_STYLE_LIST_STYLE_NONE },
  { "disc", NS_STYLE_LIST_STYLE_DISC },
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { "decimal", NS_STYLE_LIST_STYLE_DECIMAL },
  { "lower-roman", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { "upper-roman", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "lower-alpha", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "upper-alpha", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable kOldListTypeTable[] = {
  { "1", NS_STYLE_LIST_STYLE_DECIMAL },
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { nullptr, 0 }
};

bool
HTMLSharedListElement::ParseAttribute(int32_t aNamespaceID,
                                      nsAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsIPrincipal* aMaybeScriptedPrincipal,
                                      nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (mNodeInfo->Equals(nsGkAtoms::ol) ||
        mNodeInfo->Equals(nsGkAtoms::ul)) {
      if (aAttribute == nsGkAtoms::type) {
        return aResult.ParseEnumValue(aValue, kListTypeTable, false) ||
               aResult.ParseEnumValue(aValue, kOldListTypeTable, true);
      }
      if (aAttribute == nsGkAtoms::start) {
        return aResult.ParseIntValue(aValue);
      }
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLSharedListElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                             MappedDeclarations& aDecls)
{
  if (!aDecls.PropertyIsSet(eCSSProperty_list_style_type)) {
    // type: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
    if (value && value->Type() == nsAttrValue::eEnum) {
      aDecls.SetKeywordValue(eCSSProperty_list_style_type, value->GetEnumValue());
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLSharedListElement::IsAttributeMapped(const nsAtom* aAttribute) const
{
  if (mNodeInfo->Equals(nsGkAtoms::ol) ||
      mNodeInfo->Equals(nsGkAtoms::ul)) {
    static const MappedAttributeEntry attributes[] = {
      { &nsGkAtoms::type },
      { nullptr }
    };

    static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
    };

    return FindAttributeDependence(aAttribute, map);
  }

  return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
}

nsMapRuleToAttributesFunc
HTMLSharedListElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsGkAtoms::ol) ||
      mNodeInfo->Equals(nsGkAtoms::ul)) {
    return &MapAttributesIntoRule;
  }

  return nsGenericHTMLElement::GetAttributeMappingFunction();
}

JSObject*
HTMLSharedListElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  if (mNodeInfo->Equals(nsGkAtoms::ol)) {
    return HTMLOListElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  if (mNodeInfo->Equals(nsGkAtoms::dl)) {
    return HTMLDListElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::ul));
  return HTMLUListElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
