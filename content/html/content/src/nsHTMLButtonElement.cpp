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
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsFormSubmission.h"
#include "nsFormSubmissionConstants.h"
#include "nsIURL.h"
#include "nsEventStateManager.h"
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
#include "nsFocusManager.h"
#include "nsHTMLFormElement.h"
#include "nsIConstraintValidation.h"
#include "mozAutoDocUpdate.h"

#define NS_IN_SUBMIT_CLICK      (1 << 0)
#define NS_OUTER_ACTIVATE_EVENT (1 << 1)

static const nsAttrValue::EnumTable kButtonTypeTable[] = {
  { "button", NS_FORM_BUTTON_BUTTON },
  { "reset", NS_FORM_BUTTON_RESET },
  { "submit", NS_FORM_BUTTON_SUBMIT },
  { 0 }
};

// Default type is 'submit'.
static const nsAttrValue::EnumTable* kButtonDefaultType = &kButtonTypeTable[2];

class nsHTMLButtonElement : public nsGenericHTMLFormElement,
                            public nsIDOMHTMLButtonElement,
                            public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLButtonElement(already_AddRefed<nsINodeInfo> aNodeInfo);
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

  // overriden nsIFormControl methods
  NS_IMETHOD_(PRUint32) GetType() const { return mType; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  NS_IMETHOD SaveState();
  PRBool RestoreState(nsPresState* aState);

  virtual void FieldSetDisabledChanged(nsEventStates aStates, PRBool aNotify);

  nsEventStates IntrinsicState() const;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               PRBool aCompileEventHandlers);
  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify);
  /**
   * Called when an attribute has just been changed
   */
  nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                        const nsAString* aValue, PRBool aNotify);
  
  // nsIContent overrides...
  virtual PRBool IsHTMLFocusable(PRBool aWithMouse, PRBool *aIsFocusable, PRInt32 *aTabIndex);
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual void DoneCreatingElement();
  virtual nsXPCClassInfo* GetClassInfo();

  // nsIConstraintValidation
  void UpdateBarredFromConstraintValidation();

protected:
  virtual PRBool AcceptAutofocus() const
  {
    return PR_TRUE;
  }

  PRUint8 mType;
  PRPackedBool mHandlingClick;
  PRPackedBool mDisabledChanged;
  PRPackedBool mInInternalActivate;

private:
  // The analogue of defaultValue in the DOM for input and textarea
  nsresult SetDefaultValue(const nsAString& aDefaultValue);
  nsresult GetDefaultValue(nsAString& aDefaultValue);
};


// Construction, destruction


NS_IMPL_NS_NEW_HTML_ELEMENT(Button)


nsHTMLButtonElement::nsHTMLButtonElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo),
    mType(kButtonDefaultType->value),
    mHandlingClick(PR_FALSE),
    mDisabledChanged(PR_FALSE),
    mInInternalActivate(PR_FALSE)
{
}

nsHTMLButtonElement::~nsHTMLButtonElement()
{
}

// nsISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLButtonElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLButtonElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLButtonElement, nsHTMLButtonElement)

// QueryInterface implementation for nsHTMLButtonElement
NS_INTERFACE_TABLE_HEAD(nsHTMLButtonElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLButtonElement,
                                   nsIDOMHTMLButtonElement,
                                   nsIConstraintValidation)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLButtonElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLButtonElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(nsHTMLButtonElement)

// nsIDOMHTMLButtonElement


NS_IMPL_ELEMENT_CLONE(nsHTMLButtonElement)


// nsIDOMHTMLButtonElement

NS_IMETHODIMP
nsHTMLButtonElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

NS_IMPL_STRING_ATTR(nsHTMLButtonElement, AccessKey, accesskey)
NS_IMPL_BOOL_ATTR(nsHTMLButtonElement, Autofocus, autofocus)
NS_IMPL_BOOL_ATTR(nsHTMLButtonElement, Disabled, disabled)
NS_IMPL_URI_ATTR(nsHTMLButtonElement, FormAction, formaction)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLButtonElement, FormEnctype, formenctype,
                                kFormDefaultEnctype->tag)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLButtonElement, FormMethod, formmethod,
                                kFormDefaultMethod->tag)
NS_IMPL_BOOL_ATTR(nsHTMLButtonElement, FormNoValidate, formnovalidate)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, FormTarget, formtarget)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Name, name)
NS_IMPL_INT_ATTR(nsHTMLButtonElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Value, value)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLButtonElement, Type, type,
                                kButtonDefaultType->tag)

NS_IMETHODIMP
nsHTMLButtonElement::Blur()
{
  return nsGenericHTMLElement::Blur();
}

