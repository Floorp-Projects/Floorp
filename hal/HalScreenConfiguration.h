/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HalScreenConfiguration_h
#define mozilla_HalScreenConfiguration_h

#include "mozilla/Observer.h"
#include "mozilla/TypedEnumBits.h"

// Undo X11/X.h's definition of None
#undef None

namespace mozilla::hal {

// Make sure that any change to ScreenOrientation values are also made in
// mobile/android/geckoview/src/main/java/org/mozilla/gecko/GeckoScreenOrientation.java
enum class ScreenOrientation : uint32_t {
  None = 0,
  PortraitPrimary = 1u << 0,
  PortraitSecondary = 1u << 1,
  LandscapePrimary = 1u << 2,
  LandscapeSecondary = 1u << 3,
  // Default will use the natural orientation for the device, it could be
  // PortraitPrimary or LandscapePrimary depends on display resolution
  Default = 1u << 4,
};

constexpr auto kAllScreenOrientationBits = ScreenOrientation((1 << 5) - 1);

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ScreenOrientation);

}  // namespace mozilla::hal

#endif  // mozilla_HalScreenConfiguration_h
