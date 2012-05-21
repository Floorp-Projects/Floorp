/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMEventTarget.h"
#include "nsDOMError.h"
#include "nsMappedAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsContentList.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsHTMLParts.h"
#include "nsRuleData.h"
#include "nsContentUtils.h"

using namespace mozilla;

class nsHTMLTableRowElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLTableRowElement
{
public:
  nsHTMLTableRowElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLTableRowElement
  NS_DECL_NSIDOMHTMLTABLEROWELEMENT

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLTableRowElement,
                                                     nsGenericHTMLElement)

protected:
  nsresult GetSection(nsIDOMHTMLTableSectionElement** aSection);
  nsresult GetTable(nsIDOMHTMLTableElement** aTable);
  nsRefPtr<nsContentList> mCells;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(TableRow)


nsHTMLTableRowElement::nsHTMLTableRowElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLTableRowElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLTableRowElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mCells,
                                                       nsIDOMNodeList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLTableRowElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableRowElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLTableRowElement, nsHTMLTableRowElement)

// QueryInterface implementation for nsHTMLTableRowElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLTableRowElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLTableRowElement,
                                   nsIDOMHTMLTableRowElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLTableRowElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLTableRowElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLTableRowElement)


// protected method
nsresult
nsHTMLTableRowElement::GetSection(nsIDOMHTMLTableSectionElement** aSection)
{
  NS_ENSURE_ARG_POINTER(aSection);
  nsCOMPtr<nsIDOMHTMLTableSectionElement> section =
    do_QueryInterface(GetParent());
  section.forget(aSection);
  return NS_OK;
}

// protected method
nsresult
nsHTMLTableRowElement::GetTable(nsIDOMHTMLTableElement** aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  nsIContent* parent = GetParent();
  if (!parent) {
    return NS_OK;
  }

  // We may not be in a section
  nsCOMPtr<nsIDOMHTMLTableElement> table = do_QueryInterface(parent);
  if (table) {
    table.forget(aTable);
    return NS_OK;
  }

  parent = parent->GetParent();
  if (!parent) {
    return NS_OK;
  }
  table = do_QueryInterface(parent);
  table.forget(aTable);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetRowIndex(PRInt32* aValue)
{
  *aValue = -1;
  nsCOMPtr<nsIDOMHTMLTableElement> table;
  nsresult rv = GetTable(getter_AddRefs(table));
  if (NS_FAILED(rv) || !table) {
    return rv;
  }

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  table->GetRows(getter_AddRefs(rows));

  PRUint32 numRows;
  rows->GetLength(&numRows);

  for (PRUint32 i = 0; i < numRows; i++) {
    if (rows->GetNodeAt(i) == static_cast<nsIContent*>(this)) {
      *aValue = i;
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetSectionRowIndex(PRInt32* aValue)
{
  *aValue = -1;
  nsCOMPtr<nsIDOMHTMLTableSectionElement> section;
  nsresult rv = GetSection(getter_AddRefs(section));
  if (NS_FAILED(rv) || !section) {
    return rv;
  }

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  section->GetRows(getter_AddRefs(rows));

  PRUint32 numRows;
  rows->GetLength(&numRows);
  for (PRUint32 i = 0; i < numRows; i++) {
    if (rows->GetNodeAt(i) == static_cast<nsIContent*>(this)) {
      *aValue = i;
      break;
    }
  }

  return NS_OK;
}

static bool
IsCell(nsIContent *aContent, PRInt32 aNamespaceID,
       nsIAtom* aAtom, void *aData)
{
  nsIAtom* tag = aContent->Tag();

  return ((tag == nsGkAtoms::td || tag == nsGkAtoms::th) &&
          aContent->IsHTML());
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetCells(nsIDOMHTMLCollection** aValue)
{
  if (!mCells) {
    mCells = new nsContentList(this,
                               IsCell,
                               nsnull, // destroy func
                               nsnull, // closure data
                               false,
                               nsnull,
                               kNameSpaceID_XHTML,
                               false);
  }

  NS_ADDREF(*aValue = mCells);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;

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
      PRUint32 cellCount;
      cells->GetLength(&cellCount);
      if (aIndex > PRInt32(cellCount)) {
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
nsHTMLTableRowElement::DeleteCell(PRInt32 aValue)
{
  if (aValue < -1) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMHTMLCollection> cells;
  GetCells(getter_AddRefs(cells));

  nsresult rv;
  PRUint32 refIndex;
  if (aValue == -1) {
    rv = cells->GetLength(&refIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    if (refIndex == 0) {
      return NS_OK;
    }

    --refIndex;
  }
  else {
    refIndex = (PRUint32)aValue;
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

NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, Ch, _char)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, ChOff, charoff)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, VAlign, valign)


bool
nsHTMLTableRowElement::ParseAttribute(PRInt32 aNamespaceID,
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

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
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
nsHTMLTableRowElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { &nsGkAtoms::valign }, 
    { &nsGkAtoms::height },
    { nsnull }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
nsHTMLTableRowElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}
