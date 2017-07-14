/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIConstraintValidition_h___
#define nsIConstraintValidition_h___

#include "nsISupports.h"
#include "nsString.h"

class nsIDOMValidityState;

namespace mozilla {
class ErrorResult;
namespace dom {
class ValidityState;
} // namespace dom
} // namespace mozilla

#define NS_ICONSTRAINTVALIDATION_IID \
{ 0x983829da, 0x1aaf, 0x449c, \
 { 0xa3, 0x06, 0x85, 0xd4, 0xf0, 0x31, 0x1c, 0xf6 } }

/**
 * This interface is for form elements implementing the validity constraint API.
 * See: http://dev.w3.org/html5/spec/forms.html#the-constraint-validation-api
 *
 * This interface has to be implemented by all elements implementing the API
 * and only them.
 */
class nsIConstraintValidation : public nsISupports
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONSTRAINTVALIDATION_IID);

  friend class mozilla::dom::ValidityState;

  static const uint16_t sContentSpecifiedMaxLengthMessage;

  virtual ~nsIConstraintValidation();

  bool IsValid() const { return mValidityBitField == 0; }

  bool IsCandidateForConstraintValidation() const {
           return !mBarredFromConstraintValidation;
         }

  void GetValidationMessage(nsAString& aValidationMessage,
                            mozilla::ErrorResult& aError);

  enum ValidityStateType
  {
    VALIDITY_STATE_VALUE_MISSING    = 0x1 <<  0,
    VALIDITY_STATE_TYPE_MISMATCH    = 0x1 <<  1,
    VALIDITY_STATE_PATTERN_MISMATCH = 0x1 <<  2,
    VALIDITY_STATE_TOO_LONG         = 0x1 <<  3,
    VALIDITY_STATE_TOO_SHORT        = 0x1 <<  4,
    VALIDITY_STATE_RANGE_UNDERFLOW  = 0x1 <<  5,
    VALIDITY_STATE_RANGE_OVERFLOW   = 0x1 <<  6,
    VALIDITY_STATE_STEP_MISMATCH    = 0x1 <<  7,
    VALIDITY_STATE_BAD_INPUT        = 0x1 <<  8,
    VALIDITY_STATE_CUSTOM_ERROR     = 0x1 <<  9,
  };

  void SetValidityState(ValidityStateType aState,
                        bool aValue);

  // Web IDL binding methods
  bool WillValidate() const {
    return IsCandidateForConstraintValidation();
  }
  mozilla::dom::ValidityState* Validity();
  bool CheckValidity();
  bool ReportValidity();

protected:

  // You can't instantiate an object from that class.
  nsIConstraintValidation();

  nsresult GetValidity(nsIDOMValidityState** aValidity);
  nsresult CheckValidity(bool* aValidity);
  void     SetCustomValidity(const nsAString& aError);

  bool GetValidityState(ValidityStateType aState) const
  {
    return mValidityBitField & aState;
  }

  void SetBarredFromConstraintValidation(bool aBarred);

  virtual nsresult GetValidationMessage(nsAString& aValidationMessage,
                                        ValidityStateType aType) {
                     return NS_OK;
                   }

protected:
  /**
   * A pointer to the ValidityState object.
   */
  RefPtr<mozilla::dom::ValidityState>  mValidity;

private:

  /**
   * A bitfield representing the current validity state of the element.
   * Each bit represent an error. All bits to zero means the element is valid.
   */
  int16_t                       mValidityBitField;

  /**
   * Keeps track whether the element is barred from constraint validation.
   */
  bool                          mBarredFromConstraintValidation;

  /**
   * The string representing the custom error.
   */
  nsString                      mCustomValidity;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIConstraintValidation,
                              NS_ICONSTRAINTVALIDATION_IID)

#endif // nsIConstraintValidation_h___

