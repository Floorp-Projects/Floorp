/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gfxFeature.h"
#include "prprf.h"

namespace mozilla {
namespace gfx {

FeatureStatus
FeatureState::GetValue() const
{
  if (mRuntime.mStatus != FeatureStatus::Unused) {
    return mRuntime.mStatus;
  }
  if (mUser.mStatus == FeatureStatus::ForceEnabled) {
    return FeatureStatus::ForceEnabled;
  }
  if (mEnvironment.mStatus != FeatureStatus::Unused) {
    return mEnvironment.mStatus;
  }
  if (mUser.mStatus != FeatureStatus::Unused) {
    return mUser.mStatus;
  }
  return mDefault.mStatus;
}

bool
FeatureState::DisabledByDefault() const
{
  return mDefault.mStatus != FeatureStatus::Available;
}

bool
FeatureState::IsForcedOnByUser() const
{
  return mUser.mStatus == FeatureStatus::ForceEnabled;
}

void
FeatureState::EnableByDefault()
{
  // User/runtime decisions should not have been made yet.
  MOZ_ASSERT(mUser.mStatus == FeatureStatus::Unused);
  MOZ_ASSERT(mRuntime.mStatus == FeatureStatus::Unused);

  mDefault.Set(FeatureStatus::Available);
}

void
FeatureState::DisableByDefault(FeatureStatus aStatus, const char* aMessage)
{
  // User/runtime decisions should not have been made yet.
  MOZ_ASSERT(mUser.mStatus == FeatureStatus::Unused);
  MOZ_ASSERT(mRuntime.mStatus == FeatureStatus::Unused);

  mDefault.Set(aStatus, aMessage);
}

void
FeatureState::SetUser(FeatureStatus aStatus, const char* aMessage)
{
  // Default decision must have been made, but not runtime or environment.
  MOZ_ASSERT(mDefault.mStatus != FeatureStatus::Unused);
  MOZ_ASSERT(mEnvironment.mStatus == FeatureStatus::Unused);
  MOZ_ASSERT(mRuntime.mStatus == FeatureStatus::Unused);

  mUser.Set(aStatus, aMessage);
}

void
FeatureState::SetEnvironment(FeatureStatus aStatus, const char* aMessage)
{
  // Default decision must have been made, but not runtime.
  MOZ_ASSERT(mDefault.mStatus != FeatureStatus::Unused);
  MOZ_ASSERT(mRuntime.mStatus == FeatureStatus::Unused);

  mEnvironment.Set(aStatus, aMessage);
}

void
FeatureState::SetRuntime(FeatureStatus aStatus, const char* aMessage)
{
  // Default decision must have been made.
  MOZ_ASSERT(mDefault.mStatus != FeatureStatus::Unused);

  mRuntime.Set(aStatus, aMessage);
}

void
FeatureState::Instance::Set(FeatureStatus aStatus, const char* aMessage /* = nullptr */)
{
  mStatus = aStatus;
  if (aMessage) {
    PR_snprintf(mMessage, sizeof(mMessage), "%s", aMessage);
  }
}

} // namespace gfx
} // namespace mozilla
