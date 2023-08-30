/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIConstraintValidation.h"

#include "nsGenericHTMLElement.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLFieldSetElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/ValidityState.h"
#include "nsIFormControl.h"
#include "nsISimpleEnumerator.h"
#include "nsContentUtils.h"

const uint16_t nsIConstraintValidation::sContentSpecifiedMaxLengthMessage = 256;

using namespace mozilla;
using namespace mozilla::dom;

nsIConstraintValidation::nsIConstraintValidation()
    : mValidityBitField(0)
      // By default, all elements are subjects to constraint validation.
      ,
      mBarredFromConstraintValidation(false) {}

nsIConstraintValidation::~nsIConstraintValidation() = default;

mozilla::dom::ValidityState* nsIConstraintValidation::Validity() {
  if (!mValidity) {
    mValidity = new mozilla::dom::ValidityState(this);
  }

  return mValidity;
}

bool nsIConstraintValidation::CheckValidity(nsIContent& aEventTarget,
                                            bool* aEventDefaultAction) const {
  if (!IsCandidateForConstraintValidation() || IsValid()) {
    return true;
  }

  nsContentUtils::DispatchTrustedEvent(
      aEventTarget.OwnerDoc(), &aEventTarget, u"invalid"_ns, CanBubble::eNo,
      Cancelable::eYes, Composed::eDefault, aEventDefaultAction);
  return false;
}

bool nsIConstraintValidation::ReportValidity() {
  nsCOMPtr<Element> element = do_QueryInterface(this);
  MOZ_ASSERT(element, "This class should be inherited by HTML elements only!");

  bool defaultAction = true;
  if (CheckValidity(*element, &defaultAction)) {
    return true;
  }

  if (!defaultAction) {
    return false;
  }

  AutoTArray<RefPtr<Element>, 1> invalidElements;
  invalidElements.AppendElement(element);

  AutoJSAPI jsapi;
  if (!jsapi.Init(element->GetOwnerGlobal())) {
    return false;
  }
  JS::Rooted<JS::Value> detail(jsapi.cx());
  if (!ToJSValue(jsapi.cx(), invalidElements, &detail)) {
    return false;
  }

  RefPtr<CustomEvent> event =
      NS_NewDOMCustomEvent(element->OwnerDoc(), nullptr, nullptr);
  event->InitCustomEvent(jsapi.cx(), u"MozInvalidForm"_ns,
                         /* CanBubble */ true,
                         /* Cancelable */ true, detail);
  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  element->DispatchEvent(*event);

  auto* inputElement = HTMLInputElement::FromNode(element);
  if (inputElement && inputElement->State().HasState(ElementState::FOCUS)) {
    inputElement->UpdateValidityUIBits(true);
    inputElement->UpdateValidityElementStates(true);
  }

  return false;
}

void nsIConstraintValidation::SetValidityState(ValidityStateType aState,
                                               bool aValue) {
  bool previousValidity = IsValid();

  if (aValue) {
    mValidityBitField |= aState;
  } else {
    mValidityBitField &= ~aState;
  }

  // Inform the form and fieldset elements if our validity has changed.
  if (previousValidity != IsValid() && IsCandidateForConstraintValidation()) {
    nsCOMPtr<nsIFormControl> formCtrl = do_QueryInterface(this);
    NS_ASSERTION(formCtrl, "This interface should be used by form elements!");

    if (HTMLFormElement* form = formCtrl->GetForm()) {
      form->UpdateValidity(IsValid());
    }
    if (HTMLFieldSetElement* fieldSet = formCtrl->GetFieldSet()) {
      fieldSet->UpdateValidity(IsValid());
    }
  }
}

void nsIConstraintValidation::SetBarredFromConstraintValidation(bool aBarred) {
  bool previousBarred = mBarredFromConstraintValidation;

  mBarredFromConstraintValidation = aBarred;

  // Inform the form and fieldset elements if our status regarding constraint
  // validation is going to change.
  if (!IsValid() && previousBarred != mBarredFromConstraintValidation) {
    nsCOMPtr<nsIFormControl> formCtrl = do_QueryInterface(this);
    NS_ASSERTION(formCtrl, "This interface should be used by form elements!");

    // If the element is going to be barred from constraint validation, we can
    // inform the form and fieldset that we are now valid. Otherwise, we are now
    // invalid.
    if (HTMLFormElement* form = formCtrl->GetForm()) {
      form->UpdateValidity(aBarred);
    }
    HTMLFieldSetElement* fieldSet = formCtrl->GetFieldSet();
    if (fieldSet) {
      fieldSet->UpdateValidity(aBarred);
    }
  }
}
