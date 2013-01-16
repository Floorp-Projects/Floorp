/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIConstraintValidition_h___
#define nsIConstraintValidition_h___

#include "nsISupports.h"
#include "nsAutoPtr.h"
#include "nsString.h"

class nsIDOMValidityState;

namespace mozilla {
namespace dom {
class ValidityState;
}
}

#define NS_ICONSTRAINTVALIDATION_IID \
{ 0xca3824dc, 0x4f5c, 0x4878, \
 { 0xa6, 0x8a, 0x95, 0x54, 0x5f, 0xfa, 0x4b, 0xf9 } }

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

  NS_IMETHOD GetValidationMessage(nsAString& aValidationMessage);

  enum ValidityStateType
  {
    VALIDITY_STATE_VALUE_MISSING    = 0x01, // 0b00000001
    VALIDITY_STATE_TYPE_MISMATCH    = 0x02, // 0b00000010
    VALIDITY_STATE_PATTERN_MISMATCH = 0x04, // 0b00000100
    VALIDITY_STATE_TOO_LONG         = 0x08, // 0b00001000
    VALIDITY_STATE_RANGE_UNDERFLOW  = 0x10, // 0b00010000
    VALIDITY_STATE_RANGE_OVERFLOW   = 0x20, // 0b00100000
    VALIDITY_STATE_STEP_MISMATCH    = 0x40, // 0b01000000
    VALIDITY_STATE_CUSTOM_ERROR     = 0x80  // 0b10000000
  };

  void SetValidityState(ValidityStateType mState,
                        bool mValue);

protected:

  // You can't instantiate an object from that class.
  nsIConstraintValidation();

  nsresult GetValidity(nsIDOMValidityState** aValidity);
  nsresult CheckValidity(bool* aValidity);
  void     SetCustomValidity(const nsAString& aError);

  bool GetValidityState(ValidityStateType mState) const {
         return mValidityBitField & mState;
       }

  void SetBarredFromConstraintValidation(bool aBarred);

  virtual nsresult GetValidationMessage(nsAString& aValidationMessage,
                                        ValidityStateType aType) {
                     return NS_OK;
                   }

private:

  /**
   * A bitfield representing the current validity state of the element.
   * Each bit represent an error. All bits to zero means the element is valid.
   */
  int8_t                        mValidityBitField;

  /**
   * A pointer to the ValidityState object.
   */
  nsRefPtr<mozilla::dom::ValidityState>  mValidity;

  /**
   * Keeps track whether the element is barred from constraint validation.
   */
  bool                          mBarredFromConstraintValidation;

  /**
   * The string representing the custom error.
   */
  nsString                      mCustomValidity;
};

/**
 * Use these macro for class inheriting from nsIConstraintValidation to forward
 * functions to nsIConstraintValidation.
 */
#define NS_FORWARD_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY           \
  NS_IMETHOD GetValidity(nsIDOMValidityState** aValidity) {                   \
    return nsIConstraintValidation::GetValidity(aValidity);                   \
  }                                                                           \
  NS_IMETHOD GetWillValidate(bool* aWillValidate) {                         \
    *aWillValidate = IsCandidateForConstraintValidation();                    \
    return NS_OK;                                                             \
  }                                                                           \
  NS_IMETHOD GetValidationMessage(nsAString& aValidationMessage) {            \
    return nsIConstraintValidation::GetValidationMessage(aValidationMessage); \
  }                                                                           \
  NS_IMETHOD CheckValidity(bool* aValidity) {                               \
    return nsIConstraintValidation::CheckValidity(aValidity);                 \
  }

#define NS_FORWARD_NSICONSTRAINTVALIDATION                                    \
  NS_FORWARD_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY                 \
  NS_IMETHOD SetCustomValidity(const nsAString& aError) {                     \
    nsIConstraintValidation::SetCustomValidity(aError);                       \
    return NS_OK;                                                             \
  }


/* Use these macro when class declares functions from nsIConstraintValidation */
#define NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(_from)       \
  NS_IMETHODIMP _from::GetValidity(nsIDOMValidityState** aValidity) {         \
    return nsIConstraintValidation::GetValidity(aValidity);                   \
  }                                                                           \
  NS_IMETHODIMP _from::GetWillValidate(bool* aWillValidate) {               \
    *aWillValidate = IsCandidateForConstraintValidation();                    \
    return NS_OK;                                                             \
  }                                                                           \
  NS_IMETHODIMP _from::GetValidationMessage(nsAString& aValidationMessage) {  \
    return nsIConstraintValidation::GetValidationMessage(aValidationMessage); \
  }                                                                           \
  NS_IMETHODIMP _from::CheckValidity(bool* aValidity) {                     \
    return nsIConstraintValidation::CheckValidity(aValidity);                 \
  }

#define NS_IMPL_NSICONSTRAINTVALIDATION(_from)                                \
  NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(_from)             \
  NS_IMETHODIMP _from::SetCustomValidity(const nsAString& aError) {           \
    nsIConstraintValidation::SetCustomValidity(aError);                       \
    return NS_OK;                                                             \
  }

NS_DEFINE_STATIC_IID_ACCESSOR(nsIConstraintValidation,
                              NS_ICONSTRAINTVALIDATION_IID)

#endif // nsIConstraintValidation_h___

