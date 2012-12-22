/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsHTMLDivElement.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "mozilla/dom/HTMLDivElementBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_NS_NEW_HTML_ELEMENT(Div)


nsHTMLDivElement::nsHTMLDivElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetIsDOMBinding();
}

nsHTMLDivElement::~nsHTMLDivElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLDivElement, Element)
NS_IMPL_RELEASE_INHERITED(nsHTMLDivElement, Element)

DOMCI_NODE_DATA(HTMLDivElement, nsHTMLDivElement)

// QueryInterface implementation for nsHTMLDivElement
NS_INTERFACE_TABLE_HEAD(nsHTMLDivElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLDivElement, nsIDOMHTMLDivElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLDivElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLDivElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLDivElement)

JSObject*
nsHTMLDivElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return dom::HTMLDivElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

bool
nsHTMLDivElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (mNodeInfo->Equals(nsGkAtoms::marquee)) {
      if ((aAttribute == nsGkAtoms::width) ||
          (aAttribute == nsGkAtoms::height)) {
        return aResult.ParseSpecialIntValue(aValue);
      }
      if (aAttribute == nsGkAtoms::bgcolor) {
        return aResult.ParseColor(aValue);
      }
      if ((aAttribute == nsGkAtoms::hspace) ||
          (aAttribute == nsGkAtoms::vspace)) {
        return aResult.ParseIntWithBounds(aValue, 0);
      }
    }

    if (mNodeInfo->Equals(nsGkAtoms::div) &&
        aAttribute == nsGkAtoms::align) {
      return ParseDivAlignValue(aValue, aResult);
    }
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

static void
MapMarqueeAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapBGColorInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLDivElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  if (mNodeInfo->Equals(nsGkAtoms::div)) {
    static const MappedAttributeEntry* const map[] = {
      sDivAlignAttributeMap,
      sCommonAttributeMap
    };
    return FindAttributeDependence(aAttribute, map);
  }
  if (mNodeInfo->Equals(nsGkAtoms::marquee)) {  
    static const MappedAttributeEntry* const map[] = {
      sImageMarginSizeAttributeMap,
      sBackgroundColorAttributeMap,
      sCommonAttributeMap
    };
    return FindAttributeDependence(aAttribute, map);
  }

  return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
}

nsMapRuleToAttributesFunc
nsHTMLDivElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsGkAtoms::div)) {
    return &MapAttributesIntoRule;
  }
  if (mNodeInfo->Equals(nsGkAtoms::marquee)) {
    return &MapMarqueeAttributesIntoRule;
  }  
  return nsGenericHTMLElement::GetAttributeMappingFunction();
}

