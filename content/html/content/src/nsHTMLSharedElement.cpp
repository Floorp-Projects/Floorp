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
#include "nsIDOMHTMLIsIndexElement.h"
#include "nsIDOMHTMLParamElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLDirectoryElement.h"
#include "nsIDOMHTMLMenuElement.h"
#include "nsIDOMHTMLQuoteElement.h"
#include "nsIDOMHTMLBaseFontElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsRuleData.h"
#include "nsMappedAttributes.h"
#include "nsNetUtil.h"
#include "nsHTMLFormElement.h"

// XXX nav4 has type= start= (same as OL/UL)
extern nsAttrValue::EnumTable kListTypeTable[];

class nsHTMLSharedElement : public nsGenericHTMLElement,
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
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLIsIndexElement
  NS_DECL_NSIDOMHTMLISINDEXELEMENT

  // nsIDOMHTMLParamElement
  NS_DECL_NSIDOMHTMLPARAMELEMENT

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
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);

  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             PRBool aNotify);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);

  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);

  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
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
NS_INTERFACE_TABLE_HEAD(nsHTMLSharedElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_AMBIGUOUS_BEGIN(nsHTMLSharedElement,
                                                  nsIDOMHTMLParamElement)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE_AMBIGUOUS(nsHTMLSharedElement,
                                                         nsGenericHTMLElement,
                                                         nsIDOMHTMLParamElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLParamElement, param)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLIsIndexElement, isindex)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLBaseElement, base)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLDirectoryElement, dir)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLMenuElement, menu)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLQuoteElement, q)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLQuoteElement, blockquote)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLBaseFontElement, basefont)

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


NS_IMPL_ELEMENT_CLONE(nsHTMLSharedElement)

// nsIDOMHTMLParamElement
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Type, type)
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
  NS_IF_ADDREF(*aForm = FindForm());

  return NS_OK;
}

// nsIDOMHTMLBaseElement
NS_IMPL_URI_ATTR(nsHTMLSharedElement, Href, href)
NS_IMPL_STRING_ATTR(nsHTMLSharedElement, Target, target)

