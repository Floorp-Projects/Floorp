/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIFormSubmission.h"
#include "nsIURL.h"

#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDocument.h"
#include "nsGUIEvent.h"
#include "nsUnicharUtils.h"


class nsHTMLButtonElement : public nsGenericHTMLFormElement,
                            public nsIDOMHTMLButtonElement,
                            public nsIDOMNSHTMLButtonElement
{
public:
  nsHTMLButtonElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLButtonElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLButtonElement
  NS_DECL_NSIDOMHTMLBUTTONELEMENT

  // nsIDOMNSHTMLButtonElement
  // Can't just use the macro, since it shares GetType with
  // nsIDOMHTMLButtonElement
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();
  NS_IMETHOD Click();
  NS_IMETHOD SetType(const nsAString& aType);

  // overrided nsIFormControl method
  NS_IMETHOD_(PRInt32) GetType() { return mType; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);

  // nsIContent overrides...
  virtual void SetFocus(nsPresContext* aPresContext);
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull);
  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  virtual nsresult HandleDOMEvent(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);

protected:
  PRInt8 mType;
  PRPackedBool mHandlingClick;

private:
  // The analogue of defaultValue in the DOM for input and textarea
  nsresult SetDefaultValue(const nsAString& aDefaultValue);
  nsresult GetDefaultValue(nsAString& aDefaultValue);
};


// Construction, destruction


NS_IMPL_NS_NEW_HTML_ELEMENT(Button)


nsHTMLButtonElement::nsHTMLButtonElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
{
  mType = NS_FORM_BUTTON_SUBMIT; // default
  mHandlingClick = PR_FALSE;
}

nsHTMLButtonElement::~nsHTMLButtonElement()
{
}

// nsISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLButtonElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLButtonElement, nsGenericElement)


// QueryInterface implementation for nsHTMLButtonElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLButtonElement,
                                    nsGenericHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLButtonElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLButtonElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLButtonElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

// nsIDOMHTMLButtonElement


NS_IMPL_HTML_DOM_CLONENODE(Button)


// nsIDOMHTMLButtonElement

NS_IMETHODIMP
nsHTMLButtonElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

NS_IMPL_STRING_ATTR(nsHTMLButtonElement, AccessKey, accesskey)
NS_IMPL_BOOL_ATTR(nsHTMLButtonElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Name, name)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLButtonElement, TabIndex, tabindex, 0)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Value, value)
NS_IMPL_STRING_ATTR_DEFAULT_VALUE(nsHTMLButtonElement, Type, type, "submit")

