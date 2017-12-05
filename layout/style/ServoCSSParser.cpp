/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS parsing utility functions */

#include "ServoCSSParser.h"

using namespace mozilla;

/* static */ bool
ServoCSSParser::IsValidCSSColor(const nsAString& aValue)
{
  return Servo_IsValidCSSColor(&aValue);
}

/* static */ bool
ServoCSSParser::ComputeColor(ServoStyleSet* aStyleSet,
                             nscolor aCurrentColor,
                             const nsAString& aValue,
                             nscolor* aResultColor)
{
  return Servo_ComputeColor(aStyleSet ? aStyleSet->RawSet() : nullptr,
                            aCurrentColor, &aValue, aResultColor);
}

/* static */ bool
ServoCSSParser::ParseIntersectionObserverRootMargin(const nsAString& aValue,
                                                    nsCSSRect* aResult)
{
  return Servo_ParseIntersectionObserverRootMargin(&aValue, aResult);
}
