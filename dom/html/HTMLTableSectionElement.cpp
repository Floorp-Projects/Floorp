/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTableSectionElement.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsRuleData.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/HTMLTableSectionElementBinding.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(TableSection)

namespace mozilla {
namespace dom {

// you will see the phrases "rowgroup" and "section" used interchangably

HTMLTableSectionElement::~HTMLTableSectionElement()
{
}

JSObject*
HTMLTableSectionElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLTableSectionElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLTableSectionElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLTableSectionElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRows)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLTableSectionElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTableSectionElement, Element)

// QueryInterface implementation for HTMLTableSectionElement
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLTableSectionElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)


NS_IMPL_ELEMENT_CLONE(HTMLTableSectionElement)

nsIHTMLCollection*
HTMLTableSectionElement::Rows()
{
  if (!mRows) {
    mRows = new nsContentList(this,
                              mNodeInfo->NamespaceID(),
                              nsGkAtoms::tr,
                              nsGkAtoms::tr,
                              false);
  }

  return mRows;
}

already_AddRefed<nsGenericHTMLElement>
HTMLTableSectionElement::InsertRow(int32_t aIndex, ErrorResult& aError)
{
  if (aIndex < -1) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsIHTMLCollection* rows = Rows();

  uint32_t rowCount = rows->Length();
  if (aIndex > (int32_t)rowCount) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  bool doInsert = (aIndex < int32_t(rowCount)) && (aIndex != -1);

  // create the row
  nsRefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tr,
                              getter_AddRefs(nodeInfo));

  nsRefPtr<nsGenericHTMLElement> rowContent =
    NS_NewHTMLTableRowElement(nodeInfo.forget());
  if (!rowContent) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  if (doInsert) {
    nsINode::InsertBefore(*rowContent, rows->Item(aIndex), aError);
  } else {
    nsINode::AppendChild(*rowContent, aError);
  }
  return rowContent.forget();
}

void
HTMLTableSectionElement::DeleteRow(int32_t aValue, ErrorResult& aError)
{
  if (aValue < -1) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsIHTMLCollection* rows = Rows();

  uint32_t refIndex;
  if (aValue == -1) {
    refIndex = rows->Length();
    if (refIndex == 0) {
      return;
    }

    --refIndex;
  }
  else {
    refIndex = (uint32_t)aValue;
  }

  nsINode* row = rows->Item(refIndex);
  if (!row) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsINode::RemoveChild(*row, aError);
}

bool
HTMLTableSectionElement::ParseAttribute(int32_t aNamespaceID,
                                        nsIAtom* aAttribute,
                                        const nsAString& aValue,
                                        nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    /* ignore these attributes, stored simply as strings
       ch
    */
    if (aAttribute == nsGkAtoms::charoff) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::height) {
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

void
HTMLTableSectionElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                               nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    // height: value
    nsCSSValue* height = aData->ValueForHeight();
    if (height->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
      if (value && value->Type() == nsAttrValue::eInteger)
        height->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
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
HTMLTableSectionElement::IsAttributeMapped(const nsIAtom* aAttribute) const
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
HTMLTableSectionElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
