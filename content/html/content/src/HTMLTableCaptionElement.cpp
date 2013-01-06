/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/HTMLTableCaptionElement.h"
#include "nsAttrValueInlines.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "mozilla/dom/HTMLTableCaptionElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableCaption)
DOMCI_NODE_DATA(HTMLTableCaptionElement, mozilla::dom::HTMLTableCaptionElement)

namespace mozilla {
namespace dom {

HTMLTableCaptionElement::~HTMLTableCaptionElement()
{
}

JSObject*
HTMLTableCaptionElement::WrapNode(JSContext *aCx, JSObject *aScope,
                                  bool *aTriedToWrap)
{
  return HTMLTableCaptionElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

NS_IMPL_ADDREF_INHERITED(HTMLTableCaptionElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTableCaptionElement, Element)

// QueryInterface implementation for HTMLTableCaptionElement
NS_INTERFACE_TABLE_HEAD(HTMLTableCaptionElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLTableCaptionElement,
                                   nsIDOMHTMLTableCaptionElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLTableCaptionElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLTableCaptionElement)

NS_IMPL_ELEMENT_CLONE(HTMLTableCaptionElement)

NS_IMPL_STRING_ATTR(HTMLTableCaptionElement, Align, align)

static const nsAttrValue::EnumTable kCaptionAlignTable[] = {
  { "left",   NS_STYLE_CAPTION_SIDE_LEFT },
  { "right",  NS_STYLE_CAPTION_SIDE_RIGHT },
  { "top",    NS_STYLE_CAPTION_SIDE_TOP },
  { "bottom", NS_STYLE_CAPTION_SIDE_BOTTOM },
  { 0 }
};

bool
HTMLTableCaptionElement::ParseAttribute(int32_t aNamespaceID,
                                        nsIAtom* aAttribute,
                                        const nsAString& aValue,
                                        nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return aResult.ParseEnumValue(aValue, kCaptionAlignTable, false);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static 
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(TableBorder)) {
    nsCSSValue* captionSide = aData->ValueForCaptionSide();
    if (captionSide->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
      if (value && value->Type() == nsAttrValue::eEnum)
        captionSide->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLTableCaptionElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLTableCaptionElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
