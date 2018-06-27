/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLPreElement.h"
#include "mozilla/dom/HTMLPreElementBinding.h"

#include "mozilla/MappedDeclarations.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Pre)

namespace mozilla {
namespace dom {

HTMLPreElement::~HTMLPreElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLPreElement)

bool
HTMLPreElement::ParseAttribute(int32_t aNamespaceID,
                               nsAtom* aAttribute,
                               const nsAString& aValue,
                               nsIPrincipal* aMaybeScriptedPrincipal,
                               nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseIntValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLPreElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                      MappedDeclarations& aDecls)
{
  if (!aDecls.PropertyIsSet(eCSSProperty_white_space)) {
    // wrap: empty
    if (aAttributes->GetAttr(nsGkAtoms::wrap))
      aDecls.SetKeywordValue(eCSSProperty_white_space, StyleWhiteSpace::PreWrap);
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLPreElement::IsAttributeMapped(const nsAtom* aAttribute) const
{
  if (!mNodeInfo->Equals(nsGkAtoms::pre)) {
    return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
  }

  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::wrap },
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
  if (!mNodeInfo->Equals(nsGkAtoms::pre)) {
    return nsGenericHTMLElement::GetAttributeMappingFunction();
  }

  return &MapAttributesIntoRule;
}

JSObject*
HTMLPreElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLPreElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
