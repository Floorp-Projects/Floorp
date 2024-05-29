/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeySystemAccess.h"

#include <functional>

#include "DecoderDoctorDiagnostics.h"
#include "DecoderTraits.h"
#include "MP4Decoder.h"
#include "MediaContainerType.h"
#include "WebMDecoder.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/KeySystemNames.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "mozilla/dom/MediaKeySystemAccessManager.h"
#include "mozilla/dom/MediaSource.h"
#include "nsDOMString.h"
#include "nsIObserverService.h"
#include "nsMimeTypes.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"

#ifdef XP_WIN
#  include "WMFDecoderModule.h"
#endif
#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/MediaDrmProxyWrappers.h"
#endif

namespace mozilla::dom {

#define LOG(msg, ...) \
  EME_LOG("MediaKeySystemAccess::%s " msg, __func__, ##__VA_ARGS__)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaKeySystemAccess, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeySystemAccess)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeySystemAccess)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeySystemAccess)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static nsCString ToCString(const MediaKeySystemConfiguration& aConfig);

MediaKeySystemAccess::MediaKeySystemAccess(
    nsPIDOMWindowInner* aParent, const nsAString& aKeySystem,
    const MediaKeySystemConfiguration& aConfig)
    : mParent(aParent), mKeySystem(aKeySystem), mConfig(aConfig) {
  LOG("Created MediaKeySystemAccess for keysystem=%s config=%s",
      NS_ConvertUTF16toUTF8(mKeySystem).get(),
      mozilla::dom::ToCString(mConfig).get());
}

MediaKeySystemAccess::~MediaKeySystemAccess() = default;

JSObject* MediaKeySystemAccess::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return MediaKeySystemAccess_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* MediaKeySystemAccess::GetParentObject() const {
  return mParent;
}

void MediaKeySystemAccess::GetKeySystem(nsString& aOutKeySystem) const {
  aOutKeySystem.Assign(mKeySystem);
}

void MediaKeySystemAccess::GetConfiguration(
    MediaKeySystemConfiguration& aConfig) {
  aConfig = mConfig;
}

already_AddRefed<Promise> MediaKeySystemAccess::CreateMediaKeys(
    ErrorResult& aRv) {
  RefPtr<MediaKeys> keys(new MediaKeys(mParent, mKeySystem, mConfig));
  return keys->Init(aRv);
}

enum class SecureLevel {
  Software,
  Hardware,
};

static MediaKeySystemStatus EnsureCDMInstalled(const nsAString& aKeySystem,
                                               const SecureLevel aSecure,
                                               nsACString& aOutMessage) {
  if (aSecure == SecureLevel::Software &&
      !KeySystemConfig::Supports(aKeySystem)) {
    aOutMessage = "CDM is not installed"_ns;
    return MediaKeySystemStatus::Cdm_not_installed;
  }

#ifdef MOZ_WMF_CDM
  if (aSecure == SecureLevel::Hardware) {
    // Ensure we check the hardware key system name.
    nsAutoString hardwareKeySystem;
    if (IsWidevineKeySystem(aKeySystem) ||
        IsWidevineExperimentKeySystemAndSupported(aKeySystem)) {
      hardwareKeySystem =
          NS_ConvertUTF8toUTF16(kWidevineExperimentKeySystemName);
    } else if (IsPlayReadyKeySystemAndSupported(aKeySystem)) {
      hardwareKeySystem = NS_ConvertUTF8toUTF16(kPlayReadyKeySystemHardware);
    } else {
      MOZ_ASSERT_UNREACHABLE("Not supported key system for HWDRM!");
    }
    if (!KeySystemConfig::Supports(hardwareKeySystem)) {
      aOutMessage = "CDM is not installed"_ns;
      return MediaKeySystemStatus::Cdm_not_installed;
    }
  }
#endif

  return MediaKeySystemStatus::Available;
}

/* static */
MediaKeySystemStatus MediaKeySystemAccess::GetKeySystemStatus(
    const MediaKeySystemAccessRequest& aRequest, nsACString& aOutMessage) {
  const nsString& keySystem = aRequest.mKeySystem;

  MOZ_ASSERT(StaticPrefs::media_eme_enabled() ||
             IsClearkeyKeySystem(keySystem));

  LOG("checking if CDM is installed or disabled for %s",
      NS_ConvertUTF16toUTF8(keySystem).get());
  if (IsClearkeyKeySystem(keySystem)) {
    return EnsureCDMInstalled(keySystem, SecureLevel::Software, aOutMessage);
  }

  // This is used to determine if we need to download Widevine L1.
  bool shouldCheckL1Installation = false;
#ifdef MOZ_WMF_CDM
  if (StaticPrefs::media_eme_widevine_experiment_enabled()) {
    shouldCheckL1Installation =
        CheckIfHarewareDRMConfigExists(aRequest.mConfigs) ||
        IsWidevineExperimentKeySystemAndSupported(keySystem);
  }
#endif

  // Check Widevine L3
  if (IsWidevineKeySystem(keySystem) && !shouldCheckL1Installation) {
    if (Preferences::GetBool("media.gmp-widevinecdm.visible", false)) {
      if (!Preferences::GetBool("media.gmp-widevinecdm.enabled", false)) {
        aOutMessage = "Widevine EME disabled"_ns;
        return MediaKeySystemStatus::Cdm_disabled;
      }
      return EnsureCDMInstalled(keySystem, SecureLevel::Software, aOutMessage);
#ifdef MOZ_WIDGET_ANDROID
    } else if (Preferences::GetBool("media.mediadrm-widevinecdm.visible",
                                    false)) {
      bool supported =
          mozilla::java::MediaDrmProxy::IsSchemeSupported(keySystem);
      if (!supported) {
        aOutMessage = nsLiteralCString(
            "KeySystem or Minimum API level not met for Widevine EME");
        return MediaKeySystemStatus::Cdm_not_supported;
      }
      return MediaKeySystemStatus::Available;
#endif
    }
  }

#ifdef MOZ_WMF_CDM
  // Check PlayReady, which is a built-in CDM on most Windows versions.
  if (IsPlayReadyKeySystemAndSupported(keySystem) &&
      KeySystemConfig::Supports(keySystem)) {
    return MediaKeySystemStatus::Available;
  }

  // Check Widevine L1. This can be a request for experimental key systems, or
  // for a normal key system with hardware robustness.
  if ((IsWidevineExperimentKeySystemAndSupported(keySystem) ||
       IsWidevineKeySystem(keySystem)) &&
      shouldCheckL1Installation) {
    // TODO : if L3 hasn't been installed as well, should we fallback to install
    // L3?
    if (!Preferences::GetBool("media.gmp-widevinecdm-l1.enabled", false)) {
      aOutMessage = "Widevine L1 EME disabled"_ns;
      return MediaKeySystemStatus::Cdm_disabled;
    }
    return EnsureCDMInstalled(keySystem, SecureLevel::Hardware, aOutMessage);
  }
#endif

  return MediaKeySystemStatus::Cdm_not_supported;
}

static KeySystemConfig::EMECodecString ToEMEAPICodecString(
    const nsString& aCodec) {
  if (IsAACCodecString(aCodec)) {
    return KeySystemConfig::EME_CODEC_AAC;
  }
  if (aCodec.EqualsLiteral("opus")) {
    return KeySystemConfig::EME_CODEC_OPUS;
  }
  if (aCodec.EqualsLiteral("vorbis")) {
    return KeySystemConfig::EME_CODEC_VORBIS;
  }
  if (aCodec.EqualsLiteral("flac")) {
    return KeySystemConfig::EME_CODEC_FLAC;
  }
  if (IsH264CodecString(aCodec)) {
    return KeySystemConfig::EME_CODEC_H264;
  }
  if (IsAV1CodecString(aCodec)) {
    return KeySystemConfig::EME_CODEC_AV1;
  }
  if (IsVP8CodecString(aCodec)) {
    return KeySystemConfig::EME_CODEC_VP8;
  }
  if (IsVP9CodecString(aCodec)) {
    return KeySystemConfig::EME_CODEC_VP9;
  }
#ifdef MOZ_WMF
  if (IsH265CodecString(aCodec)) {
    return KeySystemConfig::EME_CODEC_HEVC;
  }
#endif
  return ""_ns;
}

static RefPtr<KeySystemConfig::SupportedConfigsPromise>
GetSupportedKeySystemConfigs(const nsAString& aKeySystem,
                             bool aIsHardwareDecryption,
                             bool aIsPrivateBrowsing) {
  using DecryptionInfo = KeySystemConfig::DecryptionInfo;
  nsTArray<KeySystemConfigRequest> requests;

  // Software Widevine and Clearkey
  if (IsWidevineKeySystem(aKeySystem) || IsClearkeyKeySystem(aKeySystem)) {
    requests.AppendElement(KeySystemConfigRequest{
        aKeySystem, DecryptionInfo::Software, aIsPrivateBrowsing});
  }
#ifdef MOZ_WMF_CDM
  if (IsPlayReadyEnabled()) {
    // PlayReady software and hardware
    if (aKeySystem.EqualsLiteral(kPlayReadyKeySystemName) ||
        aKeySystem.EqualsLiteral(kPlayReadyKeySystemHardware)) {
      requests.AppendElement(
          KeySystemConfigRequest{NS_ConvertUTF8toUTF16(kPlayReadyKeySystemName),
                                 DecryptionInfo::Software, aIsPrivateBrowsing});
      if (aIsHardwareDecryption) {
        requests.AppendElement(KeySystemConfigRequest{
            NS_ConvertUTF8toUTF16(kPlayReadyKeySystemName),
            DecryptionInfo::Hardware, aIsPrivateBrowsing});
        requests.AppendElement(KeySystemConfigRequest{
            NS_ConvertUTF8toUTF16(kPlayReadyKeySystemHardware),
            DecryptionInfo::Hardware, aIsPrivateBrowsing});
      }
    }
    // PlayReady clearlead
    if (aKeySystem.EqualsLiteral(kPlayReadyHardwareClearLeadKeySystemName)) {
      requests.AppendElement(KeySystemConfigRequest{
          NS_ConvertUTF8toUTF16(kPlayReadyHardwareClearLeadKeySystemName),
          DecryptionInfo::Hardware, aIsPrivateBrowsing});
    }
  }

  if (IsWidevineHardwareDecryptionEnabled()) {
    // Widevine hardware
    if (aKeySystem.EqualsLiteral(kWidevineExperimentKeySystemName) ||
        (IsWidevineKeySystem(aKeySystem) && aIsHardwareDecryption)) {
      requests.AppendElement(KeySystemConfigRequest{
          NS_ConvertUTF8toUTF16(kWidevineExperimentKeySystemName),
          DecryptionInfo::Hardware, aIsPrivateBrowsing});
    }
    // Widevine clearlead
    if (aKeySystem.EqualsLiteral(kWidevineExperiment2KeySystemName)) {
      requests.AppendElement(KeySystemConfigRequest{
          NS_ConvertUTF8toUTF16(kWidevineExperiment2KeySystemName),
          DecryptionInfo::Hardware, aIsPrivateBrowsing});
    }
  }
#endif
  return KeySystemConfig::CreateKeySystemConfigs(requests);
}

/* static */
RefPtr<GenericPromise> MediaKeySystemAccess::KeySystemSupportsInitDataType(
    const nsAString& aKeySystem, const nsAString& aInitDataType,
    bool aIsHardwareDecryption, bool aIsPrivateBrowsing) {
  RefPtr<GenericPromise::Private> promise =
      new GenericPromise::Private(__func__);
  GetSupportedKeySystemConfigs(aKeySystem, aIsHardwareDecryption,
                               aIsPrivateBrowsing)
      ->Then(GetMainThreadSerialEventTarget(), __func__,
             [promise, initDataType = nsString{std::move(aInitDataType)}](
                 const KeySystemConfig::SupportedConfigsPromise::
                     ResolveOrRejectValue& aResult) {
               if (aResult.IsResolve()) {
                 for (const auto& config : aResult.ResolveValue()) {
                   if (config.mInitDataTypes.Contains(initDataType)) {
                     promise->Resolve(true, __func__);
                     return;
                   }
                 }
               }
               promise->Reject(NS_ERROR_DOM_MEDIA_CDM_ERR, __func__);
             });
  return promise.forget();
}

enum CodecType { Audio, Video, Invalid };

static bool CanDecryptAndDecode(
    const nsString& aKeySystem, const nsString& aContentType,
    CodecType aCodecType,
    const KeySystemConfig::ContainerSupport& aContainerSupport,
    const nsTArray<KeySystemConfig::EMECodecString>& aCodecs,
    const Maybe<CryptoScheme>& aScheme,
    DecoderDoctorDiagnostics* aDiagnostics) {
  MOZ_ASSERT(aCodecType != Invalid);
  for (const KeySystemConfig::EMECodecString& codec : aCodecs) {
    MOZ_ASSERT(!codec.IsEmpty());

    if (aContainerSupport.DecryptsAndDecodes(codec, aScheme)) {
      // GMP can decrypt-and-decode this codec.
      continue;
    }

    if (aContainerSupport.Decrypts(codec, aScheme)) {
      IgnoredErrorResult rv;
      MediaSource::IsTypeSupported(aContentType, aDiagnostics, rv);
      if (!rv.Failed()) {
        // GMP can decrypt and is allowed to return compressed samples to
        // Gecko to decode, and Gecko has a decoder.
        continue;
      }
    }

    // Neither the GMP nor Gecko can both decrypt and decode. We don't
    // support this codec.

#if defined(XP_WIN)
    // Widevine CDM doesn't include an AAC decoder. So if WMF can't
    // decode AAC, and a codec wasn't specified, be conservative
    // and reject the MediaKeys request, since we assume Widevine
    // will be used with AAC.
    if (codec == KeySystemConfig::EME_CODEC_AAC &&
        IsWidevineKeySystem(aKeySystem) &&
        !WMFDecoderModule::CanCreateMFTDecoder(WMFStreamType::AAC)) {
      if (aDiagnostics) {
        aDiagnostics->SetKeySystemIssue(
            DecoderDoctorDiagnostics::eWidevineWithNoWMF);
      }
    }
#endif
    return false;
  }
  return true;
}

// https://w3c.github.io/encrypted-media/#dom-mediakeysystemmediacapability-encryptionscheme
// This convert `encryptionScheme` to the type of CryptoScheme, so that we can
// further check whether the scheme is supported or not in our media pipeline.
Maybe<CryptoScheme> ConvertEncryptionSchemeStrToScheme(
    const nsString& aEncryptionScheme) {
  if (DOMStringIsNull(aEncryptionScheme)) {
    // "A missing or null value indicates that any encryption scheme is
    // acceptable."
    return Nothing();
  }
  auto scheme = StringToCryptoScheme(aEncryptionScheme);
  return Some(scheme);
}

static bool ToSessionType(const nsAString& aSessionType,
                          MediaKeySessionType& aOutType) {
  Maybe<MediaKeySessionType> type =
      StringToEnum<MediaKeySessionType>(aSessionType);
  if (type.isNothing()) {
    return false;
  }
  aOutType = type.value();
  return true;
}

// 5.1.1 Is persistent session type?
static bool IsPersistentSessionType(MediaKeySessionType aSessionType) {
  return aSessionType == MediaKeySessionType::Persistent_license;
}

static bool ContainsSessionType(
    const nsTArray<KeySystemConfig::SessionType>& aTypes,
    const MediaKeySessionType& aSessionType) {
  return (aSessionType == MediaKeySessionType::Persistent_license &&
          aTypes.Contains(KeySystemConfig::SessionType::PersistentLicense)) ||
         (aSessionType == MediaKeySessionType::Temporary &&
          aTypes.Contains(KeySystemConfig::SessionType::Temporary));
}

CodecType GetMajorType(const MediaMIMEType& aMIMEType) {
  if (aMIMEType.HasAudioMajorType()) {
    return Audio;
  }
  if (aMIMEType.HasVideoMajorType()) {
    return Video;
  }
  return Invalid;
}

static CodecType GetCodecType(const KeySystemConfig::EMECodecString& aCodec) {
  if (aCodec.Equals(KeySystemConfig::EME_CODEC_AAC) ||
      aCodec.Equals(KeySystemConfig::EME_CODEC_OPUS) ||
      aCodec.Equals(KeySystemConfig::EME_CODEC_VORBIS) ||
      aCodec.Equals(KeySystemConfig::EME_CODEC_FLAC)) {
    return Audio;
  }
  if (aCodec.Equals(KeySystemConfig::EME_CODEC_H264) ||
      aCodec.Equals(KeySystemConfig::EME_CODEC_AV1) ||
      aCodec.Equals(KeySystemConfig::EME_CODEC_VP8) ||
      aCodec.Equals(KeySystemConfig::EME_CODEC_VP9) ||
      aCodec.Equals(KeySystemConfig::EME_CODEC_HEVC)) {
    return Video;
  }
  return Invalid;
}

static bool AllCodecsOfType(
    const nsTArray<KeySystemConfig::EMECodecString>& aCodecs,
    const CodecType aCodecType) {
  for (const KeySystemConfig::EMECodecString& codec : aCodecs) {
    if (GetCodecType(codec) != aCodecType) {
      return false;
    }
  }
  return true;
}

static bool IsParameterUnrecognized(const nsAString& aContentType) {
  nsAutoString contentType(aContentType);
  contentType.StripWhitespace();

  nsTArray<nsString> params;
  nsAString::const_iterator start, end, semicolon, equalSign;
  contentType.BeginReading(start);
  contentType.EndReading(end);
  semicolon = start;
  // Find any substring between ';' & '='.
  while (semicolon != end) {
    if (FindCharInReadable(';', semicolon, end)) {
      equalSign = ++semicolon;
      if (FindCharInReadable('=', equalSign, end)) {
        params.AppendElement(Substring(semicolon, equalSign));
        semicolon = equalSign;
      }
    }
  }

  for (auto param : params) {
    if (!param.LowerCaseEqualsLiteral("codecs") &&
        !param.LowerCaseEqualsLiteral("profiles")) {
      return true;
    }
  }
  return false;
}

// 3.2.2.3 Get Supported Capabilities for Audio/Video Type
// https://w3c.github.io/encrypted-media/#get-supported-capabilities-for-audio-video-type
static Sequence<MediaKeySystemMediaCapability> GetSupportedCapabilities(
    const CodecType aCodecType,
    const nsTArray<MediaKeySystemMediaCapability>& aRequestedCapabilities,
    const MediaKeySystemConfiguration& aPartialConfig,
    const KeySystemConfig& aKeySystem, DecoderDoctorDiagnostics* aDiagnostics,
    const Document* aDocument) {
  // Let local accumulated configuration be a local copy of partial
  // configuration. (Note: It's not necessary for us to maintain a local copy,
  // as we don't need to test whether capabilites from previous calls to this
  // algorithm work with the capabilities currently being considered in this
  // call. )

  // Let supported media capabilities be an empty sequence of
  // MediaKeySystemMediaCapability dictionaries.
  Sequence<MediaKeySystemMediaCapability> supportedCapabilities;

  // For each requested media capability in requested media capabilities:
  for (const MediaKeySystemMediaCapability& capabilities :
       aRequestedCapabilities) {
    // Let content type be requested media capability's contentType member.
    const nsString& contentTypeString = capabilities.mContentType;
    // Let robustness be requested media capability's robustness member.
    const nsString& robustness = capabilities.mRobustness;
    // Optional encryption scheme extension, see
    // https://github.com/WICG/encrypted-media-encryption-scheme/blob/master/explainer.md
    // This will only be exposed to JS if
    // media.eme.encrypted-media-encryption-scheme.enabled is preffed on.
    const nsString encryptionScheme = capabilities.mEncryptionScheme;
    // If content type is the empty string, return null.
    if (contentTypeString.IsEmpty()) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s','%s') rejected; "
          "audio or video capability has empty contentType.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      return Sequence<MediaKeySystemMediaCapability>();
    }
    // If content type is an invalid or unrecognized MIME type, continue
    // to the next iteration.
    Maybe<MediaContainerType> maybeContainerType =
        MakeMediaContainerType(contentTypeString);
    if (!maybeContainerType) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
          "failed to parse contentTypeString as MIME type.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      continue;
    }
    const MediaContainerType& containerType = *maybeContainerType;
    bool invalid = false;
    nsTArray<KeySystemConfig::EMECodecString> codecs;
    for (const auto& codecString :
         containerType.ExtendedType().Codecs().Range()) {
      KeySystemConfig::EMECodecString emeCodec =
          ToEMEAPICodecString(nsString(codecString));
      if (emeCodec.IsEmpty()) {
        invalid = true;
        EME_LOG(
            "MediaKeySystemConfiguration (label='%s') "
            "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
            "'%s' is an invalid codec string.",
            NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
            NS_ConvertUTF16toUTF8(contentTypeString).get(),
            NS_ConvertUTF16toUTF8(robustness).get(),
            NS_ConvertUTF16toUTF8(encryptionScheme).get(),
            NS_ConvertUTF16toUTF8(codecString).get());
        break;
      }
      codecs.AppendElement(emeCodec);
    }
    if (invalid) {
      continue;
    }

    // If the user agent does not support container, continue to the next
    // iteration. The case-sensitivity of string comparisons is determined by
    // the appropriate RFC. (Note: Per RFC 6838 [RFC6838], "Both top-level type
    // and subtype names are case-insensitive."'. We're using
    // nsContentTypeParser and that is case-insensitive and converts all its
    // parameter outputs to lower case.)
    const bool supportedInMP4 =
        MP4Decoder::IsSupportedType(containerType, aDiagnostics);
    if (supportedInMP4 && !aKeySystem.mMP4.IsSupported()) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
          "MP4 requested but unsupported.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      continue;
    }
    const bool isWebM = WebMDecoder::IsSupportedType(containerType);
    if (isWebM && !aKeySystem.mWebM.IsSupported()) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s,'%s') unsupported; "
          "WebM requested but unsupported.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      continue;
    }
    if (!supportedInMP4 && !isWebM) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
          "Unsupported or unrecognized container requested.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      continue;
    }

    // Let parameters be the RFC 6381[RFC6381] parameters, if any, specified by
    // content type.
    // If the user agent does not recognize one or more parameters, continue to
    // the next iteration.
    if (IsParameterUnrecognized(contentTypeString)) {
      continue;
    }

    // Let media types be the set of codecs and codec constraints specified by
    // parameters. The case-sensitivity of string comparisons is determined by
    // the appropriate RFC or other specification.
    // (Note: codecs array is 'parameter').

    // If media types is empty:
    if (codecs.IsEmpty()) {
      // Log deprecation warning to encourage authors to not do this!
      DeprecationWarningLog(aDocument, "MediaEMENoCodecsDeprecatedWarning");
      // TODO: Remove this once we're sure it doesn't break the web.
      // If container normatively implies a specific set of codecs and codec
      // constraints: Let parameters be that set.
      if (supportedInMP4) {
        if (aCodecType == Audio) {
          codecs.AppendElement(KeySystemConfig::EME_CODEC_AAC);
        } else if (aCodecType == Video) {
          codecs.AppendElement(KeySystemConfig::EME_CODEC_H264);
        }
      } else if (isWebM) {
        if (aCodecType == Audio) {
          codecs.AppendElement(KeySystemConfig::EME_CODEC_VORBIS);
        } else if (aCodecType == Video) {
          codecs.AppendElement(KeySystemConfig::EME_CODEC_VP8);
        }
      }
      // Otherwise: Continue to the next iteration.
      // (Note: all containers we support have implied codecs, so don't continue
      // here.)
    }

    // If container type is not strictly a audio/video type, continue to the
    // next iteration.
    const auto majorType = GetMajorType(containerType.Type());
    if (majorType == Invalid) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
          "MIME type is not an audio or video MIME type.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      continue;
    }
    if (majorType != aCodecType || !AllCodecsOfType(codecs, aCodecType)) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
          "MIME type mixes audio codecs in video capabilities "
          "or video codecs in audio capabilities.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      continue;
    }
    // If encryption scheme is non-null and is not recognized or not supported
    // by implementation, continue to the next iteration.
    const auto scheme = ConvertEncryptionSchemeStrToScheme(encryptionScheme);
    if (scheme && *scheme == CryptoScheme::None) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
          "unsupported scheme string.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      continue;
    }
    // If robustness is not the empty string and contains an unrecognized
    // value or a value not supported by implementation, continue to the
    // next iteration. String comparison is case-sensitive.
    if (!robustness.IsEmpty()) {
      if (majorType == Audio &&
          !aKeySystem.mAudioRobustness.Contains(robustness)) {
        EME_LOG(
            "MediaKeySystemConfiguration (label='%s') "
            "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
            "unsupported robustness string.",
            NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
            NS_ConvertUTF16toUTF8(contentTypeString).get(),
            NS_ConvertUTF16toUTF8(robustness).get(),
            NS_ConvertUTF16toUTF8(encryptionScheme).get());
        continue;
      }
      if (majorType == Video &&
          !aKeySystem.mVideoRobustness.Contains(robustness)) {
        EME_LOG(
            "MediaKeySystemConfiguration (label='%s') "
            "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
            "unsupported robustness string.",
            NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
            NS_ConvertUTF16toUTF8(contentTypeString).get(),
            NS_ConvertUTF16toUTF8(robustness).get(),
            NS_ConvertUTF16toUTF8(encryptionScheme).get());
        continue;
      }
      // Note: specified robustness requirements are satisfied.
    }

    // If the user agent and implementation definitely support playback of
    // encrypted media data for the combination of container, media types,
    // robustness and local accumulated configuration in combination with
    // restrictions...
    const auto& containerSupport =
        supportedInMP4 ? aKeySystem.mMP4 : aKeySystem.mWebM;
    if (!CanDecryptAndDecode(aKeySystem.mKeySystem, contentTypeString,
                             majorType, containerSupport, codecs, scheme,
                             aDiagnostics)) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') "
          "MediaKeySystemMediaCapability('%s','%s','%s') unsupported; "
          "codec unsupported by CDM requested.",
          NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
          NS_ConvertUTF16toUTF8(contentTypeString).get(),
          NS_ConvertUTF16toUTF8(robustness).get(),
          NS_ConvertUTF16toUTF8(encryptionScheme).get());
      continue;
    }

    // ... add requested media capability to supported media capabilities.
    if (!supportedCapabilities.AppendElement(capabilities, mozilla::fallible)) {
      NS_WARNING("GetSupportedCapabilities: Malloc failure");
      return Sequence<MediaKeySystemMediaCapability>();
    }

    // Note: omitting steps 3.13.2, our robustness is not sophisticated enough
    // to require considering all requirements together.
  }
  return supportedCapabilities;
}

