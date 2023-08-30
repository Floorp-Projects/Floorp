/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRadioVisitor.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "nsIConstraintValidation.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsRadioVisitor, nsIRadioVisitor)

bool nsRadioSetCheckedChangedVisitor::Visit(HTMLInputElement* aRadio) {
  NS_ASSERTION(aRadio, "Visit() passed a null button!");
  aRadio->SetCheckedChangedInternal(mCheckedChanged);
  return true;
}

bool nsRadioGetCheckedChangedVisitor::Visit(HTMLInputElement* aRadio) {
  if (aRadio == mExcludeElement) {
    return true;
  }

  NS_ASSERTION(aRadio, "Visit() passed a null button!");
  *mCheckedChanged = aRadio->GetCheckedChanged();
  return false;
}

bool nsRadioSetValueMissingState::Visit(HTMLInputElement* aRadio) {
  if (aRadio == mExcludeElement) {
    return true;
  }

  aRadio->SetValidityState(
      nsIConstraintValidation::VALIDITY_STATE_VALUE_MISSING, mValidity);
  aRadio->UpdateValidityElementStates(true);
  return true;
}

bool nsRadioUpdateStateVisitor::Visit(HTMLInputElement* aRadio) {
  if (aRadio == mExcludeElement) {
    return true;
  }
  aRadio->UpdateIndeterminateState(true);
  aRadio->UpdateValidityElementStates(true);
  return true;
}
