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
#include "nsIDOMHTMLPreElement.h"
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
#include "nsIHTMLAttributes.h"

// XXX wrap, variable, cols, tabstop


class nsHTMLPreElement : public nsGenericHTMLContainerElement,
                         public nsIDOMHTMLPreElement
{
public:
  nsHTMLPreElement();
  virtual ~nsHTMLPreElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLPreElement
  NS_IMETHOD GetWidth(PRInt32* aWidth);
  NS_IMETHOD SetWidth(PRInt32 aWidth);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
};

nsresult
NS_NewHTMLPreElement(nsIHTMLContent** aInstancePtrResult,
                     nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLPreElement* it = new nsHTMLPreElement();

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


nsHTMLPreElement::nsHTMLPreElement()
{
}

nsHTMLPreElement::~nsHTMLPreElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLPreElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLPreElement, nsGenericElement);

NS_IMPL_HTMLCONTENT_QI(nsHTMLPreElement, nsGenericHTMLContainerElement,
                       nsIDOMHTMLPreElement);


nsresult
nsHTMLPreElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLPreElement* it = new nsHTMLPreElement();

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


NS_IMPL_INT_ATTR(nsHTMLPreElement, Width, width)


NS_IMETHODIMP
nsHTMLPreElement::StringToAttribute(nsIAtom* aAttribute,
                                    const nsAReadableString& aValue,
                                    nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::cols) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::tabstop) {
    nsAutoString val(aValue);

    PRInt32 ec, tabstop = val.ToInteger(&ec);

    if (tabstop <= 0) {
      tabstop = 8;
    }

    aResult.SetIntValue(tabstop, eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

static void
MapFontAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                      nsIMutableStyleContext* aContext,
                      nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;

    // variable: empty
    aAttributes->GetAttribute(nsHTMLAtoms::variable, value);
    if (value.GetUnit() == eHTMLUnit_Empty) {
      nsStyleFont* font = (nsStyleFont*)
        aContext->GetMutableStyleData(eStyleStruct_Font);
      font->mFont.name.AssignWithConversion("serif");
    }
  }
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;

    // wrap: empty
    aAttributes->GetAttribute(nsHTMLAtoms::wrap, value);
    if (value.GetUnit() != eHTMLUnit_Null) {
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      text->mWhiteSpace = NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
    }
      
    // cols: int (nav4 attribute)
    aAttributes->GetAttribute(nsHTMLAtoms::cols, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      nsStylePosition* position = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
      position->mWidth.SetIntValue(value.GetIntValue(), eStyleUnit_Chars);
      // Force wrap property on since we want to wrap at a width
      // boundary not just a newline.
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      text->mWhiteSpace = NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
    }

    // width: int (html4 attribute == nav4 cols)
    aAttributes->GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      nsStylePosition* position = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
      position->mWidth.SetIntValue(value.GetIntValue(), eStyleUnit_Chars);
      // Force wrap property on since we want to wrap at a width
      // boundary not just a newline.
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      text->mWhiteSpace = NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
    }

    // tabstop: int
    aAttributes->GetAttribute(nsHTMLAtoms::tabstop, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      // XXX set
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLPreElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                           PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::variable) || 
      (aAttribute == nsHTMLAtoms::wrap) ||
      (aAttribute == nsHTMLAtoms::cols) ||
      (aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::tabstop)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}



NS_IMETHODIMP
nsHTMLPreElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                               nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = &MapFontAttributesInto;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLPreElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