// "Get Supported Configuration and Consent" algorithm, steps 4-7 for
// distinctive identifier, and steps 8-11 for persistent state. The steps
// are the same for both requirements/features, so we factor them out into
// a single function.
static bool CheckRequirement(
    const MediaKeysRequirement aRequirement,
    const KeySystemConfig::Requirement aKeySystemRequirement,
    MediaKeysRequirement& aOutRequirement) {
  // Let requirement be the value of candidate configuration's member.
  MediaKeysRequirement requirement = aRequirement;
  // If requirement is "optional" and feature is not allowed according to
  // restrictions, set requirement to "not-allowed".
  if (aRequirement == MediaKeysRequirement::Optional &&
      aKeySystemRequirement == KeySystemConfig::Requirement::NotAllowed) {
    requirement = MediaKeysRequirement::Not_allowed;
  }

  // Follow the steps for requirement from the following list:
  switch (requirement) {
    case MediaKeysRequirement::Required: {
      // If the implementation does not support use of requirement in
      // combination with accumulated configuration and restrictions, return
      // NotSupported.
      if (aKeySystemRequirement == KeySystemConfig::Requirement::NotAllowed) {
        return false;
      }
      break;
    }
    case MediaKeysRequirement::Optional: {
      // Continue with the following steps.
      break;
    }
    case MediaKeysRequirement::Not_allowed: {
      // If the implementation requires use of feature in combination with
      // accumulated configuration and restrictions, return NotSupported.
      if (aKeySystemRequirement == KeySystemConfig::Requirement::Required) {
        return false;
      }
      break;
    }
    default: {
      return false;
    }
  }

  // Set the requirement member of accumulated configuration to equal
  // calculated requirement.
  aOutRequirement = requirement;

  return true;
}

