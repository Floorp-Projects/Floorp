/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScreenOrientation_h
#define mozilla_dom_ScreenOrientation_h

namespace mozilla {
namespace dom {

// Make sure that any change here is also made in
// * mobile/android/base/GeckoScreenOrientationListener.java
// * embedding/android/GeckoScreenOrientationListener.java
typedef uint32_t ScreenOrientation;

static const ScreenOrientation eScreenOrientation_None               = 0;
static const ScreenOrientation eScreenOrientation_PortraitPrimary    = 1u << 0;
static const ScreenOrientation eScreenOrientation_PortraitSecondary  = 1u << 1;
static const ScreenOrientation eScreenOrientation_LandscapePrimary   = 1u << 2;
static const ScreenOrientation eScreenOrientation_LandscapeSecondary = 1u << 3;

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScreenOrientation_h
