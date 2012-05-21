/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMValidityState.h"

#include "nsDOMClassInfoID.h"


DOMCI_DATA(ValidityState, nsDOMValidityState)

NS_IMPL_ADDREF(nsDOMValidityState)
NS_IMPL_RELEASE(nsDOMValidityState)

NS_INTERFACE_MAP_BEGIN(nsDOMValidityState)
  NS_INTERFACE_MAP_ENTRY(nsIDOMValidityState)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMValidityState)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ValidityState)
NS_INTERFACE_MAP_END

nsDOMValidityState::nsDOMValidityState(nsIConstraintValidation* aConstraintValidation)
  : mConstraintValidation(aConstraintValidation)
{
}

NS_IMETHODIMP
nsDOMValidityState::GetValueMissing(bool* aValueMissing)
{
  *aValueMissing = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_VALUE_MISSING);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMValidityState::GetTypeMismatch(bool* aTypeMismatch)
{
  *aTypeMismatch = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_TYPE_MISMATCH);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMValidityState::GetPatternMismatch(bool* aPatternMismatch)
{
  *aPatternMismatch = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_PATTERN_MISMATCH);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMValidityState::GetTooLong(bool* aTooLong)
{
  *aTooLong = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_TOO_LONG);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMValidityState::GetRangeUnderflow(bool* aRangeUnderflow)
{
  *aRangeUnderflow = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_RANGE_UNDERFLOW);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMValidityState::GetRangeOverflow(bool* aRangeOverflow)
{
  *aRangeOverflow = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_RANGE_OVERFLOW);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMValidityState::GetStepMismatch(bool* aStepMismatch)
{
  *aStepMismatch = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_STEP_MISMATCH);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMValidityState::GetCustomError(bool* aCustomError)
{
  *aCustomError = GetValidityState(nsIConstraintValidation::VALIDITY_STATE_CUSTOM_ERROR);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMValidityState::GetValid(bool* aValid)
{
  *aValid = !mConstraintValidation || mConstraintValidation->IsValid();
  return NS_OK;
}

