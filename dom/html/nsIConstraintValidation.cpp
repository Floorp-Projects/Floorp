/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIConstraintValidation.h"

#include "nsAString.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLFieldSetElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/ValidityState.h"
#include "nsIFormControl.h"
#include "nsContentUtils.h"

#include "nsIFormSubmitObserver.h"
#include "nsIObserverService.h"

const uint16_t nsIConstraintValidation::sContentSpecifiedMaxLengthMessage = 256;

using namespace mozilla;
using namespace mozilla::dom;

nsIConstraintValidation::nsIConstraintValidation()
  : mValidityBitField(0)
  // By default, all elements are subjects to constraint validation.
  , mBarredFromConstraintValidation(false)
{
}

nsIConstraintValidation::~nsIConstraintValidation()
{
}

mozilla::dom::ValidityState*
nsIConstraintValidation::Validity()
{
  if (!mValidity) {
    mValidity = new mozilla::dom::ValidityState(this);
  }

  return mValidity;
}

void
nsIConstraintValidation::GetValidationMessage(nsAString& aValidationMessage,
                                              ErrorResult& aError)
{
  aValidationMessage.Truncate();

  if (IsCandidateForConstraintValidation() && !IsValid()) {
    nsCOMPtr<Element> element = do_QueryInterface(this);
    NS_ASSERTION(element, "This class should be inherited by HTML elements only!");

    nsAutoString authorMessage;
    element->GetAttr(kNameSpaceID_None, nsGkAtoms::x_moz_errormessage,
                     authorMessage);

    if (!authorMessage.IsEmpty()) {
      aValidationMessage.Assign(authorMessage);
      if (aValidationMessage.Length() > sContentSpecifiedMaxLengthMessage) {
        aValidationMessage.Truncate(sContentSpecifiedMaxLengthMessage);
      }
    } else if (GetValidityState(VALIDITY_STATE_CUSTOM_ERROR)) {
      aValidationMessage.Assign(mCustomValidity);
      if (aValidationMessage.Length() > sContentSpecifiedMaxLengthMessage) {
        aValidationMessage.Truncate(sContentSpecifiedMaxLengthMessage);
      }
    } else if (GetValidityState(VALIDITY_STATE_TOO_LONG)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_TOO_LONG);
    } else if (GetValidityState(VALIDITY_STATE_TOO_SHORT)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_TOO_SHORT);
    } else if (GetValidityState(VALIDITY_STATE_VALUE_MISSING)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_VALUE_MISSING);
    } else if (GetValidityState(VALIDITY_STATE_TYPE_MISMATCH)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_TYPE_MISMATCH);
    } else if (GetValidityState(VALIDITY_STATE_PATTERN_MISMATCH)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_PATTERN_MISMATCH);
    } else if (GetValidityState(VALIDITY_STATE_RANGE_OVERFLOW)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_RANGE_OVERFLOW);
    } else if (GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_RANGE_UNDERFLOW);
    } else if (GetValidityState(VALIDITY_STATE_STEP_MISMATCH)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_STEP_MISMATCH);
    } else if (GetValidityState(VALIDITY_STATE_BAD_INPUT)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_BAD_INPUT);
    } else {
      // There should not be other validity states.
      aError.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
  } else {
    aValidationMessage.Truncate();
  }
}

bool
nsIConstraintValidation::CheckValidity()
{
  if (!IsCandidateForConstraintValidation() || IsValid()) {
    return true;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(this);
  NS_ASSERTION(content, "This class should be inherited by HTML elements only!");

  nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(),
                                       content,
                                       NS_LITERAL_STRING("invalid"),
                                       CanBubble::eNo,
                                       Cancelable::eYes);
  return false;
}

nsresult
nsIConstraintValidation::CheckValidity(bool* aValidity)
{
  NS_ENSURE_ARG_POINTER(aValidity);

  *aValidity = CheckValidity();

  return NS_OK;
}

bool
nsIConstraintValidation::ReportValidity()
{
  if (!IsCandidateForConstraintValidation() || IsValid()) {
    return true;
  }

  nsCOMPtr<Element> element = do_QueryInterface(this);
  MOZ_ASSERT(element, "This class should be inherited by HTML elements only!");

  bool defaultAction = true;
  nsContentUtils::DispatchTrustedEvent(element->OwnerDoc(), element,
                                       NS_LITERAL_STRING("invalid"),
                                       CanBubble::eNo,
                                       Cancelable::eYes,
                                       &defaultAction);
  if (!defaultAction) {
    return false;
  }

  nsCOMPtr<nsIObserverService> service =
    mozilla::services::GetObserverService();
  if (!service) {
    NS_WARNING("No observer service available!");
    return true;
  }

  nsCOMPtr<nsISimpleEnumerator> theEnum;
  nsresult rv = service->EnumerateObservers(NS_INVALIDFORMSUBMIT_SUBJECT,
                                            getter_AddRefs(theEnum));

  // Return true on error here because that's what we always did
  NS_ENSURE_SUCCESS(rv, true);

  bool hasObserver = false;
  rv = theEnum->HasMoreElements(&hasObserver);

  nsCOMPtr<nsIMutableArray> invalidElements =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  invalidElements->AppendElement(element);

  NS_ENSURE_SUCCESS(rv, true);
  nsCOMPtr<nsISupports> inst;
  nsCOMPtr<nsIFormSubmitObserver> observer;
  bool more = true;
  while (NS_SUCCEEDED(theEnum->HasMoreElements(&more)) && more) {
    theEnum->GetNext(getter_AddRefs(inst));
    observer = do_QueryInterface(inst);

    if (observer) {
      observer->NotifyInvalidSubmit(nullptr, invalidElements);
    }
  }

  if (element->IsHTMLElement(nsGkAtoms::input) &&
      // We don't use nsContentUtils::IsFocusedContent here, because it doesn't
      // really do what we want for number controls: it's true for the
      // anonymous textnode inside, but not the number control itself.  We can
      // use the focus state, though, because that gets synced to the number
      // control by the anonymous text control.
      element->State().HasState(NS_EVENT_STATE_FOCUS)) {
    HTMLInputElement* inputElement = HTMLInputElement::FromNode(element);
    inputElement->UpdateValidityUIBits(true);
  }

  element->UpdateState(true);
  return false;
}

void
nsIConstraintValidation::SetValidityState(ValidityStateType aState,
                                          bool aValue)
{
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

    HTMLFormElement* form =
      static_cast<HTMLFormElement*>(formCtrl->GetFormElement());
    if (form) {
      form->UpdateValidity(IsValid());
    }
    HTMLFieldSetElement* fieldSet = formCtrl->GetFieldSet();
      if (fieldSet) {
      fieldSet->UpdateValidity(IsValid());
    }
  }
}

void
nsIConstraintValidation::SetCustomValidity(const nsAString& aError)
{
  mCustomValidity.Assign(aError);
  SetValidityState(VALIDITY_STATE_CUSTOM_ERROR, !mCustomValidity.IsEmpty());
}

void
nsIConstraintValidation::SetBarredFromConstraintValidation(bool aBarred)
{
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
    HTMLFormElement* form =
      static_cast<HTMLFormElement*>(formCtrl->GetFormElement());
    if (form) {
      form->UpdateValidity(aBarred);
    }
    HTMLFieldSetElement* fieldSet = formCtrl->GetFieldSet();
    if (fieldSet) {
      fieldSet->UpdateValidity(aBarred);
    }
  }
}

