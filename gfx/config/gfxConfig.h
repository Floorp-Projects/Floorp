/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_gfxConfig_h
#define mozilla_gfx_config_gfxConfig_h

#include "gfxFeature.h"
#include "gfxFallback.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace gfx {

class gfxConfig
{
public:
  // Query whether a parameter is enabled, taking into account any user or
  // runtime overrides. The algorithm works as follow:
  //
  //  1. If a runtime decision disabled the parameter, return false.
  //  2. If a user decision enabled or disabled the parameter, return true or
  //     false accordingly.
  //  3. Return whether or not the base value is enabled or disabled.
  static bool IsEnabled(Feature aFeature);

  // Query the history of a parameter. ForcedOnByUser returns whether or not
  // the user specifically used a "force" preference to enable the parameter.
  // IsDisabledByDefault returns whether or not the initial status of the
  // feature, before adding user prefs and runtime decisions, was disabled.
  static bool IsForcedOnByUser(Feature aFeature);
  static bool IsDisabledByDefault(Feature aFeature);

  // Query the raw computed status value of a parameter.
  static FeatureStatus GetValue(Feature aFeature);

  // Initialize the base value of a parameter. The return value is aEnable.
  static bool SetDefault(Feature aFeature,
                         bool aEnable,
                         FeatureStatus aDisableStatus,
                         const char* aDisableMessage);

  // Disable a parameter based on a runtime decision.
  static void Disable(Feature aFeature,
                      FeatureStatus aStatus,
                      const char* aMessage);

  // Convenience helper for Disable().
  static bool UpdateIfFailed(Feature aFeature,
                             bool aEnable,
                             FeatureStatus aDisableStatus,
                             const char* aDisableMessage)
  {
    if (!aEnable) {
      Disable(aFeature, aDisableStatus, aDisableMessage);
      return false;
    }
    return true;
  }

  // Same as SetDefault, except if the feature already has a default value
  // set, the new value will be set as a runtime value. This is useful for
  // when the base value can change (for example, via an update from the
  // parent process).
  static bool InitOrUpdate(Feature aFeature,
                           bool aEnable,
                           FeatureStatus aDisableStatus,
                           const char* aDisableMessage);

  // Set a user status that overrides the base value (but not runtime value)
  // of a parameter.
  static void UserEnable(Feature aFeature, const char* aMessage);
  static void UserForceEnable(Feature aFeature, const char* aMessage);
  static void UserDisable(Feature aFeature, const char* aMessage);

  // Query whether a fallback has been toggled.
  static bool UseFallback(Fallback aFallback);

  // Enable a fallback.
  static void EnableFallback(Fallback aFallback, const char* aMessage);

private:
  FeatureState& GetState(Feature aFeature) {
    MOZ_ASSERT(size_t(aFeature) < kNumFeatures);
    return mFeatures[size_t(aFeature)];
  }
  const FeatureState& GetState(Feature aFeature) const {
    MOZ_ASSERT(size_t(aFeature) < kNumFeatures);
    return mFeatures[size_t(aFeature)];
  }

  bool UseFallbackImpl(Fallback aFallback) const;
  void EnableFallbackImpl(Fallback aFallback);

  static void AssertStatusInitialized(Feature aFeature);

private:
  static const size_t kNumFeatures = size_t(Feature::NumValues);

private:
  FeatureState mFeatures[kNumFeatures];
  uint64_t mFallbackBits;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_config_gfxConfig_h
