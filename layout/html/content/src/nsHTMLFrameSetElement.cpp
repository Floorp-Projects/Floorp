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

static NS_DEFINE_IID(kIDOMHTMLFrameSetElementIID, NS_IDOMHTMLFRAMESETELEMENT_IID);

class nsHTMLFrameSetElement : public nsIDOMHTMLFrameSetElement,
                              public nsIJSScriptObject,
                              public nsIHTMLContent
{
public:
  nsHTMLFrameSetElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLFrameSetElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLFrameSetElement
  NS_IMETHOD GetCols(nsString& aCols);
  NS_IMETHOD SetCols(const nsString& aCols);
  NS_IMETHOD GetRows(nsString& aRows);
  NS_IMETHOD SetRows(const nsString& aRows);

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
NS_NewHTMLFrameSetElement(nsIHTMLContent** aInstancePtrResult,
                          nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLFrameSetElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}


nsHTMLFrameSetElement::nsHTMLFrameSetElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
}

nsHTMLFrameSetElement::~nsHTMLFrameSetElement()
{
}

NS_IMPL_ADDREF(nsHTMLFrameSetElement)

NS_IMPL_RELEASE(nsHTMLFrameSetElement)

nsresult
nsHTMLFrameSetElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLFrameSetElementIID)) {
    nsIDOMHTMLFrameSetElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLFrameSetElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLFrameSetElement* it = new nsHTMLFrameSetElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLFrameSetElement, Cols, cols)
NS_IMPL_STRING_ATTR(nsHTMLFrameSetElement, Rows, rows)

NS_IMETHODIMP
nsHTMLFrameSetElement::StringToAttribute(nsIAtom* aAttribute,
                                         const nsString& aValue,
                                         nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::bordercolor) {
    if (nsGenericHTMLElement::ParseColor(aValue, mInner.mDocument, aResult)) {
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
                                         nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::frameborder) {
    // XXX need to check for correct mode
    nsGenericHTMLElement::FrameborderValueToString(PR_FALSE, aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  } 
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
nsHTMLFrameSetElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                    nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameSetElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                      nsEvent* aEvent,
                                      nsIDOMEvent** aDOMEvent,
                                      PRUint32 aFlags,
                                      nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLFrameSetElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}
