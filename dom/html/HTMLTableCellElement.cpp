/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTableCellElement.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLTableElement.h"
#include "mozilla/dom/HTMLTableRowElement.h"
#include "mozilla/MappedDeclarationsBuilder.h"
#include "nsAttrValueInlines.h"
#include "celldata.h"
#include "mozilla/dom/HTMLTableCellElementBinding.h"

namespace {
enum class StyleCellScope : uint8_t { Row, Col, Rowgroup, Colgroup };
}  // namespace

NS_IMPL_NS_NEW_HTML_ELEMENT(TableCell)

namespace mozilla::dom {

HTMLTableCellElement::~HTMLTableCellElement() = default;

JSObject* HTMLTableCellElement::WrapNode(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return HTMLTableCellElement_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ELEMENT_CLONE(HTMLTableCellElement)

// protected method
HTMLTableRowElement* HTMLTableCellElement::GetRow() const {
  return HTMLTableRowElement::FromNodeOrNull(GetParent());
}

// protected method
HTMLTableElement* HTMLTableCellElement::GetTable() const {
  nsIContent* parent = GetParent();
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

int32_t HTMLTableCellElement::CellIndex() const {
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

StyleLockedDeclarationBlock*
HTMLTableCellElement::GetMappedAttributesInheritedFromTable() const {
  if (HTMLTableElement* table = GetTable()) {
    return table->GetAttributesMappedForCell();
  }
  return nullptr;
}

void HTMLTableCellElement::GetAlign(DOMString& aValue) {
  if (!GetAttr(nsGkAtoms::align, aValue)) {
    // There's no align attribute, ask the row for the alignment.
    HTMLTableRowElement* row = GetRow();
    if (row) {
      row->GetAlign(aValue);
    }
  }
}

static const nsAttrValue::EnumTable kCellScopeTable[] = {
    {"row", StyleCellScope::Row},
    {"col", StyleCellScope::Col},
    {"rowgroup", StyleCellScope::Rowgroup},
    {"colgroup", StyleCellScope::Colgroup},
    {nullptr, 0}};

void HTMLTableCellElement::GetScope(DOMString& aScope) {
  GetEnumAttr(nsGkAtoms::scope, nullptr, aScope);
}

bool HTMLTableCellElement::ParseAttribute(int32_t aNamespaceID,
                                          nsAtom* aAttribute,
                                          const nsAString& aValue,
                                          nsIPrincipal* aMaybeScriptedPrincipal,
                                          nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    /* ignore these attributes, stored simply as strings
       abbr, axis, ch, headers
    */
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
      return aResult.ParseNonzeroHTMLDimension(aValue);
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseNonzeroHTMLDimension(aValue);
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

  return nsGenericHTMLElement::ParseBackgroundAttribute(
             aNamespaceID, aAttribute, aValue, aResult) ||
         nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLTableCellElement::MapAttributesIntoRule(
    MappedDeclarationsBuilder& aBuilder) {
  MapImageSizeAttributesInto(aBuilder);

  if (!aBuilder.PropertyIsSet(eCSSProperty_white_space)) {
    // nowrap: enum
    if (aBuilder.GetAttr(nsGkAtoms::nowrap)) {
      // See if our width is not a nonzero integer width.
      const nsAttrValue* value = aBuilder.GetAttr(nsGkAtoms::width);
      nsCompatibility mode = aBuilder.Document().GetCompatibilityMode();
      if (!value || value->Type() != nsAttrValue::eInteger ||
          value->GetIntegerValue() == 0 || eCompatibility_NavQuirks != mode) {
        aBuilder.SetKeywordValue(eCSSProperty_white_space,
                                 StyleWhiteSpace::Nowrap);
      }
    }
  }

  nsGenericHTMLElement::MapDivAlignAttributeInto(aBuilder);
  nsGenericHTMLElement::MapVAlignAttributeInto(aBuilder);
  nsGenericHTMLElement::MapBackgroundAttributesInto(aBuilder);
  nsGenericHTMLElement::MapCommonAttributesInto(aBuilder);
}

NS_IMETHODIMP_(bool)
HTMLTableCellElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {
    {nsGkAtoms::align},
    {nsGkAtoms::valign},
    {nsGkAtoms::nowrap},
#if 0
    // XXXldb If these are implemented, they might need to move to
    // GetAttributeChangeHint (depending on how, and preferably not).
    { nsGkAtoms::abbr },
    { nsGkAtoms::axis },
    { nsGkAtoms::headers },
    { nsGkAtoms::scope },
#endif
    {nsGkAtoms::width},
    {nsGkAtoms::height},
    {nullptr}
  };

  static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
      sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLTableCellElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

}  // namespace mozilla::dom
