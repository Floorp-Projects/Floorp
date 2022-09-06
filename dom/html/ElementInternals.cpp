/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ElementInternals.h"

#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/dom/ElementInternalsBinding.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/HTMLElement.h"
#include "mozilla/dom/HTMLFieldSetElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/ValidityState.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ElementInternals)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ElementInternals)
  tmp->Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTarget, mSubmissionValue, mState, mValidity,
                                  mValidationAnchor);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ElementInternals)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTarget, mSubmissionValue, mState,
                                    mValidity, mValidationAnchor);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ElementInternals)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ElementInternals)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ElementInternals)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIFormControl)
  NS_INTERFACE_MAP_ENTRY(nsIConstraintValidation)
NS_INTERFACE_MAP_END

ElementInternals::ElementInternals(HTMLElement* aTarget)
    : nsIFormControl(FormControlType::FormAssociatedCustomElement),
      mTarget(aTarget),
      mForm(nullptr),
      mFieldSet(nullptr) {}

nsISupports* ElementInternals::GetParentObject() { return ToSupports(mTarget); }

JSObject* ElementInternals::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return ElementInternals_Binding::Wrap(aCx, this, aGivenProto);
}

// https://html.spec.whatwg.org/#dom-elementinternals-shadowroot
ShadowRoot* ElementInternals::GetShadowRoot() const {
  MOZ_ASSERT(mTarget);

  ShadowRoot* shadowRoot = mTarget->GetShadowRoot();
  if (shadowRoot && !shadowRoot->IsAvailableToElementInternals()) {
    return nullptr;
  }

  return shadowRoot;
}

// https://html.spec.whatwg.org/commit-snapshots/912a3fe1f29649ccf8229de56f604b3c07ffd242/#dom-elementinternals-setformvalue
void ElementInternals::SetFormValue(
    const Nullable<FileOrUSVStringOrFormData>& aValue,
    const Optional<Nullable<FileOrUSVStringOrFormData>>& aState,
    ErrorResult& aRv) {
  MOZ_ASSERT(mTarget);

  /**
   * 1. Let element be this's target element.
   * 2. If element is not a form-associated custom element, then throw a
   *    "NotSupportedError" DOMException.
   */
  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return;
  }

  /**
   * 3. Set target element's submission value to value if value is not a
   *    FormData object, or to a clone of the entry list associated with value
   *    otherwise.
   */
  mSubmissionValue.SetNull();
  if (!aValue.IsNull()) {
    const FileOrUSVStringOrFormData& value = aValue.Value();
    OwningFileOrUSVStringOrFormData& owningValue = mSubmissionValue.SetValue();
    if (value.IsFormData()) {
      owningValue.SetAsFormData() = value.GetAsFormData().Clone();
    } else if (value.IsFile()) {
      owningValue.SetAsFile() = &value.GetAsFile();
    } else {
      owningValue.SetAsUSVString() = value.GetAsUSVString();
    }
  }

  /**
   * 4. If the state argument of the function is omitted, set element's state to
   *    its submission value.
   */
  if (!aState.WasPassed()) {
    mState = mSubmissionValue;
    return;
  }

  /**
   * 5. Otherwise, if state is a FormData object, set element's state to clone
   *    of the entry list associated with state.
   * 6. Otherwise, set element's state to state.
   */
  mState.SetNull();
  if (!aState.Value().IsNull()) {
    const FileOrUSVStringOrFormData& state = aState.Value().Value();
    OwningFileOrUSVStringOrFormData& owningState = mState.SetValue();
    if (state.IsFormData()) {
      owningState.SetAsFormData() = state.GetAsFormData().Clone();
    } else if (state.IsFile()) {
      owningState.SetAsFile() = &state.GetAsFile();
    } else {
      owningState.SetAsUSVString() = state.GetAsUSVString();
    }
  }
}

// https://html.spec.whatwg.org/#dom-elementinternals-form
HTMLFormElement* ElementInternals::GetForm(ErrorResult& aRv) const {
  MOZ_ASSERT(mTarget);

  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return nullptr;
  }
  return GetForm();
}

