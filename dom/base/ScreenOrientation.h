/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScreenOrientation_h
#define mozilla_dom_ScreenOrientation_h

#include "ipc/IPCMessageUtils.h"

namespace mozilla {
namespace dom {

// Make sure that any change here is also made in
// * mobile/android/base/GeckoScreenOrientationListener.java
// * embedding/android/GeckoScreenOrientationListener.java
typedef uint32_t ScreenOrientation;

static const ScreenOrientation eScreenOrientation_None               = 0;
static const ScreenOrientation eScreenOrientation_PortraitPrimary    = 1;  // 00000001
static const ScreenOrientation eScreenOrientation_PortraitSecondary  = 2;  // 00000010
static const ScreenOrientation eScreenOrientation_Portrait           = 3;  // 00000011
static const ScreenOrientation eScreenOrientation_LandscapePrimary   = 4;  // 00000100
static const ScreenOrientation eScreenOrientation_LandscapeSecondary = 8;  // 00001000
static const ScreenOrientation eScreenOrientation_Landscape          = 12; // 00001100

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScreenOrientation_h
