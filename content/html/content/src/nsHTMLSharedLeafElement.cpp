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
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLIsIndexElement.h"
#include "nsIDOMHTMLParamElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsRuleNode.h"
#include "nsIHTMLAttributes.h"


class nsHTMLSharedLeafElement : public nsGenericHTMLLeafElement,
                                public nsIDOMHTMLEmbedElement,
                                public nsIDOMHTMLIsIndexElement,
                                public nsIDOMHTMLParamElement,
                                public nsIDOMHTMLBaseElement
{
public:
  nsHTMLSharedLeafElement();
  virtual ~nsHTMLSharedLeafElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLEmbedElement
  NS_DECL_NSIDOMHTMLEMBEDELEMENT

  // nsIDOMHTMLIsIndexElement
  NS_DECL_NSIDOMHTMLISINDEXELEMENT

  // nsIDOMHTMLParamElement Can't use the macro
  // NS_DECL_NSIDOMHTMLPARAMELEMENT since some of the methods in
  // nsIDOMHTMLParamElement clashes with methods in
  // nsIDOMHTMLEmbedElement

  NS_IMETHOD GetValue(nsAWritableString& aValue);
  NS_IMETHOD SetValue(const nsAReadableString& aValue);
  NS_IMETHOD GetValueType(nsAWritableString& aValueType);
  NS_IMETHOD SetValueType(const nsAReadableString& aValueType);

  // nsIDOMHTMLBaseElement
  NS_DECL_NSIDOMHTMLBASEELEMENT

  // nsIContent
  NS_IMETHODIMP_(PRBool) IsContentOfType(PRUint32 aFlags);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32 aModType, PRInt32& aHint) const;

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
};

nsresult
NS_NewHTMLSharedLeafElement(nsIHTMLContent** aInstancePtrResult,
                            nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLSharedLeafElement* it = new nsHTMLSharedLeafElement();

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


nsHTMLSharedLeafElement::nsHTMLSharedLeafElement()
{
}

nsHTMLSharedLeafElement::~nsHTMLSharedLeafElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSharedLeafElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLSharedLeafElement, nsGenericElement)


// QueryInterface implementation for nsHTMLSharedLeafElement
NS_HTML_CONTENT_INTERFACE_MAP_AMBIGOUS_BEGIN(nsHTMLSharedLeafElement,
                                             nsGenericHTMLLeafElement,
                                             nsIDOMHTMLEmbedElement)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMHTMLElement, nsIDOMHTMLEmbedElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLEmbedElement, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLParamElement, param)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLIsIndexElement, isindex)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLBaseElement, base)

  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLEmbedElement, embed)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLParamElement, param)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLWBRElement, wbr)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLIsIndexElement, isindex)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLBaseElement, base)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLSpacerElement, spacer)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLSharedLeafElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLSharedLeafElement* it = new nsHTMLSharedLeafElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsISupports> kungFuDeathGrip =
    NS_STATIC_CAST(nsIDOMHTMLEmbedElement *, it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMHTMLEmbedElement *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

/////////////////////////////////////////////
// Implement nsIDOMHTMLEmbedElement interface
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Height, height)
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Width, width)
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Src, src)

// nsIDOMHTMLParamElement
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Value, value)
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, ValueType, valuetype)

// nsIDOMHTMLIsIndexElement
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Prompt, prompt)


NS_IMETHODIMP
nsHTMLSharedLeafElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
#ifdef GATHER_ELEMENT_USEAGE_STATISTICS
  extern void GEUS_DumpElementCounts();

  GEUS_DumpElementCounts();
#endif

  *aForm = nsnull;/* XXX */
  return NS_OK;
}


NS_IMETHODIMP_(PRBool)
nsHTMLSharedLeafElement::IsContentOfType(PRUint32 aFlags)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::wbr)) {
    return !(aFlags & ~(eELEMENT | eHTML | eHTML_FORM_CONTROL));
  }

  return nsGenericHTMLLeafElement::IsContentOfType(aFlags);
}

// nsIDOMHTMLBaseElement
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Href, href)
NS_IMPL_STRING_ATTR(nsHTMLSharedLeafElement, Target, target)

// spacer element code

NS_IMETHODIMP
nsHTMLSharedLeafElement::StringToAttribute(nsIAtom* aAttribute,
                                           const nsAReadableString& aValue,
                                           nsHTMLValue& aResult)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    if (aAttribute == nsHTMLAtoms::size) {
      if (ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    } else if (aAttribute == nsHTMLAtoms::align) {
      if (ParseAlignValue(aValue, aResult)) {
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    } else if ((aAttribute == nsHTMLAtoms::width) ||
               (aAttribute == nsHTMLAtoms::height)) {
      if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  }

  return nsGenericHTMLLeafElement::StringToAttribute(aAttribute, aValue,
                                                     aResult);
}

NS_IMETHODIMP
nsHTMLSharedLeafElement::AttributeToString(nsIAtom* aAttribute,
                                           const nsHTMLValue& aValue,
                                           nsAWritableString& aResult) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    if (aAttribute == nsHTMLAtoms::align) {
      if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
        AlignValueToString(aValue, aResult);
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  }

  return nsGenericHTMLLeafElement::AttributeToString(aAttribute, aValue,
                                                     aResult);
}

