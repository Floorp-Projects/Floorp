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
#include "nsIDOMHTMLTitleElement.h"
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
#include "nsIDOMText.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"


class nsHTMLTitleElement : public nsGenericHTMLContainerElement,
                           public nsIDOMHTMLTitleElement
{
public:
  nsHTMLTitleElement();
  virtual ~nsHTMLTitleElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLTitleElement
  NS_DECL_IDOMHTMLTITLEELEMENT

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
};

nsresult
NS_NewHTMLTitleElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLTitleElement* it = new nsHTMLTitleElement();

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


nsHTMLTitleElement::nsHTMLTitleElement()
{
}

nsHTMLTitleElement::~nsHTMLTitleElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTitleElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTitleElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI(nsHTMLTitleElement,
                       nsGenericHTMLContainerElement,
                       nsIDOMHTMLTitleElement);


nsresult
nsHTMLTitleElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLTitleElement* it = new nsHTMLTitleElement();

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
nsHTMLTitleElement::GetText(nsAWritableString& aTitle)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> child;

  result = GetFirstChild(getter_AddRefs(child));

  if ((NS_OK == result) && child) {
    nsCOMPtr<nsIDOMText> text(do_QueryInterface(child));

    if (text) {
      text->GetData(aTitle);
    }
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLTitleElement::SetText(const nsAReadableString& aTitle)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> child;
  nsCOMPtr<nsIDocument> document;

  result = GetDocument(*getter_AddRefs(document));

  if (NS_OK == result) {
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(document));

    if (htmlDoc) {
      htmlDoc->SetTitle(aTitle);
    }   
  }

  result = GetFirstChild(getter_AddRefs(child));

  if ((NS_OK == result) && child) {
    nsCOMPtr<nsIDOMText> text(do_QueryInterface(child));
    
    if (text) {
      text->SetData(aTitle);
    }
  }

  return result;
}

NS_IMETHODIMP
nsHTMLTitleElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
