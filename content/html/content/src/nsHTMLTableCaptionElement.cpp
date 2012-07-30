/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"

using namespace mozilla;

class nsHTMLTableCaptionElement :  public nsGenericHTMLElement,
                                   public nsIDOMHTMLTableCaptionElement
{
public:
  nsHTMLTableCaptionElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLTableCaptionElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLTableCaptionElement
  NS_DECL_NSIDOMHTMLTABLECAPTIONELEMENT

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};


NS_IMPL_NS_NEW_HTML_ELEMENT(TableCaption)


nsHTMLTableCaptionElement::nsHTMLTableCaptionElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLTableCaptionElement::~nsHTMLTableCaptionElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTableCaptionElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLTableCaptionElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLTableCaptionElement, nsHTMLTableCaptionElement)

// QueryInterface implementation for nsHTMLTableCaptionElement
NS_INTERFACE_TABLE_HEAD(nsHTMLTableCaptionElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLTableCaptionElement,
                                   nsIDOMHTMLTableCaptionElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLTableCaptionElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLTableCaptionElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLTableCaptionElement)


NS_IMPL_STRING_ATTR(nsHTMLTableCaptionElement, Align, align)


static const nsAttrValue::EnumTable kCaptionAlignTable[] = {
  { "left",   NS_STYLE_CAPTION_SIDE_LEFT },
  { "right",  NS_STYLE_CAPTION_SIDE_RIGHT },
  { "top",    NS_STYLE_CAPTION_SIDE_TOP },
  { "bottom", NS_STYLE_CAPTION_SIDE_BOTTOM },
  { 0 }
};

bool
nsHTMLTableCaptionElement::ParseAttribute(PRInt32 aNamespaceID,
                                          nsIAtom* aAttribute,
                                          const nsAString& aValue,
                                          nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return aResult.ParseEnumValue(aValue, kCaptionAlignTable, false);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static 
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(TableBorder)) {
    nsCSSValue* captionSide = aData->ValueForCaptionSide();
    if (captionSide->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
      if (value && value->Type() == nsAttrValue::eEnum)
        captionSide->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLTableCaptionElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}



nsMapRuleToAttributesFunc
nsHTMLTableCaptionElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}
