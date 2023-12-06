/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EMEUtils.h"

#include "jsfriendapi.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/dom/KeySystemNames.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {

LogModule* GetEMELog() {
  static LazyLogModule log("EME");
  return log;
}

LogModule* GetEMEVerboseLog() {
  static LazyLogModule log("EMEV");
  return log;
}

void CopyArrayBufferViewOrArrayBufferData(
    const dom::ArrayBufferViewOrArrayBuffer& aBufferOrView,
    nsTArray<uint8_t>& aOutData) {
  aOutData.Clear();
  Unused << dom::AppendTypedArrayDataTo(aBufferOrView, aOutData);
}

bool IsClearkeyKeySystem(const nsAString& aKeySystem) {
  if (StaticPrefs::media_clearkey_test_key_systems_enabled()) {
    return aKeySystem.EqualsLiteral(kClearKeyKeySystemName) ||
           aKeySystem.EqualsLiteral(kClearKeyWithProtectionQueryKeySystemName);
  }
  return aKeySystem.EqualsLiteral(kClearKeyKeySystemName);
}

bool IsWidevineKeySystem(const nsAString& aKeySystem) {
  return aKeySystem.EqualsLiteral(kWidevineKeySystemName);
}

#ifdef MOZ_WMF_CDM
bool IsPlayReadyKeySystemAndSupported(const nsAString& aKeySystem) {
  if (!StaticPrefs::media_eme_playready_enabled()) {
    return false;
  }
  // 1=enabled encrypted and clear, 2=enabled encrytped.
  if (StaticPrefs::media_wmf_media_engine_enabled() != 1 &&
      StaticPrefs::media_wmf_media_engine_enabled() != 2) {
    return false;
  }
  return aKeySystem.EqualsLiteral(kPlayReadyKeySystemName) ||
         aKeySystem.EqualsLiteral(kPlayReadyKeySystemHardware);
}

bool IsWidevineExperimentKeySystemAndSupported(const nsAString& aKeySystem) {
  if (!StaticPrefs::media_eme_widevine_experiment_enabled()) {
    return false;
  }
  // 1=enabled encrypted and clear, 2=enabled encrytped.
  if (StaticPrefs::media_wmf_media_engine_enabled() != 1 &&
      StaticPrefs::media_wmf_media_engine_enabled() != 2) {
    return false;
  }
  return aKeySystem.EqualsLiteral(kWidevineExperimentKeySystemName) ||
         aKeySystem.EqualsLiteral(kWidevineExperiment2KeySystemName);
}
#endif

nsString KeySystemToProxyName(const nsAString& aKeySystem) {
  if (IsClearkeyKeySystem(aKeySystem)) {
    return u"gmp-clearkey"_ns;
  }
  if (IsWidevineKeySystem(aKeySystem)) {
    return u"gmp-widevinecdm"_ns;
  }
#ifdef MOZ_WMF_CDM
  if (IsPlayReadyKeySystemAndSupported(aKeySystem)) {
    return u"mfcdm-playready"_ns;
  }
  if (IsWidevineExperimentKeySystemAndSupported(aKeySystem)) {
    return u"mfcdm-widevine"_ns;
  }
#endif
  MOZ_ASSERT_UNREACHABLE("Not supported key system!");
  return u""_ns;
}

#define ENUM_TO_STR(enumVal) \
  case enumVal:              \
    return #enumVal

const char* ToMediaKeyStatusStr(dom::MediaKeyStatus aStatus) {
  switch (aStatus) {
    ENUM_TO_STR(dom::MediaKeyStatus::Usable);
    ENUM_TO_STR(dom::MediaKeyStatus::Expired);
    ENUM_TO_STR(dom::MediaKeyStatus::Released);
    ENUM_TO_STR(dom::MediaKeyStatus::Output_restricted);
    ENUM_TO_STR(dom::MediaKeyStatus::Output_downscaled);
    ENUM_TO_STR(dom::MediaKeyStatus::Status_pending);
    ENUM_TO_STR(dom::MediaKeyStatus::Internal_error);
    default:
      return "Undefined MediaKeyStatus!";
  }
}

#undef ENUM_TO_STR

bool IsHardwareDecryptionSupported(
    const dom::MediaKeySystemConfiguration& aConfig) {
  bool supportHardwareDecryption = false;
  for (const auto& capabilities : aConfig.mAudioCapabilities) {
    if (capabilities.mRobustness.EqualsLiteral("HW_SECURE_ALL")) {
      supportHardwareDecryption = true;
      break;
    }
  }
  for (const auto& capabilities : aConfig.mVideoCapabilities) {
    if (capabilities.mRobustness.EqualsLiteral("3000") ||
        capabilities.mRobustness.EqualsLiteral("HW_SECURE_ALL")) {
      supportHardwareDecryption = true;
      break;
    }
  }
  return supportHardwareDecryption;
}

}  // namespace mozilla
