/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLParagraphElement.h"
#include "mozilla/dom/HTMLParagraphElementBinding.h"

#include "mozilla/MappedDeclarations.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"


NS_IMPL_NS_NEW_HTML_ELEMENT(Paragraph)

namespace mozilla {
namespace dom {

HTMLParagraphElement::~HTMLParagraphElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLParagraphElement)

bool
HTMLParagraphElement::ParseAttribute(int32_t aNamespaceID,
                                     nsAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsIPrincipal* aMaybeScriptedPrincipal,
                                     nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return ParseDivAlignValue(aValue, aResult);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLParagraphElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                            MappedDeclarations& aDecls)
{
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLParagraphElement::IsAttributeMapped(const nsAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sDivAlignAttributeMap,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLParagraphElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

JSObject*
HTMLParagraphElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLParagraphElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
