/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleGeneric.h"
#include "LocalAccessible.h"
#include "LocalAccessible-inl.h"

using namespace mozilla;
using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessibleValue::GetMaximumValue(double* aValue) {
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (Intl()->IsLocal() && Intl()->AsLocal()->IsDefunct()) {
    return NS_ERROR_FAILURE;
  }

  double value = Intl()->MaxValue();

  if (!IsNaN(value)) *aValue = value;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleValue::GetMinimumValue(double* aValue) {
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (Intl()->IsLocal() && Intl()->AsLocal()->IsDefunct()) {
    return NS_ERROR_FAILURE;
  }

  double value = Intl()->MinValue();

  if (!IsNaN(value)) *aValue = value;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleValue::GetCurrentValue(double* aValue) {
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (Intl()->IsLocal() && Intl()->AsLocal()->IsDefunct()) {
    return NS_ERROR_FAILURE;
  }

  double value = Intl()->CurValue();

  if (!IsNaN(value)) *aValue = value;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleValue::SetCurrentValue(double aValue) {
  if (!Intl()) return NS_ERROR_FAILURE;

  if (Intl()->IsLocal() && Intl()->AsLocal()->IsDefunct()) {
    return NS_ERROR_FAILURE;
  }

  if (Intl()->IsLocal()) {
    Intl()->AsLocal()->SetCurValue(aValue);
  } else {
    Intl()->AsRemote()->SetCurValue(aValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleValue::GetMinimumIncrement(double* aValue) {
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (Intl()->IsLocal() && Intl()->AsLocal()->IsDefunct()) {
    return NS_ERROR_FAILURE;
  }

  double value = Intl()->Step();

  if (!IsNaN(value)) *aValue = value;

  return NS_OK;
}
