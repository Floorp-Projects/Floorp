/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ConstraintValidition_h___
#define mozilla_dom_ConstraintValidition_h___

#include "nsIConstraintValidation.h"
#include "nsString.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class ConstraintValidation : public nsIConstraintValidation {
 public:
  // Web IDL binding methods
  void GetValidationMessage(nsAString& aValidationMessage,
                            mozilla::ErrorResult& aError);
  bool CheckValidity();

 protected:
  // You can't instantiate an object from that class.
  ConstraintValidation();

  virtual ~ConstraintValidation() = default;

  void SetCustomValidity(const nsAString& aError);

  virtual nsresult GetValidationMessage(nsAString& aValidationMessage,
                                        ValidityStateType aType) {
    return NS_OK;
  }

 private:
  /**
   * The string representing the custom error.
   */
  nsString mCustomValidity;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ConstraintValidition_h___
