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
#include "nsIDOMHTMLHRElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"
#include "nsRuleNode.h"

class nsHTMLHRElement : public nsGenericHTMLLeafElement,
                        public nsIDOMHTMLHRElement
{
public:
  nsHTMLHRElement();
  virtual ~nsHTMLHRElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLHRElement
  NS_DECL_NSIDOMHTMLHRELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
};

nsresult
NS_NewHTMLHRElement(nsIHTMLContent** aInstancePtrResult,
                    nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLHRElement* it = new nsHTMLHRElement();

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


nsHTMLHRElement::nsHTMLHRElement()
{
}

nsHTMLHRElement::~nsHTMLHRElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLHRElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLHRElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLHRElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLHRElement,
                                    nsGenericHTMLLeafElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLHRElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLHRElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLHRElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLHRElement* it = new nsHTMLHRElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLHRElement, Align, align)
NS_IMPL_BOOL_ATTR(nsHTMLHRElement, NoShade, noshade)
NS_IMPL_STRING_ATTR(nsHTMLHRElement, Size, size)
NS_IMPL_STRING_ATTR(nsHTMLHRElement, Width, width)

static nsGenericHTMLElement::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

NS_IMETHODIMP
nsHTMLHRElement::StringToAttribute(nsIAtom* aAttribute,
                                   const nsAReadableString& aValue,
                                   nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::width) {
    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    if (ParseValue(aValue, 1, 100, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::noshade) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (ParseEnumValue(aValue, kAlignTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLHRElement::AttributeToString(nsIAtom* aAttribute,
                                   const nsHTMLValue& aValue,
                                   nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      EnumValueToString(aValue, kAlignTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLLeafElement::AttributeToString(aAttribute, aValue,
                                                     aResult);
}

static void
MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (!aAttributes || !aData)
    return;

  if (aData->mSID == eStyleStruct_Margin) {
    nsCSSRect* margin = aData->mMarginData->mMargin;
    nsHTMLValue value;
    // align: enum
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (eHTMLUnit_Enumerated == value.GetUnit()) {
      // Map align attribute into auto side margins
      switch (value.GetIntValue()) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        if (margin->mLeft.GetUnit() == eCSSUnit_Null)
          margin->mLeft.SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (margin->mRight.GetUnit() == eCSSUnit_Null)
          margin->mRight.SetAutoValue();
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        if (margin->mLeft.GetUnit() == eCSSUnit_Null)
          margin->mLeft.SetAutoValue();
        if (margin->mRight.GetUnit() == eCSSUnit_Null)
          margin->mRight.SetFloatValue(0.0f, eCSSUnit_Pixel);
        break;
      case NS_STYLE_TEXT_ALIGN_CENTER:
        if (margin->mLeft.GetUnit() == eCSSUnit_Null)
          margin->mLeft.SetAutoValue();
        if (margin->mRight.GetUnit() == eCSSUnit_Null)
          margin->mRight.SetAutoValue();
        break;
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Position) {
    nsHTMLValue value;
    // width: pixel, percent
    if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::width, value);
      if (value.GetUnit() == eHTMLUnit_Pixel)
        aData->mPositionData->mWidth.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
      else if (value.GetUnit() == eHTMLUnit_Percent)
        aData->mPositionData->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // size: pixel
    if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::size, value);
      if (value.GetUnit() == eHTMLUnit_Pixel)
        aData->mPositionData->mHeight.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLHRElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                          PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::noshade) {
    aHint = NS_STYLE_HINT_VISUAL;
  }
  else if ((aAttribute == nsHTMLAtoms::align) ||
             (aAttribute == nsHTMLAtoms::width) ||
             (aAttribute == nsHTMLAtoms::size)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLHRElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLHRElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
