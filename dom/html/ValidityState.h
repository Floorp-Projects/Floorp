/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ValidityState_h
#define mozilla_dom_ValidityState_h

#include "nsIDOMValidityState.h"
#include "nsIConstraintValidation.h"
#include "nsWrapperCache.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class ValidityState final : public nsIDOMValidityState,
                            public nsWrapperCache
{
  ~ValidityState() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ValidityState)
  NS_DECL_NSIDOMVALIDITYSTATE

  friend class ::nsIConstraintValidation;

  nsIConstraintValidation* GetParentObject() const {
    return mConstraintValidation;
  }

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Web IDL methods
  bool ValueMissing() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_VALUE_MISSING);
  }
  bool TypeMismatch() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_TYPE_MISMATCH);
  }
  bool PatternMismatch() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_PATTERN_MISMATCH);
  }
  bool TooLong() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_TOO_LONG);
  }
  bool TooShort() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_TOO_SHORT);
  }
  bool RangeUnderflow() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_RANGE_UNDERFLOW);
  }
  bool RangeOverflow() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_RANGE_OVERFLOW);
  }
  bool StepMismatch() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_STEP_MISMATCH);
  }
  bool BadInput() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_BAD_INPUT);
  }
  bool CustomError() const
  {
    return GetValidityState(nsIConstraintValidation::VALIDITY_STATE_CUSTOM_ERROR);
  }
  bool Valid() const
  {
    return !mConstraintValidation || mConstraintValidation->IsValid();
  }

protected:
  explicit ValidityState(nsIConstraintValidation* aConstraintValidation);

  /**
   * Helper function to get a validity state from constraint validation instance.
   */
  inline bool GetValidityState(nsIConstraintValidation::ValidityStateType aState) const
  {
    return mConstraintValidation &&
           mConstraintValidation->GetValidityState(aState);
  }

  nsCOMPtr<nsIConstraintValidation> mConstraintValidation;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ValidityState_h

