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
  if (IsPlayReadyKeySystemAndSupported(aKeySystem) &&
      WMFCDMImpl::Supports(aKeySystem)) {
    return true;
  }
#endif
  return false;
}

/* static */
bool KeySystemConfig::GetConfig(const nsAString& aKeySystem,
                                KeySystemConfig& aConfig) {
  if (!Supports(aKeySystem)) {
    return false;
  }

  if (IsClearkeyKeySystem(aKeySystem)) {
    aConfig.mKeySystem = aKeySystem;
    aConfig.mInitDataTypes.AppendElement(u"cenc"_ns);
    aConfig.mInitDataTypes.AppendElement(u"keyids"_ns);
    aConfig.mInitDataTypes.AppendElement(u"webm"_ns);
    aConfig.mPersistentState = Requirement::Optional;
    aConfig.mDistinctiveIdentifier = Requirement::NotAllowed;
    aConfig.mSessionTypes.AppendElement(SessionType::Temporary);
    aConfig.mEncryptionSchemes.AppendElement(u"cenc"_ns);
    aConfig.mEncryptionSchemes.AppendElement(u"cbcs"_ns);
    aConfig.mEncryptionSchemes.AppendElement(u"cbcs-1-9"_ns);
    if (StaticPrefs::media_clearkey_persistent_license_enabled()) {
      aConfig.mSessionTypes.AppendElement(SessionType::PersistentLicense);
    }
#if defined(XP_WIN)
    // Clearkey CDM uses WMF's H.264 decoder on Windows.
    if (WMFDecoderModule::CanCreateMFTDecoder(WMFStreamType::H264)) {
      aConfig.mMP4.SetCanDecryptAndDecode(EME_CODEC_H264);
    } else {
      aConfig.mMP4.SetCanDecrypt(EME_CODEC_H264);
    }
#else
    aConfig.mMP4.SetCanDecrypt(EME_CODEC_H264);
#endif
    aConfig.mMP4.SetCanDecrypt(EME_CODEC_AAC);
    aConfig.mMP4.SetCanDecrypt(EME_CODEC_FLAC);
    aConfig.mMP4.SetCanDecrypt(EME_CODEC_OPUS);
    aConfig.mMP4.SetCanDecrypt(EME_CODEC_VP9);
    aConfig.mWebM.SetCanDecrypt(EME_CODEC_VORBIS);
    aConfig.mWebM.SetCanDecrypt(EME_CODEC_OPUS);
    aConfig.mWebM.SetCanDecrypt(EME_CODEC_VP8);
    aConfig.mWebM.SetCanDecrypt(EME_CODEC_VP9);
    return true;
  }
  if (IsWidevineKeySystem(aKeySystem)) {
    aConfig.mKeySystem = aKeySystem;
    aConfig.mInitDataTypes.AppendElement(u"cenc"_ns);
    aConfig.mInitDataTypes.AppendElement(u"keyids"_ns);
    aConfig.mInitDataTypes.AppendElement(u"webm"_ns);
    aConfig.mPersistentState = Requirement::Optional;
    aConfig.mDistinctiveIdentifier = Requirement::NotAllowed;
    aConfig.mSessionTypes.AppendElement(SessionType::Temporary);
#ifdef MOZ_WIDGET_ANDROID
    aConfig.mSessionTypes.AppendElement(SessionType::PersistentLicense);
#endif
    aConfig.mAudioRobustness.AppendElement(u"SW_SECURE_CRYPTO"_ns);
    aConfig.mVideoRobustness.AppendElement(u"SW_SECURE_CRYPTO"_ns);
    aConfig.mVideoRobustness.AppendElement(u"SW_SECURE_DECODE"_ns);
    aConfig.mEncryptionSchemes.AppendElement(u"cenc"_ns);
    aConfig.mEncryptionSchemes.AppendElement(u"cbcs"_ns);
    aConfig.mEncryptionSchemes.AppendElement(u"cbcs-1-9"_ns);

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
         &aConfig.mMP4},
        {nsCString(VIDEO_MP4), EME_CODEC_VP9, java::MediaDrmProxy::AVC,
         &aConfig.mMP4},
        {nsCString(AUDIO_MP4), EME_CODEC_AAC, java::MediaDrmProxy::AAC,
         &aConfig.mMP4},
        {nsCString(AUDIO_MP4), EME_CODEC_FLAC, java::MediaDrmProxy::FLAC,
         &aConfig.mMP4},
        {nsCString(AUDIO_MP4), EME_CODEC_OPUS, java::MediaDrmProxy::OPUS,
         &aConfig.mMP4},
        {nsCString(VIDEO_WEBM), EME_CODEC_VP8, java::MediaDrmProxy::VP8,
         &aConfig.mWebM},
        {nsCString(VIDEO_WEBM), EME_CODEC_VP9, java::MediaDrmProxy::VP9,
         &aConfig.mWebM},
        {nsCString(AUDIO_WEBM), EME_CODEC_VORBIS, java::MediaDrmProxy::VORBIS,
         &aConfig.mWebM},
        {nsCString(AUDIO_WEBM), EME_CODEC_OPUS, java::MediaDrmProxy::OPUS,
         &aConfig.mWebM},
    };

    for (const auto& data : validationList) {
      if (java::MediaDrmProxy::IsCryptoSchemeSupported(kWidevineKeySystemName,
                                                       data.mMimeType)) {
        if (AndroidDecoderModule::SupportsMimeType(data.mMimeType) !=
            media::DecodeSupport::Unsupported) {
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
      aConfig.mMP4.SetCanDecrypt(EME_CODEC_AAC);
    }
#  else
    aConfig.mMP4.SetCanDecrypt(EME_CODEC_AAC);
#  endif
    aConfig.mMP4.SetCanDecrypt(EME_CODEC_FLAC);
    aConfig.mMP4.SetCanDecrypt(EME_CODEC_OPUS);
    aConfig.mMP4.SetCanDecryptAndDecode(EME_CODEC_H264);
    aConfig.mMP4.SetCanDecryptAndDecode(EME_CODEC_VP9);
    aConfig.mWebM.SetCanDecrypt(EME_CODEC_VORBIS);
    aConfig.mWebM.SetCanDecrypt(EME_CODEC_OPUS);
    aConfig.mWebM.SetCanDecryptAndDecode(EME_CODEC_VP8);
    aConfig.mWebM.SetCanDecryptAndDecode(EME_CODEC_VP9);
#endif
    return true;
  }
#ifdef MOZ_WMF_CDM
  if (IsPlayReadyKeySystemAndSupported(aKeySystem)) {
    RefPtr<WMFCDMImpl> cdm = MakeRefPtr<WMFCDMImpl>(aKeySystem);
    return cdm->GetCapabilities(aConfig);
  }
#endif
  return false;
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

}  // namespace mozilla
