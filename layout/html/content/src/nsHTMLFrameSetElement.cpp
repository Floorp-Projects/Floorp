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
#include "nsIDOMHTMLFrameSetElement.h"
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


class nsHTMLFrameSetElement : public nsGenericHTMLContainerElement,
                              public nsIDOMHTMLFrameSetElement
{
public:
  nsHTMLFrameSetElement();
  virtual ~nsHTMLFrameSetElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLFrameSetElement
  NS_DECL_IDOMHTMLFRAMESETELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
};

nsresult
NS_NewHTMLFrameSetElement(nsIHTMLContent** aInstancePtrResult,
                          nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLFrameSetElement* it = new nsHTMLFrameSetElement();

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


nsHTMLFrameSetElement::nsHTMLFrameSetElement()
{
}

nsHTMLFrameSetElement::~nsHTMLFrameSetElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLFrameSetElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLFrameSetElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI(nsHTMLFrameSetElement, nsGenericHTMLContainerElement,
                       nsIDOMHTMLFrameSetElement);


nsresult
nsHTMLFrameSetElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLFrameSetElement* it = new nsHTMLFrameSetElement();

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


NS_IMPL_STRING_ATTR(nsHTMLFrameSetElement, Cols, cols)
NS_IMPL_STRING_ATTR(nsHTMLFrameSetElement, Rows, rows)

NS_IMETHODIMP
nsHTMLFrameSetElement::StringToAttribute(nsIAtom* aAttribute,
                                         const nsAReadableString& aValue,
                                         nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::bordercolor) {
    if (nsGenericHTMLElement::ParseColor(aValue, mDocument, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } 
  else if (aAttribute == nsHTMLAtoms::frameborder) {
    // XXX need to check for correct mode
    if (nsGenericHTMLElement::ParseFrameborderValue(PR_FALSE, aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } 
  else if (aAttribute == nsHTMLAtoms::border) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, 100, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLFrameSetElement::AttributeToString(nsIAtom* aAttribute,
                                         const nsHTMLValue& aValue,
                                         nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::frameborder) {
    // XXX need to check for correct mode
    nsGenericHTMLElement::FrameborderValueToString(PR_FALSE, aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  } 
  return nsGenericHTMLContainerElement::AttributeToString(aAttribute, aValue,
                                                          aResult);
}

NS_IMETHODIMP
nsHTMLFrameSetElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                                PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::rows) ||
      (aAttribute == nsHTMLAtoms::cols)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameSetElement::SizeOf(nsISizeOfHandler* aSizer,
                              PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
