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
#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsRuleData.h"
#include "nsMappedAttributes.h"
#include "nsStyleContext.h"

#ifdef MOZ_SVG
#include "nsIDOMGetSVGDocument.h"
#include "nsIDOMSVGDocument.h"
#include "nsIDocument.h"
#endif

// XXX nav4 has type= start= (same as OL/UL)
extern nsAttrValue::EnumTable kListTypeTable[];

class nsHTMLSharedElement : public nsGenericHTMLElement,
                            public nsObjectLoadingContent,
                            public nsIDOMHTMLEmbedElement,
#ifdef MOZ_SVG
                            public nsIDOMGetSVGDocument,
#endif
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

#ifdef MOZ_SVG
  // nsIDOMGetSVGDocument
  NS_DECL_NSIDOMGETSVGDOCUMENT
#endif

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

  // nsIContent
  // Have to override tabindex for <embed> to act right
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  
  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual PRInt32 IntrinsicState() const;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);

  // nsObjectLoadingContent
  virtual PRUint32 GetCapabilities() const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Shared)


nsHTMLSharedElement::nsHTMLSharedElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    nsObjectLoadingContent(aNodeInfo->Equals(nsHTMLAtoms::embed) ? eType_Plugin
                                                                 : eType_Null)
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
#ifdef MOZ_SVG
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMGetSVGDocument, embed)
#endif
  NS_INTERFACE_MAP_ENTRY_IF_TAG(imgIDecoderObserver, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIStreamListener, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIRequestObserver, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIFrameLoaderOwner, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIObjectLoadingContent, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIImageLoadingContent, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIInterfaceRequestor, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIChannelEventSink, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsISupportsWeakReference, embed)
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


NS_IMPL_DOM_CLONENODE(nsHTMLSharedElement)


/////////////////////////////////////////////
// Implement nsIDOMHTMLEmbedElement interface
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Height, height)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Width, width)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Src, src)

#ifdef MOZ_SVG
// nsIDOMGetSVGDocument
NS_IMETHODIMP
nsHTMLSharedElement::GetSVGDocument(nsIDOMSVGDocument** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = nsnull;

  if (!mNodeInfo->Equals(nsHTMLAtoms::embed) || !IsInDoc()) {
    return NS_OK;
  }

  nsIDocument *sub_doc = GetOwnerDoc()->GetSubDocumentFor(this);
  if (sub_doc) {
    CallQueryInterface(sub_doc, aResult);
  }

  return NS_OK;
}
#endif

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

// TabIndex for embed
NS_IMPL_INT_ATTR(nsHTMLSharedElement, TabIndex, tabindex)

  
NS_IMETHODIMP
nsHTMLSharedElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = FindForm().get();

  return NS_OK;
}

// nsIDOMHTMLBaseElement
NS_IMPL_URI_ATTR(nsHTMLSharedElement, Href, href)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Target, target)

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

// spacer element code

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

    if (aData->mDisplayData->mDisplay.GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::type);
      if (value && value->Type() == nsAttrValue::eString) {
        nsAutoString tmp(value->GetStringValue());
        if (tmp.LowerCaseEqualsLiteral("line") ||
            tmp.LowerCaseEqualsLiteral("vert") ||
            tmp.LowerCaseEqualsLiteral("vertical") ||
            tmp.LowerCaseEqualsLiteral("block")) {
          // This is not strictly 100% compatible: if the spacer is given
          // a width of zero then it is basically ignored.
          aData->mDisplayData->mDisplay.SetIntValue(NS_STYLE_DISPLAY_BLOCK,
                                                    eCSSUnit_Enumerated);
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
        if (value->Type() == nsAttrValue::eEnum) {
          aData->mListData->mType.SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
        } else {
          aData->mListData->mType.SetIntValue(NS_STYLE_LIST_STYLE_DISC, eCSSUnit_Enumerated);
        }
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

nsMapRuleToAttributesFunc
nsHTMLSharedElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    return &EmbedMapAttributesIntoRule;
  }
  if (mNodeInfo->Equals(nsHTMLAtoms::spacer)) {
    return &SpacerMapAttributesIntoRule;
  }
  else if (mNodeInfo->Equals(nsHTMLAtoms::dir) ||
           mNodeInfo->Equals(nsHTMLAtoms::menu)) {
    return &DirectoryMenuMapAttributesIntoRule;
  }

  return nsGenericHTMLElement::GetAttributeMappingFunction();
}


nsresult
nsHTMLSharedElement::BindToTree(nsIDocument* aDocument,
                                nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  // Must start loading stuff after being in a document
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    nsAutoString uri;
    nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, uri);
    if (rv != NS_CONTENT_ATTR_NOT_THERE) {
      nsAutoString type;
      GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);
      // Don't notify: We aren't in a document yet, so we have no frames
      ObjectURIChanged(uri, PR_FALSE, NS_ConvertUTF16toUTF8(type), PR_TRUE);
    } else {
      // The constructor set the type to eType_Plugin; but if we have no src
      // attribute, then we aren't really a plugin
      Fallback(PR_FALSE);
    }
  }
  return rv;
}

void
nsHTMLSharedElement::UnbindFromTree(PRBool aDeep,
                                    PRBool aNullParent)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    RemovedFromDocument();
  }
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}



nsresult
nsHTMLSharedElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    // If we plan to call ObjectURIChanged, we want to do it first so that the
    // image load kicks off _before_ the reflow triggered by the SetAttr.  But if
    // aNotify is false, we are coming from the parser or some such place; we'll
    // get bound after all the attributes have been set, so we'll do the
    // object load from BindToTree.  Skip the ObjectURIChanged call in that case.
    if (aNotify &&
        aNameSpaceID == kNameSpaceID_None && aName == nsHTMLAtoms::src) {
      nsAutoString type;
      GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);
      ObjectURIChanged(aValue, aNotify, NS_ConvertUTF16toUTF8(type), PR_TRUE, PR_TRUE);
    }
  }

  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                       aValue, aNotify);
}

PRUint32
nsHTMLSharedElement::GetCapabilities() const
{
  return eSupportImages | eSupportPlugins
#ifdef MOZ_SVG
    | eSupportSVG
#endif
    ;
}

PRInt32
nsHTMLSharedElement::IntrinsicState() const
{
  PRInt32 state = nsGenericHTMLElement::IntrinsicState();
  if (mNodeInfo->Equals(nsHTMLAtoms::embed)) {
    state |= ObjectState();
  }
  return state;
}
