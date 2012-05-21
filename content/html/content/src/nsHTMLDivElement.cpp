/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsIDOMHTMLDivElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"

using namespace mozilla;

class nsHTMLDivElement : public nsGenericHTMLElement,
                         public nsIDOMHTMLDivElement
{
public:
  nsHTMLDivElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLDivElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLDivElement
  NS_DECL_NSIDOMHTMLDIVELEMENT

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


NS_IMPL_NS_NEW_HTML_ELEMENT(Div)


nsHTMLDivElement::nsHTMLDivElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLDivElement::~nsHTMLDivElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLDivElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLDivElement, nsGenericElement) 

DOMCI_NODE_DATA(HTMLDivElement, nsHTMLDivElement)

// QueryInterface implementation for nsHTMLDivElement
NS_INTERFACE_TABLE_HEAD(nsHTMLDivElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLDivElement, nsIDOMHTMLDivElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLDivElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLDivElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLDivElement)


NS_IMPL_STRING_ATTR(nsHTMLDivElement, Align, align)


bool
nsHTMLDivElement::ParseAttribute(PRInt32 aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (mNodeInfo->Equals(nsGkAtoms::marquee)) {
      if ((aAttribute == nsGkAtoms::width) ||
          (aAttribute == nsGkAtoms::height)) {
        return aResult.ParseSpecialIntValue(aValue);
      }
      if (aAttribute == nsGkAtoms::bgcolor) {
        return aResult.ParseColor(aValue);
      }
      if ((aAttribute == nsGkAtoms::hspace) ||
          (aAttribute == nsGkAtoms::vspace)) {
        return aResult.ParseIntWithBounds(aValue, 0);
      }
    }

    if (mNodeInfo->Equals(nsGkAtoms::div) &&
        aAttribute == nsGkAtoms::align) {
      return ParseDivAlignValue(aValue, aResult);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static void
MapMarqueeAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapBGColorInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLDivElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  if (mNodeInfo->Equals(nsGkAtoms::div)) {
    static const MappedAttributeEntry* const map[] = {
      sDivAlignAttributeMap,
      sCommonAttributeMap
    };
    return FindAttributeDependence(aAttribute, map);
  }
  if (mNodeInfo->Equals(nsGkAtoms::marquee)) {  
    static const MappedAttributeEntry* const map[] = {
      sImageMarginSizeAttributeMap,
      sBackgroundColorAttributeMap,
      sCommonAttributeMap
    };
    return FindAttributeDependence(aAttribute, map);
  }

  return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
}

nsMapRuleToAttributesFunc
nsHTMLDivElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsGkAtoms::div)) {
    return &MapAttributesIntoRule;
  }
  if (mNodeInfo->Equals(nsGkAtoms::marquee)) {
    return &MapMarqueeAttributesIntoRule;
  }  
  return nsGenericHTMLElement::GetAttributeMappingFunction();
}

