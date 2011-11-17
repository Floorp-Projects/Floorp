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

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsIDOMHTMLFontElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsIDocument.h"
#include "nsAlgorithm.h"

using namespace mozilla;

class nsHTMLFontElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLFontElement
{
public:
  nsHTMLFontElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLFontElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLFontElement
  NS_DECL_NSIDOMHTMLFONTELEMENT

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Font)


nsHTMLFontElement::nsHTMLFontElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLFontElement::~nsHTMLFontElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLFontElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLFontElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLFontElement, nsHTMLFontElement)

// QueryInterface implementation for nsHTMLFontElement
NS_INTERFACE_TABLE_HEAD(nsHTMLFontElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLFontElement, nsIDOMHTMLFontElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLFontElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLFontElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLFontElement)


NS_IMPL_STRING_ATTR(nsHTMLFontElement, Color, color)
NS_IMPL_STRING_ATTR(nsHTMLFontElement, Face, face)
NS_IMPL_STRING_ATTR(nsHTMLFontElement, Size, size)

static const nsAttrValue::EnumTable kRelFontSizeTable[] = {
  { "-10", -10 },
  { "-9", -9 },
  { "-8", -8 },
  { "-7", -7 },
  { "-6", -6 },
  { "-5", -5 },
  { "-4", -4 },
  { "-3", -3 },
  { "-2", -2 },
  { "-1", -1 },
  { "-0", 0 },
  { "+0", 0 },
  { "+1", 1 },
  { "+2", 2 },
  { "+3", 3 },
  { "+4", 4 },
  { "+5", 5 },
  { "+6", 6 },
  { "+7", 7 },
  { "+8", 8 },
  { "+9", 9 },
  { "+10", 10 },
  { 0 }
};


bool
nsHTMLFontElement::ParseAttribute(PRInt32 aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::size) {
      nsAutoString tmp(aValue);
      tmp.CompressWhitespace(true, true);
      PRUnichar ch = tmp.IsEmpty() ? 0 : tmp.First();
      if ((ch == '+' || ch == '-')) {
          if (aResult.ParseEnumValue(aValue, kRelFontSizeTable, false))
              return true;

          // truncate after digit, then parse it again.
          PRUint32 i;
          for (i = 1; i < tmp.Length(); i++) {
              ch = tmp.CharAt(i);
              if (!nsCRT::IsAsciiDigit(ch)) {
                  tmp.Truncate(i);
                  break;
              }
          }
          return aResult.ParseEnumValue(tmp, kRelFontSizeTable, false);
      }

      return aResult.ParseIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::pointSize ||
        aAttribute == nsGkAtoms::fontWeight) {
      return aResult.ParseIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::color) {
      return aResult.ParseColor(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Font)) {
    // face: string list
    nsCSSValue* family = aData->ValueForFontFamily();
    if (family->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::face);
      if (value && value->Type() == nsAttrValue::eString &&
          !value->IsEmptyString()) {
        family->SetStringValue(value->GetStringValue(), eCSSUnit_Families);
      }
    }

    // pointSize: int
    nsCSSValue* fontSize = aData->ValueForFontSize();
    if (fontSize->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::pointSize);
      if (value && value->Type() == nsAttrValue::eInteger)
        fontSize->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Point);
      else {
        // size: int, enum , 
        value = aAttributes->GetAttr(nsGkAtoms::size);
        if (value) {
          nsAttrValue::ValueType unit = value->Type();
          if (unit == nsAttrValue::eInteger || unit == nsAttrValue::eEnum) { 
            PRInt32 size;
            if (unit == nsAttrValue::eEnum) // int (+/-)
              size = value->GetEnumValue() + 3;
            else
              size = value->GetIntegerValue();

            size = clamped(size, 1, 7);
            fontSize->SetIntValue(size, eCSSUnit_Enumerated);
          }
        }
      }
    }

    // fontWeight: int
    nsCSSValue* fontWeight = aData->ValueForFontWeight();
    if (fontWeight->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::fontWeight);
      if (value && value->Type() == nsAttrValue::eInteger) // +/-
        fontWeight->SetIntValue(value->GetIntegerValue(), eCSSUnit_Integer);
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    nsCSSValue* colorValue = aData->ValueForColor();
    if (colorValue->GetUnit() == eCSSUnit_Null &&
        aData->mPresContext->UseDocumentColors()) {
      // color: color
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::color);
      nscolor color;
      if (value && value->GetColorValue(color)) {
        colorValue->SetColorValue(color);
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(TextReset) &&
      aData->mPresContext->CompatibilityMode() == eCompatibility_NavQuirks) {
    // Make <a><font color="red">text</font></a> give the text a red underline
    // in quirks mode.  The NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL flag only
    // affects quirks mode rendering.
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::color);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      nsCSSValue* decoration = aData->ValueForTextDecorationLine();
      PRInt32 newValue = NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL;
      if (decoration->GetUnit() == eCSSUnit_Enumerated) {
        newValue |= decoration->GetIntValue();
      }
      decoration->SetIntValue(newValue, eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLFontElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::face },
    { &nsGkAtoms::pointSize },
    { &nsGkAtoms::size },
    { &nsGkAtoms::fontWeight },
    { &nsGkAtoms::color },
    { nsnull }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, ArrayLength(map));
}


nsMapRuleToAttributesFunc
nsHTMLFontElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}
