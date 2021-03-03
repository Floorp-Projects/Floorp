/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleGeneric.h"
#include "LocalAccessible.h"

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

  double value;
  if (Intl()->IsLocal()) {
    value = Intl()->AsLocal()->MaxValue();
  } else {
    value = Intl()->AsRemote()->MaxValue();
  }

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

  double value;
  if (Intl()->IsLocal()) {
    value = Intl()->AsLocal()->MinValue();
  } else {
    value = Intl()->AsRemote()->MinValue();
  }

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

  double value;
  if (Intl()->IsLocal()) {
    value = Intl()->AsLocal()->CurValue();
  } else {
    value = Intl()->AsRemote()->CurValue();
  }

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

  double value;
  if (Intl()->IsLocal()) {
    value = Intl()->AsLocal()->Step();
  } else {
    value = Intl()->AsRemote()->Step();
  }

  if (!IsNaN(value)) *aValue = value;

  return NS_OK;
}
