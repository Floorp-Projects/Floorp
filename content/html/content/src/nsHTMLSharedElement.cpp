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
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLIsIndexElement.h"
#include "nsIDOMHTMLParamElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLDirectoryElement.h"
#include "nsIDOMHTMLMenuElement.h"
#include "nsIDOMHTMLQuoteElement.h"
#include "nsIDOMHTMLBaseFontElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsImageLoadingContent.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsRuleNode.h"
#include "nsMappedAttributes.h"
#include "nsStyleContext.h"

// XXX nav4 has type= start= (same as OL/UL)
extern nsHTMLValue::EnumTable kListTypeTable[];

class nsHTMLSharedElement : public nsGenericHTMLElement,
                            public nsImageLoadingContent,
                            public nsIDOMHTMLEmbedElement,
                            public nsIDOMHTMLIsIndexElement,
                            public nsIDOMHTMLParamElement,
                            public nsIDOMHTMLBaseElement,
                            public nsIDOMHTMLDirectoryElement,
                            public nsIDOMHTMLMenuElement,
                            public nsIDOMHTMLQuoteElement,
                            public nsIDOMHTMLBaseFontElement
{
public:
  nsHTMLSharedElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLSharedElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLEmbedElement
  NS_DECL_NSIDOMHTMLEMBEDELEMENT

  // nsIDOMHTMLIsIndexElement
  NS_DECL_NSIDOMHTMLISINDEXELEMENT

  // nsIDOMHTMLParamElement Can't use the macro
  // NS_DECL_NSIDOMHTMLPARAMELEMENT since some of the methods in
  // nsIDOMHTMLParamElement clashes with methods in
  // nsIDOMHTMLEmbedElement
  NS_IMETHOD GetValue(nsAString& aValue);
  NS_IMETHOD SetValue(const nsAString& aValue);
  NS_IMETHOD GetValueType(nsAString& aValueType);
  NS_IMETHOD SetValueType(const nsAString& aValueType);

  // nsIDOMHTMLBaseElement
  NS_DECL_NSIDOMHTMLBASEELEMENT

  // nsIDOMHTMLDirectoryElement
  NS_DECL_NSIDOMHTMLDIRECTORYELEMENT

  // nsIDOMHTMLMenuElement
  // Same as directoryelement

  // nsIDOMHTMLQuoteElement
  NS_DECL_NSIDOMHTMLQUOTEELEMENT

  // nsIDOMHTMLBaseFontElement
  NS_DECL_NSIDOMHTMLBASEFONTELEMENT

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Shared)


nsHTMLSharedElement::nsHTMLSharedElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLSharedElement::~nsHTMLSharedElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSharedElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLSharedElement, nsGenericElement)


// QueryInterface implementation for nsHTMLSharedElement
NS_HTML_CONTENT_INTERFACE_MAP_AMBIGOUS_BEGIN(nsHTMLSharedElement,
                                             nsGenericHTMLElement,
                                             nsIDOMHTMLEmbedElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLEmbedElement, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(imgIDecoderObserver, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIImageLoadingContent, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLParamElement, param)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLIsIndexElement, isindex)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLBaseElement, base)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLDirectoryElement, dir)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLMenuElement, menu)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLQuoteElement, q)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLQuoteElement, blockquote)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLBaseFontElement, basefont)

  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLEmbedElement, embed)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLParamElement, param)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLWBRElement, wbr)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLIsIndexElement, isindex)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLBaseElement, base)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLSpacerElement, spacer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLDirectoryElement, dir)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLMenuElement, menu)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLQuoteElement, q)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLQuoteElement, blockquote)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLBaseFontElement, basefont)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLSharedElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  nsHTMLSharedElement *it = new nsHTMLSharedElement(mNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip =
    NS_STATIC_CAST(nsIDOMHTMLEmbedElement *, it);

  CopyInnerTo(it, aDeep);

  kungFuDeathGrip.swap(*aReturn);

  return NS_OK;
}

/////////////////////////////////////////////
// Implement nsIDOMHTMLEmbedElement interface
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Height, height)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Width, width)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Src, src)

// nsIDOMHTMLParamElement
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Value, value)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, ValueType, valuetype)

// nsIDOMHTMLIsIndexElement
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Prompt, prompt)

// nsIDOMHTMLDirectoryElement
NS_IMPL_BOOL_ATTR(nsHTMLSharedElement, Compact, compact)

// nsIDOMHTMLMenuElement
//NS_IMPL_BOOL_ATTR(nsHTMLSharedElement, Compact, compact)

// nsIDOMHTMLQuoteElement
NS_IMPL_URI_ATTR(nsHTMLSharedElement, Cite, cite)

// nsIDOMHTMLBaseFontElement
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Color, color)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Face, face)
NS_IMPL_INT_ATTR(nsHTMLSharedElement, Size, size)

NS_IMETHODIMP
nsHTMLSharedElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = FindForm().get();

  return NS_OK;
}

// nsIDOMHTMLBaseElement
NS_IMPL_URI_ATTR(nsHTMLSharedElement, Href, href)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Target, target)

// spacer element code

