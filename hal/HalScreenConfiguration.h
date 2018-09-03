/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HalScreenConfiguration_h
#define mozilla_HalScreenConfiguration_h

#include "mozilla/Observer.h"

namespace mozilla {
namespace hal {

// Make sure that any change to ScreenOrientation values are also made in
// mobile/android/geckoview/src/main/java/org/mozilla/gecko/GeckoScreenOrientation.java
typedef uint32_t ScreenOrientation;

static const ScreenOrientation eScreenOrientation_None               = 0;
static const ScreenOrientation eScreenOrientation_PortraitPrimary    = 1u << 0;
static const ScreenOrientation eScreenOrientation_PortraitSecondary  = 1u << 1;
static const ScreenOrientation eScreenOrientation_LandscapePrimary   = 1u << 2;
static const ScreenOrientation eScreenOrientation_LandscapeSecondary = 1u << 3;
//eScreenOrientation_Default will use the natural orientation for the deivce,
//it could be PortraitPrimary or LandscapePrimary depends on display resolution
static const ScreenOrientation eScreenOrientation_Default            = 1u << 4;

class ScreenConfiguration;
typedef Observer<ScreenConfiguration> ScreenConfigurationObserver;

} // namespace hal
} // namespace mozilla

#endif  // mozilla_HalScreenConfiguration_h

