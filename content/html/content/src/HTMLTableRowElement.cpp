/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/HTMLTableRowElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsContentList.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsHTMLParts.h"
#include "nsRuleData.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableRow)
DOMCI_NODE_DATA(HTMLTableRowElement, mozilla::dom::HTMLTableRowElement)

namespace mozilla {
namespace dom {

HTMLTableRowElement::HTMLTableRowElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLTableRowElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLTableRowElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCells)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLTableRowElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTableRowElement, Element)

// QueryInterface implementation for HTMLTableRowElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLTableRowElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLTableRowElement,
                                   nsIDOMHTMLTableRowElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLTableRowElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLTableRowElement)


NS_IMPL_ELEMENT_CLONE(HTMLTableRowElement)


// protected method
already_AddRefed<nsIDOMHTMLTableSectionElement>
HTMLTableRowElement::GetSection() const
{
  nsCOMPtr<nsIDOMHTMLTableSectionElement> section =
    do_QueryInterface(GetParent());
  return section.forget();
}

// protected method
already_AddRefed<nsIDOMHTMLTableElement>
HTMLTableRowElement::GetTable() const
{
  nsIContent* parent = GetParent();
  if (!parent) {
    return nullptr;
  }

  // We may not be in a section
  nsCOMPtr<nsIDOMHTMLTableElement> table = do_QueryInterface(parent);
  if (table) {
    return table.forget();
  }

  parent = parent->GetParent();
  if (!parent) {
    return nullptr;
  }
  table = do_QueryInterface(parent);
  return table.forget();
}

NS_IMETHODIMP
HTMLTableRowElement::GetRowIndex(int32_t* aValue)
{
  *aValue = -1;
  nsCOMPtr<nsIDOMHTMLTableElement> table = GetTable();
  if (!table) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  table->GetRows(getter_AddRefs(rows));

  nsCOMPtr<nsIHTMLCollection> coll = do_QueryInterface(rows);
  uint32_t numRows = coll->Length();

  for (uint32_t i = 0; i < numRows; i++) {
    if (coll->GetElementAt(i) == this) {
      *aValue = i;
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableRowElement::GetSectionRowIndex(int32_t* aValue)
{
  *aValue = -1;
  nsCOMPtr<nsIDOMHTMLTableSectionElement> section = GetSection();
  if (!section) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  section->GetRows(getter_AddRefs(rows));

  nsCOMPtr<nsIHTMLCollection> coll = do_QueryInterface(rows);
  uint32_t numRows = coll->Length();
  for (uint32_t i = 0; i < numRows; i++) {
    if (coll->GetElementAt(i) == this) {
      *aValue = i;
      break;
    }
  }

  return NS_OK;
}

static bool
IsCell(nsIContent *aContent, int32_t aNamespaceID,
       nsIAtom* aAtom, void *aData)
{
  nsIAtom* tag = aContent->Tag();

  return ((tag == nsGkAtoms::td || tag == nsGkAtoms::th) &&
          aContent->IsHTML());
}

NS_IMETHODIMP
HTMLTableRowElement::GetCells(nsIDOMHTMLCollection** aValue)
{
  if (!mCells) {
    mCells = new nsContentList(this,
                               IsCell,
                               nullptr, // destroy func
                               nullptr, // closure data
                               false,
                               nullptr,
                               kNameSpaceID_XHTML,
                               false);
  }

  NS_ADDREF(*aValue = mCells);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableRowElement::InsertCell(int32_t aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = nullptr;

  if (aIndex < -1) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Make sure mCells is initialized.
  nsCOMPtr<nsIDOMHTMLCollection> cells;
  nsresult rv = GetCells(getter_AddRefs(cells));
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(mCells, "How did that happen?");

  nsCOMPtr<nsIDOMNode> nextSibling;
  // -1 means append, so should use null nextSibling
  if (aIndex != -1) {
    cells->Item(aIndex, getter_AddRefs(nextSibling));
    // Check whether we're inserting past end of list.  We want to avoid doing
    // this unless we really have to, since this has to walk all our kids.  If
    // we have a nextSibling, we're clearly not past end of list.
    if (!nextSibling) {
      uint32_t cellCount;
      cells->GetLength(&cellCount);
      if (aIndex > int32_t(cellCount)) {
        return NS_ERROR_DOM_INDEX_SIZE_ERR;
      }
    }
  }

  // create the cell
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::td,
                              getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIContent> cellContent = NS_NewHTMLTableCellElement(nodeInfo.forget());
  if (!cellContent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> cellNode(do_QueryInterface(cellContent));
  NS_ASSERTION(cellNode, "Should implement nsIDOMNode!");

  nsCOMPtr<nsIDOMNode> retChild;
  InsertBefore(cellNode, nextSibling, getter_AddRefs(retChild));

  if (retChild) {
    CallQueryInterface(retChild, aValue);
  }

  return NS_OK;
}


NS_IMETHODIMP
HTMLTableRowElement::DeleteCell(int32_t aValue)
{
  if (aValue < -1) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMHTMLCollection> cells;
  GetCells(getter_AddRefs(cells));

  nsresult rv;
  uint32_t refIndex;
  if (aValue == -1) {
    rv = cells->GetLength(&refIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    if (refIndex == 0) {
      return NS_OK;
    }

    --refIndex;
  }
  else {
    refIndex = (uint32_t)aValue;
  }

  nsCOMPtr<nsIDOMNode> cell;
  rv = cells->Item(refIndex, getter_AddRefs(cell));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!cell) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMNode> retChild;
  return RemoveChild(cell, getter_AddRefs(retChild));
}

NS_IMPL_STRING_ATTR(HTMLTableRowElement, Align, align)
NS_IMPL_STRING_ATTR(HTMLTableRowElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(HTMLTableRowElement, Ch, _char)
NS_IMPL_STRING_ATTR(HTMLTableRowElement, ChOff, charoff)
NS_IMPL_STRING_ATTR(HTMLTableRowElement, VAlign, valign)


bool
HTMLTableRowElement::ParseAttribute(int32_t aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  /*
   * ignore these attributes, stored simply as strings
   *
   * ch
   */

  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::charoff) {
      return aResult.ParseIntWithBounds(aValue, 0);
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
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    // height: value
    nsCSSValue* height = aData->ValueForHeight();
    if (height->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
      if (value && value->Type() == nsAttrValue::eInteger)
        height->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      else if (value && value->Type() == nsAttrValue::ePercent)
        height->SetPercentValue(value->GetPercentValue());
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
HTMLTableRowElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { &nsGkAtoms::valign }, 
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
HTMLTableRowElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
