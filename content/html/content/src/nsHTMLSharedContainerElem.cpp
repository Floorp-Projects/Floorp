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

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLModElement.h"
#include "nsIDOMHTMLDirectoryElement.h"
#include "nsIDOMHTMLMenuElement.h"
#include "nsIDOMHTMLOListElement.h"
#include "nsIDOMHTMLQuoteElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsMappedAttributes.h"
#include "nsRuleNode.h"


class nsHTMLSharedContainerElement : public nsGenericHTMLElement,
                                     public nsIDOMHTMLDirectoryElement,
                                     public nsIDOMHTMLMenuElement,
                                     public nsIDOMHTMLModElement,
                                     public nsIDOMHTMLOListElement,
                                     public nsIDOMHTMLQuoteElement,
                                     public nsIDOMHTMLTableCaptionElement
{
public:
  nsHTMLSharedContainerElement();
  virtual ~nsHTMLSharedContainerElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLModElement and nsIDOMHTMLQuoteElement
  NS_DECL_NSIDOMHTMLMODELEMENT

  // nsIDOMHTMLDirectoryElement (and nsIDOMHTMLMenuElement)
  NS_DECL_NSIDOMHTMLDIRECTORYELEMENT

  // nsIDOMHTMLOListElement
  NS_IMETHOD GetStart(PRInt32 *aStart);
  NS_IMETHOD SetStart(PRInt32 aStart);
  NS_IMETHOD GetType(nsAString & aType);
  NS_IMETHOD SetType(const nsAString & aType);

  // nsIDOMHTMLTableCaptionElement
  NS_DECL_NSIDOMHTMLTABLECAPTIONELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
};

nsresult
NS_NewHTMLSharedContainerElement(nsIHTMLContent** aInstancePtrResult,
                                 nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLSharedContainerElement* it = new nsHTMLSharedContainerElement();

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


nsHTMLSharedContainerElement::nsHTMLSharedContainerElement()
{
}

nsHTMLSharedContainerElement::~nsHTMLSharedContainerElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSharedContainerElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLSharedContainerElement, nsGenericElement)


// QueryInterface implementation for nsHTMLSharedContainerElement
NS_HTML_CONTENT_INTERFACE_MAP_AMBIGOUS_BEGIN(nsHTMLSharedContainerElement,
                                             nsGenericHTMLElement,
                                             nsIDOMHTMLDirectoryElement)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMHTMLElement,
                                   nsIDOMHTMLDirectoryElement)

  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLDirectoryElement, dir)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLMenuElement, menu)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLModElement, del)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLModElement, ins)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLOListElement, ol)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLQuoteElement, quote)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLTableCaptionElement, caption)

  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLDelElement, del)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLDirectoryElement, dir)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLInsElement, ins)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLMenuElement, menu)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLOListElement, ol)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLQuoteElement, quote)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(HTMLTableCaptionElement,
                                                  caption)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLSharedContainerElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLSharedContainerElement* it = new nsHTMLSharedContainerElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIContent> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMHTMLModElement *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMPL_URI_ATTR(nsHTMLSharedContainerElement, Cite, cite)
NS_IMPL_STRING_ATTR(nsHTMLSharedContainerElement, DateTime, datetime)

// For nsIDOMHTMLDirectoryElement, nsIDOMHTMLMenuElement, and
// nsIDOMHTMLOListElement
NS_IMPL_BOOL_ATTR(nsHTMLSharedContainerElement, Compact, compact)

NS_IMPL_INT_ATTR(nsHTMLSharedContainerElement, Start, start)
NS_IMPL_STRING_ATTR(nsHTMLSharedContainerElement, Type, type)

// For nsIDOMHTMLTableCaptionElement
NS_IMPL_STRING_ATTR(nsHTMLSharedContainerElement, Align, align)


