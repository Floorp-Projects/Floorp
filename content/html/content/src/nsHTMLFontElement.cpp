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
#include "nsCOMPtr.h"
#include "nsIDOMHTMLFontElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIDeviceContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsMappedAttributes.h"
#include "nsCSSStruct.h"
#include "nsRuleNode.h"
#include "nsIDocument.h"

class nsHTMLFontElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLFontElement
{
public:
  nsHTMLFontElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLFontElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLFontElement
  NS_DECL_NSIDOMHTMLFONTELEMENT

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Font)


nsHTMLFontElement::nsHTMLFontElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLFontElement::~nsHTMLFontElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLFontElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLFontElement, nsGenericElement)


// QueryInterface implementation for nsHTMLFontElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLFontElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLFontElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFontElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Font)


NS_IMPL_STRING_ATTR(nsHTMLFontElement, Color, color)
NS_IMPL_STRING_ATTR(nsHTMLFontElement, Face, face)
NS_IMPL_STRING_ATTR(nsHTMLFontElement, Size, size)


PRBool
nsHTMLFontElement::ParseAttribute(nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::size) {
    nsAutoString tmp(aValue);
    PRInt32 ec, v = tmp.ToInteger(&ec);
    if(NS_SUCCEEDED(ec)) {
      tmp.CompressWhitespace(PR_TRUE, PR_FALSE);
      PRUnichar ch = tmp.First();
      aResult.SetTo(v, (ch == '+' || ch == '-') ?
                       nsAttrValue::eEnum : nsAttrValue::eInteger);
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  if (aAttribute == nsHTMLAtoms::pointSize ||
      aAttribute == nsHTMLAtoms::fontWeight) {
    return aResult.ParseIntValue(aValue);
  }
  if (aAttribute == nsHTMLAtoms::color) {
    return aResult.ParseColor(aValue, nsGenericHTMLElement::GetOwnerDocument());
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLFontElement::AttributeToString(nsIAtom* aAttribute,
                                     const nsHTMLValue& aValue,
                                     nsAString& aResult) const
{
  if ((aAttribute == nsHTMLAtoms::size) ||
      (aAttribute == nsHTMLAtoms::pointSize) ||
      (aAttribute == nsHTMLAtoms::fontWeight)) {
    if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
      nsAutoString intVal;
      PRInt32 value = aValue.GetIntValue(); 
      intVal.AppendInt(value, 10);
      if (value >= 0) {
        aResult = NS_LITERAL_STRING("+") + intVal;
      }
      else {
        aResult = intVal;
      }
      return NS_CONTENT_ATTR_HAS_VALUE;
    }

    return NS_CONTENT_ATTR_NOT_THERE;
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_Font) {
    nsRuleDataFont& font = *(aData->mFontData);
    
    // face: string list
    if (font.mFamily.GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::face);
      if (value && value->Type() == nsAttrValue::eString &&
          !value->IsEmptyString()) {
        font.mFamily.SetStringValue(value->GetStringValue(), eCSSUnit_String);
        font.mFamilyFromHTML = PR_TRUE;
      }
    }

    // pointSize: int
    if (font.mSize.GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::pointSize);
      if (value && value->Type() == nsAttrValue::eInteger)
        font.mSize.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Point);
      else {
        // size: int, enum , 
        value = aAttributes->GetAttr(nsHTMLAtoms::size);
        if (value) {
          nsAttrValue::ValueType unit = value->Type();
          if (unit == nsAttrValue::eInteger || unit == nsAttrValue::eEnum) { 
            PRInt32 size;
            if (unit == nsAttrValue::eEnum) // int (+/-)
              size = value->GetEnumValue() + 3;  // XXX should be BASEFONT, not three see bug 3875
            else
              size = value->GetIntegerValue();

            size = ((0 < size) ? ((size < 8) ? size : 7) : 1); 
            font.mSize.SetIntValue(size, eCSSUnit_Enumerated);
          }
        }
      }
    }

    // fontWeight: int
    if (font.mWeight.GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::fontWeight);
      if (value && value->Type() == nsAttrValue::eInteger) // +/-
        font.mWeight.SetIntValue(value->GetIntegerValue(), eCSSUnit_Integer);
    }
  }
  else if (aData->mSID == eStyleStruct_Color) {
    if (aData->mColorData->mColor.GetUnit() == eCSSUnit_Null) {
      // color: color
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::color);
      nscolor color;
      if (value && value->GetColorValue(color)) {
        aData->mColorData->mColor.SetColorValue(color);
      }
    }
  }
  else if (aData->mSID == eStyleStruct_TextReset) {
    // Make <a><font color="red">text</font></a> give the text a red underline
    // in quirks mode.  The NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL flag only
    // affects quirks mode rendering.
    const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::color);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      nsCSSValue& decoration = aData->mTextData->mDecoration;
      PRInt32 newValue = NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL;
      if (decoration.GetUnit() == eCSSUnit_Enumerated) {
        newValue |= decoration.GetIntValue();
      }
      decoration.SetIntValue(newValue, eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLFontElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsHTMLAtoms::face },
    { &nsHTMLAtoms::pointSize },
    { &nsHTMLAtoms::size },
    { &nsHTMLAtoms::fontWeight },
    { &nsHTMLAtoms::color },
    { nsnull }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}


NS_IMETHODIMP
nsHTMLFontElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}
