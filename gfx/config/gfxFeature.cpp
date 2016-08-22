/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFeature.h"

#include "mozilla/Preferences.h"
#include "mozilla/Sprintf.h"
#include "nsString.h"

namespace mozilla {
namespace gfx {

bool
FeatureState::IsEnabled() const
{
  return IsInitialized() && IsFeatureStatusSuccess(GetValue());
}

FeatureStatus
FeatureState::GetValue() const
{
  if (!IsInitialized()) {
    return FeatureStatus::Unused;
  }

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
FeatureState::SetDefault(bool aEnable,
                         FeatureStatus aDisableStatus,
                         const char* aDisableMessage)
{
  if (!aEnable) {
    DisableByDefault(aDisableStatus, aDisableMessage,
                     NS_LITERAL_CSTRING("FEATURE_FAILURE_DISABLED"));
    return false;
  }
  EnableByDefault();
  return true;
}

void
FeatureState::SetDefaultFromPref(const char* aPrefName,
                                 bool aIsEnablePref,
                                 bool aDefaultValue)
{
  bool baseValue = Preferences::GetDefaultBool(aPrefName, aDefaultValue);
  SetDefault(baseValue == aIsEnablePref, FeatureStatus::Disabled, "Disabled by default");

  if (Preferences::HasUserValue(aPrefName)) {
    bool userValue = Preferences::GetBool(aPrefName, aDefaultValue);
    if (userValue == aIsEnablePref) {
      nsCString message("Enabled via ");
      message.AppendASCII(aPrefName);
      UserEnable(message.get());
    } else {
      nsCString message("Disabled via ");
      message.AppendASCII(aPrefName);
      UserDisable(message.get(), NS_LITERAL_CSTRING("FEATURE_FAILURE_PREF_OFF"));
    }
  }
}

bool
FeatureState::InitOrUpdate(bool aEnable,
                           FeatureStatus aDisableStatus,
                           const char* aDisableMessage)
{
  if (!IsInitialized()) {
    return SetDefault(aEnable, aDisableStatus, aDisableMessage);
  }
  return MaybeSetFailed(aEnable, aDisableStatus, aDisableMessage, nsCString());
}

void
FeatureState::UserEnable(const char* aMessage)
{
  AssertInitialized();
  SetUser(FeatureStatus::Available, aMessage);
}

void
FeatureState::UserForceEnable(const char* aMessage)
{
  AssertInitialized();
  SetUser(FeatureStatus::ForceEnabled, aMessage);
}

void
FeatureState::UserDisable(const char* aMessage, const nsACString& aFailureId)
{
  AssertInitialized();
  SetUser(FeatureStatus::Disabled, aMessage);
  SetFailureId(aFailureId);
}

void
FeatureState::Disable(FeatureStatus aStatus, const char* aMessage,
                      const nsACString& aFailureId)
{
  AssertInitialized();

  // We should never bother setting an environment status to "enabled," since
  // it could override an explicit user decision to disable it.
  MOZ_ASSERT(IsFeatureStatusFailure(aStatus));

  SetEnvironment(aStatus, aMessage);
  SetFailureId(aFailureId);
}

void
FeatureState::SetFailed(FeatureStatus aStatus, const char* aMessage,
                        const nsACString& aFailureId)
{
  AssertInitialized();

  // We should never bother setting a runtime status to "enabled," since it could
  // override an explicit user decision to disable it.
  MOZ_ASSERT(IsFeatureStatusFailure(aStatus));

  SetRuntime(aStatus, aMessage);
  SetFailureId(aFailureId);
}

bool
FeatureState::MaybeSetFailed(bool aEnable, FeatureStatus aStatus, const char* aMessage,
                             const nsACString& aFailureId)
{
  if (!aEnable) {
    SetFailed(aStatus, aMessage, aFailureId);
    return false;
  }
  return true;
}

bool
FeatureState::MaybeSetFailed(FeatureStatus aStatus, const char* aMessage,
                             const nsACString& aFailureId)
{
  return MaybeSetFailed(IsFeatureStatusSuccess(aStatus), aStatus, aMessage,
                        aFailureId);
}

bool
FeatureState::DisabledByDefault() const
{
  return mDefault.mStatus != FeatureStatus::Available;
}

bool
FeatureState::IsForcedOnByUser() const
{
  AssertInitialized();
  return mUser.mStatus == FeatureStatus::ForceEnabled;
}

void
FeatureState::EnableByDefault()
{
  // User/runtime decisions should not have been made yet.
  MOZ_ASSERT(!mUser.IsInitialized());
  MOZ_ASSERT(!mEnvironment.IsInitialized());
  MOZ_ASSERT(!mRuntime.IsInitialized());

  mDefault.Set(FeatureStatus::Available);
}

void
FeatureState::DisableByDefault(FeatureStatus aStatus, const char* aMessage,
                               const nsACString& aFailureId)
{
  // User/runtime decisions should not have been made yet.
  MOZ_ASSERT(!mUser.IsInitialized());
  MOZ_ASSERT(!mEnvironment.IsInitialized());
  MOZ_ASSERT(!mRuntime.IsInitialized());

  mDefault.Set(aStatus, aMessage);
  SetFailureId(aFailureId);
}

void
FeatureState::SetUser(FeatureStatus aStatus, const char* aMessage)
{
  // Default decision must have been made, but not runtime or environment.
  MOZ_ASSERT(mDefault.IsInitialized());
  MOZ_ASSERT(!mEnvironment.IsInitialized());
  MOZ_ASSERT(!mRuntime.IsInitialized());

  mUser.Set(aStatus, aMessage);
}

void
FeatureState::SetEnvironment(FeatureStatus aStatus, const char* aMessage)
{
  // Default decision must have been made, but not runtime.
  MOZ_ASSERT(mDefault.IsInitialized());
  MOZ_ASSERT(!mRuntime.IsInitialized());

  mEnvironment.Set(aStatus, aMessage);
}

void
FeatureState::SetRuntime(FeatureStatus aStatus, const char* aMessage)
{
  AssertInitialized();

  mRuntime.Set(aStatus, aMessage);
}

const char*
FeatureState::GetRuntimeMessage() const
{
  MOZ_ASSERT(IsFeatureStatusFailure(mRuntime.mStatus));
  return mRuntime.mMessage;
}

void
FeatureState::ForEachStatusChange(const StatusIterCallback& aCallback) const
{
  AssertInitialized();

  aCallback("default", mDefault.mStatus, mDefault.MessageOrNull());
  if (mUser.IsInitialized()) {
    aCallback("user", mUser.mStatus, mUser.Message());
  }
  if (mEnvironment.IsInitialized()) {
    aCallback("env", mEnvironment.mStatus, mEnvironment.Message());
  }
  if (mRuntime.IsInitialized()) {
    aCallback("runtime", mRuntime.mStatus, mRuntime.Message());
  }
}

void
FeatureState::SetFailureId(const nsACString& aFailureId)
{
  if (mFailureId.IsEmpty()) {
    mFailureId = aFailureId;
  }
}

const char*
FeatureState::GetFailureMessage() const
{
  AssertInitialized();
  MOZ_ASSERT(!IsEnabled());

  if (mRuntime.mStatus != FeatureStatus::Unused &&
      IsFeatureStatusFailure(mRuntime.mStatus))
  {
    return mRuntime.mMessage;
  }
  if (mEnvironment.mStatus != FeatureStatus::Unused &&
      IsFeatureStatusFailure(mEnvironment.mStatus))
  {
    return mEnvironment.mMessage;
  }
  if (mUser.mStatus != FeatureStatus::Unused &&
      IsFeatureStatusFailure(mUser.mStatus))
  {
    return mUser.mMessage;
  }

  MOZ_ASSERT(IsFeatureStatusFailure(mDefault.mStatus));
  return mDefault.mMessage;
}

const nsCString&
FeatureState::GetFailureId() const
{
  MOZ_ASSERT(!IsEnabled());
  return mFailureId;
}

void
FeatureState::Reset()
{
  mDefault.Set(FeatureStatus::Unused);
  mUser.Set(FeatureStatus::Unused);
  mEnvironment.Set(FeatureStatus::Unused);
  mRuntime.Set(FeatureStatus::Unused);
  mFailureId = nsCString();
}

void
FeatureState::Instance::Set(FeatureStatus aStatus, const char* aMessage /* = nullptr */)
{
  mStatus = aStatus;
  if (aMessage) {
    SprintfLiteral(mMessage, "%s", aMessage);
  } else {
    mMessage[0] = '\0';
  }
}

} // namespace gfx
} // namespace mozilla
