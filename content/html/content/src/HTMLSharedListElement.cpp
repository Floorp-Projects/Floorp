/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSharedListElement.h"
#include "mozilla/dom/HTMLDListElementBinding.h"
#include "mozilla/dom/HTMLOListElementBinding.h"
#include "mozilla/dom/HTMLUListElementBinding.h"

#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(OList)
NS_IMPL_NS_NEW_HTML_ELEMENT(DList)
NS_IMPL_NS_NEW_HTML_ELEMENT(UList)
DOMCI_DATA(HTMLOListElement, mozilla::dom::HTMLSharedListElement)
DOMCI_DATA(HTMLDListElement, mozilla::dom::HTMLSharedListElement)
DOMCI_DATA(HTMLUListElement, mozilla::dom::HTMLSharedListElement)

namespace mozilla {
namespace dom {

HTMLSharedListElement::~HTMLSharedListElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLSharedListElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLSharedListElement, Element)

nsIClassInfo* 
HTMLSharedListElement::GetClassInfoInternal()
{
  if (mNodeInfo->Equals(nsGkAtoms::ol)) {
    return NS_GetDOMClassInfoInstance(eDOMClassInfo_HTMLOListElement_id);
  }
  if (mNodeInfo->Equals(nsGkAtoms::dl)) {
    return NS_GetDOMClassInfoInstance(eDOMClassInfo_HTMLDListElement_id);
  }
  if (mNodeInfo->Equals(nsGkAtoms::ul)) {
    return NS_GetDOMClassInfoInstance(eDOMClassInfo_HTMLUListElement_id);
  }
  return nullptr;
}

// QueryInterface implementation for nsHTMLSharedListElement
NS_INTERFACE_TABLE_HEAD(HTMLSharedListElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_AMBIGUOUS_BEGIN(HTMLSharedListElement,
                                                  nsIDOMHTMLOListElement)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE_AMBIGUOUS(HTMLSharedListElement,
                                                         nsGenericHTMLElement,
                                                         nsIDOMHTMLOListElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLOListElement, ol)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLDListElement, dl)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLUListElement, ul)

  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_GETTER(GetClassInfoInternal)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(HTMLOListElement)
NS_IMPL_ELEMENT_CLONE(HTMLDListElement)
NS_IMPL_ELEMENT_CLONE(HTMLUListElement)


NS_IMPL_BOOL_ATTR(HTMLSharedListElement, Compact, compact)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(HTMLSharedListElement, Start, start, 1)
NS_IMPL_STRING_ATTR(HTMLSharedListElement, Type, type)
NS_IMPL_BOOL_ATTR(HTMLSharedListElement, Reversed, reversed)

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
  { 0 }
};

static const nsAttrValue::EnumTable kOldListTypeTable[] = {
  { "1", NS_STYLE_LIST_STYLE_DECIMAL },
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { 0 }
};

bool
HTMLSharedListElement::ParseAttribute(int32_t aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
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
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(List)) {
    nsCSSValue* listStyleType = aData->ValueForListStyleType();
    if (listStyleType->GetUnit() == eCSSUnit_Null) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
      if (value) {
        if (value->Type() == nsAttrValue::eEnum)
          listStyleType->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
        else
          listStyleType->SetIntValue(NS_STYLE_LIST_STYLE_DECIMAL, eCSSUnit_Enumerated);
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLSharedListElement::IsAttributeMapped(const nsIAtom* aAttribute) const
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
HTMLDListElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return HTMLDListElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

JSObject*
HTMLOListElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return HTMLOListElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

JSObject*
HTMLUListElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return HTMLUListElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

} // namespace dom
} // namespace mozilla
