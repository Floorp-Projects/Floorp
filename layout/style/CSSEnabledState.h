/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * enum for whether a CSS feature (property, pseudo-class, etc.) is
 * enabled in a specific context
 */

#ifndef mozilla_CSSEnabledState_h
#define mozilla_CSSEnabledState_h

#include "mozilla/TypedEnumBits.h"

namespace mozilla {

enum class CSSEnabledState
{
  // The default CSSEnabledState: only enable what's enabled for all
  // content, given the current values of preferences.
  eForAllContent = 0,
  // Enable features available in UA sheets.
  eInUASheets = 0x01,
  // Enable features available in chrome code.
  eInChrome = 0x02,
  // Special value to unconditionally enable everything. This implies
  // all the bits above, but is strictly more than just their OR-ed
  // union. This just skips any test so a feature will be enabled even
  // if it would have been disabled with all the bits above set.
  eIgnoreEnabledState = 0xff
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CSSEnabledState)

} // namespace mozilla

#endif // mozilla_CSSEnabledState_h
