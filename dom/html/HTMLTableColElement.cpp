/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTableColElement.h"
#include "mozilla/dom/HTMLTableColElementBinding.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "mozilla/MappedDeclarations.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableCol)

namespace mozilla {
namespace dom {

// use the same protection as ancient code did
// http://lxr.mozilla.org/classic/source/lib/layout/laytable.c#46
#define MAX_COLSPAN 1000

HTMLTableColElement::~HTMLTableColElement()
{
}

JSObject*
HTMLTableColElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLTableColElement_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ELEMENT_CLONE(HTMLTableColElement)

bool
HTMLTableColElement::ParseAttribute(int32_t aNamespaceID,
                                    nsAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsIPrincipal* aMaybeScriptedPrincipal,
                                    nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    /* ignore these attributes, stored simply as strings ch */
    if (aAttribute == nsGkAtoms::charoff) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::span) {
      /* protection from unrealistic large colspan values */
      aResult.ParseClampedNonNegativeInt(aValue, 1, 1, MAX_COLSPAN);
      return true;
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::align) {
      return ParseTableCellHAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::valign) {
      return ParseTableVAlignValue(aValue, aResult);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLTableColElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                           MappedDeclarations& aDecls)
{
  if (!aDecls.PropertyIsSet(eCSSProperty__x_span)) {
    // span: int
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::span);
    if (value && value->Type() == nsAttrValue::eInteger) {
      int32_t val = value->GetIntegerValue();
      // Note: Do NOT use this code for table cells!  The value "0"
      // means something special for colspan and rowspan, but for <col
      // span> and <colgroup span> it's just disallowed.
      if (val > 0) {
        aDecls.SetIntValue(eCSSProperty__x_span, value->GetIntegerValue());
      }
    }
  }

  nsGenericHTMLElement::MapWidthAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapVAlignAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLTableColElement::IsAttributeMapped(const nsAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::width },
    { &nsGkAtoms::align },
    { &nsGkAtoms::valign },
    { &nsGkAtoms::span },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLTableColElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
