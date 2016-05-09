/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_gfxFeature_h
#define mozilla_gfx_config_gfxFeature_h

#include <stdint.h>
#include "gfxTelemetry.h"
#include "mozilla/Assertions.h"
#include "mozilla/Function.h"

namespace mozilla {
namespace gfx {

#define GFX_FEATURE_MAP(_)                                                        \
  /* Name,                        Type,         Description */                    \
  _(HW_COMPOSITING,               Feature,      "Compositing")                    \
  _(D3D11_COMPOSITING,            Feature,      "Direct3D11 Compositing")         \
  _(D3D9_COMPOSITING,             Feature,      "Direct3D9 Compositing")          \
  _(DIRECT2D,                     Feature,      "Direct2D")                       \
  /* Add new entries above this comment */

enum class Feature : uint32_t {
#define MAKE_ENUM(name, type, desc) name,
  GFX_FEATURE_MAP(MAKE_ENUM)
#undef MAKE_ENUM
  NumValues
};

class FeatureState
{
  friend class gfxConfig;

 public:
  bool IsEnabled() const;
  FeatureStatus GetValue() const;

  void EnableByDefault();
  void DisableByDefault(FeatureStatus aStatus, const char* aMessage);
  bool SetDefault(bool aEnable, FeatureStatus aDisableStatus, const char* aDisableMessage);
  bool InitOrUpdate(bool aEnable,
                    FeatureStatus aDisableStatus,
                    const char* aMessage);
  void SetDefaultFromPref(const char* aPrefName,
                          bool aIsEnablePref,
                          bool aDefaultValue);
  void UserEnable(const char* aMessage);
  void UserForceEnable(const char* aMessage);
  void UserDisable(const char* aMessage);
  void Disable(FeatureStatus aStatus, const char* aMessage);
  void ForceDisable(FeatureStatus aStatus, const char* aMessage) {
    SetFailed(aStatus, aMessage);
  }
  void SetFailed(FeatureStatus aStatus, const char* aMessage);
  bool MaybeSetFailed(bool aEnable, FeatureStatus aStatus, const char* aMessage);
  bool MaybeSetFailed(FeatureStatus aStatus, const char* aMessage);

  // aType is "base", "user", "env", or "runtime".
  // aMessage may be null.
  typedef mozilla::function<void(const char* aType,
                                 FeatureStatus aStatus,
                                 const char* aMessage)> StatusIterCallback;
  void ForEachStatusChange(const StatusIterCallback& aCallback) const;

 private:
  void SetUser(FeatureStatus aStatus, const char* aMessage);
  void SetEnvironment(FeatureStatus aStatus, const char* aMessage);
  void SetRuntime(FeatureStatus aStatus, const char* aMessage);
  bool IsForcedOnByUser() const;
  bool DisabledByDefault() const;
  const char* GetRuntimeMessage() const;
  bool IsInitialized() const {
    return mDefault.IsInitialized();
  }

  void AssertInitialized() const {
    MOZ_ASSERT(IsInitialized());
  }

 private:
  struct Instance {
    char mMessage[64];
    FeatureStatus mStatus;

    void Set(FeatureStatus aStatus, const char* aMessage = nullptr);
    bool IsInitialized() const {
      return mStatus != FeatureStatus::Unused;
    }
    const char* MessageOrNull() const {
      return mMessage[0] != '\0' ? mMessage : nullptr;
    }
    const char* Message() const {
      MOZ_ASSERT(MessageOrNull());
      return mMessage;
    }
  };

  // The default state is the state we decide on startup, based on the operating
  // system or a base preference.
  //
  // The user state factors in any changes to preferences that the user made.
  //
  // The environment state factors in any additional decisions made, such as
  // availability or blacklisting.
  //
  // The runtime state factors in any problems discovered at runtime.
  Instance mDefault;
  Instance mUser;
  Instance mEnvironment;
  Instance mRuntime;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_config_gfxFeature_h
