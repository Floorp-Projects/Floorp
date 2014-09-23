/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleApplication.h"

#include "ApplicationAccessible.h"

using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessibleApplication::GetAppName(nsAString& aName)
{
  aName.Truncate();

  if (static_cast<ApplicationAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<ApplicationAccessible*>(this)->AppName(aName);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleApplication::GetAppVersion(nsAString& aVersion)
{
  aVersion.Truncate();

  if (static_cast<ApplicationAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<ApplicationAccessible*>(this)->AppVersion(aVersion);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleApplication::GetPlatformName(nsAString& aName)
{
  aName.Truncate();

  if (static_cast<ApplicationAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<ApplicationAccessible*>(this)->PlatformName(aName);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleApplication::GetPlatformVersion(nsAString& aVersion)
{
  aVersion.Truncate();

  if (static_cast<ApplicationAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<ApplicationAccessible*>(this)->PlatformVersion(aVersion);
  return NS_OK;
}
