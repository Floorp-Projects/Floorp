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
#include "nsIDOMHTMLDelElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLDelElementIID, NS_IDOMHTMLDELELEMENT_IID);

class nsHTMLDel : public nsIDOMHTMLDelElement,
                  public nsIScriptObjectOwner,
                  public nsIDOMEventReceiver,
                  public nsIHTMLContent
{
public:
  nsHTMLDel(nsIAtom* aTag);
  ~nsHTMLDel();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLDelElement
  NS_IMETHOD GetCite(nsString& aCite);
  NS_IMETHOD SetCite(const nsString& aCite);
  NS_IMETHOD GetDateTime(nsString& aDateTime);
  NS_IMETHOD SetDateTime(const nsString& aDateTime);

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
NS_NewHTMLDel(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLDel(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLDel::nsHTMLDel(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLDel::~nsHTMLDel()
{
}

NS_IMPL_ADDREF(nsHTMLDel)

NS_IMPL_RELEASE(nsHTMLDel)

nsresult
nsHTMLDel::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLDelElementIID)) {
    nsIDOMHTMLDelElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLDel::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLDel* it = new nsHTMLDel(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLDel::GetCite(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::cite, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDel::SetCite(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::cite, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLDel::GetDateTime(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::datetime, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDel::SetDateTime(const nsString& aValue)
{
  return mInner.SetAttr(nsHTMLAtoms::datetime, aValue, eSetAttrNotify_None);
}

NS_IMETHODIMP
nsHTMLDel::StringToAttribute(nsIAtom* aAttribute,
                             const nsString& aValue,
                             nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLDel::AttributeToString(nsIAtom* aAttribute,
                             nsHTMLValue& aValue,
                             nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLDel::MapAttributesInto(nsIStyleContext* aContext,
                             nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDel::HandleDOMEvent(nsIPresContext& aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
