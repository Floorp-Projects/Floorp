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
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);

class nsHTMLTextAreaElement : public nsIDOMHTMLTextAreaElement,
                  public nsIScriptObjectOwner,
                  public nsIDOMEventReceiver,
                  public nsIHTMLContent
{
public:
  nsHTMLTextAreaElement(nsIAtom* aTag);
  ~nsHTMLTextAreaElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTextAreaElement
  NS_IMETHOD GetDefaultValue(nsString& aDefaultValue);
  NS_IMETHOD SetDefaultValue(const nsString& aDefaultValue);
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetAccessKey(nsString& aAccessKey);
  NS_IMETHOD SetAccessKey(const nsString& aAccessKey);
  NS_IMETHOD GetCols(PRInt32* aCols);
  NS_IMETHOD SetCols(PRInt32 aCols);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetReadOnly(PRBool* aReadOnly);
  NS_IMETHOD SetReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetRows(PRInt32* aRows);
  NS_IMETHOD SetRows(PRInt32 aRows);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();
  NS_IMETHOD Select();

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
NS_NewHTMLTextAreaElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTextAreaElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLTextAreaElement::nsHTMLTextAreaElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLTextAreaElement::~nsHTMLTextAreaElement()
{
}

NS_IMPL_ADDREF(nsHTMLTextAreaElement)

NS_IMPL_RELEASE(nsHTMLTextAreaElement)

nsresult
nsHTMLTextAreaElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLTextAreaElementIID)) {
    nsIDOMHTMLTextAreaElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTextAreaElement::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLTextAreaElement* it = new nsHTMLTextAreaElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = nsnull;/* XXX */
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Blur()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Focus()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Select()
{
  // XXX write me
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, DefaultValue, defaultvalue, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, AccessKey, accesskey, eSetAttrNotify_None)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, Cols, cols, eSetAttrNotify_Reflow)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Disabled, disabled, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, Name, name, eSetAttrNotify_Restart)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, ReadOnly, readonly, eSetAttrNotify_Render)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, Rows, rows, eSetAttrNotify_Reflow)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, TabIndex, tabindex, eSetAttrNotify_None)

NS_IMETHODIMP
nsHTMLTextAreaElement::StringToAttribute(nsIAtom* aAttribute,
                                         const nsString& aValue,
                                         nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::AttributeToString(nsIAtom* aAttribute,
                                         nsHTMLValue& aValue,
                                         nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::MapAttributesInto(nsIStyleContext* aContext,
                                         nsIPresContext* aPresContext)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                      nsEvent* aEvent,
                                      nsIDOMEvent** aDOMEvent,
                                      PRUint32 aFlags,
                                      nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
