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
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"

static NS_DEFINE_IID(kIDOMHTMLButtonElementIID, NS_IDOMHTMLBUTTONELEMENT_IID);

class nsHTMLButtonElement : public nsIDOMHTMLButtonElement,
                            public nsIScriptObjectOwner,
                            public nsIDOMEventReceiver,
                            public nsIHTMLContent,
                            public nsIFormControl
{
public:
  nsHTMLButtonElement(nsIAtom* aTag);
  ~nsHTMLButtonElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLButtonElement
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD GetAccessKey(nsString& aAccessKey);
  NS_IMETHOD SetAccessKey(const nsString& aAccessKey);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD GetValue(nsString& aValue);
  NS_IMETHOD SetValue(const nsString& aValue);

  // nsIDOMHTMLButtonElement
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

  // nsIFormControl
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD SetWidget(nsIWidget* aWidget) { return NS_OK; };
  NS_IMETHOD Init() { return NS_OK; }

protected:
  nsGenericHTMLContainerElement mInner;
  nsIForm*                      mForm;
  PRInt32                       mType;
};

static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);

// Construction, destruction

nsresult
NS_NewHTMLButtonElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLButtonElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLButtonElement::nsHTMLButtonElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mForm = nsnull;
  mType = NS_FORM_BUTTON_BUTTON; // default
}

nsHTMLButtonElement::~nsHTMLButtonElement()
{
  if (nsnull != mForm) {
    // prevent mForm from decrementing its ref count on us
    mForm->RemoveElement(this, PR_FALSE); 
    NS_RELEASE(mForm);
  }
}

// nsISupports

NS_IMETHODIMP_(nsrefcnt) 
nsHTMLButtonElement::AddRef(void)
{
  PRInt32 refCnt = mRefCnt;  // debugging 
  return ++mRefCnt; 
}

nsresult
nsHTMLButtonElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLButtonElementIID)) {
    *aInstancePtr = (void*)(nsIDOMHTMLButtonElement*)this;
    mRefCnt++;
    return NS_OK;
  }
  else if (aIID.Equals(kIFormControlIID)) {
    *aInstancePtr = (void*)(nsIFormControl*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP_(nsrefcnt)
nsHTMLButtonElement::Release()
{
  --mRefCnt;
	if (mRefCnt <= 0) {
    delete this;                                       
    return 0;                                          
  } else if ((1 == mRefCnt) && mForm) { 
    mRefCnt = 0;
    delete this;
    return 0;
  } else {
    return mRefCnt;
  }
}


// nsIDOMHTMLButtonElement

nsresult
nsHTMLButtonElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLButtonElement* it = new nsHTMLButtonElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLButtonElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  nsresult result = NS_OK;
  *aForm = nsnull;
  if (nsnull != mForm) {
    nsIDOMHTMLFormElement* formElem = nsnull;
    result = mForm->QueryInterface(kIDOMHTMLFormElementIID, (void**)&formElem);
    if (NS_OK == result) {
      *aForm = formElem;
    }
  }
  return result;
}
NS_IMETHODIMP
nsHTMLButtonElement::GetType(nsString& aType)
{
  aType.SetString("button");
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLButtonElement, AccessKey, accesskey)
NS_IMPL_BOOL_ATTR(nsHTMLButtonElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Name, name)
NS_IMPL_INT_ATTR(nsHTMLButtonElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Value, value)

NS_IMETHODIMP
nsHTMLButtonElement::Blur()
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::Focus()
{
  // XXX write me
  return NS_OK;
}

static nsGenericHTMLElement::EnumTable kButtonTypeTable[] = {
  { "button", NS_FORM_BUTTON_BUTTON },
  { "reset", NS_FORM_BUTTON_RESET },
  { "submit", NS_FORM_BUTTON_SUBMIT },
  { 0 }
};

NS_IMETHODIMP
nsHTMLButtonElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::tabindex) {
    nsGenericHTMLElement::ParseValue(aValue, 0, 32767, aResult,
                                     eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  if (aAttribute == nsHTMLAtoms::type) {
    nsGenericHTMLElement::EnumTable *table = kButtonTypeTable;
    while (nsnull != table->tag) { 
      if (aValue.EqualsIgnoreCase(table->tag)) {
        aResult.SetIntValue(table->value, eHTMLUnit_Enumerated);
        mType = table->value;  
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
      table++;
    }
  }
  else if (aAttribute == nsHTMLAtoms::disabled) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLButtonElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::EnumValueToString(aValue, kButtonTypeTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLButtonElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus& aEventStatus)
{
  // Try script event handlers first
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                       aFlags, aEventStatus);

  if ((NS_OK == ret) && (nsEventStatus_eIgnore == aEventStatus)) {
    switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          //stateManager->SetActiveLink(this);
          NS_RELEASE(stateManager);
        }
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      {
        nsIEventStateManager *stateManager;
        nsIContent *activeLink = nsnull;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          //stateManager->GetActiveLink(&activeLink);
          NS_RELEASE(stateManager);
        }

        if (activeLink == this) {
          if (nsEventStatus_eConsumeNoDefault != aEventStatus) {
            nsAutoString base, href, target;
            GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_baseHref, base);
            GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, href);
            GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
            if (target.Length() == 0) {
              GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_baseTarget, target);
            }
            mInner.TriggerLink(aPresContext, eLinkVerb_Replace, base, href, target, PR_TRUE);
            aEventStatus = nsEventStatus_eConsumeNoDefault; 
          }
        }
      }
      break;

    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      // XXX Bring up a contextual menu provided by the application
      break;

    case NS_MOUSE_ENTER:
      //mouse enter doesn't work yet.  Use move until then.
      {
        nsAutoString base, href, target;
        GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_baseHref, base);
        GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, href);
        GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
        if (target.Length() == 0) {
          GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_baseTarget, target);
        }
        mInner.TriggerLink(aPresContext, eLinkVerb_Replace, base, href, target, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT:
      {
        nsAutoString empty;
        mInner.TriggerLink(aPresContext, eLinkVerb_Replace, empty, empty, empty, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

    default:
      break;
    }
  }
  return ret;
}

NS_IMETHODIMP
nsHTMLButtonElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = mType;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}

// An important assumption is that if aForm is null, the previous mForm will not be released
// This allows nsHTMLFormElement to deal with circular references.
NS_IMETHODIMP
nsHTMLButtonElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  nsresult result = NS_OK;
  if (nsnull == aForm) {
    mForm = nsnull;
    return NS_OK;
  } else {
    NS_IF_RELEASE(mForm);
    nsIFormControl* formControl = nsnull;
    result = QueryInterface(kIFormControlIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      result = aForm->QueryInterface(kIFormIID, (void**)&mForm); // keep the ref
      if ((NS_OK == result) && mForm) {
        mForm->AddElement(formControl);
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLButtonElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  nsGenericHTMLElement::SetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}