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
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"

static NS_DEFINE_IID(kIDOMHTMLButtonElementIID, NS_IDOMHTMLBUTTONELEMENT_IID);

class nsHTMLButtonElement : public nsIDOMHTMLButtonElement,
                            public nsIScriptObjectOwner,
                            public nsIDOMEventReceiver,
                            public nsIHTMLContent
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
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetAccessKey(nsString& aAccessKey);
  NS_IMETHOD SetAccessKey(const nsString& aAccessKey);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD SetType(const nsString& aType);
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
  nsGenericHTMLContainerElement mInner;
};

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
}

nsHTMLButtonElement::~nsHTMLButtonElement()
{
}

NS_IMPL_ADDREF(nsHTMLButtonElement)

NS_IMPL_RELEASE(nsHTMLButtonElement)

nsresult
nsHTMLButtonElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLButtonElementIID)) {
    nsIDOMHTMLButtonElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLButtonElement::CloneNode(nsIDOMNode** aReturn)
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
  *aForm = nsnull;/* XXX */
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLButtonElement, AccessKey, accesskey, eSetAttrNotify_None)
NS_IMPL_BOOL_ATTR(nsHTMLButtonElement, Disabled, disabled, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Name, name, eSetAttrNotify_Restart)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Type, type, eSetAttrNotify_Restart)
NS_IMPL_INT_ATTR(nsHTMLButtonElement, TabIndex, tabindex, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Value, value, eSetAttrNotify_Render)

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
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLButtonElement::AttributeToString(nsIAtom* aAttribute,
                                       nsHTMLValue& aValue,
                                       nsString& aResult) const
{
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
          stateManager->SetActiveLink(this);
          NS_RELEASE(stateManager);
        }
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      {
        nsIEventStateManager *stateManager;
        nsIContent *activeLink;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          stateManager->GetActiveLink(&activeLink);
          NS_RELEASE(stateManager);
        }

        if (activeLink == this) {
          nsEventStatus status;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = NS_MOUSE_LEFT_CLICK;
          HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, status);

          if (nsEventStatus_eConsumeNoDefault != status) {
            nsAutoString base, href, target;
            GetAttribute(nsString(NS_HTML_BASE_HREF), base);
            GetAttribute(nsString("href"), href);
            GetAttribute(nsString("target"), target);
            if (target.Length() == 0) {
              GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
            }
            mInner.TriggerLink(aPresContext, base, href, target, PR_TRUE);
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
        GetAttribute(nsString(NS_HTML_BASE_HREF), base);
        GetAttribute(nsString("href"), href);
        GetAttribute(nsString("target"), target);
        if (target.Length() == 0) {
          GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
        }
        mInner.TriggerLink(aPresContext, base, href, target, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT:
      {
        nsAutoString empty;
        mInner.TriggerLink(aPresContext, empty, empty, empty, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

    default:
      break;
    }
  }
  return ret;
}
