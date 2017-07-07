/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTableCellElement.h"
#include "mozilla/dom/HTMLTableElement.h"
#include "mozilla/dom/HTMLTableRowElement.h"
#include "mozilla/GenericSpecifiedValuesInlines.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsRuleWalker.h"
#include "celldata.h"
#include "mozilla/dom/HTMLTableCellElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableCell)

namespace mozilla {
namespace dom {

HTMLTableCellElement::~HTMLTableCellElement()
{
}

JSObject*
HTMLTableCellElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLTableCellElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLTableCellElement, nsGenericHTMLElement,
                            nsIDOMHTMLTableCellElement)

NS_IMPL_ELEMENT_CLONE(HTMLTableCellElement)


// protected method
HTMLTableRowElement*
HTMLTableCellElement::GetRow() const
{
  return HTMLTableRowElement::FromContentOrNull(GetParent());
}

// protected method
HTMLTableElement*
HTMLTableCellElement::GetTable() const
{
  nsIContent *parent = GetParent();
  if (!parent) {
    return nullptr;
  }

  // parent should be a row.
  nsIContent* section = parent->GetParent();
  if (!section) {
    return nullptr;
  }

  if (section->IsHTMLElement(nsGkAtoms::table)) {
    // XHTML, without a row group.
    return static_cast<HTMLTableElement*>(section);
  }

  // We have a row group.
  nsIContent* result = section->GetParent();
  if (result && result->IsHTMLElement(nsGkAtoms::table)) {
    return static_cast<HTMLTableElement*>(result);
  }

  return nullptr;
}

int32_t
HTMLTableCellElement::CellIndex() const
{
  HTMLTableRowElement* row = GetRow();
  if (!row) {
    return -1;
  }

  nsIHTMLCollection* cells = row->Cells();
  if (!cells) {
    return -1;
  }

  uint32_t numCells = cells->Length();
  for (uint32_t i = 0; i < numCells; i++) {
    if (cells->Item(i) == this) {
      return i;
    }
  }

  return -1;
}

NS_IMETHODIMP
HTMLTableCellElement::GetCellIndex(int32_t* aCellIndex)
{
  *aCellIndex = CellIndex();
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  nsresult rv = nsGenericHTMLElement::WalkContentStyleRules(aRuleWalker);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsMappedAttributes* tableInheritedAttributes = GetMappedAttributesInheritedFromTable()) {
    if (tableInheritedAttributes) {
      aRuleWalker->Forward(tableInheritedAttributes);
    }
  }
  return NS_OK;
}

nsMappedAttributes*
HTMLTableCellElement::GetMappedAttributesInheritedFromTable() const
{
  if (HTMLTableElement* table = GetTable()) {
    return table->GetAttributesMappedForCell();
  }

  return nullptr;
}

