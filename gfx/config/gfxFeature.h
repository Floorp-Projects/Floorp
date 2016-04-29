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

namespace mozilla {
namespace gfx {

#define GFX_FEATURE_MAP(_)                                                        \
  /* Name,                        Type,         Description */                    \
  _(HW_COMPOSITING,               Feature,      "Compositing")                    \
  /* Add new entries above this comment */

enum class Feature : uint32_t {
#define MAKE_ENUM(name, type, desc) name,
  GFX_FEATURE_MAP(MAKE_ENUM)
#undef MAKE_ENUM
  NumValues
};

class FeatureState
{
 public:
  FeatureStatus GetValue() const;

  void EnableByDefault();
  void DisableByDefault(FeatureStatus aStatus, const char* aMessage);
  void SetUser(FeatureStatus aStatus, const char* aMessage);
  void SetRuntime(FeatureStatus aStatus, const char* aMessage);
  bool IsForcedOnByUser() const;
  bool DisabledByDefault() const;

  void AssertInitialized() const {
    MOZ_ASSERT(mDefault.mStatus != FeatureStatus::Unused);
  }

 private:
  struct Instance {
    char mMessage[64];
    FeatureStatus mStatus;

    void Set(FeatureStatus aStatus, const char* aMessage = nullptr);
  };

  // The default state is the state we decide on startup, based on default
  // the system, environment, and default preferences.
  //
  // The user state factors in any changes to preferences that the user made.
  //
  // The runtime state factors in any problems discovered at runtime.
  Instance mDefault;
  Instance mUser;
  Instance mRuntime;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_config_gfxFeature_h
