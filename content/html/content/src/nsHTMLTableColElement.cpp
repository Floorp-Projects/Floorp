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
#include "nsIDOMHTMLTableColElement.h"
#include "nsIDOMEventTarget.h"
#include "nsMappedAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsRuleData.h"

// use the same protection as ancient code did 
// http://lxr.mozilla.org/classic/source/lib/layout/laytable.c#46
#define MAX_COLSPAN 1000

class nsHTMLTableColElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLTableColElement
{
public:
  nsHTMLTableColElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLTableColElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLTableColElement
  NS_DECL_NSIDOMHTMLTABLECOLELEMENT

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
};


NS_IMPL_NS_NEW_HTML_ELEMENT(TableCol)


nsHTMLTableColElement::nsHTMLTableColElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLTableColElement::~nsHTMLTableColElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTableColElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableColElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLTableColElement, nsHTMLTableColElement)

// QueryInterface implementation for nsHTMLTableColElement
NS_INTERFACE_TABLE_HEAD(nsHTMLTableColElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLTableColElement,
                                   nsIDOMHTMLTableColElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLTableColElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLTableColElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLTableColElement)


NS_IMPL_STRING_ATTR(nsHTMLTableColElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, Ch, _char)
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, ChOff, charoff)
NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(nsHTMLTableColElement, Span, span, 1)
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, VAlign, valign)
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, Width, width)


PRBool
nsHTMLTableColElement::ParseAttribute(PRInt32 aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    /* ignore these attributes, stored simply as strings ch */
    if (aAttribute == nsGkAtoms::charoff) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::span) {
      /* protection from unrealistic large colspan values */
      return aResult.ParseIntWithBounds(aValue, 1, MAX_COLSPAN);
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::align) {
      return ParseTableCellHAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::valign) {
      return ParseTableVAlignValue(aValue, aResult);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static 
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Table)) {
    nsCSSValue *span = aData->ValueForSpan();
    if (span->GetUnit() == eCSSUnit_Null) {
      // span: int
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::span);
      if (value && value->Type() == nsAttrValue::eInteger) {
        PRInt32 val = value->GetIntegerValue();
        // Note: Do NOT use this code for table cells!  The value "0"
        // means something special for colspan and rowspan, but for <col
        // span> and <colgroup span> it's just disallowed.
        if (val > 0) {
          span->SetIntValue(value->GetIntegerValue(), eCSSUnit_Integer);
        }
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    nsCSSValue* width = aData->ValueForWidth();
    if (width->GetUnit() == eCSSUnit_Null) {
      // width
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (value) {
        switch (value->Type()) {
        case nsAttrValue::ePercent: {
          width->SetPercentValue(value->GetPercentValue());
          break;
        }
        case nsAttrValue::eInteger: {
          width->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
          break;
        }
        default:
          break;
        }
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Text)) {
    nsCSSValue* textAlign = aData->ValueForTextAlign();
    if (textAlign->GetUnit() == eCSSUnit_Null) {
      // align: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
      if (value && value->Type() == nsAttrValue::eEnum)
        textAlign->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(TextReset)) {
    nsCSSValue* verticalAlign = aData->ValueForVerticalAlign();
    if (verticalAlign->GetUnit() == eCSSUnit_Null) {
      // valign: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::valign);
      if (value && value->Type() == nsAttrValue::eEnum)
        verticalAlign->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLTableColElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::width },
    { &nsGkAtoms::align },
    { &nsGkAtoms::valign },
    { &nsGkAtoms::span },
    { nsnull }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}


nsMapRuleToAttributesFunc
nsHTMLTableColElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}