// https://html.spec.whatwg.org/commit-snapshots/3ad5159be8f27e110a70cefadcb50fc45ec21b05/#dom-elementinternals-setvalidity
void ElementInternals::SetValidity(
    const ValidityStateFlags& aFlags, const Optional<nsAString>& aMessage,
    const Optional<NonNull<nsGenericHTMLElement>>& aAnchor, ErrorResult& aRv) {
  MOZ_ASSERT(mTarget);

  /**
   * 1. Let element be this's target element.
   * 2. If element is not a form-associated custom element, then throw a
   *    "NotSupportedError" DOMException.
   */
  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return;
  }

  /**
   * 3. If flags contains one or more true values and message is not given or is
   *    the empty string, then throw a TypeError.
   */
  if ((aFlags.mBadInput || aFlags.mCustomError || aFlags.mPatternMismatch ||
       aFlags.mRangeOverflow || aFlags.mRangeUnderflow ||
       aFlags.mStepMismatch || aFlags.mTooLong || aFlags.mTooShort ||
       aFlags.mTypeMismatch || aFlags.mValueMissing) &&
      (!aMessage.WasPassed() || aMessage.Value().IsEmpty())) {
    aRv.ThrowTypeError("Need to provide validation message");
    return;
  }

  /**
   * 4. For each entry flag → value of flags, set element's validity flag with
   *    the name flag to value.
   */
  SetValidityState(VALIDITY_STATE_VALUE_MISSING, aFlags.mValueMissing);
  SetValidityState(VALIDITY_STATE_TYPE_MISMATCH, aFlags.mTypeMismatch);
  SetValidityState(VALIDITY_STATE_PATTERN_MISMATCH, aFlags.mPatternMismatch);
  SetValidityState(VALIDITY_STATE_TOO_LONG, aFlags.mTooLong);
  SetValidityState(VALIDITY_STATE_TOO_SHORT, aFlags.mTooShort);
  SetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW, aFlags.mRangeUnderflow);
  SetValidityState(VALIDITY_STATE_RANGE_OVERFLOW, aFlags.mRangeOverflow);
  SetValidityState(VALIDITY_STATE_STEP_MISMATCH, aFlags.mStepMismatch);
  SetValidityState(VALIDITY_STATE_BAD_INPUT, aFlags.mBadInput);
  SetValidityState(VALIDITY_STATE_CUSTOM_ERROR, aFlags.mCustomError);
  mTarget->UpdateState(true);

  /**
   * 5. Set element's validation message to the empty string if message is not
   *    given or all of element's validity flags are false, or to message
   *    otherwise.
   * 6. If element's customError validity flag is true, then set element's
   *    custom validity error message to element's validation message.
   *    Otherwise, set element's custom validity error message to the empty
   *    string.
   */
  mValidationMessage =
      (!aMessage.WasPassed() || IsValid()) ? EmptyString() : aMessage.Value();

  /**
   * 7. Set element's validation anchor to null if anchor is not given.
   *    Otherwise, if anchor is not a shadow-including descendant of element,
   *    then throw a "NotFoundError" DOMException. Otherwise, set element's
   *    validation anchor to anchor.
   */
  nsGenericHTMLElement* anchor =
      aAnchor.WasPassed() ? &aAnchor.Value() : nullptr;
  // TODO: maybe create something like IsShadowIncludingDescendantOf if there
  //       are other places also need such check.
  if (anchor && (anchor == mTarget ||
                 !anchor->IsShadowIncludingInclusiveDescendantOf(mTarget))) {
    aRv.ThrowNotFoundError(
        "Validation anchor is not a shadow-including descendant of target"
        "element");
    return;
  }
  mValidationAnchor = anchor;
}

// https://html.spec.whatwg.org/#dom-elementinternals-willvalidate
bool ElementInternals::GetWillValidate(ErrorResult& aRv) const {
  MOZ_ASSERT(mTarget);

  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return false;
  }
  return WillValidate();
}

// https://html.spec.whatwg.org/#dom-elementinternals-validity
ValidityState* ElementInternals::GetValidity(ErrorResult& aRv) {
  MOZ_ASSERT(mTarget);

  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return nullptr;
  }
  return Validity();
}

// https://html.spec.whatwg.org/#dom-elementinternals-validationmessage
void ElementInternals::GetValidationMessage(nsAString& aValidationMessage,
                                            ErrorResult& aRv) const {
  MOZ_ASSERT(mTarget);

  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return;
  }
  aValidationMessage = mValidationMessage;
}

// https://html.spec.whatwg.org/#dom-elementinternals-checkvalidity
bool ElementInternals::CheckValidity(ErrorResult& aRv) {
  MOZ_ASSERT(mTarget);

  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return false;
  }
  return nsIConstraintValidation::CheckValidity(*mTarget);
}

