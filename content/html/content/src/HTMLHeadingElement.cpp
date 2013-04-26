/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLHeadingElement.h"
#include "mozilla/dom/HTMLHeadingElementBinding.h"

#include "mozilla/Util.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "mozAutoDocUpdate.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Heading)

namespace mozilla {
namespace dom {

HTMLHeadingElement::~HTMLHeadingElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLHeadingElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLHeadingElement, Element)

// QueryInterface implementation for HTMLHeadingElement
NS_INTERFACE_TABLE_HEAD(HTMLHeadingElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLHeadingElement,
                                   nsIDOMHTMLHeadingElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLHeadingElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(HTMLHeadingElement)

JSObject*
HTMLHeadingElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLHeadingElementBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_STRING_ATTR(HTMLHeadingElement, Align, align)


bool
HTMLHeadingElement::ParseAttribute(int32_t aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return ParseDivAlignValue(aValue, aResult);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLHeadingElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sDivAlignAttributeMap,
    sCommonAttributeMap
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLHeadingElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
