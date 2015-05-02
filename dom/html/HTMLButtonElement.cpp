/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLButtonElement.h"

#include "mozilla/dom/HTMLButtonElementBinding.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsFormSubmission.h"
#include "nsFormSubmissionConstants.h"
#include "nsIURL.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMEvent.h"
#include "nsIDocument.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "nsUnicharUtils.h"
#include "nsLayoutUtils.h"
#include "nsPresState.h"
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
  { 0 }
};

// Default type is 'submit'.
static const nsAttrValue::EnumTable* kButtonDefaultType = &kButtonTypeTable[2];


// Construction, destruction
HTMLButtonElement::HTMLButtonElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLFormElementWithState(aNodeInfo),
    mType(kButtonDefaultType->value),
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

NS_IMPL_ADDREF_INHERITED(HTMLButtonElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLButtonElement, Element)


// QueryInterface implementation for HTMLButtonElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLButtonElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLButtonElement,
                               nsIDOMHTMLButtonElement,
                               nsIConstraintValidation)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLFormElementWithState)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(HTMLButtonElement)

NS_IMETHODIMP
HTMLButtonElement::SetCustomValidity(const nsAString& aError) 
{
  nsIConstraintValidation::SetCustomValidity(aError);
  
  UpdateState(true);

  return NS_OK;
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
  UpdateBarredFromConstraintValidation();

  nsGenericHTMLFormElementWithState::FieldSetDisabledChanged(aNotify);
}

// nsIDOMHTMLButtonElement

NS_IMPL_ELEMENT_CLONE(HTMLButtonElement)


// nsIDOMHTMLButtonElement

NS_IMETHODIMP
HTMLButtonElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElementWithState::GetForm(aForm);
}

NS_IMPL_BOOL_ATTR(HTMLButtonElement, Autofocus, autofocus)
NS_IMPL_BOOL_ATTR(HTMLButtonElement, Disabled, disabled)
NS_IMPL_ACTION_ATTR(HTMLButtonElement, FormAction, formaction)
NS_IMPL_ENUM_ATTR_DEFAULT_MISSING_INVALID_VALUES(HTMLButtonElement, FormEnctype, formenctype,
                                                 "", kFormDefaultEnctype->tag)
NS_IMPL_ENUM_ATTR_DEFAULT_MISSING_INVALID_VALUES(HTMLButtonElement, FormMethod, formmethod,
                                                 "", kFormDefaultMethod->tag)
NS_IMPL_BOOL_ATTR(HTMLButtonElement, FormNoValidate, formnovalidate)
NS_IMPL_STRING_ATTR(HTMLButtonElement, FormTarget, formtarget)
NS_IMPL_STRING_ATTR(HTMLButtonElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLButtonElement, Value, value)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(HTMLButtonElement, Type, type,
                                kButtonDefaultType->tag)

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
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      // XXX ARG!! This is major evilness. ParseAttribute
      // shouldn't set members. Override SetAttr instead
      bool success = aResult.ParseEnumValue(aValue, kButtonTypeTable, false);
      if (success) {
        mType = aResult.GetEnumValue();
      } else {
        mType = kButtonDefaultType->value;
      }

      return success;
    }

    if (aAttribute == nsGkAtoms::formmethod) {
      return aResult.ParseEnumValue(aValue, kFormMethodTable, false);
    }
    if (aAttribute == nsGkAtoms::formenctype) {
      return aResult.ParseEnumValue(aValue, kFormEnctypeTable, false);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

bool
HTMLButtonElement::IsDisabledForEvents(uint32_t aMessage)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = do_QueryFrame(formControlFrame);
  return IsElementDisabledForEvents(aMessage, formFrame);
}

