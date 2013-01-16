/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ValidityState.h"

#include "nsDOMClassInfoID.h"


DOMCI_DATA(ValidityState, mozilla::dom::ValidityState)

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF(ValidityState)
NS_IMPL_RELEASE(ValidityState)

NS_INTERFACE_MAP_BEGIN(ValidityState)
  NS_INTERFACE_MAP_ENTRY(nsIDOMValidityState)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMValidityState)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ValidityState)
NS_INTERFACE_MAP_END

ValidityState::ValidityState(nsIConstraintValidation* aConstraintValidation)
  : mConstraintValidation(aConstraintValidation)
{
}

NS_IMETHODIMP
ValidityState::GetValueMissing(bool* aValueMissing)
{
  *aValueMissing = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_VALUE_MISSING);
  return NS_OK;
}

NS_IMETHODIMP
ValidityState::GetTypeMismatch(bool* aTypeMismatch)
{
  *aTypeMismatch = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_TYPE_MISMATCH);
  return NS_OK;
}

NS_IMETHODIMP
ValidityState::GetPatternMismatch(bool* aPatternMismatch)
{
  *aPatternMismatch = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_PATTERN_MISMATCH);
  return NS_OK;
}

NS_IMETHODIMP
ValidityState::GetTooLong(bool* aTooLong)
{
  *aTooLong = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_TOO_LONG);
  return NS_OK;
}

NS_IMETHODIMP
ValidityState::GetRangeUnderflow(bool* aRangeUnderflow)
{
  *aRangeUnderflow = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_RANGE_UNDERFLOW);
  return NS_OK;
}

NS_IMETHODIMP
ValidityState::GetRangeOverflow(bool* aRangeOverflow)
{
  *aRangeOverflow = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_RANGE_OVERFLOW);
  return NS_OK;
}

NS_IMETHODIMP
ValidityState::GetStepMismatch(bool* aStepMismatch)
{
  *aStepMismatch = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_STEP_MISMATCH);
  return NS_OK;
}

NS_IMETHODIMP
ValidityState::GetCustomError(bool* aCustomError)
{
  *aCustomError = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_CUSTOM_ERROR);
  return NS_OK;
}

NS_IMETHODIMP
ValidityState::GetValid(bool* aValid)
{
  *aValid = !mConstraintValidation || mConstraintValidation->IsValid();
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