PRBool
nsHTMLSharedElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (mNodeInfo->Equals(nsGkAtoms::spacer)) {
      if (aAttribute == nsGkAtoms::size) {
        return aResult.ParseIntWithBounds(aValue, 0);
      }
      if (aAttribute == nsGkAtoms::align) {
        return ParseAlignValue(aValue, aResult);
      }
      if (aAttribute == nsGkAtoms::width ||
          aAttribute == nsGkAtoms::height) {
        return aResult.ParseSpecialIntValue(aValue, PR_TRUE);
      }
    }
    else if (mNodeInfo->Equals(nsGkAtoms::dir) ||
             mNodeInfo->Equals(nsGkAtoms::menu)) {
      if (aAttribute == nsGkAtoms::type) {
        return aResult.ParseEnumValue(aValue, kListTypeTable);
      }
      if (aAttribute == nsGkAtoms::start) {
        return aResult.ParseIntWithBounds(aValue, 1);
      }
    }
    else if (mNodeInfo->Equals(nsGkAtoms::basefont)) {
      if (aAttribute == nsGkAtoms::size) {
        return aResult.ParseIntValue(aValue);
      }
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

// spacer element code

static void
SpacerMapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                            nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);

  if (aData->mSIDs & (NS_STYLE_INHERIT_BIT(Position) |
                      NS_STYLE_INHERIT_BIT(Display))) {
    PRBool typeIsBlock = PR_FALSE;
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
    if (value && value->Type() == nsAttrValue::eString) {
      const nsString& tmp(value->GetStringValue());
      if (tmp.LowerCaseEqualsLiteral("line") ||
          tmp.LowerCaseEqualsLiteral("vert") ||
          tmp.LowerCaseEqualsLiteral("vertical") ||
          tmp.LowerCaseEqualsLiteral("block")) {
        // This is not strictly 100% compatible: if the spacer is given
        // a width of zero then it is basically ignored.
        typeIsBlock = PR_TRUE;
      }
    }

    if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
      if (typeIsBlock) {
        // width: value
        if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
          const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
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
          const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
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
          const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::size);
          if (value && value->Type() == nsAttrValue::eInteger)
            aData->mPositionData->
              mWidth.SetFloatValue((float)value->GetIntegerValue(),
                                   eCSSUnit_Pixel);
        }
      }
    }

    if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Display)) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
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

      if (typeIsBlock) {
        if (aData->mDisplayData->mDisplay.GetUnit() == eCSSUnit_Null) {
          aData->mDisplayData->mDisplay.SetIntValue(NS_STYLE_DISPLAY_BLOCK,
                                                    eCSSUnit_Enumerated);
        }
      }
    }
    // Any new structs that don't need typeIsBlock should go outside
    // the code that calculates it.
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static void
DirectoryMenuMapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                               nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(List)) {
    if (aData->mListData->mType.GetUnit() == eCSSUnit_Null) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
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
  if (mNodeInfo->Equals(nsGkAtoms::spacer)) {
    static const MappedAttributeEntry attributes[] = {
      // XXXldb This is just wrong.
      { &nsGkAtoms::usemap },
      { &nsGkAtoms::ismap },
      { &nsGkAtoms::align },
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

  if (mNodeInfo->Equals(nsGkAtoms::dir)) {
    static const MappedAttributeEntry attributes[] = {
      { &nsGkAtoms::type },
      // { &nsGkAtoms::compact }, // XXX
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

nsresult
nsHTMLSharedElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  nsresult rv =  nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                               aValue, aNotify);

  // If the href attribute of a <base> tag is changing, we may need to update
  // the document's base URI, which will cause all the links on the page to be
  // re-resolved given the new base.
  if (NS_SUCCEEDED(rv) &&
      mNodeInfo->Equals(nsGkAtoms::base, kNameSpaceID_XHTML) &&
      aName == nsGkAtoms::href &&
      aNameSpaceID == kNameSpaceID_None &&
      GetOwnerDoc() == GetCurrentDoc()) {

    nsIDocument* doc = GetCurrentDoc();
    NS_ENSURE_TRUE(doc, NS_OK);

    // We become the first base node with an href if
    //   * there's no other base node with an href, or
    //   * we come before the first base node with an href (this would happen
    //     if we didn't have an href before this call to SetAttr).
    // Additionally, we call doc->SetFirstBaseNodeWithHref if we're the first
    // base node with an href so the document updates its base URI with our new
    // href.
    nsIContent* firstBase = doc->GetFirstBaseNodeWithHref();
    if (!firstBase || this == firstBase ||
        nsContentUtils::PositionIsBefore(this, firstBase)) {

      return doc->SetFirstBaseNodeWithHref(this);
    }
  }

  return rv;
}

// Helper function for nsHTMLSharedElement::UnbindFromTree.  Finds and returns
// the first <base> tag with an href attribute which is a child of elem, if one
// exists.
static nsIContent*
FindBaseRecursive(nsINode * const elem)
{
  // We can't use NS_GetContentList to get the list of <base> elements, because
  // that flushes content notifications, and we need this function to work in
  // UnbindFromTree.  Once we land the HTML5 parser and get rid of content
  // notifications, we should fix this up. (bug 515819)

  PRUint32 childCount;
  nsIContent * const * child = elem->GetChildArray(&childCount);
  nsIContent * const * end = child + childCount;
  for ( ; child != end; child++) {
    nsIContent *childElem = *child;

    if (childElem->NodeInfo()->Equals(nsGkAtoms::base, kNameSpaceID_XHTML) &&
        childElem->HasAttr(kNameSpaceID_None, nsGkAtoms::href))
      return childElem;

    nsIContent* base = FindBaseRecursive(childElem);
    if (base)
      return base;
  }

  return nsnull;
}

nsresult
nsHTMLSharedElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                               PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aName, aNotify);

  // If we're the first <base> with an href and our href attribute is being
  // unset, then we're no longer the first <base> with an href, and we need to
  // find the new one.
  if (NS_SUCCEEDED(rv) &&
      mNodeInfo->Equals(nsGkAtoms::base, kNameSpaceID_XHTML) &&
      aName == nsGkAtoms::href &&
      aNameSpaceID == kNameSpaceID_None &&
      GetOwnerDoc() == GetCurrentDoc()) {

    nsIDocument* doc = GetCurrentDoc();
    NS_ENSURE_TRUE(doc, NS_OK);

    // If we're not the first <base> in the document, then unsetting our href
    // doesn't affect the document's base URI.
    if (this != doc->GetFirstBaseNodeWithHref())
      return NS_OK;

    // We're the first base, but we don't have an href; find the first base
    // which does have an href, and set the document's first base to that.
    nsIContent* newBaseNode = FindBaseRecursive(doc);
    return doc->SetFirstBaseNodeWithHref(newBaseNode);
  }

  return rv;
}

nsresult
nsHTMLSharedElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);

  // The document stores a pointer to its first <base> element, which we may
  // need to update here.
  if (NS_SUCCEEDED(rv) &&
      mNodeInfo->Equals(nsGkAtoms::base, kNameSpaceID_XHTML) &&
      HasAttr(kNameSpaceID_None, nsGkAtoms::href) &&
      aDocument) {

    // If there's no <base> in the document, or if this comes before the one
    // that's currently there, set the document's first <base> to this.
    nsINode* curBaseNode = aDocument->GetFirstBaseNodeWithHref();
    if (!curBaseNode ||
        nsContentUtils::PositionIsBefore(this, curBaseNode)) {

      aDocument->SetFirstBaseNodeWithHref(this);
    }
  }

  return rv;
}

void
nsHTMLSharedElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  nsCOMPtr<nsIDocument> doc = GetCurrentDoc();

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  // If we're removing a <base> from a document, we may need to update the
  // document's record of the first base node.
  if (doc && mNodeInfo->Equals(nsGkAtoms::base, kNameSpaceID_XHTML)) {

    // If we're not the first base node, then we don't need to do anything.
    if (this != doc->GetFirstBaseNodeWithHref())
      return;

    // If we were the first base node, we need to find the new first base.

    nsIContent* newBaseNode = FindBaseRecursive(doc);
    doc->SetFirstBaseNodeWithHref(newBaseNode);
  }
}

nsMapRuleToAttributesFunc
nsHTMLSharedElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsGkAtoms::spacer)) {
    return &SpacerMapAttributesIntoRule;
  }
  else if (mNodeInfo->Equals(nsGkAtoms::dir) ||
           mNodeInfo->Equals(nsGkAtoms::menu)) {
    return &DirectoryMenuMapAttributesIntoRule;
  }

  return nsGenericHTMLElement::GetAttributeMappingFunction();
}
