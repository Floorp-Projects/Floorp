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
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
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
#include "nsLayoutUtils.h"
#include "nsEventDispatcher.h"
#include "nsPresState.h"
#include "nsLayoutErrors.h"

#define NS_IN_SUBMIT_CLICK (1 << 0)

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
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

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

  // overriden nsIFormControl methods
  NS_IMETHOD_(PRInt32) GetType() const { return mType; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  NS_IMETHOD SaveState();
  PRBool RestoreState(nsPresState* aState);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify);
  
  // nsIContent overrides...
  virtual void SetFocus(nsPresContext* aPresContext);
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull);
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual void DoneCreatingElement();

protected:
  PRInt8 mType;
  PRPackedBool mHandlingClick;
  PRPackedBool mDisabledChanged;

private:
  // The analogue of defaultValue in the DOM for input and textarea
  nsresult SetDefaultValue(const nsAString& aDefaultValue);
  nsresult GetDefaultValue(nsAString& aDefaultValue);
};


// Construction, destruction


NS_IMPL_NS_NEW_HTML_ELEMENT(Button)


nsHTMLButtonElement::nsHTMLButtonElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo),
    mType(NS_FORM_BUTTON_SUBMIT),  // default
    mHandlingClick(PR_FALSE),
    mDisabledChanged(PR_FALSE)
{
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


NS_IMPL_ELEMENT_CLONE(nsHTMLButtonElement)


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
  if (ShouldBlur(this)) {
    SetElementFocus(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::Focus()
{
  if (ShouldFocus(this)) {
    SetElementFocus(PR_TRUE);
  }

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
  nsCOMPtr<nsIDocument> doc = GetCurrentDoc();

  if (doc) {
    nsIPresShell *shell = doc->GetShellAt(0);
    if (shell) {
      nsCOMPtr<nsPresContext> context = shell->GetPresContext();
      if (context) {
        // Click() is never called from native code, but it may be
        // called from chrome JS. Mark this event trusted if Click()
        // is called from chrome code.
        nsMouseEvent event(nsContentUtils::IsCallerChrome(),
                           NS_MOUSE_CLICK, nsnull,
                           nsMouseEvent::eReal);
        nsEventStatus status = nsEventStatus_eIgnore;
        nsEventDispatcher::Dispatch(NS_STATIC_CAST(nsIContent*, this), context,
                                    &event, nsnull, &status);
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
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return;
  }

  SetFocusAndScrollIntoView(aPresContext);
}

static const nsAttrValue::EnumTable kButtonTypeTable[] = {
  { "button", NS_FORM_BUTTON_BUTTON },
  { "reset", NS_FORM_BUTTON_RESET },
  { "submit", NS_FORM_BUTTON_SUBMIT },
  { 0 }
};

PRBool
nsHTMLButtonElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::type && kNameSpaceID_None == aNamespaceID) {
    // XXX ARG!! This is major evilness. ParseAttribute
    // shouldn't set members. Override SetAttr instead
    PRBool res = aResult.ParseEnumValue(aValue, kButtonTypeTable);
    if (res) {
      mType = aResult.GetEnumValue();
    }
    return res;
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsresult
nsHTMLButtonElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  // Do not process any DOM events if the element is disabled
  aVisitor.mCanHandle = PR_FALSE;
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

  //FIXME Should this use NS_UI_ACTIVATE, not NS_MOUSE_CLICK?
  //      https://bugzilla.mozilla.org/show_bug.cgi?id=309348#c16
  PRBool bInSubmitClick = mType == NS_FORM_BUTTON_SUBMIT &&
                          NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent) &&
                          mForm;

  if (bInSubmitClick) {
    aVisitor.mItemFlags |= NS_IN_SUBMIT_CLICK;
    // tell the form that we are about to enter a click handler.
    // that means that if there are scripted submissions, the
    // latest one will be deferred until after the exit point of the handler.
    mForm->OnSubmitClickBegin();
  }

  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

nsresult
nsHTMLButtonElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  nsresult rv = NS_OK;
  if (!aVisitor.mPresContext) {
    return rv;
  }
  // mForm is null if the event handler removed us from the document (bug 194582).
  if ((aVisitor.mItemFlags & NS_IN_SUBMIT_CLICK) && mForm) {
    // tell the form that we are about to exit a click handler
    // so the form knows not to defer subsequent submissions
    // the pending ones that were created during the handler
    // will be flushed or forgoten.
    mForm->OnSubmitClickEnd();
  }

  if (nsEventStatus_eIgnore == aVisitor.mEventStatus) {
    switch (aVisitor.mEvent->message) {
      case NS_KEY_PRESS:
      case NS_KEY_UP:
        {
          // For backwards compat, trigger buttons with space or enter
          // (bug 25300)
          nsKeyEvent * keyEvent = (nsKeyEvent *)aVisitor.mEvent;
          if ((keyEvent->keyCode == NS_VK_RETURN &&
               NS_KEY_PRESS == aVisitor.mEvent->message) ||
              keyEvent->keyCode == NS_VK_SPACE &&
              NS_KEY_UP == aVisitor.mEvent->message) {
            nsEventStatus status = nsEventStatus_eIgnore;

            nsMouseEvent event(NS_IS_TRUSTED_EVENT(aVisitor.mEvent),
                               NS_MOUSE_CLICK, nsnull,
                               nsMouseEvent::eReal);
            nsEventDispatcher::Dispatch(NS_STATIC_CAST(nsIContent*, this),
                                        aVisitor.mPresContext, &event, nsnull,
                                        &status);
          }
        }
        break;// NS_KEY_PRESS

      case NS_MOUSE_CLICK:
        {
          if (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
            nsCOMPtr<nsIPresShell> presShell =
              aVisitor.mPresContext->GetPresShell();
            if (presShell) {
              // single-click
              nsUIEvent event(NS_IS_TRUSTED_EVENT(aVisitor.mEvent),
                              NS_UI_ACTIVATE, 1);
              nsEventStatus status = nsEventStatus_eIgnore;
  
              presShell->HandleDOMEventWithTarget(this, &event, &status);
              aVisitor.mEventStatus = status;
            }
          }
        }
        break;

      case NS_UI_ACTIVATE:
        {
          if (mForm && (mType == NS_FORM_BUTTON_SUBMIT ||
                        mType == NS_FORM_BUTTON_RESET)) {
            nsFormEvent event(PR_TRUE,
                              (mType == NS_FORM_BUTTON_RESET)
                              ? NS_FORM_RESET : NS_FORM_SUBMIT);
            event.originator      = this;
            nsEventStatus status  = nsEventStatus_eIgnore;

            nsCOMPtr<nsIPresShell> presShell =
              aVisitor.mPresContext->GetPresShell();
            // If |nsIPresShell::Destroy| has been called due to
            // handling the event, the pres context will return
            // a null pres shell.  See bug 125624.
            //
            // Using presShell to dispatch the event. It makes sure that
            // event is not handled if the window is being destroyed.
            if (presShell) {
              nsCOMPtr<nsIContent> form(do_QueryInterface(mForm));
              presShell->HandleDOMEventWithTarget(form, &event, &status);
            }
          }
        }
        break;// NS_MOUSE_CLICK

      case NS_MOUSE_BUTTON_DOWN:
        {
          if (aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT) {
            if (NS_STATIC_CAST(nsMouseEvent*, aVisitor.mEvent)->button ==
                  nsMouseEvent::eLeftButton) {
              aVisitor.mPresContext->EventStateManager()->
                SetContentState(this, NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS);
              aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
            } else if (NS_STATIC_CAST(nsMouseEvent*, aVisitor.mEvent)->button ==
                         nsMouseEvent::eMiddleButton ||
                       NS_STATIC_CAST(nsMouseEvent*, aVisitor.mEvent)->button ==
                         nsMouseEvent::eRightButton) {
              // cancel all of these events for buttons
              //XXXsmaug What to do with these events? Why these should be cancelled?
              aVisitor.mDOMEvent->StopPropagation();
            }
          }
        }
        break;

      // cancel all of these events for buttons
      //XXXsmaug What to do with these events? Why these should be cancelled?
      case NS_MOUSE_BUTTON_UP:
      case NS_MOUSE_DOUBLECLICK:
        {
          if (aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT &&
              aVisitor.mDOMEvent &&
              (NS_STATIC_CAST(nsMouseEvent*, aVisitor.mEvent)->button ==
                 nsMouseEvent::eMiddleButton ||
               NS_STATIC_CAST(nsMouseEvent*, aVisitor.mEvent)->button ==
                 nsMouseEvent::eRightButton)) {
            aVisitor.mDOMEvent->StopPropagation();
          }
        }
        break;

      case NS_MOUSE_ENTER_SYNTH:
        {
          aVisitor.mPresContext->EventStateManager()->
            SetContentState(this, NS_EVENT_STATE_HOVER);
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
        break;

        // XXX this doesn't seem to do anything yet
      case NS_MOUSE_EXIT_SYNTH:
        {
          aVisitor.mPresContext->EventStateManager()->
            SetContentState(nsnull, NS_EVENT_STATE_HOVER);
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
        break;

      default:
        break;
    }
  } else {
    switch (aVisitor.mEvent->message) {
      // Make sure any pending submissions from a call to
      // form.submit() in a left click handler or an activate
      // handler gets flushed, even if the event handler prevented
      // the default action.
      case NS_MOUSE_CLICK:
        if (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
          if (mForm && mType == NS_FORM_BUTTON_SUBMIT) {
            // Tell the form to flush a possible pending submission.
            // the reason is that the script returned false (the event was
            // not ignored) so if there is a stored submission, it needs to
            // be submitted immediatelly.
            mForm->FlushPendingSubmission();
          }
        }
        break;
      case NS_UI_ACTIVATE:
        if (mForm && mType == NS_FORM_BUTTON_SUBMIT) {
          // Tell the form to flush a possible pending submission.
          // the reason is that the script returned false (the event was
          // not ignored) so if there is a stored submission, it needs to
          // be submitted immediatelly.
          mForm->FlushPendingSubmission();
        }
        break;// NS_UI_ACTIVATE
    } //switch
  } //if

  return rv;
}

nsresult
nsHTMLButtonElement::GetDefaultValue(nsAString& aDefaultValue)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::value, aDefaultValue);
  return NS_OK;
}

nsresult
nsHTMLButtonElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::value, aDefaultValue, PR_TRUE);
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
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::name, name)) {
    return NS_OK;
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

void
nsHTMLButtonElement::DoneCreatingElement()
{
  // Restore state as needed.
  RestoreFormControlState(this, this);
}

nsresult
nsHTMLButtonElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                   const nsAString* aValue, PRBool aNotify)
{
  if (aNotify && aName == nsGkAtoms::disabled &&
      aNameSpaceID == kNameSpaceID_None) {
    mDisabledChanged = PR_TRUE;
  }

  return nsGenericHTMLFormElement::BeforeSetAttr(aNameSpaceID, aName,
                                                 aValue, aNotify);
}

NS_IMETHODIMP
nsHTMLButtonElement::SaveState()
{
  if (!mDisabledChanged) {
    return NS_OK;
  }
  
  nsPresState *state = nsnull;
  nsresult rv = GetPrimaryPresState(this, &state);
  if (state) {
    PRBool disabled;
    GetDisabled(&disabled);
    if (disabled) {
      rv |= state->SetStateProperty(NS_LITERAL_STRING("disabled"),
                                    NS_LITERAL_STRING("t"));
    } else {
      rv |= state->SetStateProperty(NS_LITERAL_STRING("disabled"),
                                    NS_LITERAL_STRING("f"));
    }
    NS_ASSERTION(NS_SUCCEEDED(rv), "disabled save failed!");
  }

  return rv;
}

PRBool
nsHTMLButtonElement::RestoreState(nsPresState* aState)
{
  nsAutoString disabled;
  nsresult rv =
    aState->GetStateProperty(NS_LITERAL_STRING("disabled"), disabled);
  NS_ASSERTION(NS_SUCCEEDED(rv), "disabled restore failed!");
  if (rv == NS_STATE_PROPERTY_EXISTS) {
    SetDisabled(disabled.EqualsLiteral("t"));
  }

  return PR_FALSE;
}
