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
#include "nsIDOMHTMLScriptElement.h"
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
#include "nsITextContent.h"


class nsHTMLScriptElement : public nsGenericHTMLContainerElement,
                            public nsIDOMHTMLScriptElement
{
public:
  nsHTMLScriptElement();
  virtual ~nsHTMLScriptElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLScriptElement
  NS_DECL_IDOMHTMLSCRIPTELEMENT

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
};

nsresult
NS_NewHTMLScriptElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLScriptElement* it = new nsHTMLScriptElement();

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


nsHTMLScriptElement::nsHTMLScriptElement()
{
}

nsHTMLScriptElement::~nsHTMLScriptElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLScriptElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLScriptElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI(nsHTMLScriptElement, nsGenericHTMLContainerElement,
                       nsIDOMHTMLScriptElement);


nsresult
nsHTMLScriptElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLScriptElement* it = new nsHTMLScriptElement();

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
nsHTMLScriptElement::GetText(nsAWritableString& aValue)
{
  PRInt32 i, count = 0;
  nsresult rv = NS_OK;

  aValue.Truncate();

  ChildCount(count);

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child;

    rv = ChildAt(i, *getter_AddRefs(child));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(child));

    if (node) {
      nsAutoString tmp;
      node->GetNodeValue(tmp);

      aValue.Append(tmp);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::SetText(const nsAReadableString& aValue)
{
  nsCOMPtr<nsIContent> content;
  PRInt32 i, count = 0;
  nsresult rv = NS_OK;

  ChildCount(count);

  if (count) {
    for (i = count-1; i > 1; i--) {
      RemoveChildAt(i, PR_FALSE);
    }

    rv = ChildAt(0, *getter_AddRefs(content));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = NS_NewTextNode(getter_AddRefs(content));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InsertChildAt(content, 0, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (content) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));

    if (node) {
      rv = node->SetNodeValue(aValue);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLScriptElement::GetHtmlFor(nsAWritableString& aValue)
{
  // XXX write me
//  GetAttribute(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::SetHtmlFor(const nsAReadableString& aValue)
{
  // XXX write me
//  return SetAttr(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::GetEvent(nsAWritableString& aValue)
{
  // XXX write me
//  GetAttribute(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::SetEvent(const nsAReadableString& aValue)
{
  // XXX write me
//  return SetAttr(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Charset, charset)
NS_IMPL_BOOL_ATTR(nsHTMLScriptElement, Defer, defer)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Type, type)


NS_IMETHODIMP
nsHTMLScriptElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
