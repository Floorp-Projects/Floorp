/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/HTMLTableRowElement.h"
#include "mozilla/dom/HTMLTableElement.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsRuleData.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/HTMLTableRowElementBinding.h"
#include "nsContentList.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableRow)

namespace mozilla {
namespace dom {

JSObject*
HTMLTableRowElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLTableRowElementBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLTableRowElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLTableRowElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCells)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLTableRowElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTableRowElement, Element)

// QueryInterface implementation for HTMLTableRowElement
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLTableRowElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)


NS_IMPL_ELEMENT_CLONE(HTMLTableRowElement)


// protected method
HTMLTableSectionElement*
HTMLTableRowElement::GetSection() const
{
  nsIContent* parent = GetParent();
  if (parent->IsHTML() && (parent->Tag() == nsGkAtoms::thead ||
                           parent->Tag() == nsGkAtoms::tbody ||
                           parent->Tag() == nsGkAtoms::tfoot)) {
    return static_cast<HTMLTableSectionElement*>(parent);
  }
  return nullptr;
}

// protected method
HTMLTableElement*
HTMLTableRowElement::GetTable() const
{
  nsIContent* parent = GetParent();
  if (!parent) {
    return nullptr;
  }

  // We may not be in a section
  HTMLTableElement* table = HTMLTableElement::FromContent(parent);
  if (table) {
    return table;
  }

  return HTMLTableElement::FromContentOrNull(parent->GetParent());
}

int32_t
HTMLTableRowElement::RowIndex() const
{
  HTMLTableElement* table = GetTable();
  if (!table) {
    return -1;
  }

  nsIHTMLCollection* rows = table->Rows();

  uint32_t numRows = rows->Length();

  for (uint32_t i = 0; i < numRows; i++) {
    if (rows->GetElementAt(i) == this) {
      return i;
    }
  }

  return -1;
}

int32_t
HTMLTableRowElement::SectionRowIndex() const
{
  HTMLTableSectionElement* section = GetSection();
  if (!section) {
    return -1;
  }

  nsCOMPtr<nsIHTMLCollection> coll = section->Rows();
  uint32_t numRows = coll->Length();
  for (uint32_t i = 0; i < numRows; i++) {
    if (coll->GetElementAt(i) == this) {
      return i;
    }
  }

  return -1;
}

static bool
IsCell(nsIContent *aContent, int32_t aNamespaceID,
       nsIAtom* aAtom, void *aData)
{
  nsIAtom* tag = aContent->Tag();

  return ((tag == nsGkAtoms::td || tag == nsGkAtoms::th) &&
          aContent->IsHTML());
}

nsIHTMLCollection*
HTMLTableRowElement::Cells()
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

  return mCells;
}

already_AddRefed<nsGenericHTMLElement>
HTMLTableRowElement::InsertCell(int32_t aIndex,
                                ErrorResult& aError)
{
  if (aIndex < -1) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // Make sure mCells is initialized.
  nsIHTMLCollection* cells = Cells();

  NS_ASSERTION(mCells, "How did that happen?");

  nsCOMPtr<nsINode> nextSibling;
  // -1 means append, so should use null nextSibling
  if (aIndex != -1) {
    nextSibling = cells->Item(aIndex);
    // Check whether we're inserting past end of list.  We want to avoid doing
    // this unless we really have to, since this has to walk all our kids.  If
    // we have a nextSibling, we're clearly not past end of list.
    if (!nextSibling) {
      uint32_t cellCount = cells->Length();
      if (aIndex > int32_t(cellCount)) {
        aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
        return nullptr;
      }
    }
  }

  // create the cell
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::td,
                              getter_AddRefs(nodeInfo));

  nsRefPtr<nsGenericHTMLElement> cell =
    NS_NewHTMLTableCellElement(nodeInfo.forget());
  if (!cell) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsINode::InsertBefore(*cell, nextSibling, aError);

  return cell.forget();
}

void
HTMLTableRowElement::DeleteCell(int32_t aValue, ErrorResult& aError)
{
  if (aValue < -1) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsIHTMLCollection* cells = Cells();

  uint32_t refIndex;
  if (aValue == -1) {
    refIndex = cells->Length();
    if (refIndex == 0) {
      return;
    }

    --refIndex;
  }
  else {
    refIndex = (uint32_t)aValue;
  }

  nsCOMPtr<nsINode> cell = cells->Item(refIndex);
  if (!cell) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsINode::RemoveChild(*cell, aError);
}

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
