/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSharedListElement.h"
#include "mozilla/dom/HTMLDListElementBinding.h"
#include "mozilla/dom/HTMLOListElementBinding.h"
#include "mozilla/dom/HTMLUListElementBinding.h"
#include "mozilla/dom/HTMLLIElement.h"

#include "mozilla/MappedDeclarationsBuilder.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(SharedList)

namespace mozilla::dom {

HTMLSharedListElement::~HTMLSharedListElement() = default;

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLSharedListElement,
                                               nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLSharedListElement)

bool HTMLSharedListElement::ParseAttribute(
    int32_t aNamespaceID, nsAtom* aAttribute, const nsAString& aValue,
    nsIPrincipal* aMaybeScriptedPrincipal, nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (mNodeInfo->Equals(nsGkAtoms::ul)) {
      if (aAttribute == nsGkAtoms::type) {
        return aResult.ParseEnumValue(aValue, HTMLLIElement::kULTypeTable,
                                      false);
      }
    }
    if (mNodeInfo->Equals(nsGkAtoms::ol)) {
      if (aAttribute == nsGkAtoms::type) {
        return aResult.ParseEnumValue(aValue, HTMLLIElement::kOLTypeTable,
                                      true);
      }
      if (aAttribute == nsGkAtoms::start) {
        return aResult.ParseIntValue(aValue);
      }
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLSharedListElement::MapAttributesIntoRule(
    MappedDeclarationsBuilder& aBuilder) {
  if (!aBuilder.PropertyIsSet(eCSSProperty_list_style_type)) {
    const nsAttrValue* value = aBuilder.GetAttr(nsGkAtoms::type);
    if (value && value->Type() == nsAttrValue::eEnum) {
      aBuilder.SetKeywordValue(eCSSProperty_list_style_type,
                               value->GetEnumValue());
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aBuilder);
}

void HTMLSharedListElement::MapOLAttributesIntoRule(
    MappedDeclarationsBuilder& aBuilder) {
  if (!aBuilder.PropertyIsSet(eCSSProperty_counter_reset)) {
    const nsAttrValue* startAttr = aBuilder.GetAttr(nsGkAtoms::start);
    bool haveStart = startAttr && startAttr->Type() == nsAttrValue::eInteger;
    int32_t start = 0;
    if (haveStart) {
      start = startAttr->GetIntegerValue() - 1;
    }
    bool haveReversed = !!aBuilder.GetAttr(nsGkAtoms::reversed);
    if (haveReversed) {
      if (haveStart) {
        start += 2;  // i.e. the attr value + 1
      } else {
        start = std::numeric_limits<int32_t>::min();
      }
    }
    if (haveStart || haveReversed) {
      aBuilder.SetCounterResetListItem(start, haveReversed);
    }
  }

  HTMLSharedListElement::MapAttributesIntoRule(aBuilder);
}

NS_IMETHODIMP_(bool)
HTMLSharedListElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  if (mNodeInfo->Equals(nsGkAtoms::ul)) {
    static const MappedAttributeEntry attributes[] = {{nsGkAtoms::type},
                                                      {nullptr}};

    static const MappedAttributeEntry* const map[] = {
        attributes,
        sCommonAttributeMap,
    };

    return FindAttributeDependence(aAttribute, map);
  }

  if (mNodeInfo->Equals(nsGkAtoms::ol)) {
    static const MappedAttributeEntry attributes[] = {{nsGkAtoms::type},
                                                      {nsGkAtoms::start},
                                                      {nsGkAtoms::reversed},
                                                      {nullptr}};

    static const MappedAttributeEntry* const map[] = {
        attributes,
        sCommonAttributeMap,
    };

    return FindAttributeDependence(aAttribute, map);
  }

  return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
}

nsMapRuleToAttributesFunc HTMLSharedListElement::GetAttributeMappingFunction()
    const {
  if (mNodeInfo->Equals(nsGkAtoms::ul)) {
    return &MapAttributesIntoRule;
  }
  if (mNodeInfo->Equals(nsGkAtoms::ol)) {
    return &MapOLAttributesIntoRule;
  }

  return nsGenericHTMLElement::GetAttributeMappingFunction();
}

JSObject* HTMLSharedListElement::WrapNode(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  if (mNodeInfo->Equals(nsGkAtoms::ol)) {
    return HTMLOListElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  if (mNodeInfo->Equals(nsGkAtoms::dl)) {
    return HTMLDListElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::ul));
  return HTMLUListElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