static void
SpacerMapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                            nsRuleData* aData)
{
  if (!aAttributes || !aData)
    return;

  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImagePositionAttributeInto(aAttributes, aData);

  if (aData->mPositionData) {
    nsHTMLValue value;

    const nsStyleDisplay* display = (const nsStyleDisplay*)
      aData->mStyleContext->GetStyleData(eStyleStruct_Display);

    PRBool typeIsBlock = (display->mDisplay == NS_STYLE_DISPLAY_BLOCK);

    if (typeIsBlock) {
      // width: value
      if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
        aAttributes->GetAttribute(nsHTMLAtoms::width, value);
        if (value.GetUnit() == eHTMLUnit_Pixel) {
          aData->mPositionData->
            mWidth.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
        } else if (value.GetUnit() == eHTMLUnit_Percent) {
          aData->mPositionData->
            mWidth.SetPercentValue(value.GetPercentValue());
        }
      }

      // height: value
      if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
        aAttributes->GetAttribute(nsHTMLAtoms::height, value);
        if (value.GetUnit() == eHTMLUnit_Pixel) {
          aData->mPositionData->
            mHeight.SetFloatValue((float)value.GetPixelValue(),
                                  eCSSUnit_Pixel);
        } else if (value.GetUnit() == eHTMLUnit_Percent) {
          aData->mPositionData->
            mHeight.SetPercentValue(value.GetPercentValue());
        }
      }
    } else {
      // size: value
      if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
        aAttributes->GetAttribute(nsHTMLAtoms::size, value);
        if (value.GetUnit() == eHTMLUnit_Pixel)
          aData->mPositionData->
            mWidth.SetFloatValue((float)value.GetPixelValue(),
                                 eCSSUnit_Pixel);
      }
    }
  } else if (aData->mDisplayData) {
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      PRUint8 align = (PRUint8)(value.GetIntValue());
      if (aData->mDisplayData &&
          aData->mDisplayData->mFloat.GetUnit() == eCSSUnit_Null) {
        if (align == NS_STYLE_TEXT_ALIGN_LEFT)
          aData->mDisplayData->mFloat.SetIntValue(NS_STYLE_FLOAT_LEFT,
                                                  eCSSUnit_Enumerated);
        else if (align == NS_STYLE_TEXT_ALIGN_RIGHT)
          aData->mDisplayData->mFloat.SetIntValue(NS_STYLE_FLOAT_RIGHT,
                                                  eCSSUnit_Enumerated);
      }
    }

    if (aData->mDisplayData->mDisplay == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::type, value);
      if (eHTMLUnit_String == value.GetUnit()) {
        nsAutoString tmp;
        value.GetStringValue(tmp);
        if (tmp.EqualsIgnoreCase("line") ||
            tmp.EqualsIgnoreCase("vert") ||
            tmp.EqualsIgnoreCase("vertical") ||
            tmp.EqualsIgnoreCase("block")) {
          // This is not strictly 100% compatible: if the spacer is given
          // a width of zero then it is basically ignored.
          aData->mDisplayData->mDisplay = NS_STYLE_DISPLAY_BLOCK;
        }
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static void
PlainMapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                           nsRuleData* aData)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}


NS_IMETHODIMP
nsHTMLSharedLeafElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                                  PRInt32 aModType,
                                                  PRInt32& aHint) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    if ((aAttribute == nsHTMLAtoms::usemap) ||
        (aAttribute == nsHTMLAtoms::ismap)) {
      aHint = NS_STYLE_HINT_FRAMECHANGE;
    } else if (aAttribute == nsHTMLAtoms::align) {
      aHint = NS_STYLE_HINT_REFLOW;
    } else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
      if (!GetImageMappedAttributesImpact(aAttribute, aHint)) {
        if (!GetImageBorderAttributeImpact(aAttribute, aHint)) {
          aHint = NS_STYLE_HINT_CONTENT;
        }
      }
    }
  }

  return nsGenericHTMLLeafElement::GetMappedAttributeImpact(aAttribute,
                                                            aModType, aHint);
}

NS_IMETHODIMP
nsHTMLSharedLeafElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    aMapRuleFunc = &SpacerMapAttributesIntoRule;
  } else {
    aMapRuleFunc = &PlainMapAttributesIntoRule;
  }

  return NS_OK;
}


#ifdef DEBUG
NS_IMETHODIMP
nsHTMLSharedLeafElement::SizeOf(nsISizeOfHandler* aSizer,
                                PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
