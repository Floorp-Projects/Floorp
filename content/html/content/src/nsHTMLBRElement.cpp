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
#include "nsIDOMHTMLBRElement.h"
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


class nsHTMLBRElement : public nsGenericHTMLLeafElement,
                        public nsIDOMHTMLBRElement
{
public:
  nsHTMLBRElement();
  virtual ~nsHTMLBRElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLBRElement
  NS_DECL_IDOMHTMLBRELEMENT    

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
};

nsresult
NS_NewHTMLBRElement(nsIHTMLContent** aInstancePtrResult,
                    nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLBRElement* it = new nsHTMLBRElement();

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


nsHTMLBRElement::nsHTMLBRElement()
{
}

nsHTMLBRElement::~nsHTMLBRElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLBRElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLBRElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI(nsHTMLBRElement, nsGenericHTMLLeafElement,
                       nsIDOMHTMLBRElement)


nsresult
nsHTMLBRElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLBRElement* it = new nsHTMLBRElement();

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


NS_IMPL_STRING_ATTR(nsHTMLBRElement, Clear, clear)

static nsGenericHTMLElement::EnumTable kClearTable[] = {
  { "left", NS_STYLE_CLEAR_LEFT },
  { "right", NS_STYLE_CLEAR_RIGHT },
  { "all", NS_STYLE_CLEAR_LEFT_AND_RIGHT },
  { "both", NS_STYLE_CLEAR_LEFT_AND_RIGHT },
  { 0 }
};

NS_IMETHODIMP
nsHTMLBRElement::StringToAttribute(nsIAtom* aAttribute,
                                   const nsAReadableString& aValue,
                                   nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::clear) {
    if (ParseEnumValue(aValue, kClearTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLBRElement::AttributeToString(nsIAtom* aAttribute,
                                   const nsHTMLValue& aValue,
                                   nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::clear) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      EnumValueToString(aValue, kClearTable, aResult);
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
  nsStyleDisplay* display = (nsStyleDisplay*)
    aContext->GetMutableStyleData(eStyleStruct_Display);

  nsHTMLValue value;
  aAttributes->GetAttribute(nsHTMLAtoms::clear, value);
  if (value.GetUnit() == eHTMLUnit_Enumerated) {
    display->mBreakType = value.GetIntValue();
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLBRElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                          PRInt32& aHint) const
{
  if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (nsHTMLAtoms::clear == aAttribute) {
      aHint = NS_STYLE_HINT_REFLOW;
    }
    else {
      aHint = NS_STYLE_HINT_CONTENT;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBRElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                              nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBRElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
