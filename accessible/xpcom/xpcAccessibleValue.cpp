/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleGeneric.h"
#include "Accessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessibleValue::GetMaximumValue(double* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (Intl().IsNull())
    return NS_ERROR_FAILURE;

  if (Intl().IsAccessible() && Intl().AsAccessible()->IsDefunct())
    return NS_ERROR_FAILURE;

  double value;
  if (Intl().IsAccessible()) {
    value = Intl().AsAccessible()->MaxValue();
  } else { 
    value = Intl().AsProxy()->MaxValue();
  }

  if (!IsNaN(value))
    *aValue = value;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleValue::GetMinimumValue(double* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (Intl().IsNull())
    return NS_ERROR_FAILURE;

  if (Intl().IsAccessible() && Intl().AsAccessible()->IsDefunct())
    return NS_ERROR_FAILURE;

  double value;
  if (Intl().IsAccessible()) {
    value = Intl().AsAccessible()->MinValue();
  } else { 
    value = Intl().AsProxy()->MinValue();
  }

  if (!IsNaN(value))
    *aValue = value;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleValue::GetCurrentValue(double* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (Intl().IsNull())
    return NS_ERROR_FAILURE;

  if (Intl().IsAccessible() && Intl().AsAccessible()->IsDefunct())
    return NS_ERROR_FAILURE;

  double value;
  if (Intl().IsAccessible()) {
    value = Intl().AsAccessible()->CurValue();
  } else { 
    value = Intl().AsProxy()->CurValue();
  }

  if (!IsNaN(value))
    *aValue = value;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleValue::SetCurrentValue(double aValue)
{
  if (Intl().IsNull())
    return NS_ERROR_FAILURE;

  if (Intl().IsAccessible() && Intl().AsAccessible()->IsDefunct())
    return NS_ERROR_FAILURE;

  if (Intl().IsAccessible()) {
    Intl().AsAccessible()->SetCurValue(aValue);
  } else { 
    Intl().AsProxy()->SetCurValue(aValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleValue::GetMinimumIncrement(double* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (Intl().IsNull())
    return NS_ERROR_FAILURE;

  if (Intl().IsAccessible() && Intl().AsAccessible()->IsDefunct())
    return NS_ERROR_FAILURE;

  double value;
  if (Intl().IsAccessible()) {
    value = Intl().AsAccessible()->Step();
  } else { 
    value = Intl().AsProxy()->Step();
  }

  if (!IsNaN(value))
    *aValue = value;

  return NS_OK;
}
