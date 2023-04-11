/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLButtonElement.h"

#include "HTMLFormSubmissionConstants.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/HTMLButtonElementBinding.h"
#include "nsAttrValueInlines.h"
#include "nsIContentInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFormControl.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "mozilla/dom/Document.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEvents.h"
#include "nsUnicharUtils.h"
#include "nsLayoutUtils.h"
#include "mozilla/PresState.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozAutoDocUpdate.h"

#define NS_IN_SUBMIT_CLICK (1 << 0)
#define NS_OUTER_ACTIVATE_EVENT (1 << 1)

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Button)

namespace mozilla::dom {

static const nsAttrValue::EnumTable kButtonTypeTable[] = {
    {"button", FormControlType::ButtonButton},
    {"reset", FormControlType::ButtonReset},
    {"submit", FormControlType::ButtonSubmit},
    {nullptr, 0}};

// Default type is 'submit'.
static const nsAttrValue::EnumTable* kButtonDefaultType = &kButtonTypeTable[2];

// Construction, destruction
HTMLButtonElement::HTMLButtonElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser)
    : nsGenericHTMLFormControlElementWithState(
          std::move(aNodeInfo), aFromParser,
          FormControlType(kButtonDefaultType->value)),
      mDisabledChanged(false),
      mInInternalActivate(false),
      mInhibitStateRestoration(aFromParser & FROM_PARSER_FRAGMENT) {
  // Set up our default state: enabled
  AddStatesSilently(ElementState::ENABLED);
}

HTMLButtonElement::~HTMLButtonElement() = default;

// nsISupports

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLButtonElement,
                                   nsGenericHTMLFormControlElementWithState,
                                   mValidity)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(
    HTMLButtonElement, nsGenericHTMLFormControlElementWithState,
    nsIConstraintValidation)

void HTMLButtonElement::SetCustomValidity(const nsAString& aError) {
  ConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);
}

void HTMLButtonElement::UpdateBarredFromConstraintValidation() {
  SetBarredFromConstraintValidation(
      mType == FormControlType::ButtonButton ||
      mType == FormControlType::ButtonReset ||
      HasFlag(ELEMENT_IS_DATALIST_OR_HAS_DATALIST_ANCESTOR) || IsDisabled());
}

void HTMLButtonElement::FieldSetDisabledChanged(bool aNotify) {
  // FieldSetDisabledChanged *has* to be called *before*
  // UpdateBarredFromConstraintValidation, because the latter depends on our
  // disabled state.
  nsGenericHTMLFormControlElementWithState::FieldSetDisabledChanged(aNotify);

  UpdateBarredFromConstraintValidation();
  UpdateState(aNotify);
}

NS_IMPL_ELEMENT_CLONE(HTMLButtonElement)

void HTMLButtonElement::GetFormEnctype(nsAString& aFormEncType) {
  GetEnumAttr(nsGkAtoms::formenctype, "", kFormDefaultEnctype->tag,
              aFormEncType);
}

void HTMLButtonElement::GetFormMethod(nsAString& aFormMethod) {
  GetEnumAttr(nsGkAtoms::formmethod, "", kFormDefaultMethod->tag, aFormMethod);
}

void HTMLButtonElement::GetType(nsAString& aType) {
  GetEnumAttr(nsGkAtoms::type, kButtonDefaultType->tag, aType);
}

int32_t HTMLButtonElement::TabIndexDefault() { return 0; }

bool HTMLButtonElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                        int32_t* aTabIndex) {
  if (nsGenericHTMLFormControlElementWithState::IsHTMLFocusable(
          aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  *aIsFocusable = IsFormControlDefaultFocusable(aWithMouse) && !IsDisabled();

  return false;
}

bool HTMLButtonElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, kButtonTypeTable, false,
                                    kButtonDefaultType);
    }

    if (aAttribute == nsGkAtoms::formmethod) {
      if (StaticPrefs::dom_dialog_element_enabled() || IsInChromeDocument()) {
        return aResult.ParseEnumValue(aValue, kFormMethodTableDialogEnabled,
                                      false);
      }
      return aResult.ParseEnumValue(aValue, kFormMethodTable, false);
    }
    if (aAttribute == nsGkAtoms::formenctype) {
      return aResult.ParseEnumValue(aValue, kFormEnctypeTable, false);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

bool HTMLButtonElement::IsDisabledForEvents(WidgetEvent* aEvent) {
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = do_QueryFrame(formControlFrame);
  return IsElementDisabledForEvents(aEvent, formFrame);
}

void HTMLButtonElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  aVisitor.mCanHandle = false;

  if (IsDisabledForEvents(aVisitor.mEvent)) {
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
        !mInInternalActivate && aVisitor.mEvent->mOriginalTarget == this));

  if (outerActivateEvent) {
    aVisitor.mItemFlags |= NS_OUTER_ACTIVATE_EVENT;
    if (mType == FormControlType::ButtonSubmit && mForm &&
        !aVisitor.mEvent->mFlags.mMultiplePreActionsPrevented) {
      aVisitor.mEvent->mFlags.mMultiplePreActionsPrevented = true;
      aVisitor.mItemFlags |= NS_IN_SUBMIT_CLICK;
      aVisitor.mItemData = static_cast<Element*>(mForm);
      // tell the form that we are about to enter a click handler.
      // that means that if there are scripted submissions, the
      // latest one will be deferred until after the exit point of the handler.
      mForm->OnSubmitClickBegin(this);
    }
  }

  nsGenericHTMLElement::GetEventTargetParent(aVisitor);
}

