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


class nsHTMLTitleElement : public nsIDOMHTMLTitleElement,
                           public nsIJSScriptObject,
                           public nsIHTMLContent
{
public:
  nsHTMLTitleElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLTitleElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTitleElement
  NS_DECL_IDOMHTMLTITLEELEMENT

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
NS_NewHTMLTitleElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLTitleElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLTitleElement::nsHTMLTitleElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
}

nsHTMLTitleElement::~nsHTMLTitleElement()
{
}

NS_IMPL_ADDREF(nsHTMLTitleElement)

NS_IMPL_RELEASE(nsHTMLTitleElement)

nsresult
nsHTMLTitleElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLTitleElement))) {
    nsIDOMHTMLTitleElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTitleElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLTitleElement* it = new nsHTMLTitleElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLTitleElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsAReadableString& aValue,
                                      nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTitleElement::AttributeToString(nsIAtom* aAttribute,
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
nsHTMLTitleElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTitleElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP 
nsHTMLTitleElement::GetText(nsAWritableString& aTitle)
{
  nsresult result = NS_OK;
  nsIDOMNode* child;

  result = GetFirstChild(&child);
  if ((NS_OK == result) && (nsnull != child)) {
    nsIDOMText* text;
    
    result = child->QueryInterface(NS_GET_IID(nsIDOMText), (void**)&text);
    if (NS_OK == result) {
      text->GetData(aTitle);
      NS_RELEASE(text);
    }
    NS_RELEASE(child);
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLTitleElement::SetText(const nsAReadableString& aTitle)
{
  nsresult result = NS_OK;
  nsIDOMNode* child;
  nsIDocument* document;

  result = GetDocument(document);
  if (NS_OK == result) {
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(document));
    if (htmlDoc) {
      htmlDoc->SetTitle(aTitle);
    }   
    NS_RELEASE(document);
  }

  result = GetFirstChild(&child);
  if ((NS_OK == result) && (nsnull != child)) {
    nsIDOMText* text;
    
    result = child->QueryInterface(NS_GET_IID(nsIDOMText), (void**)&text);
    if (NS_OK == result) {
      text->SetData(aTitle);
      NS_RELEASE(text);
    }
    NS_RELEASE(child);
  }

  return result;
}

NS_IMETHODIMP
nsHTMLTitleElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLTitleElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}
