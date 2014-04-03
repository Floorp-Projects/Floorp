/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRadioVisitor.h"
#include "nsAutoPtr.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "nsIConstraintValidation.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS1(nsRadioVisitor, nsIRadioVisitor)

bool
nsRadioSetCheckedChangedVisitor::Visit(nsIFormControl* aRadio)
{
  nsRefPtr<HTMLInputElement> radio =
    static_cast<HTMLInputElement*>(aRadio);
  NS_ASSERTION(radio, "Visit() passed a null button!");

  radio->SetCheckedChangedInternal(mCheckedChanged);
  return true;
}

bool
nsRadioGetCheckedChangedVisitor::Visit(nsIFormControl* aRadio)
{
  if (aRadio == mExcludeElement) {
    return true;
  }

  nsRefPtr<HTMLInputElement> radio =
    static_cast<HTMLInputElement*>(aRadio);
  NS_ASSERTION(radio, "Visit() passed a null button!");

  *mCheckedChanged = radio->GetCheckedChanged();
  return false;
}

bool
nsRadioSetValueMissingState::Visit(nsIFormControl* aRadio)
{
  if (aRadio == mExcludeElement) {
    return true;
  }

  HTMLInputElement* input = static_cast<HTMLInputElement*>(aRadio);

  input->SetValidityState(nsIConstraintValidation::VALIDITY_STATE_VALUE_MISSING,
                          mValidity);

  input->UpdateState(true);

  return true;
}