nsresult HTMLButtonElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
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

      if (RefPtr<PresShell> presShell = aVisitor.mPresContext->GetPresShell()) {
        nsEventStatus status = nsEventStatus_eIgnore;
        mInInternalActivate = true;
        presShell->HandleDOMEventWithTarget(this, &actEvent, &status);
        mInInternalActivate = false;

        // If activate is cancelled, we must do the same as when click is
        // cancelled (revert the checkbox to its original value).
        if (status == nsEventStatus_eConsumeNoDefault) {
          aVisitor.mEventStatus = status;
        }
      }
    }
  }

  if ((aVisitor.mItemFlags & NS_IN_SUBMIT_CLICK)) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aVisitor.mItemData));
    RefPtr<HTMLFormElement> form = HTMLFormElement::FromNodeOrNull(content);
    MOZ_ASSERT(form);
    // tell the form that we are about to exit a click handler
    // so the form knows not to defer subsequent submissions
    // the pending ones that were created during the handler
    // will be flushed or forgoten.
    form->OnSubmitClickEnd();
  }

  if (nsEventStatus_eIgnore == aVisitor.mEventStatus) {
    WidgetKeyboardEvent* keyEvent = aVisitor.mEvent->AsKeyboardEvent();
    if (keyEvent && keyEvent->IsTrusted()) {
      HandleKeyboardActivation(aVisitor);
    }

    if (aVisitor.mItemFlags & NS_OUTER_ACTIVATE_EVENT) {
      if (mForm) {
        // Hold a strong ref while dispatching
        RefPtr<mozilla::dom::HTMLFormElement> form(mForm);
        if (mType == FormControlType::ButtonReset) {
          form->MaybeReset(this);
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        } else if (mType == FormControlType::ButtonSubmit) {
          form->MaybeSubmit(this);
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
        // https://html.spec.whatwg.org/multipage/form-elements.html#attr-button-type-button-state
        // NS_FORM_BUTTON_BUTTON do nothing.
        return rv;
      }
    }
  }

  if ((aVisitor.mItemFlags & NS_IN_SUBMIT_CLICK)) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aVisitor.mItemData));
    RefPtr<HTMLFormElement> form = HTMLFormElement::FromNodeOrNull(content);
    MOZ_ASSERT(form);
    // Tell the form to flush a possible pending submission.
    // the reason is that the script returned false (the event was
    // not ignored) so if there is a stored submission, it needs to
    // be submitted immediatelly.
    // Note, NS_IN_SUBMIT_CLICK is set only when we're in outer activate event.
    form->FlushPendingSubmission();
  }

  return rv;
}

nsresult HTMLButtonElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  nsresult rv =
      nsGenericHTMLFormControlElementWithState::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  UpdateBarredFromConstraintValidation();

  // Update our state; we may now be the default submit element
  UpdateState(false);

  return NS_OK;
}

void HTMLButtonElement::UnbindFromTree(bool aNullParent) {
  nsGenericHTMLFormControlElementWithState::UnbindFromTree(aNullParent);

  UpdateBarredFromConstraintValidation();

  // Update our state; we may no longer be the default submit element
  UpdateState(false);
}

NS_IMETHODIMP
HTMLButtonElement::Reset() { return NS_OK; }

NS_IMETHODIMP
HTMLButtonElement::SubmitNamesValues(FormData* aFormData) {
  //
  // We only submit if we were the button pressed
  //
  if (aFormData->GetSubmitterElement() != this) {
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
  return aFormData->AddNameValuePair(name, value);
}

void HTMLButtonElement::DoneCreatingElement() {
  if (!mInhibitStateRestoration) {
    GenerateStateKey();
    RestoreFormControlState();
  }
}

nsresult HTMLButtonElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                          const nsAttrValueOrString* aValue,
                                          bool aNotify) {
  if (aNotify && aName == nsGkAtoms::disabled &&
      aNameSpaceID == kNameSpaceID_None) {
    mDisabledChanged = true;
  }

  return nsGenericHTMLFormControlElementWithState::BeforeSetAttr(
      aNameSpaceID, aName, aValue, aNotify);
}

nsresult HTMLButtonElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                         const nsAttrValue* aValue,
                                         const nsAttrValue* aOldValue,
                                         nsIPrincipal* aSubjectPrincipal,
                                         bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::type) {
      if (aValue) {
        mType = FormControlType(aValue->GetEnumValue());
      } else {
        mType = FormControlType(kButtonDefaultType->value);
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

  return nsGenericHTMLFormControlElementWithState::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

void HTMLButtonElement::SaveState() {
  if (!mDisabledChanged) {
    return;
  }

  PresState* state = GetPrimaryPresState();
  if (state) {
    // We do not want to save the real disabled state but the disabled
    // attribute.
    state->disabled() = HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
    state->disabledSet() = true;
  }
}

bool HTMLButtonElement::RestoreState(PresState* aState) {
  if (aState && aState->disabledSet() && !aState->disabled()) {
    SetDisabled(false, IgnoreErrors());
  }

  return false;
}

ElementState HTMLButtonElement::IntrinsicState() const {
  ElementState state =
      nsGenericHTMLFormControlElementWithState::IntrinsicState();

  if (IsCandidateForConstraintValidation()) {
    if (IsValid()) {
      state |= ElementState::VALID | ElementState::USER_VALID;
    } else {
      state |= ElementState::INVALID | ElementState::USER_INVALID;
    }
  }

  return state;
}

JSObject* HTMLButtonElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLButtonElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