PRBool
nsHTMLSharedElement::ParseAttribute(nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    if (aAttribute == nsHTMLAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return PR_TRUE;
    }
  }
  else if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    if (aAttribute == nsHTMLAtoms::size) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsHTMLAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (aAttribute == nsHTMLAtoms::width ||
        aAttribute == nsHTMLAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
    }
  }
  else if (mNodeInfo->Equals(nsHTMLAtoms::dir) ||
           mNodeInfo->Equals(nsHTMLAtoms::menu)) {
    if (aAttribute == nsHTMLAtoms::type) {
      return aResult.ParseEnumValue(aValue, kListTypeTable);
    }
    if (aAttribute == nsHTMLAtoms::start) {
      return aResult.ParseIntWithBounds(aValue, 1);
    }
  }
  else if (mNodeInfo->Equals(nsHTMLAtoms::basefont)) {
    if (aAttribute == nsHTMLAtoms::size) {
      return aResult.ParseIntValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLSharedElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAString& aResult) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    if (aAttribute == nsHTMLAtoms::align) {
      if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
        AlignValueToString(aValue, aResult);
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  }
  else if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    if (aAttribute == nsHTMLAtoms::align) {
      if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
        AlignValueToString(aValue, aResult);
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  }
  else if (mNodeInfo->Equals(nsHTMLAtoms::dir) ||
           mNodeInfo->Equals(nsHTMLAtoms::menu)) {
    if (aAttribute == nsHTMLAtoms::type) {
      aValue.EnumValueToString(kListTypeTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static void
SpacerMapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                            nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);

  if (aData->mSID == eStyleStruct_Position) {
    const nsStyleDisplay* display = aData->mStyleContext->GetStyleDisplay();

    PRBool typeIsBlock = (display->mDisplay == NS_STYLE_DISPLAY_BLOCK);

    if (typeIsBlock) {
      // width: value
      if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
        const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::width);
        if (value && value->Type() == nsAttrValue::eInteger) {
          aData->mPositionData->
            mWidth.SetFloatValue((float)value->GetIntegerValue(),
                                 eCSSUnit_Pixel);
        } else if (value && value->Type() == nsAttrValue::ePercent) {
          aData->mPositionData->
            mWidth.SetPercentValue(value->GetPercentValue());
        }
      }

      // height: value
      if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
        const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::height);
        if (value && value->Type() == nsAttrValue::eInteger) {
          aData->mPositionData->
            mHeight.SetFloatValue((float)value->GetIntegerValue(),
                                  eCSSUnit_Pixel);
        } else if (value && value->Type() == nsAttrValue::ePercent) {
          aData->mPositionData->
            mHeight.SetPercentValue(value->GetPercentValue());
        }
      }
    } else {
      // size: value
      if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
        const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::size);
        if (value && value->Type() == nsAttrValue::eInteger)
          aData->mPositionData->
            mWidth.SetFloatValue((float)value->GetIntegerValue(),
                                 eCSSUnit_Pixel);
      }
    }
  } else if (aData->mSID == eStyleStruct_Display) {
    const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum) {
      PRInt32 align = value->GetEnumValue();
      if (aData->mDisplayData->mFloat.GetUnit() == eCSSUnit_Null) {
        if (align == NS_STYLE_TEXT_ALIGN_LEFT)
          aData->mDisplayData->mFloat.SetIntValue(NS_STYLE_FLOAT_LEFT,
                                                  eCSSUnit_Enumerated);
        else if (align == NS_STYLE_TEXT_ALIGN_RIGHT)
          aData->mDisplayData->mFloat.SetIntValue(NS_STYLE_FLOAT_RIGHT,
                                                  eCSSUnit_Enumerated);
      }
    }

    if (aData->mDisplayData->mDisplay == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::type);
      if (value && value->Type() == nsAttrValue::eString) {
        nsAutoString tmp(value->GetStringValue());
        if (tmp.LowerCaseEqualsLiteral("line") ||
            tmp.LowerCaseEqualsLiteral("vert") ||
            tmp.LowerCaseEqualsLiteral("vertical") ||
            tmp.LowerCaseEqualsLiteral("block")) {
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
EmbedMapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                           nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static void
DirectoryMenuMapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                               nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_List) {
    if (aData->mListData->mType.GetUnit() == eCSSUnit_Null) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::type);
      if (value) {
        if (value->Type() == nsAttrValue::eEnum)
          aData->mListData->mType.SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
        else
          aData->mListData->mType.SetIntValue(NS_STYLE_LIST_STYLE_DISC, eCSSUnit_Enumerated);
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLSharedElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    static const MappedAttributeEntry* const map[] = {
      sCommonAttributeMap,
      sImageMarginSizeAttributeMap,
      sImageAlignAttributeMap,
      sImageBorderAttributeMap
    };

    return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
  }

  if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    static const MappedAttributeEntry attributes[] = {
      // XXXldb This is just wrong.
      { &nsHTMLAtoms::usemap },
      { &nsHTMLAtoms::ismap },
      { &nsHTMLAtoms::align },
      { nsnull }
    };

    static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
      sImageMarginSizeAttributeMap,
      sImageBorderAttributeMap,
    };

    return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
  }

  if (mNodeInfo->Equals(nsHTMLAtoms::dir)) {
    static const MappedAttributeEntry attributes[] = {
      { &nsHTMLAtoms::type },
      // { &nsHTMLAtoms::compact }, // XXX
      { nsnull} 
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
nsHTMLSharedElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    aMapRuleFunc = &EmbedMapAttributesIntoRule;
  }
  else if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    aMapRuleFunc = &SpacerMapAttributesIntoRule;
  }
  else if (mNodeInfo->Equals(nsHTMLAtoms::dir) ||
           mNodeInfo->Equals(nsHTMLAtoms::menu)) {
    aMapRuleFunc = &DirectoryMenuMapAttributesIntoRule;
  }
  else {
    nsGenericHTMLElement::GetAttributeMappingFunction(aMapRuleFunc);
  }
  
  return NS_OK;
}