// https://html.spec.whatwg.org/#dom-elementinternals-reportvalidity
bool ElementInternals::ReportValidity(ErrorResult& aRv) {
  MOZ_ASSERT(mTarget);

  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return false;
  }

  bool defaultAction = true;
  if (nsIConstraintValidation::CheckValidity(*mTarget, &defaultAction)) {
    return true;
  }

  if (!defaultAction) {
    return false;
  }

  AutoTArray<RefPtr<Element>, 1> invalidElements;
  invalidElements.AppendElement(mTarget);

  AutoJSAPI jsapi;
  if (!jsapi.Init(mTarget->GetOwnerGlobal())) {
    return false;
  }
  JS::Rooted<JS::Value> detail(jsapi.cx());
  if (!ToJSValue(jsapi.cx(), invalidElements, &detail)) {
    return false;
  }

  mTarget->UpdateState(true);

  RefPtr<CustomEvent> event =
      NS_NewDOMCustomEvent(mTarget->OwnerDoc(), nullptr, nullptr);
  event->InitCustomEvent(jsapi.cx(), u"MozInvalidForm"_ns,
                         /* CanBubble */ true,
                         /* Cancelable */ true, detail);
  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  mTarget->DispatchEvent(*event);

  return false;
}

// https://html.spec.whatwg.org/#dom-elementinternals-labels
already_AddRefed<nsINodeList> ElementInternals::GetLabels(
    ErrorResult& aRv) const {
  MOZ_ASSERT(mTarget);

  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return nullptr;
  }
  return mTarget->Labels();
}

nsGenericHTMLElement* ElementInternals::GetValidationAnchor(
    ErrorResult& aRv) const {
  MOZ_ASSERT(mTarget);

  if (!mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return nullptr;
  }
  return mValidationAnchor;
}

void ElementInternals::SetForm(HTMLFormElement* aForm) { mForm = aForm; }

void ElementInternals::ClearForm(bool aRemoveFromForm, bool aUnbindOrDelete) {
  if (mTarget) {
    mTarget->ClearForm(aRemoveFromForm, aUnbindOrDelete);
  }
}

NS_IMETHODIMP ElementInternals::Reset() {
  if (mTarget) {
    MOZ_ASSERT(mTarget->IsFormAssociatedElement());
    nsContentUtils::EnqueueLifecycleCallback(ElementCallbackType::eFormReset,
                                             mTarget, {});
  }
  return NS_OK;
}

NS_IMETHODIMP ElementInternals::SubmitNamesValues(FormData* aFormData) {
  if (!mTarget) {
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_ASSERT(mTarget->IsFormAssociatedElement());

  // https://html.spec.whatwg.org/#face-entry-construction
  if (!mSubmissionValue.IsNull()) {
    if (mSubmissionValue.Value().IsFormData()) {
      aFormData->Append(mSubmissionValue.Value().GetAsFormData());
      return NS_OK;
    }

    // Get the name
    nsAutoString name;
    if (!mTarget->GetAttr(nsGkAtoms::name, name) || name.IsEmpty()) {
      return NS_OK;
    }

    if (mSubmissionValue.Value().IsUSVString()) {
      return aFormData->AddNameValuePair(
          name, mSubmissionValue.Value().GetAsUSVString());
    }

    return aFormData->AddNameBlobPair(name,
                                      mSubmissionValue.Value().GetAsFile());
  }
  return NS_OK;
}

void ElementInternals::UpdateFormOwner() {
  if (mTarget) {
    mTarget->UpdateFormOwner();
  }
}

void ElementInternals::UpdateBarredFromConstraintValidation() {
  if (mTarget) {
    MOZ_ASSERT(mTarget->IsFormAssociatedElement());
    SetBarredFromConstraintValidation(
        mTarget->HasAttr(nsGkAtoms::readonly) ||
        mTarget->HasFlag(ELEMENT_IS_DATALIST_OR_HAS_DATALIST_ANCESTOR) ||
        mTarget->IsDisabled());
  }
}

void ElementInternals::Unlink() {
  if (mForm) {
    // Don't notify, since we're being destroyed in any case.
    ClearForm(true, true);
  }
  if (mFieldSet) {
    mFieldSet->RemoveElement(mTarget);
  }
}

}  // namespace mozilla::dom