// 3.1.1.2, step 12
// Follow the steps for the first matching condition from the following list:
// If the sessionTypes member is present in candidate configuration.
// Let session types be candidate configuration's sessionTypes member.
// Otherwise let session types be ["temporary"].
// Note: This returns an empty array on malloc failure.
static Sequence<nsString> UnboxSessionTypes(
    const Optional<Sequence<nsString>>& aSessionTypes) {
  Sequence<nsString> sessionTypes;
  if (aSessionTypes.WasPassed()) {
    sessionTypes = aSessionTypes.Value();
  } else {
    // Note: fallible. Results in an empty array.
    (void)sessionTypes.AppendElement(ToString(MediaKeySessionType::Temporary),
                                     mozilla::fallible);
  }
  return sessionTypes;
}

// 3.1.1.2 Get Supported Configuration and Consent
static bool GetSupportedConfig(const KeySystemConfig& aKeySystem,
                               const MediaKeySystemConfiguration& aCandidate,
                               MediaKeySystemConfiguration& aOutConfig,
                               DecoderDoctorDiagnostics* aDiagnostics,
                               const Document* aDocument) {
  EME_LOG("Compare implementation '%s'\n with request '%s'",
          NS_ConvertUTF16toUTF8(aKeySystem.GetDebugInfo()).get(),
          ToCString(aCandidate).get());
  // Let accumulated configuration be a new MediaKeySystemConfiguration
  // dictionary.
  MediaKeySystemConfiguration config;
  // Set the label member of accumulated configuration to equal the label member
  // of candidate configuration.
  config.mLabel = aCandidate.mLabel;
  // If the initDataTypes member of candidate configuration is non-empty, run
  // the following steps:
  if (!aCandidate.mInitDataTypes.IsEmpty()) {
    // Let supported types be an empty sequence of DOMStrings.
    nsTArray<nsString> supportedTypes;
    // For each value in candidate configuration's initDataTypes member:
    for (const nsString& initDataType : aCandidate.mInitDataTypes) {
      // Let initDataType be the value.
      // If the implementation supports generating requests based on
      // initDataType, add initDataType to supported types. String comparison is
      // case-sensitive. The empty string is never supported.
      if (aKeySystem.mInitDataTypes.Contains(initDataType)) {
        supportedTypes.AppendElement(initDataType);
      }
    }
    // If supported types is empty, return NotSupported.
    if (supportedTypes.IsEmpty()) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') rejected; "
          "no supported initDataTypes provided.",
          NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // Set the initDataTypes member of accumulated configuration to supported
    // types.
    if (!config.mInitDataTypes.Assign(supportedTypes)) {
      return false;
    }
  }

  if (!CheckRequirement(aCandidate.mDistinctiveIdentifier,
                        aKeySystem.mDistinctiveIdentifier,
                        config.mDistinctiveIdentifier)) {
    EME_LOG(
        "MediaKeySystemConfiguration (label='%s') rejected; "
        "distinctiveIdentifier requirement not satisfied.",
        NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
    return false;
  }

  if (!CheckRequirement(aCandidate.mPersistentState,
                        aKeySystem.mPersistentState, config.mPersistentState)) {
    EME_LOG(
        "MediaKeySystemConfiguration (label='%s') rejected; "
        "persistentState requirement not satisfied.",
        NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
    return false;
  }

  Sequence<nsString> sessionTypes(UnboxSessionTypes(aCandidate.mSessionTypes));
  if (sessionTypes.IsEmpty()) {
    // Malloc failure.
    return false;
  }

  // For each value in session types:
  for (const auto& sessionTypeString : sessionTypes) {
    // Let session type be the value.
    MediaKeySessionType sessionType;
    if (!ToSessionType(sessionTypeString, sessionType)) {
      // (Assume invalid sessionType is unsupported as per steps below).
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') rejected; "
          "invalid session type specified.",
          NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // If accumulated configuration's persistentState value is "not-allowed"
    // and the Is persistent session type? algorithm returns true for session
    // type return NotSupported.
    if (config.mPersistentState == MediaKeysRequirement::Not_allowed &&
        IsPersistentSessionType(sessionType)) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') rejected; "
          "persistent session requested but keysystem doesn't"
          "support persistent state.",
          NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // If the implementation does not support session type in combination
    // with accumulated configuration and restrictions for other reasons,
    // return NotSupported.
    if (!ContainsSessionType(aKeySystem.mSessionTypes, sessionType)) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') rejected; "
          "session type '%s' unsupported by keySystem.",
          NS_ConvertUTF16toUTF8(aCandidate.mLabel).get(),
          NS_ConvertUTF16toUTF8(sessionTypeString).get());
      return false;
    }
    // If accumulated configuration's persistentState value is "optional"
    // and the result of running the Is persistent session type? algorithm
    // on session type is true, change accumulated configuration's
    // persistentState value to "required".
    if (config.mPersistentState == MediaKeysRequirement::Optional &&
        IsPersistentSessionType(sessionType)) {
      config.mPersistentState = MediaKeysRequirement::Required;
    }
  }
  // Set the sessionTypes member of accumulated configuration to session types.
  config.mSessionTypes.Construct(std::move(sessionTypes));

  // If the videoCapabilities and audioCapabilities members in candidate
  // configuration are both empty, return NotSupported.
  if (aCandidate.mAudioCapabilities.IsEmpty() &&
      aCandidate.mVideoCapabilities.IsEmpty()) {
    // TODO: Most sites using EME still don't pass capabilities, so we
    // can't reject on it yet without breaking them. So add this later.
    // Log deprecation warning to encourage authors to not do this!
    DeprecationWarningLog(aDocument, "MediaEMENoCapabilitiesDeprecatedWarning");
  }

  // If the videoCapabilities member in candidate configuration is non-empty:
  if (!aCandidate.mVideoCapabilities.IsEmpty()) {
    // Let video capabilities be the result of executing the Get Supported
    // Capabilities for Audio/Video Type algorithm on Video, candidate
    // configuration's videoCapabilities member, accumulated configuration,
    // and restrictions.
    Sequence<MediaKeySystemMediaCapability> caps =
        GetSupportedCapabilities(Video, aCandidate.mVideoCapabilities, config,
                                 aKeySystem, aDiagnostics, aDocument);
    // If video capabilities is null, return NotSupported.
    if (caps.IsEmpty()) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') rejected; "
          "no supported video capabilities.",
          NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // Set the videoCapabilities member of accumulated configuration to video
    // capabilities.
    config.mVideoCapabilities = std::move(caps);
  } else {
    // Otherwise:
    // Set the videoCapabilities member of accumulated configuration to an empty
    // sequence.
  }

  // If the audioCapabilities member in candidate configuration is non-empty:
  if (!aCandidate.mAudioCapabilities.IsEmpty()) {
    // Let audio capabilities be the result of executing the Get Supported
    // Capabilities for Audio/Video Type algorithm on Audio, candidate
    // configuration's audioCapabilities member, accumulated configuration, and
    // restrictions.
    Sequence<MediaKeySystemMediaCapability> caps =
        GetSupportedCapabilities(Audio, aCandidate.mAudioCapabilities, config,
                                 aKeySystem, aDiagnostics, aDocument);
    // If audio capabilities is null, return NotSupported.
    if (caps.IsEmpty()) {
      EME_LOG(
          "MediaKeySystemConfiguration (label='%s') rejected; "
          "no supported audio capabilities.",
          NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // Set the audioCapabilities member of accumulated configuration to audio
    // capabilities.
    config.mAudioCapabilities = std::move(caps);
  } else {
    // Otherwise:
    // Set the audioCapabilities member of accumulated configuration to an empty
    // sequence.
  }

  // If accumulated configuration's distinctiveIdentifier value is "optional",
  // follow the steps for the first matching condition from the following list:
  if (config.mDistinctiveIdentifier == MediaKeysRequirement::Optional) {
    // If the implementation requires use Distinctive Identifier(s) or
    // Distinctive Permanent Identifier(s) for any of the combinations
    // in accumulated configuration
    if (aKeySystem.mDistinctiveIdentifier ==
        KeySystemConfig::Requirement::Required) {
      // Change accumulated configuration's distinctiveIdentifier value to
      // "required".
      config.mDistinctiveIdentifier = MediaKeysRequirement::Required;
    } else {
      // Otherwise, change accumulated configuration's distinctiveIdentifier
      // value to "not-allowed".
      config.mDistinctiveIdentifier = MediaKeysRequirement::Not_allowed;
    }
  }

  // If accumulated configuration's persistentState value is "optional", follow
  // the steps for the first matching condition from the following list:
  if (config.mPersistentState == MediaKeysRequirement::Optional) {
    // If the implementation requires persisting state for any of the
    // combinations in accumulated configuration
    if (aKeySystem.mPersistentState == KeySystemConfig::Requirement::Required) {
      // Change accumulated configuration's persistentState value to "required".
      config.mPersistentState = MediaKeysRequirement::Required;
    } else {
      // Otherwise, change accumulated configuration's persistentState
      // value to "not-allowed".
      config.mPersistentState = MediaKeysRequirement::Not_allowed;
    }
  }

  // Note: Omitting steps 20-22. We don't ask for consent.

#if defined(XP_WIN)
  // Widevine CDM doesn't include an AAC decoder. So if WMF can't decode AAC,
  // and a codec wasn't specified, be conservative and reject the MediaKeys
  // request.
  if (IsWidevineKeySystem(aKeySystem.mKeySystem) &&
      (aCandidate.mAudioCapabilities.IsEmpty() ||
       aCandidate.mVideoCapabilities.IsEmpty()) &&
      !WMFDecoderModule::CanCreateMFTDecoder(WMFStreamType::AAC)) {
    if (aDiagnostics) {
      aDiagnostics->SetKeySystemIssue(
          DecoderDoctorDiagnostics::eWidevineWithNoWMF);
    }
    EME_LOG(
        "MediaKeySystemConfiguration (label='%s') rejected; "
        "WMF required for Widevine decoding, but it's not available.",
        NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
    return false;
  }
#endif

  // Return accumulated configuration.
  aOutConfig = config;

  return true;
}

/* static */
RefPtr<KeySystemConfig::KeySystemConfigPromise>
MediaKeySystemAccess::GetSupportedConfig(MediaKeySystemAccessRequest* aRequest,
                                         bool aIsPrivateBrowsing,
                                         const Document* aDocument) {
  nsTArray<KeySystemConfig> implementations;
  const bool isHardwareDecryptionRequest =
      CheckIfHarewareDRMConfigExists(aRequest->mConfigs) ||
      DoesKeySystemSupportHardwareDecryption(aRequest->mKeySystem);

  RefPtr<KeySystemConfig::KeySystemConfigPromise::Private> promise =
      new KeySystemConfig::KeySystemConfigPromise::Private(__func__);
  GetSupportedKeySystemConfigs(aRequest->mKeySystem,
                               isHardwareDecryptionRequest, aIsPrivateBrowsing)
      ->Then(GetMainThreadSerialEventTarget(), __func__,
             [promise, aRequest, document = RefPtr<const Document>{aDocument}](
                 const KeySystemConfig::SupportedConfigsPromise::
                     ResolveOrRejectValue& aResult) {
               if (aResult.IsResolve()) {
                 MediaKeySystemConfiguration outConfig;
                 for (const auto& implementation : aResult.ResolveValue()) {
                   for (const MediaKeySystemConfiguration& candidate :
                        aRequest->mConfigs) {
                     if (mozilla::dom::GetSupportedConfig(
                             implementation, candidate, outConfig,
                             &aRequest->mDiagnostics, document)) {
                       promise->Resolve(std::move(outConfig), __func__);
                       return;
                     }
                   }
                 }
               }
               promise->Reject(false, __func__);
             });
  return promise.forget();
}

/* static */
void MediaKeySystemAccess::NotifyObservers(nsPIDOMWindowInner* aWindow,
                                           const nsAString& aKeySystem,
                                           MediaKeySystemStatus aStatus) {
  RequestMediaKeySystemAccessNotification data;
  data.mKeySystem = aKeySystem;
  data.mStatus = aStatus;
  nsAutoString json;
  data.ToJSON(json);
  EME_LOG("MediaKeySystemAccess::NotifyObservers() %s",
          NS_ConvertUTF16toUTF8(json).get());
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(aWindow, MediaKeys::kMediaKeysRequestTopic,
                         json.get());
  }
}

static nsCString ToCString(const nsString& aString) {
  nsCString str("'");
  str.Append(NS_ConvertUTF16toUTF8(aString));
  str.AppendLiteral("'");
  return str;
}

static nsCString ToCString(const MediaKeysRequirement aValue) {
  nsCString str("'");
  str.AppendASCII(GetEnumString(aValue));
  str.AppendLiteral("'");
  return str;
}

static nsCString ToCString(const MediaKeySystemMediaCapability& aValue) {
  nsCString str;
  str.AppendLiteral("{contentType=");
  str.Append(ToCString(aValue.mContentType));
  str.AppendLiteral(", robustness=");
  str.Append(ToCString(aValue.mRobustness));
  str.AppendLiteral(", encryptionScheme=");
  str.Append(ToCString(aValue.mEncryptionScheme));
  str.AppendLiteral("}");
  return str;
}

template <class Type>
static nsCString ToCString(const Sequence<Type>& aSequence) {
  nsCString str;
  str.AppendLiteral("[");
  StringJoinAppend(str, ","_ns, aSequence,
                   [](nsACString& dest, const Type& element) {
                     dest.Append(ToCString(element));
                   });
  str.AppendLiteral("]");
  return str;
}

template <class Type>
static nsCString ToCString(const Optional<Sequence<Type>>& aOptional) {
  nsCString str;
  if (aOptional.WasPassed()) {
    str.Append(ToCString(aOptional.Value()));
  } else {
    str.AppendLiteral("[]");
  }
  return str;
}

static nsCString ToCString(const MediaKeySystemConfiguration& aConfig) {
  nsCString str;
  str.AppendLiteral("{label=");
  str.Append(ToCString(aConfig.mLabel));

  str.AppendLiteral(", initDataTypes=");
  str.Append(ToCString(aConfig.mInitDataTypes));

  str.AppendLiteral(", audioCapabilities=");
  str.Append(ToCString(aConfig.mAudioCapabilities));

  str.AppendLiteral(", videoCapabilities=");
  str.Append(ToCString(aConfig.mVideoCapabilities));

  str.AppendLiteral(", distinctiveIdentifier=");
  str.Append(ToCString(aConfig.mDistinctiveIdentifier));

  str.AppendLiteral(", persistentState=");
  str.Append(ToCString(aConfig.mPersistentState));

  str.AppendLiteral(", sessionTypes=");
  str.Append(ToCString(aConfig.mSessionTypes));

  str.AppendLiteral("}");

  return str;
}

/* static */
nsCString MediaKeySystemAccess::ToCString(
    const Sequence<MediaKeySystemConfiguration>& aConfig) {
  return mozilla::dom::ToCString(aConfig);
}

#undef LOG

}  // namespace mozilla::dom
