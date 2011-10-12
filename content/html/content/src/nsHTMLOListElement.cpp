/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLOListElement.h"
#include "nsIDOMHTMLDListElement.h"
#include "nsIDOMHTMLUListElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"

class nsHTMLSharedListElement : public nsGenericHTMLElement,
                                public nsIDOMHTMLOListElement,
                                public nsIDOMHTMLDListElement,
                                public nsIDOMHTMLUListElement
{
public:
  nsHTMLSharedListElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLSharedListElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLOListElement
  NS_DECL_NSIDOMHTMLOLISTELEMENT

  // nsIDOMHTMLDListElement
  // fully declared by NS_DECL_NSIDOMHTMLOLISTELEMENT

  // nsIDOMHTMLUListElement
  // fully declared by NS_DECL_NSIDOMHTMLOLISTELEMENT

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo()
  {
    return static_cast<nsXPCClassInfo*>(GetClassInfoInternal());
  }
  nsIClassInfo* GetClassInfoInternal();
};


NS_IMPL_NS_NEW_HTML_ELEMENT(SharedList)


nsHTMLSharedListElement::nsHTMLSharedListElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLSharedListElement::~nsHTMLSharedListElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSharedListElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLSharedListElement, nsGenericElement) 


DOMCI_DATA(HTMLOListElement, nsHTMLSharedListElement)
DOMCI_DATA(HTMLDListElement, nsHTMLSharedListElement)
DOMCI_DATA(HTMLUListElement, nsHTMLSharedListElement)

nsIClassInfo* 
nsHTMLSharedListElement::GetClassInfoInternal()
{
  if (mNodeInfo->Equals(nsGkAtoms::ol)) {
    return NS_GetDOMClassInfoInstance(eDOMClassInfo_HTMLOListElement_id);
  }
  if (mNodeInfo->Equals(nsGkAtoms::dl)) {
    return NS_GetDOMClassInfoInstance(eDOMClassInfo_HTMLDListElement_id);
  }
  if (mNodeInfo->Equals(nsGkAtoms::ul)) {
    return NS_GetDOMClassInfoInstance(eDOMClassInfo_HTMLUListElement_id);
  }
  return nsnull;
}

// QueryInterface implementation for nsHTMLSharedListElement
NS_INTERFACE_TABLE_HEAD(nsHTMLSharedListElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_AMBIGUOUS_BEGIN(nsHTMLSharedListElement,
                                                  nsIDOMHTMLOListElement)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE_AMBIGUOUS(nsHTMLSharedListElement,
                                                         nsGenericHTMLElement,
                                                         nsIDOMHTMLOListElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLOListElement, ol)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLDListElement, dl)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLUListElement, ul)

  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_GETTER(GetClassInfoInternal)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(nsHTMLSharedListElement)


NS_IMPL_BOOL_ATTR(nsHTMLSharedListElement, Compact, compact)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLSharedListElement, Start, start, 1)
NS_IMPL_STRING_ATTR(nsHTMLSharedListElement, Type, type)

// Shared with nsHTMLSharedElement.cpp
nsAttrValue::EnumTable kListTypeTable[] = {
  { "none", NS_STYLE_LIST_STYLE_NONE },
  { "disc", NS_STYLE_LIST_STYLE_DISC },
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { "decimal", NS_STYLE_LIST_STYLE_DECIMAL },
  { "lower-roman", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { "upper-roman", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "lower-alpha", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "upper-alpha", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { 0 }
};

static const nsAttrValue::EnumTable kOldListTypeTable[] = {
  { "1", NS_STYLE_LIST_STYLE_DECIMAL },
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { 0 }
};

bool
nsHTMLSharedListElement::ParseAttribute(PRInt32 aNamespaceID,
                                        nsIAtom* aAttribute,
                                        const nsAString& aValue,
                                        nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (mNodeInfo->Equals(nsGkAtoms::ol) ||
        mNodeInfo->Equals(nsGkAtoms::ul)) {
      if (aAttribute == nsGkAtoms::type) {
        return aResult.ParseEnumValue(aValue, kListTypeTable, PR_FALSE) ||
               aResult.ParseEnumValue(aValue, kOldListTypeTable, PR_TRUE);
      }
      if (aAttribute == nsGkAtoms::start) {
        return aResult.ParseIntValue(aValue);
      }
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(List)) {
    nsCSSValue* listStyleType = aData->ValueForListStyleType();
    if (listStyleType->GetUnit() == eCSSUnit_Null) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
      if (value) {
        if (value->Type() == nsAttrValue::eEnum)
          listStyleType->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
        else
          listStyleType->SetIntValue(NS_STYLE_LIST_STYLE_DECIMAL, eCSSUnit_Enumerated);
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLSharedListElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  if (mNodeInfo->Equals(nsGkAtoms::ol) ||
      mNodeInfo->Equals(nsGkAtoms::ul)) {
    static const MappedAttributeEntry attributes[] = {
      { &nsGkAtoms::type },
      { nsnull }
    };

    static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
    };

    return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
  }

  return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
}

nsMapRuleToAttributesFunc
nsHTMLSharedListElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsGkAtoms::ol) ||
      mNodeInfo->Equals(nsGkAtoms::ul)) {
    return &MapAttributesIntoRule;
  }

  return nsGenericHTMLElement::GetAttributeMappingFunction();
}
