/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLIFrameElementIID, NS_IDOMHTMLIFRAMEELEMENT_IID);

class nsHTMLIFrameElement : public nsIDOMHTMLIFrameElement,
                            public nsIScriptObjectOwner,
                            public nsIDOMEventReceiver,
                            public nsIHTMLContent
{
public:
  nsHTMLIFrameElement(nsIAtom* aTag);
  ~nsHTMLIFrameElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLIFrameElement
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetFrameBorder(nsString& aFrameBorder);
  NS_IMETHOD SetFrameBorder(const nsString& aFrameBorder);
  NS_IMETHOD GetHeight(nsString& aHeight);
  NS_IMETHOD SetHeight(const nsString& aHeight);
  NS_IMETHOD GetLongDesc(nsString& aLongDesc);
  NS_IMETHOD SetLongDesc(const nsString& aLongDesc);
  NS_IMETHOD GetMarginHeight(nsString& aMarginHeight);
  NS_IMETHOD SetMarginHeight(const nsString& aMarginHeight);
  NS_IMETHOD GetMarginWidth(nsString& aMarginWidth);
  NS_IMETHOD SetMarginWidth(const nsString& aMarginWidth);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetScrolling(nsString& aScrolling);
  NS_IMETHOD SetScrolling(const nsString& aScrolling);
  NS_IMETHOD GetSrc(nsString& aSrc);
  NS_IMETHOD SetSrc(const nsString& aSrc);
  NS_IMETHOD GetWidth(nsString& aWidth);
  NS_IMETHOD SetWidth(const nsString& aWidth);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsHTMLGenericContainerContent mInner;
};

nsresult
NS_NewHTMLIFrameElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLIFrameElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLIFrameElement::nsHTMLIFrameElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLIFrameElement::~nsHTMLIFrameElement()
{
}

NS_IMPL_ADDREF(nsHTMLIFrameElement)

NS_IMPL_RELEASE(nsHTMLIFrameElement)

nsresult
nsHTMLIFrameElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLIFrameElementIID)) {
    nsIDOMHTMLIFrameElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLIFrameElement::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLIFrameElement* it = new nsHTMLIFrameElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Align, align, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, FrameBorder, frameborder, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Height, height, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, LongDesc, longdesc, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, MarginHeight, marginheight, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, MarginWidth, marginwidth, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Name, name, eSetAttrNotify_Restart)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Scrolling, scrolling, eSetAttrNotify_Restart)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Src, src, eSetAttrNotify_Restart)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Width, width, eSetAttrNotify_Reflow)

NS_IMETHODIMP
nsHTMLIFrameElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsString& aValue,
                                       nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLIFrameElement::AttributeToString(nsIAtom* aAttribute,
                                       nsHTMLValue& aValue,
                                       nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLIFrameElement::MapAttributesInto(nsIStyleContext* aContext,
                                       nsIPresContext* aPresContext)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLIFrameElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
