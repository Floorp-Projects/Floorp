/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxConfig.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/GraphicsMessages.h"
#include "plstr.h"

namespace mozilla {
namespace gfx {

static StaticAutoPtr<gfxConfig> sConfig;

/* static */ FeatureState& gfxConfig::GetFeature(Feature aFeature) {
  return sConfig->GetState(aFeature);
}

/* static */
bool gfxConfig::IsEnabled(Feature aFeature) {
  const FeatureState& state = sConfig->GetState(aFeature);
  return state.IsEnabled();
}

/* static */
bool gfxConfig::IsDisabledByDefault(Feature aFeature) {
  const FeatureState& state = sConfig->GetState(aFeature);
  return state.DisabledByDefault();
}

/* static */
bool gfxConfig::IsForcedOnByUser(Feature aFeature) {
  const FeatureState& state = sConfig->GetState(aFeature);
  return state.IsForcedOnByUser();
}

/* static */
FeatureStatus gfxConfig::GetValue(Feature aFeature) {
  const FeatureState& state = sConfig->GetState(aFeature);
  return state.GetValue();
}

/* static */
bool gfxConfig::SetDefault(Feature aFeature, bool aEnable,
                           FeatureStatus aDisableStatus,
                           const char* aDisableMessage) {
  FeatureState& state = sConfig->GetState(aFeature);
  return state.SetDefault(aEnable, aDisableStatus, aDisableMessage);
}

/* static */
void gfxConfig::DisableByDefault(Feature aFeature, FeatureStatus aDisableStatus,
                                 const char* aDisableMessage,
                                 const nsACString& aFailureId) {
  FeatureState& state = sConfig->GetState(aFeature);
  state.DisableByDefault(aDisableStatus, aDisableMessage, aFailureId);
}

/* static */
void gfxConfig::EnableByDefault(Feature aFeature) {
  FeatureState& state = sConfig->GetState(aFeature);
  state.EnableByDefault();
}

/* static */
void gfxConfig::SetDefaultFromPref(Feature aFeature, const char* aPrefName,
                                   bool aIsEnablePref, bool aDefaultValue) {
  FeatureState& state = sConfig->GetState(aFeature);
  return state.SetDefaultFromPref(aPrefName, aIsEnablePref, aDefaultValue);
}

/* static */
bool gfxConfig::InitOrUpdate(Feature aFeature, bool aEnable,
                             FeatureStatus aDisableStatus,
                             const char* aDisableMessage) {
  FeatureState& state = sConfig->GetState(aFeature);
  return state.InitOrUpdate(aEnable, aDisableStatus, aDisableMessage);
}

/* static */
void gfxConfig::SetFailed(Feature aFeature, FeatureStatus aStatus,
                          const char* aMessage, const nsACString& aFailureId) {
  FeatureState& state = sConfig->GetState(aFeature);
  state.SetFailed(aStatus, aMessage, aFailureId);
}

/* static */
void gfxConfig::Disable(Feature aFeature, FeatureStatus aStatus,
                        const char* aMessage, const nsACString& aFailureId) {
  FeatureState& state = sConfig->GetState(aFeature);
  state.Disable(aStatus, aMessage, aFailureId);
}

/* static */
void gfxConfig::UserEnable(Feature aFeature, const char* aMessage) {
  FeatureState& state = sConfig->GetState(aFeature);
  state.UserEnable(aMessage);
}

/* static */
void gfxConfig::UserForceEnable(Feature aFeature, const char* aMessage) {
  FeatureState& state = sConfig->GetState(aFeature);
  state.UserForceEnable(aMessage);
}

/* static */
void gfxConfig::UserDisable(Feature aFeature, const char* aMessage,
                            const nsACString& aFailureId) {
  FeatureState& state = sConfig->GetState(aFeature);
  state.UserDisable(aMessage, aFailureId);
}

/* static */
void gfxConfig::Reenable(Feature aFeature, Fallback aFallback) {
  FeatureState& state = sConfig->GetState(aFeature);
  MOZ_ASSERT(IsFeatureStatusFailure(state.GetValue()));

  const char* message = state.GetRuntimeMessage();
  EnableFallback(aFallback, message);
  state.SetRuntime(FeatureStatus::Available, nullptr, nsCString());
}

/* static */
void gfxConfig::Reset(Feature aFeature) {
  FeatureState& state = sConfig->GetState(aFeature);
  state.Reset();
}

/* static */
void gfxConfig::Inherit(Feature aFeature, FeatureStatus aStatus) {
  FeatureState& state = sConfig->GetState(aFeature);

  state.Reset();

  switch (aStatus) {
    case FeatureStatus::Unused:
      break;
    case FeatureStatus::Available:
      gfxConfig::EnableByDefault(aFeature);
      break;
    case FeatureStatus::ForceEnabled:
      gfxConfig::EnableByDefault(aFeature);
      gfxConfig::UserForceEnable(aFeature, "Inherited from parent process");
      break;
    default:
      gfxConfig::SetDefault(aFeature, false, aStatus,
                            "Disabled in parent process");
      break;
  }
}

/* static */
void gfxConfig::Inherit(EnumSet<Feature> aFeatures,
                        const DevicePrefs& aDevicePrefs) {
  for (Feature feature : aFeatures) {
    FeatureStatus status = FeatureStatus::Unused;
    switch (feature) {
      case Feature::HW_COMPOSITING:
        status = aDevicePrefs.hwCompositing();
        break;
      case Feature::D3D11_COMPOSITING:
        status = aDevicePrefs.d3d11Compositing();
        break;
      case Feature::OPENGL_COMPOSITING:
        status = aDevicePrefs.oglCompositing();
        break;
      case Feature::DIRECT2D:
        status = aDevicePrefs.useD2D1();
        break;
      default:
        break;
    }
    gfxConfig::Inherit(feature, status);
  }
}

/* static */
bool gfxConfig::UseFallback(Fallback aFallback) {
  return sConfig->UseFallbackImpl(aFallback);
}

/* static */
void gfxConfig::EnableFallback(Fallback aFallback, const char* aMessage) {
  if (!NS_IsMainThread()) {
    nsCString message(aMessage);
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("gfxConfig::EnableFallback", [=]() -> void {
          gfxConfig::EnableFallback(aFallback, message.get());
        }));
    return;
  }

