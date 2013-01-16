/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ValidityState_h
#define mozilla_dom_ValidityState_h

#include "nsIDOMValidityState.h"
#include "nsIConstraintValidation.h"

namespace mozilla {
namespace dom {

class ValidityState MOZ_FINAL : public nsIDOMValidityState
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMVALIDITYSTATE

  friend class ::nsIConstraintValidation;

protected:
  ValidityState(nsIConstraintValidation* aConstraintValidation);

  /**
   * This function should be called by nsIConstraintValidation
   * to set mConstraintValidation to null to be sure
   * it will not be used when the object is destroyed.
   */
  inline void Disconnect()
  {
    mConstraintValidation = nullptr;
  }

  /**
   * Helper function to get a validity state from constraint validation instance.
   */
  inline bool GetValidityState(nsIConstraintValidation::ValidityStateType aState) const
  {
    return mConstraintValidation &&
           mConstraintValidation->GetValidityState(aState);
  }

  // Weak reference to owner which will call Disconnect() when being destroyed.
  nsIConstraintValidation*       mConstraintValidation;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ValidityState_h

