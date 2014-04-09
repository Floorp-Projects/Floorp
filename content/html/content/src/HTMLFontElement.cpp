/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLFontElement.h"
#include "mozilla/dom/HTMLFontElementBinding.h"
#include "nsAttrValueInlines.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Font)

namespace mozilla {
namespace dom {

HTMLFontElement::~HTMLFontElement()
{
}

JSObject*
HTMLFontElement::WrapNode(JSContext *aCx)
{
  return HTMLFontElementBinding::Wrap(aCx, this);
}

NS_IMPL_ELEMENT_CLONE(HTMLFontElement)

bool
HTMLFontElement::ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::size) {
      int32_t size = nsContentUtils::ParseLegacyFontSize(aValue);
      if (size) {
        aResult.SetTo(size, &aValue);
        return true;
      }
      return false;
    }
    if (aAttribute == nsGkAtoms::color) {
      return aResult.ParseColor(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLFontElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                       nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Font)) {
    // face: string list
    nsCSSValue* family = aData->ValueForFontFamily();
    if (family->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::face);
      if (value && value->Type() == nsAttrValue::eString &&
          !value->IsEmptyString()) {
        family->SetStringValue(value->GetStringValue(), eCSSUnit_Families);
      }
    }

    // size: int
    nsCSSValue* fontSize = aData->ValueForFontSize();
    if (fontSize->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::size);
      if (value && value->Type() == nsAttrValue::eInteger) {
        fontSize->SetIntValue(value->GetIntegerValue(), eCSSUnit_Enumerated);
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    nsCSSValue* colorValue = aData->ValueForColor();
    if (colorValue->GetUnit() == eCSSUnit_Null &&
        aData->mPresContext->UseDocumentColors()) {
      // color: color
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::color);
      nscolor color;
      if (value && value->GetColorValue(color)) {
        colorValue->SetColorValue(color);
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(TextReset) &&
      aData->mPresContext->CompatibilityMode() == eCompatibility_NavQuirks) {
    // Make <a><font color="red">text</font></a> give the text a red underline
    // in quirks mode.  The NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL flag only
    // affects quirks mode rendering.
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::color);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      nsCSSValue* decoration = aData->ValueForTextDecorationLine();
      int32_t newValue = NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL;
      if (decoration->GetUnit() == eCSSUnit_Enumerated) {
        newValue |= decoration->GetIntValue();
      }
      decoration->SetIntValue(newValue, eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLFontElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::face },
    { &nsGkAtoms::size },
    { &nsGkAtoms::color },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLFontElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