NS_IMETHODIMP
HTMLTableCellElement::SetAbbr(const nsAString& aAbbr)
{
  ErrorResult rv;
  SetAbbr(aAbbr, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetAbbr(nsAString& aAbbr)
{
  DOMString abbr;
  GetAbbr(abbr);
  abbr.ToString(aAbbr);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetAxis(const nsAString& aAxis)
{
  ErrorResult rv;
  SetAxis(aAxis, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetAxis(nsAString& aAxis)
{
  DOMString axis;
  GetAxis(axis);
  axis.ToString(aAxis);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetAlign(const nsAString& aAlign)
{
  ErrorResult rv;
  SetAlign(aAlign, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetAlign(nsAString& aAlign)
{
  DOMString align;
  GetAlign(align);
  align.ToString(aAlign);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetVAlign(const nsAString& aVAlign)
{
  ErrorResult rv;
  SetVAlign(aVAlign, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetVAlign(nsAString& aVAlign)
{
  DOMString vAlign;
  GetVAlign(vAlign);
  vAlign.ToString(aVAlign);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetCh(const nsAString& aCh)
{
  ErrorResult rv;
  SetCh(aCh, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetCh(nsAString& aCh)
{
  DOMString ch;
  GetCh(ch);
  ch.ToString(aCh);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetChOff(const nsAString& aChOff)
{
  ErrorResult rv;
  SetChOff(aChOff, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetChOff(nsAString& aChOff)
{
  DOMString chOff;
  GetChOff(chOff);
  chOff.ToString(aChOff);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetBgColor(const nsAString& aBgColor)
{
  ErrorResult rv;
  SetBgColor(aBgColor, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetBgColor(nsAString& aBgColor)
{
  DOMString bgColor;
  GetBgColor(bgColor);
  bgColor.ToString(aBgColor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetHeight(const nsAString& aHeight)
{
  ErrorResult rv;
  SetHeight(aHeight, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetHeight(nsAString& aHeight)
{
  DOMString height;
  GetHeight(height);
  height.ToString(aHeight);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetWidth(const nsAString& aWidth)
{
  ErrorResult rv;
  SetWidth(aWidth, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetWidth(nsAString& aWidth)
{
  DOMString width;
  GetWidth(width);
  width.ToString(aWidth);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetNoWrap(bool aNoWrap)
{
  ErrorResult rv;
  SetNoWrap(aNoWrap, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetNoWrap(bool* aNoWrap)
{
  *aNoWrap = NoWrap();
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetScope(const nsAString& aScope)
{
  ErrorResult rv;
  SetScope(aScope, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetScope(nsAString& aScope)
{
  DOMString scope;
  GetScope(scope);
  scope.ToString(aScope);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetHeaders(const nsAString& aHeaders)
{
  ErrorResult rv;
  SetHeaders(aHeaders, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetHeaders(nsAString& aHeaders)
{
  DOMString headers;
  GetHeaders(headers);
  headers.ToString(aHeaders);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetColSpan(int32_t aColSpan)
{
  ErrorResult rv;
  SetColSpan(aColSpan, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetColSpan(int32_t* aColSpan)
{
  *aColSpan = ColSpan();
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableCellElement::SetRowSpan(int32_t aRowSpan)
{
  ErrorResult rv;
  SetRowSpan(aRowSpan, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLTableCellElement::GetRowSpan(int32_t* aRowSpan)
{
  *aRowSpan = RowSpan();
  return NS_OK;
}

void
HTMLTableCellElement::GetAlign(DOMString& aValue)
{
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::align, aValue)) {
    // There's no align attribute, ask the row for the alignment.
    HTMLTableRowElement* row = GetRow();
    if (row) {
      row->GetAlign(aValue);
    }
  }
}

static const nsAttrValue::EnumTable kCellScopeTable[] = {
  { "row",      NS_STYLE_CELL_SCOPE_ROW },
  { "col",      NS_STYLE_CELL_SCOPE_COL },
  { "rowgroup", NS_STYLE_CELL_SCOPE_ROWGROUP },
  { "colgroup", NS_STYLE_CELL_SCOPE_COLGROUP },
  { nullptr,    0 }
};

void
HTMLTableCellElement::GetScope(DOMString& aScope)
{
  GetEnumAttr(nsGkAtoms::scope, nullptr, aScope);
}

bool
HTMLTableCellElement::ParseAttribute(int32_t aNamespaceID,
                                     nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    /* ignore these attributes, stored simply as strings
       abbr, axis, ch, headers
    */
    if (aAttribute == nsGkAtoms::charoff) {
      /* attributes that resolve to integers with a min of 0 */
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::colspan) {
      aResult.ParseClampedNonNegativeInt(aValue, 1, 1, MAX_COLSPAN);
      return true;
    }
    if (aAttribute == nsGkAtoms::rowspan) {
      aResult.ParseClampedNonNegativeInt(aValue, 1, 0, MAX_ROWSPAN);
      // quirks mode does not honor the special html 4 value of 0
      if (aResult.GetIntegerValue() == 0 && InNavQuirksMode(OwnerDoc())) {
        aResult.SetTo(1, &aValue);
      }
      return true;
    }
    if (aAttribute == nsGkAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::align) {
      return ParseTableCellHAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::bgcolor) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::scope) {
      return aResult.ParseEnumValue(aValue, kCellScopeTable, false);
    }
    if (aAttribute == nsGkAtoms::valign) {
      return ParseTableVAlignValue(aValue, aResult);
    }
  }

  return nsGenericHTMLElement::ParseBackgroundAttribute(aNamespaceID,
                                                        aAttribute, aValue,
                                                        aResult) ||
         nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLTableCellElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                            GenericSpecifiedValues* aData)
{
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Position))) {
    // width: value
    if (!aData->PropertyIsSet(eCSSProperty_width)) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (value && value->Type() == nsAttrValue::eInteger) {
        if (value->GetIntegerValue() > 0)
          aData->SetPixelValue(eCSSProperty_width, (float)value->GetIntegerValue());
        // else 0 implies auto for compatibility.
      }
      else if (value && value->Type() == nsAttrValue::ePercent) {
        if (value->GetPercentValue() > 0.0f)
          aData->SetPercentValue(eCSSProperty_width, value->GetPercentValue());
        // else 0 implies auto for compatibility
      }
    }
    // height: value
    if (!aData->PropertyIsSet(eCSSProperty_height)) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
      if (value && value->Type() == nsAttrValue::eInteger) {
        if (value->GetIntegerValue() > 0)
          aData->SetPixelValue(eCSSProperty_height, (float)value->GetIntegerValue());
        // else 0 implies auto for compatibility.
      }
      else if (value && value->Type() == nsAttrValue::ePercent) {
        if (value->GetPercentValue() > 0.0f)
          aData->SetPercentValue(eCSSProperty_height, value->GetPercentValue());
        // else 0 implies auto for compatibility
      }
    }
  }
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Text))) {
    if (!aData->PropertyIsSet(eCSSProperty_white_space)) {
      // nowrap: enum
      if (aAttributes->GetAttr(nsGkAtoms::nowrap)) {
        // See if our width is not a nonzero integer width.
        const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
        nsCompatibility mode = aData->PresContext()->CompatibilityMode();
        if (!value || value->Type() != nsAttrValue::eInteger ||
            value->GetIntegerValue() == 0 ||
            eCompatibility_NavQuirks != mode) {
          aData->SetKeywordValue(eCSSProperty_white_space, StyleWhiteSpace::Nowrap);
        }
      }
    }
  }

  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapVAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLTableCellElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { &nsGkAtoms::valign },
    { &nsGkAtoms::nowrap },
#if 0
    // XXXldb If these are implemented, they might need to move to
    // GetAttributeChangeHint (depending on how, and preferably not).
    { &nsGkAtoms::abbr },
    { &nsGkAtoms::axis },
    { &nsGkAtoms::headers },
    { &nsGkAtoms::scope },
#endif
    { &nsGkAtoms::width },
    { &nsGkAtoms::height },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLTableCellElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
