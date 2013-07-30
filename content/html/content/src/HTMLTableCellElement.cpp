/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/HTMLTableCellElement.h"
#include "mozilla/dom/HTMLTableElement.h"
#include "mozilla/dom/HTMLTableRowElement.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsRuleData.h"
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
HTMLTableCellElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLTableCellElementBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_ADDREF_INHERITED(HTMLTableCellElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTableCellElement, Element)

// QueryInterface implementation for HTMLTableCellElement
NS_INTERFACE_MAP_BEGIN(HTMLTableCellElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
NS_ELEMENT_INTERFACE_MAP_END


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

  if (section->IsHTML(nsGkAtoms::table)) {
    // XHTML, without a row group.
    return static_cast<HTMLTableElement*>(section);
  }

  // We have a row group.
  nsIContent* result = section->GetParent();
  if (result && result->IsHTML(nsGkAtoms::table)) {
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
HTMLTableCellElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  nsresult rv = nsGenericHTMLElement::WalkContentStyleRules(aRuleWalker);
  NS_ENSURE_SUCCESS(rv, rv);

  if (HTMLTableElement* table = GetTable()) {
    nsMappedAttributes* tableInheritedAttributes =
      table->GetAttributesMappedForCell();
    if (tableInheritedAttributes) {
      aRuleWalker->Forward(tableInheritedAttributes);
    }
  }
  return NS_OK;
}

void
HTMLTableCellElement::GetAlign(nsString& aValue)
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
  { 0 }
};

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
      bool res = aResult.ParseIntWithBounds(aValue, -1);
      if (res) {
        int32_t val = aResult.GetIntegerValue();
        // reset large colspan values as IE and opera do
        // quirks mode does not honor the special html 4 value of 0
        if (val > MAX_COLSPAN || val < 0 ||
            (0 == val && InNavQuirksMode(OwnerDoc()))) {
          aResult.SetTo(1);
        }
      }
      return res;
    }
    if (aAttribute == nsGkAtoms::rowspan) {
      bool res = aResult.ParseIntWithBounds(aValue, -1, MAX_ROWSPAN);
      if (res) {
        int32_t val = aResult.GetIntegerValue();
        // quirks mode does not honor the special html 4 value of 0
        if (val < 0 || (0 == val && InNavQuirksMode(OwnerDoc()))) {
          aResult.SetTo(1);
        }
      }
      return res;
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

static
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                           nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    // width: value
    nsCSSValue* width = aData->ValueForWidth();
    if (width->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (value && value->Type() == nsAttrValue::eInteger) {
        if (value->GetIntegerValue() > 0)
          width->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
        // else 0 implies auto for compatibility.
      }
      else if (value && value->Type() == nsAttrValue::ePercent) {
        if (value->GetPercentValue() > 0.0f)
          width->SetPercentValue(value->GetPercentValue());
        // else 0 implies auto for compatibility
      }
    }

    // height: value
    nsCSSValue* height = aData->ValueForHeight();
    if (height->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
      if (value && value->Type() == nsAttrValue::eInteger) {
        if (value->GetIntegerValue() > 0)
          height->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
        // else 0 implies auto for compatibility.
      }
      else if (value && value->Type() == nsAttrValue::ePercent) {
        if (value->GetPercentValue() > 0.0f)
          height->SetPercentValue(value->GetPercentValue());
        // else 0 implies auto for compatibility
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Text)) {
    nsCSSValue* textAlign = aData->ValueForTextAlign();
    if (textAlign->GetUnit() == eCSSUnit_Null) {
      // align: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
      if (value && value->Type() == nsAttrValue::eEnum)
        textAlign->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }

    nsCSSValue* whiteSpace = aData->ValueForWhiteSpace();
    if (whiteSpace->GetUnit() == eCSSUnit_Null) {
      // nowrap: enum
      if (aAttributes->GetAttr(nsGkAtoms::nowrap)) {
        // See if our width is not a nonzero integer width.
        const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
        nsCompatibility mode = aData->mPresContext->CompatibilityMode();
        if (!value || value->Type() != nsAttrValue::eInteger ||
            value->GetIntegerValue() == 0 ||
            eCompatibility_NavQuirks != mode) {
          whiteSpace->SetIntValue(NS_STYLE_WHITESPACE_NOWRAP, eCSSUnit_Enumerated);
        }
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(TextReset)) {
    nsCSSValue* verticalAlign = aData->ValueForVerticalAlign();
    if (verticalAlign->GetUnit() == eCSSUnit_Null) {
      // valign: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::valign);
      if (value && value->Type() == nsAttrValue::eEnum)
        verticalAlign->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }
  
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
