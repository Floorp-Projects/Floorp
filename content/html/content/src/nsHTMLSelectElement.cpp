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
#include "nsIDOMHTMLSelectElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);

class nsHTMLSelectElement : public nsIDOMHTMLSelectElement,
                   public nsIScriptObjectOwner,
                   public nsIDOMEventReceiver,
                   public nsIHTMLContent
{
public:
  nsHTMLSelectElement(nsIAtom* aTag);
  ~nsHTMLSelectElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLSelectElement
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD SetType(const nsString& aType);
  NS_IMETHOD GetSelectedIndex(PRInt32* aSelectedIndex);
  NS_IMETHOD SetSelectedIndex(PRInt32 aSelectedIndex);
  NS_IMETHOD GetValue(nsString& aValue);
  NS_IMETHOD SetValue(const nsString& aValue);
  NS_IMETHOD GetLength(PRInt32* aLength);
  NS_IMETHOD SetLength(PRInt32 aLength);
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetOptions(nsIDOMHTMLCollection** aOptions);
  NS_IMETHOD SetOptions(nsIDOMHTMLCollection* aOptions);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetMultiple(PRBool* aMultiple);
  NS_IMETHOD SetMultiple(PRBool aMultiple);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetSize(PRInt32* aSize);
  NS_IMETHOD SetSize(PRInt32 aSize);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore);
  NS_IMETHOD Remove(PRInt32 aIndex);
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();

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
NS_NewHTMLSelectElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLSelectElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLSelectElement::nsHTMLSelectElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLSelectElement::~nsHTMLSelectElement()
{
}

NS_IMPL_ADDREF(nsHTMLSelectElement)

NS_IMPL_RELEASE(nsHTMLSelectElement)

nsresult
nsHTMLSelectElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLSelectElementIID)) {
    nsIDOMHTMLSelectElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLSelectElement::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLSelectElement* it = new nsHTMLSelectElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLSelectElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = nsnull;/* XXX */
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetOptions(nsIDOMHTMLCollection** aValue)
{
  *aValue = nsnull;/* XXX */
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetOptions(nsIDOMHTMLCollection* aValue)
{
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Type, type, eSetAttrNotify_Restart)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, SelectedIndex, selectedindex, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Value, value, eSetAttrNotify_Render)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, Length, length, eSetAttrNotify_Restart)
NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Disabled, disabled, eSetAttrNotify_Render)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, Multiple, multiple, eSetAttrNotify_Restart)
NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Name, name, eSetAttrNotify_Restart)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, Size, size, eSetAttrNotify_Render)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, TabIndex, tabindex, eSetAttrNotify_Render)

NS_IMETHODIMP
nsHTMLSelectElement::Blur()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::Focus()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLSelectElement::AttributeToString(nsIAtom* aAttribute,
                              nsHTMLValue& aValue,
                              nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLSelectElement::MapAttributesInto(nsIStyleContext* aContext,
                              nsIPresContext* aPresContext)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::HandleDOMEvent(nsIPresContext& aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
