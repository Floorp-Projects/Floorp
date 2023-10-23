/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/nsGenericHTMLElementWithFetchPriorityAttribute.h"

#include "mozilla/dom/FetchPriority.h"

namespace mozilla {

// Can't use `using namespace dom;` because it breaks the unified build.
using dom::FetchPriority;
using dom::kFetchPriorityAttributeValueAuto;
using dom::kFetchPriorityAttributeValueHigh;
using dom::kFetchPriorityAttributeValueLow;

bool nsGenericHTMLElementWithFetchPriorityAttribute::ParseAttribute(
    int32_t aNamespaceID, nsAtom* aAttribute, const nsAString& aValue,
    nsIPrincipal* aMaybeScriptedPrincipal, nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::fetchpriority) {
    ParseFetchPriority(aValue, aResult);
    return true;
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void nsGenericHTMLElementWithFetchPriorityAttribute::GetFetchPriority(
    nsAString& aFetchPriority) const {
  // <https://html.spec.whatwg.org/multipage/urls-and-fetching.html#fetch-priority-attributes>.
  GetEnumAttr(nsGkAtoms::fetchpriority, kFetchPriorityAttributeValueAuto,
              aFetchPriority);
}

/* static */
dom::FetchPriority
nsGenericHTMLElementWithFetchPriorityAttribute::ToFetchPriority(
    const nsAString& aValue) {
  nsAttrValue attrValue;
  ParseFetchPriority(aValue, attrValue);
  MOZ_ASSERT(attrValue.Type() == nsAttrValue::eEnum);
  return FetchPriority(attrValue.GetEnumValue());
}

namespace {
// <https://html.spec.whatwg.org/multipage/urls-and-fetching.html#fetch-priority-attributes>.
static const nsAttrValue::EnumTable kFetchPriorityEnumTable[] = {
    {kFetchPriorityAttributeValueHigh, FetchPriority::High},
    {kFetchPriorityAttributeValueLow, FetchPriority::Low},
    {kFetchPriorityAttributeValueAuto, FetchPriority::Auto},
    {nullptr, 0}};

// <https://html.spec.whatwg.org/multipage/urls-and-fetching.html#fetch-priority-attributes>.
static const nsAttrValue::EnumTable*
    kFetchPriorityEnumTableInvalidValueDefault = &kFetchPriorityEnumTable[2];
}  // namespace

/* static */
void nsGenericHTMLElementWithFetchPriorityAttribute::ParseFetchPriority(
    const nsAString& aValue, nsAttrValue& aResult) {
  aResult.ParseEnumValue(aValue, kFetchPriorityEnumTable,
                         false /* aCaseSensitive */,
                         kFetchPriorityEnumTableInvalidValueDefault);
}

}  // namespace mozilla