NS_IMETHODIMP
nsHTMLButtonElement::Focus()
{
  return nsGenericHTMLElement::Focus();
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
    nsIPresShell *shell = doc->GetShell();
    if (shell) {
      nsRefPtr<nsPresContext> context = shell->GetPresContext();
      if (context) {
        // Click() is never called from native code, but it may be
        // called from chrome JS. Mark this event trusted if Click()
        // is called from chrome code.
        nsMouseEvent event(nsContentUtils::IsCallerChrome(),
                           NS_MOUSE_CLICK, nsnull,
                           nsMouseEvent::eReal);
        event.inputSource = nsIDOMNSMouseEvent::MOZ_SOURCE_UNKNOWN;
        nsEventStatus status = nsEventStatus_eIgnore;
        nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this), context,
                                    &event, nsnull, &status);
      }
    }
  }

  mHandlingClick = PR_FALSE;

  return NS_OK;
}

PRBool
nsHTMLButtonElement::IsHTMLFocusable(PRBool aWithMouse, PRBool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return PR_TRUE;
  }

  *aIsFocusable = 
#ifdef XP_MACOSX
    (!aWithMouse || nsFocusManager::sMouseFocusesFormControl) &&
#endif
    !IsDisabled();

  return PR_FALSE;
}

PRBool
nsHTMLButtonElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      // XXX ARG!! This is major evilness. ParseAttribute
      // shouldn't set members. Override SetAttr instead
      PRBool success = aResult.ParseEnumValue(aValue, kButtonTypeTable, PR_FALSE);
      if (success) {
        mType = aResult.GetEnumValue();
      } else {
        mType = kButtonDefaultType->value;
      }

      return success;
    }

    if (aAttribute == nsGkAtoms::formmethod) {
      return aResult.ParseEnumValue(aValue, kFormMethodTable, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::formenctype) {
      return aResult.ParseEnumValue(aValue, kFormEnctypeTable, PR_FALSE);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsresult
nsHTMLButtonElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  // Do not process any DOM events if the element is disabled
  aVisitor.mCanHandle = PR_FALSE;
  if (IsDisabled()) {
    return NS_OK;
  }

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);

  if (formControlFrame) {
    nsIFrame* formFrame = do_QueryFrame(formControlFrame);
    if (formFrame) {
      const nsStyleUserInterface* uiStyle = formFrame->GetStyleUserInterface();

      if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
          uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
        return NS_OK;
    }
  }

  // Track whether we're in the outermost Dispatch invocation that will
  // cause activation of the input.  That is, if we're a click event, or a
  // DOMActivate that was dispatched directly, this will be set, but if we're
  // a DOMActivate dispatched from click handling, it will not be set.
  PRBool outerActivateEvent =
    (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent) ||
     (aVisitor.mEvent->message == NS_UI_ACTIVATE &&
      !mInInternalActivate));

  if (outerActivateEvent) {
    aVisitor.mItemFlags |= NS_OUTER_ACTIVATE_EVENT;
    if (mType == NS_FORM_BUTTON_SUBMIT && mForm) {
      aVisitor.mItemFlags |= NS_IN_SUBMIT_CLICK;
      // tell the form that we are about to enter a click handler.
      // that means that if there are scripted submissions, the
      // latest one will be deferred until after the exit point of the handler.
      mForm->OnSubmitClickBegin(this);
    }
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

  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault &&
      NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
    nsUIEvent actEvent(NS_IS_TRUSTED_EVENT(aVisitor.mEvent), NS_UI_ACTIVATE, 1);

    nsCOMPtr<nsIPresShell> shell = aVisitor.mPresContext->GetPresShell();
    if (shell) {
      nsEventStatus status = nsEventStatus_eIgnore;
      mInInternalActivate = PR_TRUE;
      shell->HandleDOMEventWithTarget(this, &actEvent, &status);
      mInInternalActivate = PR_FALSE;

      // If activate is cancelled, we must do the same as when click is
      // cancelled (revert the checkbox to its original value).
      if (status == nsEventStatus_eConsumeNoDefault)
        aVisitor.mEventStatus = status;
    }
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
            event.inputSource = nsIDOMNSMouseEvent::MOZ_SOURCE_KEYBOARD;
            nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                        aVisitor.mPresContext, &event, nsnull,
                                        &status);
          }
        }
        break;// NS_KEY_PRESS

      case NS_MOUSE_BUTTON_DOWN:
        {
          if (aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT) {
            if (static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
                  nsMouseEvent::eLeftButton) {
              if (NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
                nsIEventStateManager* esm =
                  aVisitor.mPresContext->EventStateManager();
                nsEventStateManager::SetActiveManager(
                  static_cast<nsEventStateManager*>(esm), this);
              }
              nsIFocusManager* fm = nsFocusManager::GetFocusManager();
              if (fm)
                fm->SetFocus(this, nsIFocusManager::FLAG_BYMOUSE |
                                   nsIFocusManager::FLAG_NOSCROLL);
              aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
            } else if (static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
                         nsMouseEvent::eMiddleButton ||
                       static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
                         nsMouseEvent::eRightButton) {
              // cancel all of these events for buttons
              //XXXsmaug What to do with these events? Why these should be cancelled?
              if (aVisitor.mDOMEvent) {
                aVisitor.mDOMEvent->StopPropagation();
              }
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
              (static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
                 nsMouseEvent::eMiddleButton ||
               static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
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
    if (aVisitor.mItemFlags & NS_OUTER_ACTIVATE_EVENT) {
      if (mForm && (mType == NS_FORM_BUTTON_SUBMIT ||
                    mType == NS_FORM_BUTTON_RESET)) {
        nsFormEvent event(PR_TRUE,
                          (mType == NS_FORM_BUTTON_RESET)
                          ? NS_FORM_RESET : NS_FORM_SUBMIT);
        event.originator     = this;
        nsEventStatus status = nsEventStatus_eIgnore;

        nsCOMPtr<nsIPresShell> presShell =
          aVisitor.mPresContext->GetPresShell();
        // If |nsIPresShell::Destroy| has been called due to
        // handling the event, the pres context will return
        // a null pres shell.  See bug 125624.
        //
        // Using presShell to dispatch the event. It makes sure that
        // event is not handled if the window is being destroyed.
        if (presShell && (event.message != NS_FORM_SUBMIT ||
                          mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate) ||
                          // We know the element is a submit control, if this check is moved,
                          // make sure formnovalidate is used only if it's a submit control.
                          HasAttr(kNameSpaceID_None, nsGkAtoms::formnovalidate) ||
                          mForm->CheckValidFormSubmission())) {
          // TODO: removing this code and have the submit event sent by the form
          // see bug 592124.
          // Hold a strong ref while dispatching
          nsRefPtr<nsHTMLFormElement> form(mForm);
          presShell->HandleDOMEventWithTarget(mForm, &event, &status);
        }
      }
    }
  } else if ((aVisitor.mItemFlags & NS_IN_SUBMIT_CLICK) && mForm) {
    // Tell the form to flush a possible pending submission.
    // the reason is that the script returned false (the event was
    // not ignored) so if there is a stored submission, it needs to
    // be submitted immediatelly.
    // Note, NS_IN_SUBMIT_CLICK is set only when we're in outer activate event.
    mForm->FlushPendingSubmission();
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
nsHTMLButtonElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  nsresult rv = NS_OK;

  //
  // We only submit if we were the button pressed
  //
  if (aFormSubmission->GetOriginatingElement() != this) {
    return NS_OK;
  }

  // Disabled elements don't submit
  if (IsDisabled()) {
    return NS_OK;
  }

  //
  // Get the name (if no name, no submit)
  //
  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
  if (name.IsEmpty()) {
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
  rv = aFormSubmission->AddNameValuePair(name, value);

  return rv;
}

void
nsHTMLButtonElement::DoneCreatingElement()
{
  // Restore state as needed.
  RestoreFormControlState(this, this);
}

nsresult
nsHTMLButtonElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there is a disabled fieldset in the parent chain, the element is now
  // barred from constraint validation.
  UpdateBarredFromConstraintValidation();

  return rv;
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

nsresult
nsHTMLButtonElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                  const nsAString* aValue, PRBool aNotify)
{
  nsEventStates states;

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::type) {
      if (!aValue) {
        mType = kButtonDefaultType->value;
      }

      UpdateBarredFromConstraintValidation();
      states |= NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID |
                NS_EVENT_STATE_MOZ_SUBMITINVALID;
    } else if (aName == nsGkAtoms::disabled) {
      UpdateBarredFromConstraintValidation();
      states |= NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID;
    }

    if (aNotify && !states.IsEmpty()) {
      nsIDocument* doc = GetCurrentDoc();
      if (doc) {
        MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
        doc->ContentStatesChanged(this, nsnull, states);
      }
    }
  }

  return nsGenericHTMLFormElement::AfterSetAttr(aNameSpaceID, aName,
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
    // We do not want to save the real disabled state but the disabled
    // attribute.
    state->SetDisabled(HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
  }

  return rv;
}

PRBool
nsHTMLButtonElement::RestoreState(nsPresState* aState)
{
  if (aState && aState->IsDisabledSet()) {
    SetDisabled(aState->GetDisabled());
  }

  return PR_FALSE;
}

nsEventStates
nsHTMLButtonElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLFormElement::IntrinsicState();

  if (IsCandidateForConstraintValidation()) {
    state |= IsValid() ? NS_EVENT_STATE_VALID : NS_EVENT_STATE_INVALID;
  }

  if (mForm && !mForm->GetValidity() && IsSubmitControl()) {
    state |= NS_EVENT_STATE_MOZ_SUBMITINVALID;
  }

  return state;
}

// nsIConstraintValidation

NS_IMETHODIMP
nsHTMLButtonElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
    doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_INVALID |
                                            NS_EVENT_STATE_VALID);
  }

  return NS_OK;
}

void
nsHTMLButtonElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(mType == NS_FORM_BUTTON_BUTTON ||
                                    mType == NS_FORM_BUTTON_RESET ||
                                    IsDisabled());
}

void
nsHTMLButtonElement::FieldSetDisabledChanged(nsEventStates aStates, PRBool aNotify)
{
  UpdateBarredFromConstraintValidation();

  aStates |= NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID;
  nsGenericHTMLFormElement::FieldSetDisabledChanged(aStates, aNotify);
}

