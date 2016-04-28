/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gfxConfig.h"

namespace mozilla {
namespace gfx {

static gfxConfig sConfig;

/* static */ FeatureState&
gfxConfig::GetFeature(Feature aFeature)
{
  return sConfig.GetState(aFeature);
}

/* static */ bool
gfxConfig::IsEnabled(Feature aFeature)
{
  const FeatureState& state = sConfig.GetState(aFeature);
  return state.IsEnabled();
}

/* static */ bool
gfxConfig::IsDisabledByDefault(Feature aFeature)
{
  const FeatureState& state = sConfig.GetState(aFeature);
  return state.DisabledByDefault();
}

/* static */ bool
gfxConfig::IsForcedOnByUser(Feature aFeature)
{
  const FeatureState& state = sConfig.GetState(aFeature);
  return state.IsForcedOnByUser();
}

/* static */ FeatureStatus
gfxConfig::GetValue(Feature aFeature)
{
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
  return state.SetDefault(aEnable, aDisableStatus, aDisableMessage);
}

/* static */ void
gfxConfig::DisableByDefault(Feature aFeature,
                            FeatureStatus aDisableStatus,
                            const char* aDisableMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  state.DisableByDefault(aDisableStatus, aDisableMessage);
}

/* static */ void
gfxConfig::EnableByDefault(Feature aFeature)
{
  FeatureState& state = sConfig.GetState(aFeature);
  state.EnableByDefault();
}

/* static */ bool
gfxConfig::InitOrUpdate(Feature aFeature,
                        bool aEnable,
                        FeatureStatus aDisableStatus,
                        const char* aDisableMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  return state.InitOrUpdate(aEnable, aDisableStatus, aDisableMessage);
}

/* static */ void
gfxConfig::SetFailed(Feature aFeature, FeatureStatus aStatus, const char* aMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  state.SetFailed(aStatus, aMessage);
}

/* static */ void
gfxConfig::Disable(Feature aFeature, FeatureStatus aStatus, const char* aMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  state.Disable(aStatus, aMessage);
}

/* static */ void
gfxConfig::UserEnable(Feature aFeature, const char* aMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  state.UserEnable(aMessage);
}

/* static */ void
gfxConfig::UserForceEnable(Feature aFeature, const char* aMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  state.UserForceEnable(aMessage);
}

/* static */ void
gfxConfig::UserDisable(Feature aFeature, const char* aMessage)
{
  FeatureState& state = sConfig.GetState(aFeature);
  state.UserDisable(aMessage);
}

/* static */ void
gfxConfig::Reenable(Feature aFeature, Fallback aFallback)
{
  FeatureState& state = sConfig.GetState(aFeature);
  MOZ_ASSERT(IsFeatureStatusFailure(state.GetValue()));

  const char* message = state.GetRuntimeMessage();
  EnableFallback(aFallback, message);
  state.SetRuntime(FeatureStatus::Available, nullptr);
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
