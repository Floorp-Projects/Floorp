/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIConstraintValidition_h___
#define nsIConstraintValidition_h___

#include "nsISupports.h"
#include "nsAutoPtr.h"
#include "nsString.h"

class nsDOMValidityState;
class nsIDOMValidityState;

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

  friend class nsDOMValidityState;

  static const PRUint16 sContentSpecifiedMaxLengthMessage;

  virtual ~nsIConstraintValidation();

  PRBool IsValid() const { return mValidityBitField == 0; }

  PRBool IsCandidateForConstraintValidation() const {
           return !mBarredFromConstraintValidation;
         }

  NS_IMETHOD GetValidationMessage(nsAString& aValidationMessage);

protected:

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

  // You can't instantiate an object from that class.
  nsIConstraintValidation();

  nsresult GetValidity(nsIDOMValidityState** aValidity);
  nsresult CheckValidity(PRBool* aValidity);
  void     SetCustomValidity(const nsAString& aError);

  bool GetValidityState(ValidityStateType mState) const {
         return mValidityBitField & mState;
       }

  void SetValidityState(ValidityStateType mState,
                        PRBool mValue);

  void SetBarredFromConstraintValidation(PRBool aBarred);

  virtual nsresult GetValidationMessage(nsAString& aValidationMessage,
                                        ValidityStateType aType) {
                     return NS_OK;
                   }

private:

  /**
   * A bitfield representing the current validity state of the element.
   * Each bit represent an error. All bits to zero means the element is valid.
   */
  PRInt8                        mValidityBitField;

  /**
   * A pointer to the ValidityState object.
   */
  nsRefPtr<nsDOMValidityState>  mValidity;

  /**
   * Keeps track whether the element is barred from constraint validation.
   */
  PRBool                        mBarredFromConstraintValidation;

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
  NS_IMETHOD GetWillValidate(PRBool* aWillValidate) {                         \
    *aWillValidate = IsCandidateForConstraintValidation();                    \
    return NS_OK;                                                             \
  }                                                                           \
  NS_IMETHOD GetValidationMessage(nsAString& aValidationMessage) {            \
    return nsIConstraintValidation::GetValidationMessage(aValidationMessage); \
  }                                                                           \
  NS_IMETHOD CheckValidity(PRBool* aValidity) {                               \
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
  NS_IMETHODIMP _from::GetWillValidate(PRBool* aWillValidate) {               \
    *aWillValidate = IsCandidateForConstraintValidation();                    \
    return NS_OK;                                                             \
  }                                                                           \
  NS_IMETHODIMP _from::GetValidationMessage(nsAString& aValidationMessage) {  \
    return nsIConstraintValidation::GetValidationMessage(aValidationMessage); \
  }                                                                           \
  NS_IMETHODIMP _from::CheckValidity(PRBool* aValidity) {                     \
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

