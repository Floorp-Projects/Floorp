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
#include "nsIDOMHTMLElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIHTMLAttributes.h"
#include "nsIJSScriptObject.h"
#include "nsSize.h"
#include "nsIDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsLayoutUtils.h"
#include "nsIWebShell.h"
#include "nsIFrame.h"
#include "nsImageFrame.h"
#include "nsLayoutAtoms.h"


// XXX nav attrs: suppress

class nsHTMLSpacerElement : public nsGenericHTMLLeafElement,
                            public nsIDOMHTMLElement
{
public:
  nsHTMLSpacerElement();
  virtual ~nsHTMLSpacerElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

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
NS_NewHTMLSpacerElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLSpacerElement* it = new nsHTMLSpacerElement();

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


nsHTMLSpacerElement::nsHTMLSpacerElement()
{
}

nsHTMLSpacerElement::~nsHTMLSpacerElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSpacerElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLSpacerElement, nsGenericElement);

NS_IMPL_HTMLCONTENT_QI0(nsHTMLSpacerElement, nsGenericHTMLLeafElement);


nsresult
nsHTMLSpacerElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLSpacerElement* it = new nsHTMLSpacerElement();

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

NS_IMETHODIMP
nsHTMLSpacerElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsAReadableString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::size) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if ((aAttribute == nsHTMLAtoms::width) ||
           (aAttribute == nsHTMLAtoms::height)) {
    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLSpacerElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      AlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLLeafElement::AttributeToString(aAttribute, aValue,
                                                     aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  NS_PRECONDITION(aContext, "no style context");
  NS_PRECONDITION(aPresContext, "no pres context");

  if (aAttributes && aPresContext && aContext) {
    nsHTMLValue value;
    nsStyleDisplay* display = (nsStyleDisplay*)
      aContext->GetMutableStyleData(eStyleStruct_Display);
    nsStylePosition* position = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (eHTMLUnit_Enumerated == value.GetUnit()) {
      switch (value.GetIntValue()) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        display->mFloats = NS_STYLE_FLOAT_LEFT;
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        display->mFloats = NS_STYLE_FLOAT_RIGHT;
        break;
      default:
        break;
      }
    }

    nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext,
                                                 aPresContext);

    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    PRBool typeIsBlock = PR_FALSE;
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
        display->mDisplay = NS_STYLE_DISPLAY_BLOCK;
        if (tmp.EqualsIgnoreCase("block")) {
          typeIsBlock = PR_TRUE;
        }
      }
    }

    if (typeIsBlock) {
      // width: value
      aAttributes->GetAttribute(nsHTMLAtoms::width, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
        position->mWidth.SetCoordValue(twips);
      }
      else if (value.GetUnit() == eHTMLUnit_Percent) {
        position->mWidth.SetPercentValue(value.GetPercentValue());
      }

      // height: value
      aAttributes->GetAttribute(nsHTMLAtoms::height, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
        position->mHeight.SetCoordValue(twips);
      }
      else if (value.GetUnit() == eHTMLUnit_Percent) {
        position->mHeight.SetPercentValue(value.GetPercentValue());
      }
    }
    else
    {
      // size: value
      aAttributes->GetAttribute(nsHTMLAtoms::size, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
        position->mWidth.SetCoordValue(twips);
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}


NS_IMETHODIMP
nsHTMLSpacerElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::usemap) ||
      (aAttribute == nsHTMLAtoms::ismap)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (!GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (!GetImageBorderAttributeImpact(aAttribute, aHint)) {
        aHint = NS_STYLE_HINT_CONTENT;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSpacerElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSpacerElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