  if (XRE_IsGPUProcess()) {
    nsCString message(aMessage);
    Unused << GPUParent::GetSingleton()->SendUsedFallback(aFallback, message);
    return;
  }

  sConfig->EnableFallbackImpl(aFallback, aMessage);
}

bool gfxConfig::UseFallbackImpl(Fallback aFallback) const {
  return !!(mFallbackBits & (uint64_t(1) << uint64_t(aFallback)));
}

void gfxConfig::EnableFallbackImpl(Fallback aFallback, const char* aMessage) {
  if (!UseFallbackImpl(aFallback)) {
    MOZ_ASSERT(mNumFallbackLogEntries < kNumFallbacks);

    FallbackLogEntry& entry = mFallbackLog[mNumFallbackLogEntries];
    mNumFallbackLogEntries++;

    entry.mFallback = aFallback;
    PL_strncpyz(entry.mMessage, aMessage, sizeof(entry.mMessage));
  }
  mFallbackBits |= (uint64_t(1) << uint64_t(aFallback));
}

struct FeatureInfo {
  const char* name;
  const char* description;
};
static const FeatureInfo sFeatureInfo[] = {
#define FOR_EACH_FEATURE(name, type, desc) {#name, desc},
    GFX_FEATURE_MAP(FOR_EACH_FEATURE)
#undef FOR_EACH_FEATURE
        {nullptr, nullptr}};

/* static */
void gfxConfig::ForEachFeature(const FeatureIterCallback& aCallback) {
  for (size_t i = 0; i < kNumFeatures; i++) {
    FeatureState& state = GetFeature(static_cast<Feature>(i));
    if (!state.IsInitialized()) {
      continue;
    }

    aCallback(sFeatureInfo[i].name, sFeatureInfo[i].description, state);
  }
}

static const char* sFallbackNames[] = {
#define FOR_EACH_FALLBACK(name) #name,
    GFX_FALLBACK_MAP(FOR_EACH_FALLBACK)
#undef FOR_EACH_FALLBACK
        nullptr};

/* static  */
void gfxConfig::ForEachFallback(const FallbackIterCallback& aCallback) {
  sConfig->ForEachFallbackImpl(aCallback);
}

void gfxConfig::ForEachFallbackImpl(const FallbackIterCallback& aCallback) {
  for (size_t i = 0; i < mNumFallbackLogEntries; i++) {
    const FallbackLogEntry& entry = mFallbackLog[i];
    aCallback(sFallbackNames[size_t(entry.mFallback)], entry.mMessage);
  }
}

/* static */ const nsCString& gfxConfig::GetFailureId(Feature aFeature) {
  const FeatureState& state = sConfig->GetState(aFeature);
  return state.GetFailureId();
}

/* static */
void gfxConfig::ImportChange(Feature aFeature,
                             const Maybe<FeatureFailure>& aChange) {
  if (aChange.isNothing()) {
    return;
  }

  const FeatureFailure& failure = aChange.ref();
  gfxConfig::SetFailed(aFeature, failure.status(), failure.message().get(),
                       failure.failureId());
}

/* static */
void gfxConfig::Init() { sConfig = new gfxConfig(); }

/* static */
void gfxConfig::Shutdown() { sConfig = nullptr; }

}  // namespace gfx
}  // namespace mozilla