nsresult
HTMLButtonElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent->message)) {
    return NS_OK;
  }

  // Track whether we're in the outermost Dispatch invocation that will
  // cause activation of the input.  That is, if we're a click event, or a
  // DOMActivate that was dispatched directly, this will be set, but if we're
  // a DOMActivate dispatched from click handling, it will not be set.
  WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
  bool outerActivateEvent =
    ((mouseEvent && mouseEvent->IsLeftClickEvent()) ||
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
HTMLButtonElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  nsresult rv = NS_OK;
  if (!aVisitor.mPresContext) {
    return rv;
  }

  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) {
    WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
    if (mouseEvent && mouseEvent->IsLeftClickEvent()) {
      // XXX Activating actually occurs even if it's caused by untrusted event.
      //     Therefore, shouldn't this be always trusted event?
      InternalUIEvent actEvent(aVisitor.mEvent->mFlags.mIsTrusted,
                               NS_UI_ACTIVATE);
      actEvent.detail = 1;

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
    switch (aVisitor.mEvent->message) {
      case NS_KEY_PRESS:
      case NS_KEY_UP:
        {
          // For backwards compat, trigger buttons with space or enter
          // (bug 25300)
          WidgetKeyboardEvent* keyEvent = aVisitor.mEvent->AsKeyboardEvent();
          if ((keyEvent->keyCode == NS_VK_RETURN &&
               NS_KEY_PRESS == aVisitor.mEvent->message) ||
              (keyEvent->keyCode == NS_VK_SPACE &&
               NS_KEY_UP == aVisitor.mEvent->message)) {
            nsEventStatus status = nsEventStatus_eIgnore;

            WidgetMouseEvent event(aVisitor.mEvent->mFlags.mIsTrusted,
                                   NS_MOUSE_CLICK, nullptr,
                                   WidgetMouseEvent::eReal);
            event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
            EventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                      aVisitor.mPresContext, &event, nullptr,
                                      &status);
            aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
        break;// NS_KEY_PRESS

      case NS_MOUSE_BUTTON_DOWN:
        {
          WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
          if (mouseEvent->button == WidgetMouseEvent::eLeftButton) {
            if (mouseEvent->mFlags.mIsTrusted) {
              EventStateManager* esm =
                aVisitor.mPresContext->EventStateManager();
              EventStateManager::SetActiveManager(
                static_cast<EventStateManager*>(esm), this);
            }
            nsIFocusManager* fm = nsFocusManager::GetFocusManager();
            if (fm)
              fm->SetFocus(this, nsIFocusManager::FLAG_BYMOUSE |
                                 nsIFocusManager::FLAG_NOSCROLL);
            mouseEvent->mFlags.mMultipleActionsPrevented = true;
          } else if (mouseEvent->button == WidgetMouseEvent::eMiddleButton ||
                     mouseEvent->button == WidgetMouseEvent::eRightButton) {
            // cancel all of these events for buttons
            //XXXsmaug What to do with these events? Why these should be cancelled?
            if (aVisitor.mDOMEvent) {
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
          WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
          if (aVisitor.mDOMEvent &&
              (mouseEvent->button == WidgetMouseEvent::eMiddleButton ||
               mouseEvent->button == WidgetMouseEvent::eRightButton)) {
            aVisitor.mDOMEvent->StopPropagation();
          }
        }
        break;

      case NS_MOUSE_OVER:
        {
          aVisitor.mPresContext->EventStateManager()->
            SetContentState(this, NS_EVENT_STATE_HOVER);
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
        break;

        // XXX this doesn't seem to do anything yet
      case NS_MOUSE_OUT:
        {
          aVisitor.mPresContext->EventStateManager()->
            SetContentState(nullptr, NS_EVENT_STATE_HOVER);
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
        break;

      default:
        break;
    }
    if (aVisitor.mItemFlags & NS_OUTER_ACTIVATE_EVENT) {
      if (mForm && (mType == NS_FORM_BUTTON_SUBMIT ||
                    mType == NS_FORM_BUTTON_RESET)) {
        InternalFormEvent event(true,
          (mType == NS_FORM_BUTTON_RESET) ? NS_FORM_RESET : NS_FORM_SUBMIT);
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
          nsRefPtr<HTMLFormElement> form(mForm);
          presShell->HandleDOMEventWithTarget(mForm, &event, &status);
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
HTMLButtonElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
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
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
  if (name.IsEmpty()) {
    return NS_OK;
  }

  //
  // Get the value
  //
  nsAutoString value;
  nsresult rv = GetValue(value);
  if (NS_FAILED(rv)) {
    return rv;
  }

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
HTMLButtonElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
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
HTMLButtonElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::type) {
      if (!aValue) {
        mType = kButtonDefaultType->value;
      }
    }

    if (aName == nsGkAtoms::type || aName == nsGkAtoms::disabled) {
      UpdateBarredFromConstraintValidation();
      UpdateState(aNotify); 
    }
  }

  return nsGenericHTMLFormElementWithState::AfterSetAttr(aNameSpaceID, aName,
                                                         aValue, aNotify);
}

NS_IMETHODIMP
HTMLButtonElement::SaveState()
{
  if (!mDisabledChanged) {
    return NS_OK;
  }
  
  nsPresState* state = GetPrimaryPresState();
  if (state) {
    // We do not want to save the real disabled state but the disabled
    // attribute.
    state->SetDisabled(HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
  }

  return NS_OK;
}

bool
HTMLButtonElement::RestoreState(nsPresState* aState)
{
  if (aState && aState->IsDisabledSet()) {
    SetDisabled(aState->GetDisabled());
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
  return HTMLButtonElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
