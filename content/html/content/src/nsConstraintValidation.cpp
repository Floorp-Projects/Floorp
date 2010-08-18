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

#include "nsConstraintValidation.h"

#include "nsAString.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLFormElement.h"

nsConstraintValidation::~nsConstraintValidation()
{
  if (mValidity) {
    mValidity->Disconnect();
  }
}

nsresult
nsConstraintValidation::GetValidity(nsIDOMValidityState** aValidity)
{
  if (!mValidity) {
    mValidity = new nsDOMValidityState(this);
  }

  NS_ADDREF(*aValidity = mValidity);

  return NS_OK;
}

nsresult
nsConstraintValidation::GetWillValidate(PRBool* aWillValidate,
                                        nsGenericHTMLFormElement* aElement)
{
  *aWillValidate = IsCandidateForConstraintValidation(aElement);
  return NS_OK;
}

nsresult
nsConstraintValidation::GetValidationMessage(nsAString & aValidationMessage,
                                             nsGenericHTMLFormElement* aElement)
{
  aValidationMessage.Truncate();

  if (IsCandidateForConstraintValidation(aElement) && !IsValid()) {
    if (!mCustomValidity.IsEmpty()) {
      aValidationMessage.Assign(mCustomValidity);
    } else if (IsTooLong()) {
      GetValidationMessage(aValidationMessage, VALIDATION_MESSAGE_TOO_LONG);
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
nsConstraintValidation::CheckValidity(PRBool* aValidity,
                                      nsGenericHTMLFormElement* aElement)
{
  if (!IsCandidateForConstraintValidation(aElement) || IsValid()) {
    *aValidity = PR_TRUE;
    return NS_OK;
  }

  *aValidity = PR_FALSE;

  return nsContentUtils::DispatchTrustedEvent(aElement->GetOwnerDoc(),
                                              static_cast<nsIContent*>(aElement),
                                              NS_LITERAL_STRING("invalid"),
                                              PR_FALSE, PR_TRUE);
}

nsresult
nsConstraintValidation::SetCustomValidity(const nsAString & aError)
{
  mCustomValidity.Assign(aError);
  return NS_OK;
}

PRBool
nsConstraintValidation::HasCustomError() const
{
  return !mCustomValidity.IsEmpty();
}

PRBool
nsConstraintValidation::IsValid()
{
  return !(IsValueMissing() || HasTypeMismatch() || HasPatternMismatch() ||
           IsTooLong() || HasRangeUnderflow() || HasRangeOverflow() ||
           HasStepMismatch() || HasCustomError());
}

PRBool
nsConstraintValidation::IsCandidateForConstraintValidation(nsGenericHTMLFormElement* aElement)
{
  /**
   * An element is never candidate for constraint validation if:
   * - it is disabled ;
   * - TODO: or it's ancestor is a datalist element (bug 555840).
   * We are doing these checks here to prevent writing them in every
   * |IsBarredFromConstraintValidation| function.
   */

  // At the moment, every elements which can be candidate for constraint
  // validation can be disabled. However, using |CanBeDisabled| is future-proof.
  if (aElement->CanBeDisabled() &&
      aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return PR_FALSE;
  }

  return !IsBarredFromConstraintValidation();
}

