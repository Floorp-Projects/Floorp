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
#ifdef MOZ_WMF_CDM
// A sub key system of `kWidevineKeySystem` only used in experiments.
inline constexpr char kWidevineExperimentKeySystemName[] =
    "com.widevine.alpha.experiment";
// A sub key system of `kWidevineKeySystem` only used in experiments to support
// hardware decryption with codecs that support clear lead.
inline constexpr char kWidevineExperiment2KeySystemName[] =
    "com.widevine.alpha.experiment2";
// API name used for searching GMP Wideivine L1 plugin.
inline constexpr char kWidevineExperimentAPIName[] = "windows-mf-cdm";

// https://learn.microsoft.com/en-us/playready/overview/key-system-strings
inline constexpr char kPlayReadyKeySystemName[] =
    "com.microsoft.playready.recommendation";
inline constexpr char kPlayReadyKeySystemHardware[] =
    "com.microsoft.playready.recommendation.3000";

// A sub key system of `kPlayReadyKeySystemName` only used in experiments to
// support hardware decryption with codecs that support clear lead.
inline constexpr char kPlayReadyHardwareClearLeadKeySystemName[] =
    "com.microsoft.playready.recommendation.3000.clearlead";
#endif
}  // namespace mozilla

#endif  // DOM_MEDIA_EME_KEY_SYSTEM_NAMES_H_
