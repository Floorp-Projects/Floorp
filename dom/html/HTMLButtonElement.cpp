/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLButtonElement.h"

#include "HTMLFormSubmissionConstants.h"
#include "mozilla/dom/HTMLButtonElementBinding.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFormControl.h"
#include "nsIURL.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDocument.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "nsUnicharUtils.h"
#include "nsLayoutUtils.h"
#include "mozilla/PresState.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozAutoDocUpdate.h"

#define NS_IN_SUBMIT_CLICK      (1 << 0)
#define NS_OUTER_ACTIVATE_EVENT (1 << 1)

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Button)

namespace mozilla {
namespace dom {

static const nsAttrValue::EnumTable kButtonTypeTable[] = {
  { "button", NS_FORM_BUTTON_BUTTON },
  { "reset", NS_FORM_BUTTON_RESET },
  { "submit", NS_FORM_BUTTON_SUBMIT },
  { nullptr, 0 }
};

// Default type is 'submit'.
static const nsAttrValue::EnumTable* kButtonDefaultType = &kButtonTypeTable[2];


// Construction, destruction
HTMLButtonElement::HTMLButtonElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLFormElementWithState(aNodeInfo, kButtonDefaultType->value),
    mDisabledChanged(false),
    mInInternalActivate(false),
    mInhibitStateRestoration(!!(aFromParser & FROM_PARSER_FRAGMENT))
{
  // Set up our default state: enabled
  AddStatesSilently(NS_EVENT_STATE_ENABLED);
}

HTMLButtonElement::~HTMLButtonElement()
{
}

// nsISupports

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLButtonElement,
                                   nsGenericHTMLFormElementWithState,
                                   mValidity)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLButtonElement,
                                             nsGenericHTMLFormElementWithState,
                                             nsIConstraintValidation)

void
HTMLButtonElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);
}

void
HTMLButtonElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(mType == NS_FORM_BUTTON_BUTTON ||
                                    mType == NS_FORM_BUTTON_RESET ||
                                    IsDisabled());
}

void
HTMLButtonElement::FieldSetDisabledChanged(bool aNotify)
{

  // FieldSetDisabledChanged *has* to be called *before*
  // UpdateBarredFromConstraintValidation, because the latter depends on our
  // disabled state.
  nsGenericHTMLFormElementWithState::FieldSetDisabledChanged(aNotify);

  UpdateBarredFromConstraintValidation();
  UpdateState(aNotify);
}

NS_IMPL_ELEMENT_CLONE(HTMLButtonElement)

void
HTMLButtonElement::GetFormEnctype(nsAString& aFormEncType)
{
  GetEnumAttr(nsGkAtoms::formenctype, "", kFormDefaultEnctype->tag, aFormEncType);
}

void
HTMLButtonElement::GetFormMethod(nsAString& aFormMethod)
{
  GetEnumAttr(nsGkAtoms::formmethod, "", kFormDefaultMethod->tag, aFormMethod);
}

void
HTMLButtonElement::GetType(nsAString& aType)
{
  GetEnumAttr(nsGkAtoms::type, kButtonDefaultType->tag, aType);
}

int32_t
HTMLButtonElement::TabIndexDefault()
{
  return 0;
}

bool
HTMLButtonElement::IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex)
{
  if (nsGenericHTMLFormElementWithState::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  *aIsFocusable =
#ifdef XP_MACOSX
    (!aWithMouse || nsFocusManager::sMouseFocusesFormControl) &&
#endif
    !IsDisabled();

  return false;
}

bool
HTMLButtonElement::ParseAttribute(int32_t aNamespaceID,
                                  nsAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsIPrincipal* aMaybeScriptedPrincipal,
                                  nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, kButtonTypeTable, false,
                                    kButtonDefaultType);
    }

    if (aAttribute == nsGkAtoms::formmethod) {
      return aResult.ParseEnumValue(aValue, kFormMethodTable, false);
    }
    if (aAttribute == nsGkAtoms::formenctype) {
      return aResult.ParseEnumValue(aValue, kFormEnctypeTable, false);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

bool
HTMLButtonElement::IsDisabledForEvents(EventMessage aMessage)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = do_QueryFrame(formControlFrame);
  return IsElementDisabledForEvents(aMessage, formFrame);
}

