/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTableRowElement.h"
#include "mozilla/dom/HTMLTableElement.h"
#include "mozilla/MappedDeclarations.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/HTMLTableRowElementBinding.h"
#include "nsContentList.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableRow)

namespace mozilla {
namespace dom {

HTMLTableRowElement::~HTMLTableRowElement()
{
}

JSObject*
HTMLTableRowElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLTableRowElement_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLTableRowElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLTableRowElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCells)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLTableRowElement,
                                               nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLTableRowElement)


// protected method
HTMLTableSectionElement*
HTMLTableRowElement::GetSection() const
{
  nsIContent* parent = GetParent();
  if (parent &&
      parent->IsAnyOfHTMLElements(nsGkAtoms::thead,
                                  nsGkAtoms::tbody,
                                  nsGkAtoms::tfoot)) {
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
  HTMLTableElement* table = HTMLTableElement::FromNode(parent);
  if (table) {
    return table;
  }

  return HTMLTableElement::FromNodeOrNull(parent->GetParent());
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
IsCell(Element *aElement, int32_t aNamespaceID,
       nsAtom* aAtom, void *aData)
{
  return aElement->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th);
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
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nsContentUtils::QNameChanged(mNodeInfo, nsGkAtoms::td,
                               getter_AddRefs(nodeInfo));

  RefPtr<nsGenericHTMLElement> cell =
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
                                    nsAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsIPrincipal* aMaybeScriptedPrincipal,
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
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLTableRowElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                           MappedDeclarations& aDecls)
{
  nsGenericHTMLElement::MapHeightAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapVAlignAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLTableRowElement::IsAttributeMapped(const nsAtom* aAttribute) const
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
