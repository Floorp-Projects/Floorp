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
#include "mozilla/Function.h"

namespace mozilla {
namespace gfx {

// Manages the history and state of a graphics feature. The flow of a feature
// is:
//   - A default value, set by all.js, gfxPrefs, or gfxPlatform.
//   - A user value, set by an external value or user pref.
//   - An environment value, determined by system/hardware factors or nsIGfxInfo.
//   - A runtime value, determined by any failures encountered after enabling
//     the feature.
//
// Each state change for a feature is recorded in this class.
class gfxConfig
{
public:
  // Return the full state history of a feature.
  static FeatureState& GetFeature(Feature aFeature);

  // Query whether a parameter is enabled, taking into account any user or
  // runtime overrides. The algorithm works as follow:
  //
  //  1. If a runtime decision disabled the feature, return false.
  //  2. If the user force-enabled the feature, return true.
  //  3. If the environment disabled the feature, return false.
  //  4. If the user specified a decision, return it.
  //  5. Return the base setting for the feature.
  static bool IsEnabled(Feature aFeature);

  // Query the history of a parameter. ForcedOnByUser returns whether or not
  // the user specifically used a "force" preference to enable the parameter.
  // IsDisabledByDefault returns whether or not the initial status of the
  // feature, before adding user prefs and runtime decisions, was disabled.
  static bool IsForcedOnByUser(Feature aFeature);
  static bool IsDisabledByDefault(Feature aFeature);

  // Query the status value of a parameter. This is computed similar to
  // IsEnabled:
  //
  //  1. If a runtime failure was set, return it.
  //  2. If the user force-enabled the feature, return ForceEnabled.
  //  3. If an environment status was set, return it.
  //  4. If a user status was set, return it.
  //  5. Return the default status.
  static FeatureStatus GetValue(Feature aFeature);

  // Initialize the base value of a parameter. The return value is aEnable.
  static bool SetDefault(Feature aFeature,
                         bool aEnable,
                         FeatureStatus aDisableStatus,
                         const char* aDisableMessage);
  static void DisableByDefault(Feature aFeature,
                               FeatureStatus aDisableStatus,
                               const char* aDisableMessage,
                               const nsACString& aFailureId = EmptyCString());
  static void EnableByDefault(Feature aFeature);

  // Set a environment status that overrides both the default and user
  // statuses; this should be used to disable features based on system
  // or hardware problems that can be determined up-front. The only
  // status that can override this decision is the user force-enabling
  // the feature.
  static void Disable(Feature aFeature,
                      FeatureStatus aStatus,
                      const char* aMessage,
                      const nsACString& aFailureId = EmptyCString());

  // Given a preference name, infer the default value and whether or not the
  // user has changed it. |aIsEnablePref| specifies whether or not the pref
  // is intended to enable a feature (true), or disable it (false).
  static void SetDefaultFromPref(Feature aFeature,
                                 const char* aPrefName,
                                 bool aIsEnablePref,
                                 bool aDefaultValue);

  // Disable a parameter based on a runtime decision. This permanently
  // disables the feature, since runtime decisions override all other
  // decisions.
  static void SetFailed(Feature aFeature,
                        FeatureStatus aStatus,
                        const char* aMessage,
                        const nsACString& aFailureId = EmptyCString());

  // Force a feature to be disabled permanently. This is the same as
  // SetFailed(), but the name may be clearer depending on the context.
  static void ForceDisable(Feature aFeature,
                           FeatureStatus aStatus,
                           const char* aMessage,
                           const nsACString& aFailureId = EmptyCString())
  {
    SetFailed(aFeature, aStatus, aMessage, aFailureId);
  }

  // Convenience helpers for SetFailed().
  static bool MaybeSetFailed(Feature aFeature,
                             bool aEnable,
                             FeatureStatus aDisableStatus,
                             const char* aDisableMessage,
                             const nsACString& aFailureId = EmptyCString())
  {
    if (!aEnable) {
      SetFailed(aFeature, aDisableStatus, aDisableMessage, aFailureId);
      return false;
    }
    return true;
  }

  // Convenience helper for SetFailed().
  static bool MaybeSetFailed(Feature aFeature,
                             FeatureStatus aStatus,
                             const char* aDisableMessage,
                             const nsACString& aFailureId = EmptyCString())
  {
    return MaybeSetFailed(
      aFeature,
      (aStatus != FeatureStatus::Available &&
       aStatus != FeatureStatus::ForceEnabled),
      aStatus,
      aDisableMessage, aFailureId);
  }

  // Re-enables a feature that was previously disabled, by attaching it to a
  // fallback. The fallback inherits the message that was used for disabling
  // the feature. This can be used, for example, when D3D11 fails at runtime
  // but we acquire a second, successful device with WARP.
  static void Reenable(Feature aFeature, Fallback aFallback);

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
  static void UserDisable(Feature aFeature, const char* aMessage, const nsACString& aFailureId = EmptyCString());

  // Query whether a fallback has been toggled.
  static bool UseFallback(Fallback aFallback);

  // Enable a fallback.
  static void EnableFallback(Fallback aFallback, const char* aMessage);

  // Run a callback for each initialized FeatureState.
  typedef mozilla::function<void(const char* aName,
                                 const char* aDescription,
                                 FeatureState& aFeature)> FeatureIterCallback;
  static void ForEachFeature(const FeatureIterCallback& aCallback);

  // Run a callback for each enabled fallback.
  typedef mozilla::function<void(const char* aName, const char* aMsg)> 
    FallbackIterCallback;
  static void ForEachFallback(const FallbackIterCallback& aCallback);

  // Get the most descriptive failure id message for this feature.
  static const nsACString& GetFailureId(Feature aFeature);

  static void Init();
  static void Shutdown();

private:
  void ForEachFallbackImpl(const FallbackIterCallback& aCallback);

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
  void EnableFallbackImpl(Fallback aFallback, const char* aMessage);

private:
  static const size_t kNumFeatures = size_t(Feature::NumValues);
  static const size_t kNumFallbacks = size_t(Fallback::NumValues);

private:
  FeatureState mFeatures[kNumFeatures];
  uint64_t mFallbackBits;

private:
  struct FallbackLogEntry {
    Fallback mFallback;
    char mMessage[80];
  };

  FallbackLogEntry mFallbackLog[kNumFallbacks];
  size_t mNumFallbackLogEntries;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_config_gfxConfig_h