void
HTMLButtonElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent->mMessage)) {
    return;
  }

  // Track whether we're in the outermost Dispatch invocation that will
  // cause activation of the input.  That is, if we're a click event, or a
  // DOMActivate that was dispatched directly, this will be set, but if we're
  // a DOMActivate dispatched from click handling, it will not be set.
  WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
  bool outerActivateEvent =
    ((mouseEvent && mouseEvent->IsLeftClickEvent()) ||
     (aVisitor.mEvent->mMessage == eLegacyDOMActivate &&
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

  nsGenericHTMLElement::GetEventTargetParent(aVisitor);
}

nsresult
HTMLButtonElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  nsresult rv = NS_OK;
  if (!aVisitor.mPresContext) {
    return rv;
  }

  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) {
    WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
    if (mouseEvent && mouseEvent->IsLeftClickEvent()) {
      // DOMActive event should be trusted since the activation is actually
      // occurred even if the cause is an untrusted click event.
      InternalUIEvent actEvent(true, eLegacyDOMActivate, mouseEvent);
      actEvent.mDetail = 1;

      nsCOMPtr<nsIPresShell> shell = aVisitor.mPresContext->GetPresShell();
      if (shell) {
        nsEventStatus status = nsEventStatus_eIgnore;
        mInInternalActivate = true;
        shell->HandleDOMEventWithTarget(this, &actEvent, &status);
        mInInternalActivate = false;

        // If activate is cancelled, we must do the same as when click is
        // cancelled (revert the checkbox to its original value).
        if (status == nsEventStatus_eConsumeNoDefault) {
          aVisitor.mEventStatus = status;
        }
      }
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
    switch (aVisitor.mEvent->mMessage) {
      case eKeyPress:
      case eKeyUp:
        {
          // For backwards compat, trigger buttons with space or enter
          // (bug 25300)
          WidgetKeyboardEvent* keyEvent = aVisitor.mEvent->AsKeyboardEvent();
          if ((keyEvent->mKeyCode == NS_VK_RETURN &&
               eKeyPress == aVisitor.mEvent->mMessage) ||
              (keyEvent->mKeyCode == NS_VK_SPACE &&
               eKeyUp == aVisitor.mEvent->mMessage)) {
            DispatchSimulatedClick(this, aVisitor.mEvent->IsTrusted(),
                                   aVisitor.mPresContext);
            aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
        break;

      default:
        break;
    }
    if (aVisitor.mItemFlags & NS_OUTER_ACTIVATE_EVENT) {
      if (mForm && (mType == NS_FORM_BUTTON_SUBMIT ||
                    mType == NS_FORM_BUTTON_RESET)) {
        InternalFormEvent event(true,
          (mType == NS_FORM_BUTTON_RESET) ? eFormReset : eFormSubmit);
        event.mOriginator = this;
        nsEventStatus status = nsEventStatus_eIgnore;

        nsCOMPtr<nsIPresShell> presShell =
          aVisitor.mPresContext->GetPresShell();
        // If |nsIPresShell::Destroy| has been called due to
        // handling the event, the pres context will return
        // a null pres shell.  See bug 125624.
        //
        // Using presShell to dispatch the event. It makes sure that
        // event is not handled if the window is being destroyed.
        if (presShell && (event.mMessage != eFormSubmit ||
                          mForm->SubmissionCanProceed(this))) {
          // TODO: removing this code and have the submit event sent by the form
          // see bug 592124.
          // Hold a strong ref while dispatching
          RefPtr<HTMLFormElement> form(mForm);
          presShell->HandleDOMEventWithTarget(form, &event, &status);
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
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
HTMLButtonElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv =
    nsGenericHTMLFormElementWithState::BindToTree(aDocument, aParent, aBindingParent,
                                                  aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update our state; we may now be the default submit element
  UpdateState(false);

  return NS_OK;
}

void
HTMLButtonElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsGenericHTMLFormElementWithState::UnbindFromTree(aDeep, aNullParent);

  // Update our state; we may no longer be the default submit element
  UpdateState(false);
}

NS_IMETHODIMP
HTMLButtonElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLButtonElement::SubmitNamesValues(HTMLFormSubmission* aFormSubmission)
{
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
  GetHTMLAttr(nsGkAtoms::name, name);
  if (name.IsEmpty()) {
    return NS_OK;
  }

  //
  // Get the value
  //
  nsAutoString value;
  GetHTMLAttr(nsGkAtoms::value, value);

  //
  // Submit
  //
  return aFormSubmission->AddNameValuePair(name, value);
}

void
HTMLButtonElement::DoneCreatingElement()
{
  if (!mInhibitStateRestoration) {
    nsresult rv = GenerateStateKey();
    if (NS_SUCCEEDED(rv)) {
      RestoreFormControlState();
    }
  }
}

nsresult
HTMLButtonElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify)
{
  if (aNotify && aName == nsGkAtoms::disabled &&
      aNameSpaceID == kNameSpaceID_None) {
    mDisabledChanged = true;
  }

  return nsGenericHTMLFormElementWithState::BeforeSetAttr(aNameSpaceID, aName,
                                                          aValue, aNotify);
}

nsresult
HTMLButtonElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::type) {
      if (aValue) {
        mType = aValue->GetEnumValue();
      } else {
        mType = kButtonDefaultType->value;
      }
    }

    if (aName == nsGkAtoms::type || aName == nsGkAtoms::disabled) {
      if (aName == nsGkAtoms::disabled) {
        // This *has* to be called *before* validity state check because
        // UpdateBarredFromConstraintValidation depends on our disabled state.
        UpdateDisabledState(aNotify);
      }

      UpdateBarredFromConstraintValidation();
    }
  }

  return nsGenericHTMLFormElementWithState::AfterSetAttr(aNameSpaceID, aName,
                                                         aValue, aOldValue,
                                                         aSubjectPrincipal, aNotify);
}

NS_IMETHODIMP
HTMLButtonElement::SaveState()
{
  if (!mDisabledChanged) {
    return NS_OK;
  }

  PresState* state = GetPrimaryPresState();
  if (state) {
    // We do not want to save the real disabled state but the disabled
    // attribute.
    state->disabled() = HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
    state->disabledSet() = true;
  }

  return NS_OK;
}

bool
HTMLButtonElement::RestoreState(PresState* aState)
{
  if (aState && aState->disabledSet() && !aState->disabled()) {
    SetDisabled(false, IgnoreErrors());
  }

  return false;
}

EventStates
HTMLButtonElement::IntrinsicState() const
{
  EventStates state = nsGenericHTMLFormElementWithState::IntrinsicState();

  if (IsCandidateForConstraintValidation()) {
    if (IsValid()) {
      state |= NS_EVENT_STATE_VALID;
      if (!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) {
        state |= NS_EVENT_STATE_MOZ_UI_VALID;
      }
    } else {
      state |= NS_EVENT_STATE_INVALID;
      if (!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) {
        state |= NS_EVENT_STATE_MOZ_UI_INVALID;
      }
    }
  }

  if (mForm && !mForm->GetValidity() && IsSubmitControl()) {
    state |= NS_EVENT_STATE_MOZ_SUBMITINVALID;
  }

  return state;
}

JSObject*
HTMLButtonElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLButtonElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