nsHTMLValue::EnumTable kListTypeTable[] = {
  { "none", NS_STYLE_LIST_STYLE_NONE },
  { "disc", NS_STYLE_LIST_STYLE_DISC },
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { "decimal", NS_STYLE_LIST_STYLE_DECIMAL },
  { "lower-roman", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { "upper-roman", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "lower-alpha", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "upper-alpha", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { 0 }
};

nsHTMLValue::EnumTable kOldListTypeTable[] = {
  { "1", NS_STYLE_LIST_STYLE_OLD_DECIMAL },
  { "A", NS_STYLE_LIST_STYLE_OLD_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_OLD_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_OLD_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_OLD_LOWER_ROMAN },
  { 0 }
};

static const nsHTMLValue::EnumTable kCaptionAlignTable[] = {
  { "left",  NS_SIDE_LEFT },
  { "right", NS_SIDE_RIGHT },
  { "top",   NS_SIDE_TOP},
  { "bottom",NS_SIDE_BOTTOM},
  { 0 }
};


NS_IMETHODIMP
nsHTMLSharedContainerElement::StringToAttribute(nsIAtom* aAttribute,
                                                const nsAString& aValue,
                                                nsHTMLValue& aResult)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::dir)  ||
      mNodeInfo->Equals(nsHTMLAtoms::menu) ||
      mNodeInfo->Equals(nsHTMLAtoms::ol)) {
    if (aAttribute == nsHTMLAtoms::type) {
      if (aResult.ParseEnumValue(aValue, kListTypeTable)) {
        return NS_CONTENT_ATTR_HAS_VALUE;
      }

      if (mNodeInfo->Equals(nsHTMLAtoms::ol)) {
        if (aResult.ParseEnumValue(aValue, kOldListTypeTable, PR_TRUE)) {
          return NS_CONTENT_ATTR_HAS_VALUE;
        }
      }
    } else if (aAttribute == nsHTMLAtoms::start) {
      if (aResult.ParseIntWithBounds(aValue, eHTMLUnit_Integer, 1)) {
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  } else if (mNodeInfo->Equals(nsHTMLAtoms::caption)) {
    if (aAttribute == nsHTMLAtoms::align) {
      if (aResult.ParseEnumValue(aValue, kCaptionAlignTable)) {
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  }

  return nsGenericHTMLElement::StringToAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLSharedContainerElement::AttributeToString(nsIAtom* aAttribute,
                                                const nsHTMLValue& aValue,
                                                nsAString& aResult) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::dir) ||
      mNodeInfo->Equals(nsHTMLAtoms::menu)) {
    if (aAttribute == nsHTMLAtoms::type) {
      aValue.EnumValueToString(kListTypeTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } else if (mNodeInfo->Equals(nsHTMLAtoms::ol)) {
    if (aAttribute == nsHTMLAtoms::type) {
      PRInt32 v = aValue.GetIntValue();
      switch (v) {
        case NS_STYLE_LIST_STYLE_OLD_DECIMAL:
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
  } else if (mNodeInfo->Equals(nsHTMLAtoms::caption)) {
    if (aAttribute == nsHTMLAtoms::align) {
      if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
        aValue.EnumValueToString(kCaptionAlignTable, aResult);

        return NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP_(PRBool)
nsHTMLSharedContainerElement::IsAttributeMapped(const nsIAtom* aAttr) const
{
  if (nsGenericHTMLElement::IsAttributeMapped(aAttr)) {
    return PR_TRUE;
  }

  if (mNodeInfo->Equals(nsHTMLAtoms::dir)  ||
      mNodeInfo->Equals(nsHTMLAtoms::menu) ||
      mNodeInfo->Equals(nsHTMLAtoms::ol)) {
    if (aAttr == nsHTMLAtoms::type) {
      return PR_TRUE;
    }
  } else if (mNodeInfo->Equals(nsHTMLAtoms::caption)) {
    if (aAttr == nsHTMLAtoms::align) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

static void
MapDirAndMenuAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                nsRuleData* aData)
{
  if (!aData || !aAttributes)
    return;

  if (aData->mListData) {
    if (aData->mListData->mType.GetUnit() == eCSSUnit_Null) {
      nsHTMLValue value;
      // type: enum
      if (aAttributes->GetAttribute(nsHTMLAtoms::type, value) !=
          NS_CONTENT_ATTR_NOT_THERE) {
        if (value.GetUnit() == eHTMLUnit_Enumerated) {
          aData->mListData->mType.SetIntValue(value.GetIntValue(),
                                              eCSSUnit_Enumerated);
        }
        else {
          aData->mListData->mType.SetIntValue(NS_STYLE_LIST_STYLE_DISC,
                                              eCSSUnit_Enumerated);
        }
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static void
MapOlAttributesIntoRule(const nsMappedAttributes* aAttributes,
                        nsRuleData* aData)
{
  if (!aData || !aAttributes)
    return;

  if (aData->mListData) {
    if (aData->mListData->mType.GetUnit() == eCSSUnit_Null) {
      nsHTMLValue value;
      // type: enum
      if (aAttributes->GetAttribute(nsHTMLAtoms::type, value) !=
          NS_CONTENT_ATTR_NOT_THERE) {
        if (value.GetUnit() == eHTMLUnit_Enumerated) {
          aData->mListData->mType.SetIntValue(value.GetIntValue(),
                                              eCSSUnit_Enumerated);
        }
        else {
          aData->mListData->mType.SetIntValue(NS_STYLE_LIST_STYLE_DECIMAL,
                                              eCSSUnit_Enumerated);
        }
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static 
void MapCaptionAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                  nsRuleData* aData)
{
  if (!aAttributes || !aData)
    return;

  if (aData->mSID == eStyleStruct_TableBorder && aData->mTableData) {
    if (aData->mTableData->mCaptionSide.GetUnit() == eCSSUnit_Null) {
      nsHTMLValue value;
      aAttributes->GetAttribute(nsHTMLAtoms::align, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated)
        aData->mTableData->mCaptionSide.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}


NS_IMETHODIMP
nsHTMLSharedContainerElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::dir) ||
      mNodeInfo->Equals(nsHTMLAtoms::menu)) {
    aMapRuleFunc = &MapDirAndMenuAttributesIntoRule;

    return NS_OK;
  }

  if (mNodeInfo->Equals(nsHTMLAtoms::ol)) {
    aMapRuleFunc = &MapOlAttributesIntoRule;

    return NS_OK;
  }

  if (mNodeInfo->Equals(nsHTMLAtoms::caption)) {
    aMapRuleFunc = &MapCaptionAttributesIntoRule;

    return NS_OK;
  }

  return
    nsGenericHTMLElement::GetAttributeMappingFunction(aMapRuleFunc);
}
