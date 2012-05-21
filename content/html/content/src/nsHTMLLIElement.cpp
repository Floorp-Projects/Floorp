/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsIDOMHTMLLIElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"

using namespace mozilla;

class nsHTMLLIElement : public nsGenericHTMLElement,
                        public nsIDOMHTMLLIElement
{
public:
  nsHTMLLIElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLLIElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLLIElement
  NS_DECL_NSIDOMHTMLLIELEMENT

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }
};


NS_IMPL_NS_NEW_HTML_ELEMENT(LI)


nsHTMLLIElement::nsHTMLLIElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLLIElement::~nsHTMLLIElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLLIElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLLIElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLLIElement, nsHTMLLIElement)

// QueryInterface implementation for nsHTMLLIElement
NS_INTERFACE_TABLE_HEAD(nsHTMLLIElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLLIElement, nsIDOMHTMLLIElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLLIElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLLIElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLLIElement)


NS_IMPL_STRING_ATTR(nsHTMLLIElement, Type, type)
NS_IMPL_INT_ATTR(nsHTMLLIElement, Value, value)

// values that are handled case-insensitively
static const nsAttrValue::EnumTable kUnorderedListItemTypeTable[] = {
  { "disc", NS_STYLE_LIST_STYLE_DISC },
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { 0 }
};

// values that are handled case-sensitively
static const nsAttrValue::EnumTable kOrderedListItemTypeTable[] = {
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { "1", NS_STYLE_LIST_STYLE_DECIMAL },
  { 0 }
};

bool
nsHTMLLIElement::ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, kOrderedListItemTypeTable,
                                    true) ||
             aResult.ParseEnumValue(aValue, kUnorderedListItemTypeTable, false);
    }
    if (aAttribute == nsGkAtoms::value) {
      return aResult.ParseIntValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(List)) {
    nsCSSValue* listStyleType = aData->ValueForListStyleType();
    if (listStyleType->GetUnit() == eCSSUnit_Null) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
      if (value && value->Type() == nsAttrValue::eEnum)
        listStyleType->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLLIElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::type },
    { nsnull },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}



nsMapRuleToAttributesFunc
nsHTMLLIElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}
