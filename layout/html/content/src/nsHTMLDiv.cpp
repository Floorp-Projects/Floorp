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
#include "nsIDOMHTMLDivElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

// XXX missing nav attributes

static NS_DEFINE_IID(kIDOMHTMLDivElementIID, NS_IDOMHTMLDIVELEMENT_IID);

class nsHTMLDiv : public nsIDOMHTMLDivElement,
                  public nsIScriptObjectOwner,
                  public nsIDOMEventReceiver,
                  public nsIHTMLContent
{
public:
  nsHTMLDiv(nsIAtom* aTag);
  ~nsHTMLDiv();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLDivElement
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);

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
NS_NewHTMLDiv(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLDiv(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLDiv::nsHTMLDiv(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLDiv::~nsHTMLDiv()
{
}

NS_IMPL_ADDREF(nsHTMLDiv)

NS_IMPL_RELEASE(nsHTMLDiv)

nsresult
nsHTMLDiv::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLDivElementIID)) {
    nsIDOMHTMLDivElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLDiv::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLDiv* it = new nsHTMLDiv(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLDiv::GetAlign(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::align, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDiv::SetAlign(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::align, aValue, eSetAttrNotify_Reflow);
}

NS_IMETHODIMP
nsHTMLDiv::StringToAttribute(nsIAtom* aAttribute,
                             const nsString& aValue,
                             nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (nsHTMLGenericContent::ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLDiv::AttributeToString(nsIAtom* aAttribute,
                             nsHTMLValue& aValue,
                             nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsHTMLGenericContent::AlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLDiv::MapAttributesInto(nsIStyleContext* aContext,
                             nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDiv::HandleDOMEvent(nsIPresContext& aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
