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

#include "nsIConstraintValidation.h"

#include "nsAString.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLFormElement.h"
#include "nsDOMValidityState.h"
#include "nsIFormControl.h"
#include "nsContentUtils.h"

const PRUint16 nsIConstraintValidation::sContentSpecifiedMaxLengthMessage = 256;

nsIConstraintValidation::nsIConstraintValidation()
  : mValidityBitField(0)
  , mValidity(nsnull)
  // By default, all elements are subjects to constraint validation.
  , mBarredFromConstraintValidation(false)
{
}

nsIConstraintValidation::~nsIConstraintValidation()
{
  if (mValidity) {
    mValidity->Disconnect();
  }
}

nsresult
nsIConstraintValidation::GetValidity(nsIDOMValidityState** aValidity)
{
  if (!mValidity) {
    mValidity = new nsDOMValidityState(this);
  }

  NS_ADDREF(*aValidity = mValidity);

  return NS_OK;
}

NS_IMETHODIMP
nsIConstraintValidation::GetValidationMessage(nsAString& aValidationMessage)
{
  aValidationMessage.Truncate();

  if (IsCandidateForConstraintValidation() && !IsValid()) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(this);
    NS_ASSERTION(content, "This class should be inherited by HTML elements only!");

    nsAutoString authorMessage;
    content->GetAttr(kNameSpaceID_None, nsGkAtoms::x_moz_errormessage,
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
    } else if (GetValidityState(VALIDITY_STATE_VALUE_MISSING)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_VALUE_MISSING);
    } else if (GetValidityState(VALIDITY_STATE_TYPE_MISMATCH)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_TYPE_MISMATCH);
    } else if (GetValidityState(VALIDITY_STATE_PATTERN_MISMATCH)) {
      GetValidationMessage(aValidationMessage, VALIDITY_STATE_PATTERN_MISMATCH);
    } else {
      // TODO: The other messages have not been written
      // because related constraint validation are not implemented yet.
      // We should not be here.
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    aValidationMessage.Truncate();
  }

  return NS_OK;
}

nsresult
nsIConstraintValidation::CheckValidity(bool* aValidity)
{
  if (!IsCandidateForConstraintValidation() || IsValid()) {
    *aValidity = true;
    return NS_OK;
  }

  *aValidity = false;

  nsCOMPtr<nsIContent> content = do_QueryInterface(this);
  NS_ASSERTION(content, "This class should be inherited by HTML elements only!");

  return nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(), content,
                                              NS_LITERAL_STRING("invalid"),
                                              false, true);
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

  // Inform the form element if our validity has changed.
  if (previousValidity != IsValid() && IsCandidateForConstraintValidation()) {
    nsCOMPtr<nsIFormControl> formCtrl = do_QueryInterface(this);
    NS_ASSERTION(formCtrl, "This interface should be used by form elements!");

    nsHTMLFormElement* form =
      static_cast<nsHTMLFormElement*>(formCtrl->GetFormElement());
    if (form) {
      form->UpdateValidity(IsValid());
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

  // Inform the form element if our status regarding constraint validation
  // is going to change.
  if (!IsValid() && previousBarred != mBarredFromConstraintValidation) {
    nsCOMPtr<nsIFormControl> formCtrl = do_QueryInterface(this);
    NS_ASSERTION(formCtrl, "This interface should be used by form elements!");

    nsHTMLFormElement* form =
      static_cast<nsHTMLFormElement*>(formCtrl->GetFormElement());
    if (form) {
      // If the element is going to be barred from constraint validation,
      // we can inform the form that we are now valid.
      // Otherwise, we are now invalid.
      form->UpdateValidity(aBarred);
    }
  }
}

