/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIDOMHTMLTableColElement.h"
#include "nsIHTMLTableColElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"


class nsHTMLTableColElement : public nsGenericHTMLContainerElement,
                              public nsIDOMHTMLTableColElement,
                              public nsIHTMLTableColElement
{
public:
  nsHTMLTableColElement();
  virtual ~nsHTMLTableColElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLTableColElement
  NS_DECL_IDOMHTMLTABLECOLELEMENT

  // nsIHTMLTableColElement
  NS_IMETHOD GetSpanValue(PRInt32* aSpan);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
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

NS_IMPL_HTMLCONTENT_QI2(nsHTMLTableColElement,
                        nsGenericHTMLContainerElement,
                        nsIDOMHTMLTableColElement,
                        nsIHTMLTableColElement);


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

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLTableColElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, Ch, _char)
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, ChOff, charoff)
NS_IMPL_INT_ATTR(nsHTMLTableColElement, Span, span)
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, VAlign, valign)
NS_IMPL_STRING_ATTR(nsHTMLTableColElement, Width, width)


NS_IMETHODIMP
nsHTMLTableColElement::StringToAttribute(nsIAtom* aAttribute,
                                         const nsAReadableString& aValue,
                                         nsHTMLValue& aResult)
{
  /* ignore these attributes, stored simply as strings ch */
  /* attributes that resolve to integers */
  if (aAttribute == nsHTMLAtoms::choff) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::span) {
    if (ParseValue(aValue, 1, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    /* attributes that resolve to integers or percents or proportions */

    if (ParseValueOrPercentOrProportional(aValue, aResult, eHTMLUnit_Pixel)) {
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
                                         nsAWritableString& aResult) const
{
  /* ignore these attributes, stored already as strings
     ch
   */
  /* ignore attributes that are of standard types
     choff, span
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
  else if (aAttribute == nsHTMLAtoms::width) {
    if (ValueOrPercentOrProportionalToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLContainerElement::AttributeToString(aAttribute, aValue,
                                                          aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  if (aAttributes) {
    nsHTMLValue value;
    nsStyleText* textStyle = nsnull;

    // width
    aAttributes->GetAttribute(nsHTMLAtoms::width, value);

    if (value.GetUnit() != eHTMLUnit_Null) {
      nsStylePosition* position = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);

      switch (value.GetUnit()) {
      case eHTMLUnit_Percent:
        position->mWidth.SetPercentValue(value.GetPercentValue());
        break;

      case eHTMLUnit_Pixel:
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        position->mWidth.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
        break;

      case eHTMLUnit_Proportional:
        position->mWidth.SetIntValue(value.GetIntValue(), eStyleUnit_Proportional);
        break;
      default:
        break;
      }
    }

    // align: enum
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mTextAlign = value.GetIntValue();
    }
    
    // valign: enum
    aAttributes->GetAttribute(nsHTMLAtoms::valign, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      if (nsnull==textStyle)
        textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mVerticalAlign.SetIntValue(value.GetIntValue(),
                                            eStyleUnit_Enumerated);
    }

    // span: int
    aAttributes->GetAttribute(nsHTMLAtoms::span, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      nsStyleTable *tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
      tableStyle->mSpan = value.GetIntValue();
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLTableColElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                                PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::align) ||
      (aAttribute == nsHTMLAtoms::valign) ||
      (aAttribute == nsHTMLAtoms::span)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableColElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                    nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
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

NS_IMETHODIMP
nsHTMLTableColElement::SizeOf(nsISizeOfHandler* aSizer,
                              PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
