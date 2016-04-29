/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gfxConfig.h"

namespace mozilla {
namespace gfx {

static gfxConfig sConfig;

// When querying the current stae of a feature, assert that we initialized some
// default value for the feature already. This ensures that the config/init
// order is correct.
/* static */ void
gfxConfig::AssertStatusInitialized(Feature aFeature)
{
  sConfig.GetState(aFeature).AssertInitialized();
}

/* static */ bool
gfxConfig::IsEnabled(Feature aFeature)
{
  AssertStatusInitialized(aFeature);

  FeatureStatus status = GetValue(aFeature);
  return status == FeatureStatus::Available || status == FeatureStatus::ForceEnabled;
}

/* static */ bool
gfxConfig::IsDisabledByDefault(Feature aFeature)
{
  AssertStatusInitialized(aFeature);

  const FeatureState& state = sConfig.GetState(aFeature);
  return state.DisabledByDefault();
}

/* static */ bool
gfxConfig::IsForcedOnByUser(Feature aFeature)
{
  AssertStatusInitialized(aFeature);

  const FeatureState& state = sConfig.GetState(aFeature);
  return state.IsForcedOnByUser();
}

/* static */ FeatureStatus
gfxConfig::GetValue(Feature aFeature)
{
  AssertStatusInitialized(aFeature);

  const FeatureState& state = sConfig.GetState(aFeature);
  return state.GetValue();
}

/* static */ bool
gfxConfig::SetDefault(Feature aFeature,
                      bool aEnable,
                      FeatureStatus aDisableStatus,
                      const char* aDisableMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  if (!aEnable) {
    state.DisableByDefault(aDisableStatus, aDisableMessage);
    return false;
  }
  state.EnableByDefault();
  return true;
}

/* static */ bool
gfxConfig::InitOrUpdate(Feature aFeature,
                        bool aEnable,
                        FeatureStatus aDisableStatus,
                        const char* aDisableMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  if (!state.IsInitialized()) {
    return SetDefault(aFeature, aEnable, aDisableStatus, aDisableMessage);
  }
  return MaybeSetFailed(aFeature, aEnable, aDisableStatus, aDisableMessage);
}

/* static */ void
gfxConfig::SetFailed(Feature aFeature, FeatureStatus aStatus, const char* aMessage)
{
  AssertStatusInitialized(aFeature);

  // We should never bother setting a runtime status to "enabled," since it could
  // override an explicit user decision to disable it.
  MOZ_ASSERT(IsFeatureStatusFailure(aStatus));

  FeatureState& state = sConfig.GetState(aFeature);
  state.SetRuntime(aStatus, aMessage);
}

/* static */ void
gfxConfig::Disable(Feature aFeature, FeatureStatus aStatus, const char* aMessage)
{
  AssertStatusInitialized(aFeature);

  // We should never bother setting an environment status to "enabled," since
  // it could override an explicit user decision to disable it.
  MOZ_ASSERT(IsFeatureStatusFailure(aStatus));

  FeatureState& state = sConfig.GetState(aFeature);
  state.SetEnvironment(aStatus, aMessage);
}

/* static */ void
gfxConfig::UserEnable(Feature aFeature, const char* aMessage)
{
  AssertStatusInitialized(aFeature);

  FeatureState& state = sConfig.GetState(aFeature);
  state.SetUser(FeatureStatus::Available, aMessage);
}
/* static */ void
gfxConfig::UserForceEnable(Feature aFeature, const char* aMessage)
{
  AssertStatusInitialized(aFeature);

  FeatureState& state = sConfig.GetState(aFeature);
  state.SetUser(FeatureStatus::ForceEnabled, aMessage);
}

/* static */ void
gfxConfig::UserDisable(Feature aFeature, const char* aMessage)
{
  AssertStatusInitialized(aFeature);

  FeatureState& state = sConfig.GetState(aFeature);
  state.SetUser(FeatureStatus::Disabled, aMessage);
}

/* static */ bool
gfxConfig::UseFallback(Fallback aFallback)
{
  return sConfig.UseFallbackImpl(aFallback);
}

/* static */ void
gfxConfig::EnableFallback(Fallback aFallback, const char* aMessage)
{
  // Ignore aMessage for now.
  sConfig.EnableFallbackImpl(aFallback);
}

bool
gfxConfig::UseFallbackImpl(Fallback aFallback) const
{
  return !!(mFallbackBits & (uint64_t(1) << uint64_t(aFallback)));
}

void
gfxConfig::EnableFallbackImpl(Fallback aFallback)
{
  mFallbackBits |= (uint64_t(1) << uint64_t(aFallback));
}

} // namespace gfx
} // namespace mozilla
