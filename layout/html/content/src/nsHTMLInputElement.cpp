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
#include "nsIDOMHTMLInputElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

class nsHTMLInputElement : public nsIDOMHTMLInputElement,
                    public nsIScriptObjectOwner,
                    public nsIDOMEventReceiver,
                    public nsIHTMLContent
{
public:
  nsHTMLInputElement(nsIAtom* aTag);
  ~nsHTMLInputElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLInputElement
  NS_IMETHOD GetDefaultValue(nsString& aDefaultValue);
  NS_IMETHOD SetDefaultValue(const nsString& aDefaultValue);
  NS_IMETHOD GetDefaultChecked(PRBool* aDefaultChecked);
  NS_IMETHOD SetDefaultChecked(PRBool aDefaultChecked);
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD GetAccept(nsString& aAccept);
  NS_IMETHOD SetAccept(const nsString& aAccept);
  NS_IMETHOD GetAccessKey(nsString& aAccessKey);
  NS_IMETHOD SetAccessKey(const nsString& aAccessKey);
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetAlt(nsString& aAlt);
  NS_IMETHOD SetAlt(const nsString& aAlt);
  NS_IMETHOD GetChecked(PRBool* aChecked);
  NS_IMETHOD SetChecked(PRBool aChecked);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetMaxLength(PRInt32* aMaxLength);
  NS_IMETHOD SetMaxLength(PRInt32 aMaxLength);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetReadOnly(PRBool* aReadOnly);
  NS_IMETHOD SetReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetSize(nsString& aSize);
  NS_IMETHOD SetSize(const nsString& aSize);
  NS_IMETHOD GetSrc(nsString& aSrc);
  NS_IMETHOD SetSrc(const nsString& aSrc);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD GetUseMap(nsString& aUseMap);
  NS_IMETHOD SetUseMap(const nsString& aUseMap);
  NS_IMETHOD GetValue(nsString& aValue);
  NS_IMETHOD SetValue(const nsString& aValue);
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();
  NS_IMETHOD Select();
  NS_IMETHOD Click();

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLLeafElement mInner;
};

nsresult
NS_NewHTMLInputElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLInputElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLInputElement::nsHTMLInputElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLInputElement::~nsHTMLInputElement()
{
}

NS_IMPL_ADDREF(nsHTMLInputElement)

NS_IMPL_RELEASE(nsHTMLInputElement)

nsresult
nsHTMLInputElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLInputElementIID)) {
    nsIDOMHTMLInputElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLInputElement::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLInputElement* it = new nsHTMLInputElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLInputElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = nsnull;/* XXX */
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLInputElement, DefaultValue, defaultvalue, eSetAttrNotify_None)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, DefaultChecked, defaultchecked, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Accept, accept, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, AccessKey, accesskey, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Align, align, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Alt, alt, eSetAttrNotify_None)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Checked, checked, eSetAttrNotify_Render)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Disabled, disabled, eSetAttrNotify_Render)
NS_IMPL_INT_ATTR(nsHTMLInputElement, MaxLength, maxlength, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Name, name, eSetAttrNotify_Restart)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, ReadOnly, readonly, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Size, size, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Src, src, eSetAttrNotify_Render)
NS_IMPL_INT_ATTR(nsHTMLInputElement, TabIndex, tabindex, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, UseMap, usemap, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Value, value, eSetAttrNotify_Render)

NS_IMETHODIMP
nsHTMLInputElement::GetType(nsString& aValue)
{
  mInner.GetAttribute(nsHTMLAtoms::type, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::Blur()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::Focus()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::Select()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::Click()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::StringToAttribute(nsIAtom* aAttribute,
                               const nsString& aValue,
                               nsHTMLValue& aResult)
{
  // XXX align
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLInputElement::AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const
{
  // XXX align
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  // XXX align
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLInputElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLInputElement::HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
