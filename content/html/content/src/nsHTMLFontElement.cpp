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
  nsHTMLFontElement();
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

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD_(PRBool) HasAttributeDependentStyle(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
};

nsresult
NS_NewHTMLFontElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLFontElement* it = new nsHTMLFontElement();

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


nsHTMLFontElement::nsHTMLFontElement()
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


nsresult
nsHTMLFontElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLFontElement* it = new nsHTMLFontElement();

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


NS_IMPL_STRING_ATTR(nsHTMLFontElement, Color, color)
NS_IMPL_STRING_ATTR(nsHTMLFontElement, Face, face)
NS_IMPL_STRING_ATTR(nsHTMLFontElement, Size, size)


NS_IMETHODIMP
nsHTMLFontElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsHTMLValue& aResult)
{
  if ((aAttribute == nsHTMLAtoms::size) ||
      (aAttribute == nsHTMLAtoms::pointSize) ||
      (aAttribute == nsHTMLAtoms::fontWeight)) {
    nsAutoString tmp(aValue);
      //rickg: fixed flaw where ToInteger error code was not being checked.
      //       This caused wrong default value for font size.
    PRInt32 ec, v = tmp.ToInteger(&ec);
    if(NS_SUCCEEDED(ec)){
      tmp.CompressWhitespace(PR_TRUE, PR_FALSE);
      PRUnichar ch = tmp.IsEmpty() ? 0 : tmp.First();
      aResult.SetIntValue(v, ((ch == '+') || (ch == '-')) ?
                          eHTMLUnit_Integer : eHTMLUnit_Enumerated);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::color) {
    if (aResult.ParseColor(aValue, nsGenericHTMLElement::GetOwnerDocument())) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLFontElement::AttributeToString(nsIAtom* aAttribute,
                                     const nsHTMLValue& aValue,
                                     nsAString& aResult) const
{
  if ((aAttribute == nsHTMLAtoms::size) ||
      (aAttribute == nsHTMLAtoms::pointSize) ||
      (aAttribute == nsHTMLAtoms::fontWeight)) {
    aResult.Truncate();
    nsAutoString intVal;
    if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
      intVal.AppendInt(aValue.GetIntValue(), 10);
      aResult.Append(intVal);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
    else if (aValue.GetUnit() == eHTMLUnit_Integer) {
      PRInt32 value = aValue.GetIntValue(); 
      if (value >= 0) {
        aResult.Append(NS_LITERAL_STRING("+"));
      }
      intVal.AppendInt(value, 10);      
      aResult.Append(intVal);
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
    nsHTMLValue value;
    
    // face: string list
    if (font.mFamily.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::face, value);
      if (value.GetUnit() == eHTMLUnit_String) {
        nsAutoString familyList;
        value.GetStringValue(familyList);
        if (!familyList.IsEmpty()) {
          font.mFamily.SetStringValue(familyList, eCSSUnit_String);
          font.mFamilyFromHTML = PR_TRUE;
        }
      }
    }

    // pointSize: int, enum
    if (font.mSize.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::pointSize, value);
      if (value.GetUnit() == eHTMLUnit_Integer ||
          value.GetUnit() == eHTMLUnit_Enumerated) {
        PRInt32 val = value.GetIntValue();
        font.mSize.SetFloatValue((float)val, eCSSUnit_Point);
      }
      else {
        // size: int, enum , 
        aAttributes->GetAttribute(nsHTMLAtoms::size, value);
        nsHTMLUnit unit = value.GetUnit();
        if (unit == eHTMLUnit_Integer || unit == eHTMLUnit_Enumerated) { 
          PRInt32 size = value.GetIntValue();
          if (unit == eHTMLUnit_Integer) // int (+/-)
            size += 3;  // XXX should be BASEFONT, not three
	            
          size = ((0 < size) ? ((size < 8) ? size : 7) : 1); 
          font.mSize.SetIntValue(size, eCSSUnit_Enumerated);
        }
      }
    }

    // fontWeight: int, enum
    if (font.mWeight.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::fontWeight, value);
      if (value.GetUnit() == eHTMLUnit_Integer) // +/-
        font.mWeight.SetIntValue(value.GetIntValue(), eCSSUnit_Integer);
      else if (value.GetUnit() == eHTMLUnit_Enumerated)
        font.mWeight.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
    }
  }
  else if (aData->mSID == eStyleStruct_Color) {
    if (aData->mColorData->mColor.GetUnit() == eCSSUnit_Null) {
      // color: color
      nsHTMLValue value;
      if (NS_CONTENT_ATTR_NOT_THERE !=
          aAttributes->GetAttribute(nsHTMLAtoms::color, value)) {
        if (((eHTMLUnit_Color == value.GetUnit())) ||
            (eHTMLUnit_ColorName == value.GetUnit()))
          aData->mColorData->mColor.SetColorValue(value.GetColorValue());
      }
    }
  }
  else if (aData->mSID == eStyleStruct_TextReset) {
    // Make <a><font color="red">text</font></a> give the text a red underline
    // in quirks mode.  The NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL flag only
    // affects quirks mode rendering.
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_NOT_THERE !=
        aAttributes->GetAttribute(nsHTMLAtoms::color, value) &&
        (eHTMLUnit_Color == value.GetUnit() ||
         eHTMLUnit_ColorName == value.GetUnit())) {
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
nsHTMLFontElement::HasAttributeDependentStyle(const nsIAtom* aAttribute) const
{
  static const AttributeDependenceEntry attributes[] = {
    { &nsHTMLAtoms::face },
    { &nsHTMLAtoms::pointSize },
    { &nsHTMLAtoms::size },
    { &nsHTMLAtoms::fontWeight },
    { &nsHTMLAtoms::color },
    { nsnull }
  };

  static const AttributeDependenceEntry* const map[] = {
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
