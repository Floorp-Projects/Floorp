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
#include "nsIDOMHTMLPreElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsCSSStruct.h"

// XXX wrap, variable, cols, tabstop


class nsHTMLPreElement : public nsGenericHTMLElement,
                         public nsIDOMHTMLPreElement
{
public:
  nsHTMLPreElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLPreElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLPreElement
  NS_IMETHOD GetWidth(PRInt32* aWidth);
  NS_IMETHOD SetWidth(PRInt32 aWidth);

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Pre)


nsHTMLPreElement::nsHTMLPreElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLPreElement::~nsHTMLPreElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLPreElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLPreElement, nsGenericElement)


// QueryInterface implementation for nsHTMLPreElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLPreElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLPreElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLPreElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Pre)


NS_IMPL_INT_ATTR(nsHTMLPreElement, Width, width)


PRBool
nsHTMLPreElement::ParseAttribute(nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::cols) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }
  if (aAttribute == nsHTMLAtoms::width) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_Font) {
    // variable
    if (aAttributes->GetAttr(nsHTMLAtoms::variable))
      aData->mFontData->mFamily.SetStringValue(NS_LITERAL_STRING("serif"),
                                               eCSSUnit_String);
  }
  else if (aData->mSID == eStyleStruct_Position) {
    if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
      // width: int (html4 attribute == nav4 cols)
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::width);
      if (!value || value->Type() != nsAttrValue::eInteger) {
        // cols: int (nav4 attribute)
        value = aAttributes->GetAttr(nsHTMLAtoms::cols);
      }

      if (value && value->Type() == nsAttrValue::eInteger)
        aData->mPositionData->mWidth.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Char);
    }
  }
  else if (aData->mSID == eStyleStruct_Text) {
    if (aData->mTextData->mWhiteSpace.GetUnit() == eCSSUnit_Null) {
      // wrap: empty
      if (aAttributes->GetAttr(nsHTMLAtoms::wrap))
        aData->mTextData->mWhiteSpace.SetIntValue(NS_STYLE_WHITESPACE_MOZ_PRE_WRAP, eCSSUnit_Enumerated);

      // width: int (html4 attribute == nav4 cols)
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::width);
      if (!value || value->Type() != nsAttrValue::eInteger) {
        // cols: int (nav4 attribute)
        value = aAttributes->GetAttr(nsHTMLAtoms::cols);
      }

      if (value && value->Type() == nsAttrValue::eInteger) {
        // Force wrap property on since we want to wrap at a width
        // boundary not just a newline.
        aData->mTextData->mWhiteSpace.SetIntValue(NS_STYLE_WHITESPACE_MOZ_PRE_WRAP, eCSSUnit_Enumerated);
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLPreElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsHTMLAtoms::variable },
    { &nsHTMLAtoms::wrap },
    { &nsHTMLAtoms::cols },
    { &nsHTMLAtoms::width },
    { nsnull },
  };
  
  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

NS_IMETHODIMP
nsHTMLPreElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}
