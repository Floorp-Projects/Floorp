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
#include "nsIDOMHTMLTableColElement.h"
#include "nsIHTMLTableColElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsMappedAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsRuleNode.h"

// use the same protection as ancient code did 
// http://lxr.mozilla.org/classic/source/lib/layout/laytable.c#46
#define MAX_COLSPAN 1000

class nsHTMLTableColElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLTableColElement,
                              public nsIHTMLTableColElement
{
public:
  nsHTMLTableColElement();
  virtual ~nsHTMLTableColElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLTableColElement
  NS_DECL_NSIDOMHTMLTABLECOLELEMENT

  // nsIHTMLTableColElement
  NS_IMETHOD GetSpanValue(PRInt32* aSpan);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
};

nsresult
NS_NewHTMLTableColElement(nsIHTMLContent** aInstancePtrResult,
                          nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLTableColElement* it = new nsHTMLTableColElement();

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


nsHTMLTableColElement::nsHTMLTableColElement()
{
}

nsHTMLTableColElement::~nsHTMLTableColElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTableColElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableColElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLTableColElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTableColElement,
                                    nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTableColElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIHTMLTableColElement, col) // for col only
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTableColElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLTableColElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLTableColElement* it = new nsHTMLTableColElement();

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


NS_IMPL_STRING_ATTR_DEFAULT_VALUE(nsHTMLTableColElement, Align, align, "left")
NS_IMPL_STRING_ATTR_DEFAULT_VALUE(nsHTMLTableColElement, Ch, _char, ".")
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, ChOff, charoff)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLTableColElement, Span, span, 1)
NS_IMPL_STRING_ATTR_DEFAULT_VALUE(nsHTMLTableColElement, VAlign, valign, "middle")
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, Width, width)


NS_IMETHODIMP
nsHTMLTableColElement::StringToAttribute(nsIAtom* aAttribute,
                                         const nsAString& aValue,
                                         nsHTMLValue& aResult)
{
  /* ignore these attributes, stored simply as strings ch */
  /* attributes that resolve to integers */
  if (aAttribute == nsHTMLAtoms::charoff) {
    if (aResult.ParseIntWithBounds(aValue, eHTMLUnit_Integer, 0)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::span) {
    /* protection from unrealistic large colspan values */
    if (aResult.ParseIntWithBounds(aValue, eHTMLUnit_Integer, 1, MAX_COLSPAN)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    /* attributes that resolve to integers or percents or proportions */

    if (aResult.ParseSpecialIntValue(aValue, eHTMLUnit_Integer, PR_TRUE, PR_TRUE)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    /* other attributes */

    if (ParseTableCellHAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (ParseTableVAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableColElement::AttributeToString(nsIAtom* aAttribute,
                                         const nsHTMLValue& aValue,
                                         nsAString& aResult) const
{
  /* ignore these attributes, stored already as strings
     ch
   */
  /* ignore attributes that are of standard types
     charoff, span
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (TableCellHAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (TableVAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static 
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_Position &&
      aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
    // width
    nsHTMLValue value;
    if (aAttributes->GetAttribute(nsHTMLAtoms::width, value) !=
        NS_CONTENT_ATTR_NOT_THERE) {
      switch (value.GetUnit()) {
      case eHTMLUnit_Percent: {
        aData->mPositionData->mWidth.SetPercentValue(value.GetPercentValue());
        break;
      }
      case eHTMLUnit_Integer: {
        aData->mPositionData->mWidth.SetFloatValue((float)value.GetIntValue(), eCSSUnit_Pixel);
        break;
      }
      case eHTMLUnit_Proportional: {
        aData->mPositionData->mWidth.SetFloatValue((float)value.GetIntValue(), eCSSUnit_Proportional);
        break;
      }
      default:
        break;
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Text) {
    if (aData->mTextData->mTextAlign.GetUnit() == eCSSUnit_Null) {
      // align: enum
      nsHTMLValue value;
      aAttributes->GetAttribute(nsHTMLAtoms::align, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated)
        aData->mTextData->mTextAlign.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
    }
  }
  else if (aData->mSID == eStyleStruct_TextReset) {
    if (aData->mTextData->mVerticalAlign.GetUnit() == eCSSUnit_Null) {
      // valign: enum
      nsHTMLValue value;
      aAttributes->GetAttribute(nsHTMLAtoms::valign, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) 
        aData->mTextData->mVerticalAlign.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static 
void ColMapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                              nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_Table && 
      aData->mTableData->mSpan.GetUnit() == eCSSUnit_Null) {
    // span: int
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::span, value);
    if (value.GetUnit() == eHTMLUnit_Integer)
      aData->mTableData->mSpan.SetIntValue(value.GetIntValue(),
                                           eCSSUnit_Integer);
  }

  MapAttributesIntoRule(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLTableColElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsHTMLAtoms::width },
    { &nsHTMLAtoms::align },
    { &nsHTMLAtoms::valign },
    { nsnull }
  };

  static const MappedAttributeEntry span_attribute[] = {
    { &nsHTMLAtoms::span },
    { nsnull }
  };

  static const MappedAttributeEntry* const col_map[] = {
    attributes,
    sCommonAttributeMap,
  };

  static const MappedAttributeEntry* const colspan_map[] = {
    attributes,
    span_attribute,
    sCommonAttributeMap,
  };

  // we don't match "span" if we're a <col>
  // XXXldb Should this be reflected in the mapping function?
  if (mNodeInfo->Equals(nsHTMLAtoms::col))
    return FindAttributeDependence(aAttribute, col_map,
                                   NS_ARRAY_LENGTH(col_map));
  return FindAttributeDependence(aAttribute, colspan_map,
                                 NS_ARRAY_LENGTH(colspan_map));
}


NS_IMETHODIMP
nsHTMLTableColElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::col)) {
    aMapRuleFunc = &ColMapAttributesIntoRule;
  } else {
    aMapRuleFunc = &MapAttributesIntoRule;
  }

  return NS_OK;
}


NS_METHOD nsHTMLTableColElement::GetSpanValue(PRInt32* aSpan)
{
  if (nsnull!=aSpan) {
    PRInt32 span=-1;
    GetSpan(&span);

    if (-1==span)
      span=1; // the default;

    *aSpan = span;
  }

  return NS_OK;
}
