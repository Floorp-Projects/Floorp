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
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"

class nsHTMLSharedListElement : public nsGenericHTMLElement,
                                public nsIDOMHTMLOListElement,
                                public nsIDOMHTMLDListElement,
                                public nsIDOMHTMLUListElement
{
public:
  nsHTMLSharedListElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLSharedListElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

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

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(SharedList)


nsHTMLSharedListElement::nsHTMLSharedListElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLSharedListElement::~nsHTMLSharedListElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSharedListElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLSharedListElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLSharedListElement
NS_HTML_CONTENT_INTERFACE_MAP_AMBIGOUS_BEGIN(nsHTMLSharedListElement,
                                             nsGenericHTMLElement,
                                             nsIDOMHTMLOListElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLOListElement, ol)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLDListElement, dl)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLUListElement, ul)

  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLOListElement, ol)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLDListElement, dl)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLUListElement, ul)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLSharedListElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  nsHTMLSharedListElement *it = new nsHTMLSharedListElement(mNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip =
    NS_STATIC_CAST(nsIDOMHTMLOListElement *, it);

  CopyInnerTo(it, aDeep);

  kungFuDeathGrip.swap(*aReturn);

  return NS_OK;
}


NS_IMPL_BOOL_ATTR(nsHTMLSharedListElement, Compact, compact)
NS_IMPL_INT_ATTR(nsHTMLSharedListElement, Start, start)
NS_IMPL_STRING_ATTR(nsHTMLSharedListElement, Type, type)


nsHTMLValue::EnumTable kListTypeTable[] = {
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

nsHTMLValue::EnumTable kOldListTypeTable[] = {
  { "1", NS_STYLE_LIST_STYLE_OLD_DECIMAL },
  { "A", NS_STYLE_LIST_STYLE_OLD_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_OLD_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_OLD_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_OLD_LOWER_ROMAN },
  { 0 }
};

PRBool
nsHTMLSharedListElement::ParseAttribute(nsIAtom* aAttribute,
                                        const nsAString& aValue,
                                        nsAttrValue& aResult)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::ol) ||
      mNodeInfo->Equals(nsHTMLAtoms::ul)) {
    if (aAttribute == nsHTMLAtoms::type) {
      return aResult.ParseEnumValue(aValue, kListTypeTable) ||
             aResult.ParseEnumValue(aValue, kOldListTypeTable, PR_TRUE);
    }
    if (aAttribute == nsHTMLAtoms::start) {
      return aResult.ParseIntValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLSharedListElement::AttributeToString(nsIAtom* aAttribute,
                                           const nsHTMLValue& aValue,
                                           nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type &&
      (mNodeInfo->Equals(nsHTMLAtoms::ol) ||
       mNodeInfo->Equals(nsHTMLAtoms::ul))) {
    PRInt32 v = aValue.GetIntValue();
    switch (v) {
      case NS_STYLE_LIST_STYLE_OLD_DECIMAL:
      case NS_STYLE_LIST_STYLE_OLD_LOWER_ROMAN:
      case NS_STYLE_LIST_STYLE_OLD_UPPER_ROMAN:
      case NS_STYLE_LIST_STYLE_OLD_LOWER_ALPHA:
      case NS_STYLE_LIST_STYLE_OLD_UPPER_ALPHA:
        aValue.EnumValueToString(kOldListTypeTable, aResult);
        break;
      default:
        aValue.EnumValueToString(kListTypeTable, aResult);
        break;
    }

    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_List) {
    if (aData->mListData->mType.GetUnit() == eCSSUnit_Null) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::type);
      if (value) {
        if (value->Type() == nsAttrValue::eEnum)
          aData->mListData->mType.SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
        else
          aData->mListData->mType.SetIntValue(NS_STYLE_LIST_STYLE_DECIMAL, eCSSUnit_Enumerated);
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLSharedListElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::ol) ||
      mNodeInfo->Equals(nsHTMLAtoms::ul)) {
    static const MappedAttributeEntry attributes[] = {
      { &nsHTMLAtoms::type },
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

NS_IMETHODIMP
nsHTMLSharedListElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::ol) ||
      mNodeInfo->Equals(nsHTMLAtoms::ul)) {
    aMapRuleFunc = &MapAttributesIntoRule;
  }
  else {
    nsGenericHTMLElement::GetAttributeMappingFunction(aMapRuleFunc);
  }
  return NS_OK;
}
