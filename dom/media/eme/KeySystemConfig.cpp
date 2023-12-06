/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeySystemConfig.h"

#include "EMEUtils.h"
#include "GMPUtils.h"
#include "KeySystemNames.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsPrintfCString.h"

#ifdef XP_WIN
#  include "WMFDecoderModule.h"
#endif
#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidDecoderModule.h"
#  include "mozilla/java/MediaDrmProxyWrappers.h"
#  include "nsMimeTypes.h"
#endif

#ifdef MOZ_WMF_CDM
#  include "mediafoundation/WMFCDMImpl.h"
#endif

namespace mozilla {

/* static */
bool KeySystemConfig::Supports(const nsAString& aKeySystem) {
  nsCString api = nsLiteralCString(CHROMIUM_CDM_API);
  nsCString name = NS_ConvertUTF16toUTF8(aKeySystem);

  if (HaveGMPFor(api, {name})) {
    return true;
  }
#ifdef MOZ_WIDGET_ANDROID
  // Check if we can use MediaDrm for this keysystem.
  if (mozilla::java::MediaDrmProxy::IsSchemeSupported(name)) {
    return true;
  }
#endif
#if MOZ_WMF_CDM
  if ((IsPlayReadyKeySystemAndSupported(aKeySystem) ||
       IsWidevineExperimentKeySystemAndSupported(aKeySystem)) &&
      WMFCDMImpl::Supports(aKeySystem)) {
    return true;
  }
#endif
  return false;
}

/* static */
bool KeySystemConfig::CreateKeySystemConfigs(
    const nsAString& aKeySystem, nsTArray<KeySystemConfig>& aOutConfigs) {
  if (!Supports(aKeySystem)) {
    return false;
  }

  if (IsClearkeyKeySystem(aKeySystem)) {
    KeySystemConfig* config = aOutConfigs.AppendElement();
    config->mKeySystem = aKeySystem;
    config->mInitDataTypes.AppendElement(u"cenc"_ns);
    config->mInitDataTypes.AppendElement(u"keyids"_ns);
    config->mInitDataTypes.AppendElement(u"webm"_ns);
    config->mPersistentState = Requirement::Optional;
    config->mDistinctiveIdentifier = Requirement::NotAllowed;
    config->mSessionTypes.AppendElement(SessionType::Temporary);
    config->mEncryptionSchemes.AppendElement(u"cenc"_ns);
    config->mEncryptionSchemes.AppendElement(u"cbcs"_ns);
    config->mEncryptionSchemes.AppendElement(u"cbcs-1-9"_ns);
    if (StaticPrefs::media_clearkey_persistent_license_enabled()) {
      config->mSessionTypes.AppendElement(SessionType::PersistentLicense);
    }
#if defined(XP_WIN)
    // Clearkey CDM uses WMF's H.264 decoder on Windows.
    if (WMFDecoderModule::CanCreateMFTDecoder(WMFStreamType::H264)) {
      config->mMP4.SetCanDecryptAndDecode(EME_CODEC_H264);
    } else {
      config->mMP4.SetCanDecrypt(EME_CODEC_H264);
    }
#else
    config->mMP4.SetCanDecrypt(EME_CODEC_H264);
#endif
    config->mMP4.SetCanDecrypt(EME_CODEC_AAC);
    config->mMP4.SetCanDecrypt(EME_CODEC_FLAC);
    config->mMP4.SetCanDecrypt(EME_CODEC_OPUS);
    config->mMP4.SetCanDecrypt(EME_CODEC_VP9);
    config->mWebM.SetCanDecrypt(EME_CODEC_VORBIS);
    config->mWebM.SetCanDecrypt(EME_CODEC_OPUS);
    config->mWebM.SetCanDecrypt(EME_CODEC_VP8);
    config->mWebM.SetCanDecrypt(EME_CODEC_VP9);

    if (StaticPrefs::media_clearkey_test_key_systems_enabled()) {
      // Add testing key systems. These offer the same capabilities as the
      // base clearkey system, so just clone clearkey and change the name.
      KeySystemConfig clearkeyWithProtectionQuery{*config};
      clearkeyWithProtectionQuery.mKeySystem.AssignLiteral(
          kClearKeyWithProtectionQueryKeySystemName);
      aOutConfigs.AppendElement(std::move(clearkeyWithProtectionQuery));
    }
    return true;
  }

  if (IsWidevineKeySystem(aKeySystem)) {
    KeySystemConfig* config = aOutConfigs.AppendElement();
    config->mKeySystem = aKeySystem;
    config->mInitDataTypes.AppendElement(u"cenc"_ns);
    config->mInitDataTypes.AppendElement(u"keyids"_ns);
    config->mInitDataTypes.AppendElement(u"webm"_ns);
    config->mPersistentState = Requirement::Optional;
    config->mDistinctiveIdentifier = Requirement::NotAllowed;
    config->mSessionTypes.AppendElement(SessionType::Temporary);
#ifdef MOZ_WIDGET_ANDROID
    config->mSessionTypes.AppendElement(SessionType::PersistentLicense);
#endif
    config->mAudioRobustness.AppendElement(u"SW_SECURE_CRYPTO"_ns);
    config->mVideoRobustness.AppendElement(u"SW_SECURE_CRYPTO"_ns);
    config->mVideoRobustness.AppendElement(u"SW_SECURE_DECODE"_ns);
    config->mEncryptionSchemes.AppendElement(u"cenc"_ns);
    config->mEncryptionSchemes.AppendElement(u"cbcs"_ns);
    config->mEncryptionSchemes.AppendElement(u"cbcs-1-9"_ns);

#if defined(MOZ_WIDGET_ANDROID)
    // MediaDrm.isCryptoSchemeSupported only allows passing
    // "video/mp4" or "video/webm" for mimetype string.
    // See
    // https://developer.android.com/reference/android/media/MediaDrm.html#isCryptoSchemeSupported(java.util.UUID,
    // java.lang.String) for more detail.
    typedef struct {
      const nsCString& mMimeType;
      const nsCString& mEMECodecType;
      const char16_t* mCodecType;
      KeySystemConfig::ContainerSupport* mSupportType;
    } DataForValidation;

    DataForValidation validationList[] = {
        {nsCString(VIDEO_MP4), EME_CODEC_H264, java::MediaDrmProxy::AVC,
         &config->mMP4},
        {nsCString(VIDEO_MP4), EME_CODEC_VP9, java::MediaDrmProxy::AVC,
         &config->mMP4},
        {nsCString(AUDIO_MP4), EME_CODEC_AAC, java::MediaDrmProxy::AAC,
         &config->mMP4},
        {nsCString(AUDIO_MP4), EME_CODEC_FLAC, java::MediaDrmProxy::FLAC,
         &config->mMP4},
        {nsCString(AUDIO_MP4), EME_CODEC_OPUS, java::MediaDrmProxy::OPUS,
         &config->mMP4},
        {nsCString(VIDEO_WEBM), EME_CODEC_VP8, java::MediaDrmProxy::VP8,
         &config->mWebM},
        {nsCString(VIDEO_WEBM), EME_CODEC_VP9, java::MediaDrmProxy::VP9,
         &config->mWebM},
        {nsCString(AUDIO_WEBM), EME_CODEC_VORBIS, java::MediaDrmProxy::VORBIS,
         &config->mWebM},
        {nsCString(AUDIO_WEBM), EME_CODEC_OPUS, java::MediaDrmProxy::OPUS,
         &config->mWebM},
    };

    for (const auto& data : validationList) {
      if (java::MediaDrmProxy::IsCryptoSchemeSupported(kWidevineKeySystemName,
                                                       data.mMimeType)) {
        if (!AndroidDecoderModule::SupportsMimeType(data.mMimeType).isEmpty()) {
          data.mSupportType->SetCanDecryptAndDecode(data.mEMECodecType);
        } else {
          data.mSupportType->SetCanDecrypt(data.mEMECodecType);
        }
      }
    }
#else
#  if defined(XP_WIN)
    // Widevine CDM doesn't include an AAC decoder. So if WMF can't
    // decode AAC, and a codec wasn't specified, be conservative
    // and reject the MediaKeys request, since we assume Widevine
    // will be used with AAC.
    if (WMFDecoderModule::CanCreateMFTDecoder(WMFStreamType::AAC)) {
      config->mMP4.SetCanDecrypt(EME_CODEC_AAC);
    }
#  else
    config->mMP4.SetCanDecrypt(EME_CODEC_AAC);
#  endif
    config->mMP4.SetCanDecrypt(EME_CODEC_FLAC);
    config->mMP4.SetCanDecrypt(EME_CODEC_OPUS);
    config->mMP4.SetCanDecryptAndDecode(EME_CODEC_H264);
    config->mMP4.SetCanDecryptAndDecode(EME_CODEC_VP9);
    config->mWebM.SetCanDecrypt(EME_CODEC_VORBIS);
    config->mWebM.SetCanDecrypt(EME_CODEC_OPUS);
    config->mWebM.SetCanDecryptAndDecode(EME_CODEC_VP8);
    config->mWebM.SetCanDecryptAndDecode(EME_CODEC_VP9);
#endif
    return true;
  }
#ifdef MOZ_WMF_CDM
  if (IsPlayReadyKeySystemAndSupported(aKeySystem) ||
      IsWidevineExperimentKeySystemAndSupported(aKeySystem)) {
    RefPtr<WMFCDMImpl> cdm = MakeRefPtr<WMFCDMImpl>(aKeySystem);
    return cdm->GetCapabilities(aOutConfigs);
  }
#endif
  return false;
}

bool KeySystemConfig::IsSameKeySystem(const nsAString& aKeySystem) const {
#ifdef MOZ_WMF_CDM
  // We want to map Widevine experiment key system to normal Widevine key system
  // as well.
  if (IsWidevineExperimentKeySystemAndSupported(mKeySystem)) {
    return mKeySystem.Equals(aKeySystem) ||
           aKeySystem.EqualsLiteral(kWidevineKeySystemName);
  }
#endif
  return mKeySystem.Equals(aKeySystem);
}

nsString KeySystemConfig::GetDebugInfo() const {
  nsString debugInfo;
  debugInfo.Append(mKeySystem);
  debugInfo.AppendLiteral(",init-data-type=");
  for (const auto& type : mInitDataTypes) {
    debugInfo.Append(type);
    debugInfo.AppendLiteral(",");
  }
  debugInfo.AppendASCII(
      nsPrintfCString(",persistent=%s", RequirementToStr(mPersistentState))
          .get());
  debugInfo.AppendASCII(
      nsPrintfCString(",distinctive=%s",
                      RequirementToStr(mDistinctiveIdentifier))
          .get());
  debugInfo.AppendLiteral(",sessionType=");
  for (const auto& type : mSessionTypes) {
    debugInfo.AppendASCII(nsPrintfCString("%s,", SessionTypeToStr(type)).get());
  }
  debugInfo.AppendLiteral(",video-robustness=");
  for (const auto& robustness : mVideoRobustness) {
    debugInfo.Append(robustness);
  }
  debugInfo.AppendLiteral(",audio-robustness=");
  for (const auto& robustness : mAudioRobustness) {
    debugInfo.Append(robustness);
  }
  debugInfo.AppendLiteral(",scheme=");
  for (const auto& scheme : mEncryptionSchemes) {
    debugInfo.Append(scheme);
  }
  debugInfo.AppendLiteral(",MP4=");
  debugInfo.Append(NS_ConvertUTF8toUTF16(mMP4.GetDebugInfo()));
  debugInfo.AppendLiteral(",WEBM=");
  debugInfo.Append(NS_ConvertUTF8toUTF16(mWebM.GetDebugInfo()));
  return debugInfo;
}

KeySystemConfig::SessionType ConvertToKeySystemConfigSessionType(
    dom::MediaKeySessionType aType) {
  switch (aType) {
    case dom::MediaKeySessionType::Temporary:
      return KeySystemConfig::SessionType::Temporary;
    case dom::MediaKeySessionType::Persistent_license:
      return KeySystemConfig::SessionType::PersistentLicense;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid session type");
      return KeySystemConfig::SessionType::Temporary;
  }
}

const char* SessionTypeToStr(KeySystemConfig::SessionType aType) {
  switch (aType) {
    case KeySystemConfig::SessionType::Temporary:
      return "Temporary";
    case KeySystemConfig::SessionType::PersistentLicense:
      return "PersistentLicense";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid session type");
      return "Invalid";
  }
}

const char* RequirementToStr(KeySystemConfig::Requirement aRequirement) {
  switch (aRequirement) {
    case KeySystemConfig::Requirement::Required:
      return "required";
    case KeySystemConfig::Requirement::Optional:
      return "optional";
    default:
      return "not-allowed";
  }
}

}  // namespace mozilla
