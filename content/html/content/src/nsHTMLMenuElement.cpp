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
#include "nsIDOMHTMLMenuElement.h"
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

// XXX nav4 has type= start= (same as OL/UL)

extern nsGenericHTMLElement::EnumTable kListTypeTable[];


class nsHTMLMenuElement : public nsGenericHTMLContainerElement,
                          public nsIDOMHTMLMenuElement
{
public:
  nsHTMLMenuElement();
  virtual ~nsHTMLMenuElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLMenuElement
  NS_IMETHOD GetCompact(PRBool* aCompact);
  NS_IMETHOD SetCompact(PRBool aCompact);

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
NS_NewHTMLMenuElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLMenuElement* it = new nsHTMLMenuElement();

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


nsHTMLMenuElement::nsHTMLMenuElement()
{
}

nsHTMLMenuElement::~nsHTMLMenuElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLMenuElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLMenuElement, nsGenericElement);

NS_IMPL_HTMLCONTENT_QI(nsHTMLMenuElement, nsGenericHTMLContainerElement,
                       nsIDOMHTMLMenuElement)


nsresult
nsHTMLMenuElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLMenuElement* it = new nsHTMLMenuElement();

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


NS_IMPL_BOOL_ATTR(nsHTMLMenuElement, Compact, compact)


NS_IMETHODIMP
nsHTMLMenuElement::StringToAttribute(nsIAtom* aAttribute,
                                     const nsAReadableString& aValue,
                                     nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (ParseEnumValue(aValue, kListTypeTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::start) {
    if (ParseValue(aValue, 1, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLMenuElement::AttributeToString(nsIAtom* aAttribute,
                                     const nsHTMLValue& aValue,
                                     nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    EnumValueToString(aValue, kListTypeTable, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return nsGenericHTMLContainerElement::AttributeToString(aAttribute, aValue,
                                                          aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;
    nsStyleList* list = (nsStyleList*)
      aContext->GetMutableStyleData(eStyleStruct_List);

    // type: enum
    aAttributes->GetAttribute(nsHTMLAtoms::type, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      list->mListStyleType = value.GetIntValue();
    }
    else if (value.GetUnit() != eHTMLUnit_Null) {
      list->mListStyleType = NS_STYLE_LIST_STYLE_BASIC;
    }

    // compact: empty
    aAttributes->GetAttribute(nsHTMLAtoms::compact, value);
    if (value.GetUnit() != eHTMLUnit_Null) {
      // XXX set
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLMenuElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                            PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMenuElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMenuElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
