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


class nsHTMLScriptElement : public nsIDOMHTMLScriptElement,
                     public nsIJSScriptObject,
                     public nsIHTMLContent
{
public:
  nsHTMLScriptElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLScriptElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLScriptElement
  NS_DECL_IDOMHTMLSCRIPTELEMENT

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
};

nsresult
NS_NewHTMLScriptElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLScriptElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLScriptElement::nsHTMLScriptElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
}

nsHTMLScriptElement::~nsHTMLScriptElement()
{
}

NS_IMPL_ADDREF(nsHTMLScriptElement)

NS_IMPL_RELEASE(nsHTMLScriptElement)

nsresult
nsHTMLScriptElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLScriptElement))) {
    nsIDOMHTMLScriptElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLScriptElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLScriptElement* it = new nsHTMLScriptElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
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
//  mInner.GetAttribute(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::SetHtmlFor(const nsAReadableString& aValue)
{
  // XXX write me
//  return mInner.SetAttr(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::GetEvent(nsAWritableString& aValue)
{
  // XXX write me
//  mInner.GetAttribute(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::SetEvent(const nsAReadableString& aValue)
{
  // XXX write me
//  return mInner.SetAttr(nsHTMLAtoms::charset, aValue);
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Charset, charset)
NS_IMPL_BOOL_ATTR(nsHTMLScriptElement, Defer, defer)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Type, type)

NS_IMETHODIMP
nsHTMLScriptElement::StringToAttribute(nsIAtom* aAttribute,
                                const nsAReadableString& aValue,
                                nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLScriptElement::AttributeToString(nsIAtom* aAttribute,
                                const nsHTMLValue& aValue,
                                nsAWritableString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLScriptElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                  nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLScriptElement::HandleDOMEvent(nsIPresContext* aPresContext,
                             nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent,
                             PRUint32 aFlags,
                             nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLScriptElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  mInner.SizeOf(aSizer, aResult, sizeof(*this));
#endif
  return NS_OK;
}
