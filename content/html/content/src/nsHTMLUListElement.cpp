/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLUListElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsMappedAttributes.h"
#include "nsRuleNode.h"

extern nsHTMLValue::EnumTable kListTypeTable[];
extern nsHTMLValue::EnumTable kOldListTypeTable[];

class nsHTMLUListElement : public nsGenericHTMLElement,
                           public nsIDOMHTMLUListElement
{
public:
  nsHTMLUListElement();
  virtual ~nsHTMLUListElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLUListElement
  NS_DECL_NSIDOMHTMLULISTELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD_(PRBool) HasAttributeDependentStyle(const nsIAtom* aAttribute) const;
};

nsresult
NS_NewHTMLUListElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLUListElement* it = new nsHTMLUListElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLUListElement::nsHTMLUListElement()
{
}

nsHTMLUListElement::~nsHTMLUListElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLUListElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLUListElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLUListElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLUListElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLUListElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLUListElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLUListElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLUListElement* it = new nsHTMLUListElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMPL_BOOL_ATTR(nsHTMLUListElement, Compact, compact)
NS_IMPL_STRING_ATTR(nsHTMLUListElement, Type, type)


NS_IMETHODIMP
nsHTMLUListElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (aResult.ParseEnumValue(aValue, kListTypeTable)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }

    if (aResult.ParseEnumValue(aValue, kOldListTypeTable, PR_TRUE)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  if (aAttribute == nsHTMLAtoms::start) {
    if (aResult.ParseIntWithBounds(aValue, eHTMLUnit_Integer, 1)) {
      return NS_CONTENT_ATTR_HAS_VALUE; 
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLUListElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    PRInt32 v = aValue.GetIntValue();
    switch (v) {
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
      nsHTMLValue value;
      // type: enum
      aAttributes->GetAttribute(nsHTMLAtoms::type, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated)
        aData->mListData->mType.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
      else if (value.GetUnit() != eHTMLUnit_Null)
        aData->mListData->mType.SetIntValue(NS_STYLE_LIST_STYLE_DISC, eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLUListElement::HasAttributeDependentStyle(const nsIAtom* aAttribute) const
{
  static const AttributeDependenceEntry attributes[] = {
    { &nsHTMLAtoms::type },
    { nsnull }
  };

  static const AttributeDependenceEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}


NS_IMETHODIMP
nsHTMLUListElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}
