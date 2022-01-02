/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_EME_KEY_SYSTEM_NAMES_H_
#define DOM_MEDIA_EME_KEY_SYSTEM_NAMES_H_

// Header for key system names. Keep these separate from some of our other
// EME utils because want to use these strings in contexts where other utils
// may not build correctly. Specifically at time of writing:
// - The GMP doesn't have prefs available, so we want to avoid utils that
//   touch the pref service.
// - The clear key CDM links a limited subset of what normal Fx does, so we
//   need to avoid any utils that touch things like XUL.

namespace mozilla {
// EME Key System Strings.
inline constexpr char kClearKeyKeySystemName[] = "org.w3.clearkey";
inline constexpr char kClearKeyWithProtectionQueryKeySystemName[] =
    "org.mozilla.clearkey_with_protection_query";
inline constexpr char kWidevineKeySystemName[] = "com.widevine.alpha";
}  // namespace mozilla

#endif  // DOM_MEDIA_EME_KEY_SYSTEM_NAMES_H_