NS_IMETHODIMP
nsHTMLButtonElement::Blur()
{
  SetElementFocus(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::Focus()
{
  SetElementFocus(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::Click()
{
  if (mHandlingClick)
    return NS_OK;

  mHandlingClick = PR_TRUE;
  // Hold on to the document in case one of the events makes it die or
  // something...
  nsCOMPtr<nsIDocument> doc = mDocument; 

  if (mDocument) {
    nsIPresShell *shell = doc->GetShellAt(0);
    if (shell) {
      nsCOMPtr<nsPresContext> context;
      shell->GetPresContext(getter_AddRefs(context));
      if (context) {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event(NS_MOUSE_LEFT_CLICK);
        HandleDOMEvent(context, &event, nsnull,
                       NS_EVENT_FLAG_INIT, &status);
      }
    }
  }

  mHandlingClick = PR_FALSE;

  return NS_OK;
}

PRBool
nsHTMLButtonElement::IsFocusable(PRInt32 *aTabIndex)
{
  if (!nsGenericHTMLElement::IsFocusable(aTabIndex)) {
    return PR_FALSE;
  }
  if (aTabIndex && (sTabFocusModel & eTabFocus_formElementsMask) == 0) {
    *aTabIndex = -1;
  }
  return PR_TRUE;
}

void
nsHTMLButtonElement::SetFocus(nsPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  // first see if we are disabled or not. If disabled then do nothing.
  if (HasAttr(kNameSpaceID_None, nsHTMLAtoms::disabled)) {
    return;
  }

  aPresContext->EventStateManager()->SetContentState(this,
                                                     NS_EVENT_STATE_FOCUS);

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
  }
}

static const nsHTMLValue::EnumTable kButtonTypeTable[] = {
  { "button", NS_FORM_BUTTON_BUTTON },
  { "reset", NS_FORM_BUTTON_RESET },
  { "submit", NS_FORM_BUTTON_SUBMIT },
  { 0 }
};

PRBool
nsHTMLButtonElement::ParseAttribute(nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::type) {
    // XXX ARG!! This is major evilness. ParseAttribute
    // shouldn't set members. Override SetAttr instead
    PRBool res = aResult.ParseEnumValue(aValue, kButtonTypeTable);
    if (res) {
      mType = aResult.GetEnumValue();
    }
    return res;
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLButtonElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      aValue.EnumValueToString(kButtonTypeTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLFormElement::AttributeToString(aAttribute, aValue,
                                                     aResult);
}

nsresult
nsHTMLButtonElement::HandleDOMEvent(nsPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // Do not process any DOM events if the element is disabled
  PRBool bDisabled;
  nsresult rv = GetDisabled(&bDisabled);
  if (NS_FAILED(rv) || bDisabled) {
    return rv;
  }

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);

  if (formControlFrame) {
    nsIFrame* formFrame = nsnull;
    CallQueryInterface(formControlFrame, &formFrame);

    if (formFrame) {
      const nsStyleUserInterface* uiStyle = formFrame->GetStyleUserInterface();

      if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
          uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
        return NS_OK;
    }
  }

  PRBool bInSubmitClick = mType == NS_FORM_BUTTON_SUBMIT && 
                          !(aFlags & NS_EVENT_FLAG_CAPTURE) &&
                          !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) &&
                          aEvent->message == NS_MOUSE_LEFT_CLICK &&
                          mForm;

  if (bInSubmitClick) {
    // tell the form that we are about to enter a click handler.
    // that means that if there are scripted submissions, the
    // latest one will be deferred until after the exit point of the handler. 
    mForm->OnSubmitClickBegin();
  }

  // Try script event handlers first
  nsresult ret;
  ret = nsGenericHTMLFormElement::HandleDOMEvent(aPresContext, aEvent,
                                                 aDOMEvent, aFlags,
                                                 aEventStatus);

  // mForm is null if the event handler removed us from the document (bug 194582).
  if (bInSubmitClick && mForm) {
    // tell the form that we are about to exit a click handler
    // so the form knows not to defer subsequent submissions
    // the pending ones that were created during the handler
    // will be flushed or forgoten.
    mForm->OnSubmitClickEnd();
  }

  if (NS_SUCCEEDED(ret) && 
     !(aFlags & NS_EVENT_FLAG_CAPTURE) && !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT)) {
    if (nsEventStatus_eIgnore == *aEventStatus) { 
      switch (aEvent->message) {

      case NS_KEY_PRESS:
      case NS_KEY_UP:
        {
          // For backwards compat, trigger buttons with space or enter
          // (bug 25300)
          nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
          if ((keyEvent->keyCode == NS_VK_RETURN && NS_KEY_PRESS == aEvent->message) ||
              keyEvent->keyCode == NS_VK_SPACE  && NS_KEY_UP == aEvent->message) {
            nsEventStatus status = nsEventStatus_eIgnore;
            nsMouseEvent event(NS_MOUSE_LEFT_CLICK);
            rv = HandleDOMEvent(aPresContext, &event, nsnull,
                                NS_EVENT_FLAG_INIT, &status);
          }
        }
        break;// NS_KEY_PRESS

      case NS_MOUSE_LEFT_CLICK:
        {
          nsIPresShell *presShell = aPresContext->GetPresShell();
          if (presShell) {
            nsDOMUIEvent event(NS_DOMUI_ACTIVATE, 1); // single-click
            nsEventStatus status = nsEventStatus_eIgnore;

            presShell->HandleDOMEventWithTarget(this, &event, &status);
            *aEventStatus = status;
          }
        }
        break;

      case NS_DOMUI_ACTIVATE:
        {
          if (mForm) {
            if (mType == NS_FORM_BUTTON_SUBMIT || mType == NS_FORM_BUTTON_RESET) {
              nsFormEvent event((mType == NS_FORM_BUTTON_RESET)
                                ? NS_FORM_RESET : NS_FORM_SUBMIT);
              event.originator      = this;
              nsEventStatus status  = nsEventStatus_eIgnore;

              nsIPresShell *presShell = aPresContext->GetPresShell();
              // If |nsIPresShell::Destroy| has been called due to
              // handling the event (base class HandleDOMEvent, above),
              // the pres context will return a null pres shell.  See
              // bug 125624.
              if (presShell) {
                nsCOMPtr<nsIContent> form(do_QueryInterface(mForm));
                presShell->HandleDOMEventWithTarget(form, &event, &status);
              }
            }
          }
        }
        break;// NS_MOUSE_LEFT_CLICK

      case NS_MOUSE_LEFT_BUTTON_DOWN:
        {
          aPresContext->EventStateManager()->
            SetContentState(this,
                            NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS);

          *aEventStatus = nsEventStatus_eConsumeNoDefault; 
        }
        break;

      // cancel all of these events for buttons
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_MIDDLE_DOUBLECLICK:
      case NS_MOUSE_RIGHT_DOUBLECLICK:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_UP:
        {
          nsCOMPtr<nsIDOMNSEvent> nsevent;

          if (aDOMEvent) {
            nsevent = do_QueryInterface(*aDOMEvent);
          }

          if (nsevent) {
            nsevent->PreventBubble();
          } else {
            ret = NS_ERROR_FAILURE;
          }
        }

        break;

      case NS_MOUSE_ENTER_SYNTH:
        {
          aPresContext->EventStateManager()->
            SetContentState(this, NS_EVENT_STATE_HOVER);

          *aEventStatus = nsEventStatus_eConsumeNoDefault; 
        }
        break;

        // XXX this doesn't seem to do anything yet
      case NS_MOUSE_EXIT_SYNTH:
        {
          aPresContext->EventStateManager()->
            SetContentState(nsnull, NS_EVENT_STATE_HOVER);

          *aEventStatus = nsEventStatus_eConsumeNoDefault; 
        }
        break;

      default:
        break;
      }
	  } else {
      switch (aEvent->message) {
      case NS_DOMUI_ACTIVATE:
        if (mForm && mType == NS_FORM_BUTTON_SUBMIT) {
          // tell the form to flush a possible pending submission.
          // the reason is that the script returned false (the event was
          // not ignored) so if there is a stored submission, it needs to
          // be submitted immediatelly.
          mForm->FlushPendingSubmission();
        }
        break;// NS_DOMUI_ACTIVATE
      } //switch
    } //if
  } //if

  return ret;
}

nsresult
nsHTMLButtonElement::GetDefaultValue(nsAString& aDefaultValue)
{
  return GetAttr(kNameSpaceID_None, nsHTMLAtoms::value, aDefaultValue);
}

nsresult
nsHTMLButtonElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::value, aDefaultValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLButtonElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                       nsIContent* aSubmitElement)
{
  nsresult rv = NS_OK;

  //
  // We only submit if we were the button pressed
  //
  if (aSubmitElement != this) {
    return NS_OK;
  }

  //
  // Disabled elements don't submit
  //
  PRBool disabled;
  rv = GetDisabled(&disabled);
  if (NS_FAILED(rv) || disabled) {
    return rv;
  }

  //
  // Get the name (if no name, no submit)
  //
  nsAutoString name;
  rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, name);
  if (NS_FAILED(rv) || rv == NS_CONTENT_ATTR_NOT_THERE) {
    return rv;
  }

  //
  // Get the value
  //
  nsAutoString value;
  rv = GetValue(value);
  if (NS_FAILED(rv)) {
    return rv;
  }

  //
  // Submit
  //
  rv = aFormSubmission->AddNameValuePair(this, name, value);

  return rv;
}
