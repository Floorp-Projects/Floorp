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
#include "nsIDOMHTMLOptionElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);

class nsHTMLOption : public nsIDOMHTMLOptionElement,
                     public nsIScriptObjectOwner,
                     public nsIDOMEventReceiver,
                     public nsIHTMLContent
{
public:
  nsHTMLOption(nsIAtom* aTag);
  ~nsHTMLOption();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLOptionElement
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetDefaultSelected(PRBool* aDefaultSelected);
  NS_IMETHOD SetDefaultSelected(PRBool aDefaultSelected);
  NS_IMETHOD GetText(nsString& aText);
  NS_IMETHOD SetText(const nsString& aText);
  NS_IMETHOD GetIndex(PRInt32* aIndex);
  NS_IMETHOD SetIndex(PRInt32 aIndex);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetLabel(nsString& aLabel);
  NS_IMETHOD SetLabel(const nsString& aLabel);
  NS_IMETHOD GetSelected(PRBool* aSelected);
  NS_IMETHOD SetSelected(PRBool aSelected);
  NS_IMETHOD GetValue(nsString& aValue);
  NS_IMETHOD SetValue(const nsString& aValue);

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
NS_NewHTMLOption(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLOption(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLOption::nsHTMLOption(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLOption::~nsHTMLOption()
{
}

NS_IMPL_ADDREF(nsHTMLOption)

NS_IMPL_RELEASE(nsHTMLOption)

nsresult
nsHTMLOption::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLOptionElementIID)) {
    nsIDOMHTMLOptionElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLOption::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLOption* it = new nsHTMLOption(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLOption::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = nsnull;/* XXX */
  return NS_OK;
}

NS_IMPL_BOOL_ATTR(nsHTMLOption, DefaultSelected, defaultselected, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLOption, Text, text, eSetAttrNotify_Render)
NS_IMPL_INT_ATTR(nsHTMLOption, Index, index, eSetAttrNotify_None)
NS_IMPL_BOOL_ATTR(nsHTMLOption, Disabled, disabled, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLOption, Label, label, eSetAttrNotify_Render)
NS_IMPL_BOOL_ATTR(nsHTMLOption, Selected, selected, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLOption, Value, value, eSetAttrNotify_Render)

NS_IMETHODIMP
nsHTMLOption::StringToAttribute(nsIAtom* aAttribute,
                                const nsString& aValue,
                                nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLOption::AttributeToString(nsIAtom* aAttribute,
                                nsHTMLValue& aValue,
                                nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLOption::MapAttributesInto(nsIStyleContext* aContext,
                                nsIPresContext* aPresContext)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOption::HandleDOMEvent(nsIPresContext& aPresContext,
                             nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent,
                             PRUint32 aFlags,
                             nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
