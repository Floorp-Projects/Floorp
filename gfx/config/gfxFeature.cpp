/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFeature.h"

#include "mozilla/Preferences.h"
#include "mozilla/Sprintf.h"
#include "nsString.h"

namespace mozilla {
namespace gfx {

bool FeatureState::IsEnabled() const {
  return IsInitialized() && IsFeatureStatusSuccess(GetValue());
}

FeatureStatus FeatureState::GetValue() const {
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

bool FeatureState::SetDefault(bool aEnable, FeatureStatus aDisableStatus,
                              const char* aDisableMessage) {
  if (!aEnable) {
    DisableByDefault(aDisableStatus, aDisableMessage,
                     "FEATURE_FAILURE_DISABLED"_ns);
    return false;
  }
  EnableByDefault();
  return true;
}

void FeatureState::SetDefaultFromPref(const char* aPrefName, bool aIsEnablePref,
                                      bool aDefaultValue,
                                      Maybe<bool> aUserValue) {
  bool baseValue =
      Preferences::GetBool(aPrefName, aDefaultValue, PrefValueKind::Default);
  SetDefault(baseValue == aIsEnablePref, FeatureStatus::Disabled,
             "Disabled by default");

  if (aUserValue) {
    if (*aUserValue == aIsEnablePref) {
      nsCString message("Enabled via ");
      message.AppendASCII(aPrefName);
      UserEnable(message.get());
    } else {
      nsCString message("Disabled via ");
      message.AppendASCII(aPrefName);
      UserDisable(message.get(), "FEATURE_FAILURE_PREF_OFF"_ns);
    }
  }
}

void FeatureState::SetDefaultFromPref(const char* aPrefName, bool aIsEnablePref,
                                      bool aDefaultValue) {
  Maybe<bool> userValue;
  if (Preferences::HasUserValue(aPrefName)) {
    userValue.emplace(Preferences::GetBool(aPrefName, aDefaultValue));
  }

  SetDefaultFromPref(aPrefName, aIsEnablePref, aDefaultValue, userValue);
}

bool FeatureState::InitOrUpdate(bool aEnable, FeatureStatus aDisableStatus,
                                const char* aDisableMessage) {
  if (!IsInitialized()) {
    return SetDefault(aEnable, aDisableStatus, aDisableMessage);
  }
  return MaybeSetFailed(aEnable, aDisableStatus, aDisableMessage, nsCString());
}

void FeatureState::UserEnable(const char* aMessage) {
  AssertInitialized();
  SetUser(FeatureStatus::Available, aMessage, nsCString());
}

void FeatureState::UserForceEnable(const char* aMessage) {
  AssertInitialized();
  SetUser(FeatureStatus::ForceEnabled, aMessage, nsCString());
}

void FeatureState::UserDisable(const char* aMessage,
                               const nsACString& aFailureId) {
  AssertInitialized();
  SetUser(FeatureStatus::Disabled, aMessage, aFailureId);
}

void FeatureState::Disable(FeatureStatus aStatus, const char* aMessage,
                           const nsACString& aFailureId) {
  AssertInitialized();

  // We should never bother setting an environment status to "enabled," since
  // it could override an explicit user decision to disable it.
  MOZ_ASSERT(IsFeatureStatusFailure(aStatus));

  SetEnvironment(aStatus, aMessage, aFailureId);
}

void FeatureState::SetFailed(FeatureStatus aStatus, const char* aMessage,
                             const nsACString& aFailureId) {
  AssertInitialized();

  // We should never bother setting a runtime status to "enabled," since it
  // could override an explicit user decision to disable it.
  MOZ_ASSERT(IsFeatureStatusFailure(aStatus));

  SetRuntime(aStatus, aMessage, aFailureId);
}

bool FeatureState::MaybeSetFailed(bool aEnable, FeatureStatus aStatus,
                                  const char* aMessage,
                                  const nsACString& aFailureId) {
  if (!aEnable) {
    SetFailed(aStatus, aMessage, aFailureId);
    return false;
  }
  return true;
}

bool FeatureState::MaybeSetFailed(FeatureStatus aStatus, const char* aMessage,
                                  const nsACString& aFailureId) {
  return MaybeSetFailed(IsFeatureStatusSuccess(aStatus), aStatus, aMessage,
                        aFailureId);
}

bool FeatureState::DisabledByDefault() const {
  return mDefault.mStatus != FeatureStatus::Available;
}

bool FeatureState::IsForcedOnByUser() const {
  AssertInitialized();
  return mUser.mStatus == FeatureStatus::ForceEnabled;
}

void FeatureState::EnableByDefault() {
  // User/runtime decisions should not have been made yet.
  MOZ_ASSERT(!mUser.IsInitialized());
  MOZ_ASSERT(!mEnvironment.IsInitialized());
  MOZ_ASSERT(!mRuntime.IsInitialized());

  mDefault.Set(FeatureStatus::Available);
}

void FeatureState::DisableByDefault(FeatureStatus aStatus, const char* aMessage,
                                    const nsACString& aFailureId) {
  // User/runtime decisions should not have been made yet.
  MOZ_ASSERT(!mUser.IsInitialized());
  MOZ_ASSERT(!mEnvironment.IsInitialized());
  MOZ_ASSERT(!mRuntime.IsInitialized());

  // Make sure that when disabling we actually use a failure status.
  MOZ_ASSERT(IsFeatureStatusFailure(aStatus));

  mDefault.Set(aStatus, aMessage, aFailureId);
}

void FeatureState::SetUser(FeatureStatus aStatus, const char* aMessage,
                           const nsACString& aFailureId) {
  // Default decision must have been made, but not runtime or environment.
  MOZ_ASSERT(mDefault.IsInitialized());
  MOZ_ASSERT(!mEnvironment.IsInitialized());
  MOZ_ASSERT(!mRuntime.IsInitialized());

  mUser.Set(aStatus, aMessage, aFailureId);
}

void FeatureState::SetEnvironment(FeatureStatus aStatus, const char* aMessage,
                                  const nsACString& aFailureId) {
  // Default decision must have been made, but not runtime.
  MOZ_ASSERT(mDefault.IsInitialized());
  MOZ_ASSERT(!mRuntime.IsInitialized());

  mEnvironment.Set(aStatus, aMessage, aFailureId);
}

void FeatureState::SetRuntime(FeatureStatus aStatus, const char* aMessage,
                              const nsACString& aFailureId) {
  AssertInitialized();

  mRuntime.Set(aStatus, aMessage, aFailureId);
}

const char* FeatureState::GetRuntimeMessage() const {
  MOZ_ASSERT(IsFeatureStatusFailure(mRuntime.mStatus));
  return mRuntime.mMessage;
}

void FeatureState::ForEachStatusChange(
    const StatusIterCallback& aCallback) const {
  AssertInitialized();

  aCallback("default", mDefault.mStatus, mDefault.MessageOrNull(),
            mDefault.FailureId());
  if (mUser.IsInitialized()) {
    aCallback("user", mUser.mStatus, mUser.Message(), mUser.FailureId());
  }
  if (mEnvironment.IsInitialized()) {
    aCallback("env", mEnvironment.mStatus, mEnvironment.Message(),
              mEnvironment.FailureId());
  }
  if (mRuntime.IsInitialized()) {
    aCallback("runtime", mRuntime.mStatus, mRuntime.Message(),
              mRuntime.FailureId());
  }
}

const char* FeatureState::GetFailureMessage() const {
  AssertInitialized();
  MOZ_ASSERT(!IsEnabled());

  if (mRuntime.mStatus != FeatureStatus::Unused &&
      IsFeatureStatusFailure(mRuntime.mStatus)) {
    return mRuntime.mMessage;
  }
  if (mEnvironment.mStatus != FeatureStatus::Unused &&
      IsFeatureStatusFailure(mEnvironment.mStatus)) {
    return mEnvironment.mMessage;
  }
  if (mUser.mStatus != FeatureStatus::Unused &&
      IsFeatureStatusFailure(mUser.mStatus)) {
    return mUser.mMessage;
  }

  MOZ_ASSERT(IsFeatureStatusFailure(mDefault.mStatus));
  return mDefault.mMessage;
}

const nsCString& FeatureState::GetFailureId() const {
  MOZ_ASSERT(!IsEnabled());

  if (mRuntime.mStatus != FeatureStatus::Unused) {
    return mRuntime.mFailureId;
  }
  if (mEnvironment.mStatus != FeatureStatus::Unused) {
    return mEnvironment.mFailureId;
  }
  if (mUser.mStatus != FeatureStatus::Unused) {
    return mUser.mFailureId;
  }

  return mDefault.mFailureId;
}

nsCString FeatureState::GetStatusAndFailureIdString() const {
  nsCString status;
  auto value = GetValue();
  switch (value) {
    case FeatureStatus::Blocklisted:
    case FeatureStatus::Disabled:
    case FeatureStatus::Unavailable:
    case FeatureStatus::UnavailableNoAngle:
    case FeatureStatus::Blocked:
      status.AppendPrintf("%s:%s", FeatureStatusToString(value),
                          GetFailureId().get());
      break;
    case FeatureStatus::Failed:
      status.AppendPrintf("%s:%s", FeatureStatusToString(value),
                          GetFailureMessage());
      break;
    default:
      status.Append(FeatureStatusToString(value));
      break;
  }

  return status;
}

void FeatureState::Reset() {
  mDefault.Set(FeatureStatus::Unused);
  mUser.Set(FeatureStatus::Unused);
  mEnvironment.Set(FeatureStatus::Unused);
  mRuntime.Set(FeatureStatus::Unused);
}

void FeatureState::Instance::Set(FeatureStatus aStatus) {
  mStatus = aStatus;
  mMessage[0] = '\0';
  mFailureId.Truncate();
}

void FeatureState::Instance::Set(FeatureStatus aStatus, const char* aMessage,
                                 const nsACString& aFailureId) {
  mStatus = aStatus;
  if (aMessage) {
    SprintfLiteral(mMessage, "%s", aMessage);
  } else {
    mMessage[0] = '\0';
  }
  mFailureId.Assign(aFailureId);
}

}  // namespace gfx
}  // namespace mozilla
