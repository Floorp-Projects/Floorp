/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeySystemAccess.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/Preferences.h"
#include "MediaContainerType.h"
#include "MediaPrefs.h"
#ifdef MOZ_FMP4
#include "MP4Decoder.h"
#endif
#ifdef XP_WIN
#include "WMFDecoderModule.h"
#endif
#include "nsContentCID.h"
#include "nsServiceManagerUtils.h"
#include "mozIGeckoMediaPluginService.h"
#include "VideoUtils.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "mozilla/EMEUtils.h"
#include "GMPUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "DecoderDoctorDiagnostics.h"
#include "WebMDecoder.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsUnicharUtils.h"
#include "mozilla/dom/MediaSource.h"
#ifdef MOZ_WIDGET_ANDROID
#include "FennecJNIWrappers.h"
#endif

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaKeySystemAccess,
                                      mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeySystemAccess)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeySystemAccess)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeySystemAccess)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static nsCString
ToCString(const MediaKeySystemConfiguration& aConfig);

MediaKeySystemAccess::MediaKeySystemAccess(nsPIDOMWindowInner* aParent,
                                           const nsAString& aKeySystem,
                                           const MediaKeySystemConfiguration& aConfig)
  : mParent(aParent)
  , mKeySystem(aKeySystem)
  , mConfig(aConfig)
{
  EME_LOG("Created MediaKeySystemAccess for keysystem=%s config=%s",
          NS_ConvertUTF16toUTF8(mKeySystem).get(), mozilla::dom::ToCString(mConfig).get());
}

MediaKeySystemAccess::~MediaKeySystemAccess()
{
}

JSObject*
MediaKeySystemAccess::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaKeySystemAccessBinding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner*
MediaKeySystemAccess::GetParentObject() const
{
  return mParent;
}

void
MediaKeySystemAccess::GetKeySystem(nsString& aOutKeySystem) const
{
  aOutKeySystem.Assign(mKeySystem);
}

void
MediaKeySystemAccess::GetConfiguration(MediaKeySystemConfiguration& aConfig)
{
  aConfig = mConfig;
}

already_AddRefed<Promise>
MediaKeySystemAccess::CreateMediaKeys(ErrorResult& aRv)
{
  RefPtr<MediaKeys> keys(new MediaKeys(mParent,
                                       mKeySystem,
                                       mConfig));
  return keys->Init(aRv);
}

static bool
HavePluginForKeySystem(const nsCString& aKeySystem)
{
  nsCString api = MediaPrefs::EMEChromiumAPIEnabled()
                    ? NS_LITERAL_CSTRING(CHROMIUM_CDM_API)
                    : NS_LITERAL_CSTRING(GMP_API_DECRYPTOR);

  bool havePlugin = HaveGMPFor(api, { aKeySystem });
#ifdef MOZ_WIDGET_ANDROID
  // Check if we can use MediaDrm for this keysystem.
  if (!havePlugin) {
    havePlugin = mozilla::java::MediaDrmProxy::IsSchemeSupported(aKeySystem);
  }
#endif
  return havePlugin;
}

static MediaKeySystemStatus
EnsureCDMInstalled(const nsAString& aKeySystem,
                    nsACString& aOutMessage)
{
  if (!HavePluginForKeySystem(NS_ConvertUTF16toUTF8(aKeySystem))) {
    aOutMessage = NS_LITERAL_CSTRING("CDM is not installed");
    return MediaKeySystemStatus::Cdm_not_installed;
  }

  return MediaKeySystemStatus::Available;
}

/* static */
MediaKeySystemStatus
MediaKeySystemAccess::GetKeySystemStatus(const nsAString& aKeySystem,
                                         nsACString& aOutMessage)
{
  MOZ_ASSERT(MediaPrefs::EMEEnabled() || IsClearkeyKeySystem(aKeySystem));

  if (IsClearkeyKeySystem(aKeySystem)) {
    return EnsureCDMInstalled(aKeySystem, aOutMessage);
  }

  if (IsWidevineKeySystem(aKeySystem)) {
    if (Preferences::GetBool("media.gmp-widevinecdm.visible", false)) {
      if (!Preferences::GetBool("media.gmp-widevinecdm.enabled", false)) {
        aOutMessage = NS_LITERAL_CSTRING("Widevine EME disabled");
        return MediaKeySystemStatus::Cdm_disabled;
      }
      return EnsureCDMInstalled(aKeySystem, aOutMessage);
#ifdef MOZ_WIDGET_ANDROID
    } else if (Preferences::GetBool("media.mediadrm-widevinecdm.visible", false)) {
        nsCString keySystem = NS_ConvertUTF16toUTF8(aKeySystem);
        bool supported = mozilla::java::MediaDrmProxy::IsSchemeSupported(keySystem);
        if (!supported) {
          aOutMessage = NS_LITERAL_CSTRING("KeySystem or Minimum API level not met for Widevine EME");
          return MediaKeySystemStatus::Cdm_not_supported;
        }
        return MediaKeySystemStatus::Available;
#endif
    }
  }

  return MediaKeySystemStatus::Cdm_not_supported;
}

typedef nsCString EMECodecString;

static NS_NAMED_LITERAL_CSTRING(EME_CODEC_AAC, "aac");
static NS_NAMED_LITERAL_CSTRING(EME_CODEC_OPUS, "opus");
static NS_NAMED_LITERAL_CSTRING(EME_CODEC_VORBIS, "vorbis");
static NS_NAMED_LITERAL_CSTRING(EME_CODEC_H264, "h264");
static NS_NAMED_LITERAL_CSTRING(EME_CODEC_VP8, "vp8");
static NS_NAMED_LITERAL_CSTRING(EME_CODEC_VP9, "vp9");

EMECodecString
ToEMEAPICodecString(const nsString& aCodec)
{
  if (IsAACCodecString(aCodec)) {
    return EME_CODEC_AAC;
  }
  if (aCodec.EqualsLiteral("opus")) {
    return EME_CODEC_OPUS;
  }
  if (aCodec.EqualsLiteral("vorbis")) {
    return EME_CODEC_VORBIS;
  }
  if (IsH264CodecString(aCodec)) {
    return EME_CODEC_H264;
  }
  if (IsVP8CodecString(aCodec)) {
    return EME_CODEC_VP8;
  }
  if (IsVP9CodecString(aCodec)) {
    return EME_CODEC_VP9;
  }
  return EmptyCString();
}

// A codec can be decrypted-and-decoded by the CDM, or only decrypted
// by the CDM and decoded by Gecko. Not both.
struct KeySystemContainerSupport
{
  bool IsSupported() const
  {
    return !mCodecsDecoded.IsEmpty() || !mCodecsDecrypted.IsEmpty();
  }

  // CDM decrypts and decodes using a DRM robust decoder, and passes decoded
  // samples back to Gecko for rendering.
  bool DecryptsAndDecodes(EMECodecString aCodec) const
  {
    return mCodecsDecoded.Contains(aCodec);
  }

  // CDM decrypts and passes the decrypted samples back to Gecko for decoding.
  bool Decrypts(EMECodecString aCodec) const
  {
    return mCodecsDecrypted.Contains(aCodec);
  }

  void SetCanDecryptAndDecode(EMECodecString aCodec)
  {
    // Can't both decrypt and decrypt-and-decode a codec.
    MOZ_ASSERT(!Decrypts(aCodec));
    // Prevent duplicates.
    MOZ_ASSERT(!DecryptsAndDecodes(aCodec));
    mCodecsDecoded.AppendElement(aCodec);
  }

  void SetCanDecrypt(EMECodecString aCodec)
  {
    // Prevent duplicates.
    MOZ_ASSERT(!Decrypts(aCodec));
    // Can't both decrypt and decrypt-and-decode a codec.
    MOZ_ASSERT(!DecryptsAndDecodes(aCodec));
    mCodecsDecrypted.AppendElement(aCodec);
  }

private:
  nsTArray<EMECodecString> mCodecsDecoded;
  nsTArray<EMECodecString> mCodecsDecrypted;
};

enum class KeySystemFeatureSupport
{
  Prohibited = 1,
  Requestable = 2,
  Required = 3,
};

struct KeySystemConfig
{
  nsString mKeySystem;
  nsTArray<nsString> mInitDataTypes;
  KeySystemFeatureSupport mPersistentState = KeySystemFeatureSupport::Prohibited;
  KeySystemFeatureSupport mDistinctiveIdentifier = KeySystemFeatureSupport::Prohibited;
  nsTArray<MediaKeySessionType> mSessionTypes;
  nsTArray<nsString> mVideoRobustness;
  nsTArray<nsString> mAudioRobustness;
  KeySystemContainerSupport mMP4;
  KeySystemContainerSupport mWebM;
};

static nsTArray<KeySystemConfig>
GetSupportedKeySystems()
{
  nsTArray<KeySystemConfig> keySystemConfigs;

  {
    if (HavePluginForKeySystem(kEMEKeySystemClearkey)) {
      KeySystemConfig clearkey;
      clearkey.mKeySystem = NS_ConvertUTF8toUTF16(kEMEKeySystemClearkey);
      clearkey.mInitDataTypes.AppendElement(NS_LITERAL_STRING("cenc"));
      clearkey.mInitDataTypes.AppendElement(NS_LITERAL_STRING("keyids"));
      clearkey.mInitDataTypes.AppendElement(NS_LITERAL_STRING("webm"));
      clearkey.mPersistentState = KeySystemFeatureSupport::Requestable;
      clearkey.mDistinctiveIdentifier = KeySystemFeatureSupport::Prohibited;
      clearkey.mSessionTypes.AppendElement(MediaKeySessionType::Temporary);
      if (MediaPrefs::ClearKeyPersistentLicenseEnabled()) {
        clearkey.mSessionTypes.AppendElement(MediaKeySessionType::Persistent_license);
      }
#if defined(XP_WIN)
      // Clearkey CDM uses WMF's H.264 decoder on Windows.
      if (WMFDecoderModule::HasH264()) {
        clearkey.mMP4.SetCanDecryptAndDecode(EME_CODEC_H264);
      } else {
        clearkey.mMP4.SetCanDecrypt(EME_CODEC_H264);
      }
#else
      clearkey.mMP4.SetCanDecrypt(EME_CODEC_H264);
#endif
      clearkey.mMP4.SetCanDecrypt(EME_CODEC_AAC);
      if (Preferences::GetBool("media.eme.vp9-in-mp4.enabled", false)) {
        clearkey.mMP4.SetCanDecrypt(EME_CODEC_VP9);
      }
      clearkey.mWebM.SetCanDecrypt(EME_CODEC_VORBIS);
      clearkey.mWebM.SetCanDecrypt(EME_CODEC_OPUS);
      clearkey.mWebM.SetCanDecrypt(EME_CODEC_VP8);
      clearkey.mWebM.SetCanDecrypt(EME_CODEC_VP9);
      keySystemConfigs.AppendElement(Move(clearkey));
    }
  }
  {
    if (HavePluginForKeySystem(kEMEKeySystemWidevine)) {
      KeySystemConfig widevine;
      widevine.mKeySystem = NS_ConvertUTF8toUTF16(kEMEKeySystemWidevine);
      widevine.mInitDataTypes.AppendElement(NS_LITERAL_STRING("cenc"));
      widevine.mInitDataTypes.AppendElement(NS_LITERAL_STRING("keyids"));
      widevine.mInitDataTypes.AppendElement(NS_LITERAL_STRING("webm"));
      widevine.mPersistentState = KeySystemFeatureSupport::Requestable;
      widevine.mDistinctiveIdentifier = KeySystemFeatureSupport::Prohibited;
      widevine.mSessionTypes.AppendElement(MediaKeySessionType::Temporary);
#ifdef MOZ_WIDGET_ANDROID
      widevine.mSessionTypes.AppendElement(MediaKeySessionType::Persistent_license);
#endif
      widevine.mAudioRobustness.AppendElement(NS_LITERAL_STRING("SW_SECURE_CRYPTO"));
      widevine.mVideoRobustness.AppendElement(NS_LITERAL_STRING("SW_SECURE_CRYPTO"));
      widevine.mVideoRobustness.AppendElement(NS_LITERAL_STRING("SW_SECURE_DECODE"));
#if defined(XP_WIN)
      // Widevine CDM doesn't include an AAC decoder. So if WMF can't
      // decode AAC, and a codec wasn't specified, be conservative
      // and reject the MediaKeys request, since we assume Widevine
      // will be used with AAC.
      if (WMFDecoderModule::HasAAC()) {
        widevine.mMP4.SetCanDecrypt(EME_CODEC_AAC);
      }
#elif !defined(MOZ_WIDGET_ANDROID)
      widevine.mMP4.SetCanDecrypt(EME_CODEC_AAC);
#endif

#if defined(MOZ_WIDGET_ANDROID)
      using namespace mozilla::java;
      // MediaDrm.isCryptoSchemeSupported only allows passing
      // "video/mp4" or "video/webm" for mimetype string.
      // See https://developer.android.com/reference/android/media/MediaDrm.html#isCryptoSchemeSupported(java.util.UUID, java.lang.String)
      // for more detail.
      typedef struct {
        const nsCString& mMimeType;
        const nsCString& mEMECodecType;
        const char16_t* mCodecType;
        KeySystemContainerSupport* mSupportType;
      } DataForValidation;

      DataForValidation validationList[] = {
        { nsCString("video/mp4"), EME_CODEC_H264, MediaDrmProxy::AVC, &widevine.mMP4 },
        { nsCString("video/mp4"), EME_CODEC_VP9, MediaDrmProxy::AVC, &widevine.mMP4 },
        { nsCString("audio/mp4"), EME_CODEC_AAC, MediaDrmProxy::AAC, &widevine.mMP4 },
        { nsCString("video/webm"), EME_CODEC_VP8, MediaDrmProxy::VP8, &widevine.mWebM },
        { nsCString("video/webm"), EME_CODEC_VP9, MediaDrmProxy::VP9, &widevine.mWebM},
        { nsCString("audio/webm"), EME_CODEC_VORBIS, MediaDrmProxy::VORBIS, &widevine.mWebM},
        { nsCString("audio/webm"), EME_CODEC_OPUS, MediaDrmProxy::OPUS, &widevine.mWebM},
      };

      for (const auto& data: validationList) {
        if (MediaDrmProxy::IsCryptoSchemeSupported(kEMEKeySystemWidevine,
                                                   data.mMimeType)) {
          if (MediaDrmProxy::CanDecode(data.mCodecType)) {
            data.mSupportType->SetCanDecryptAndDecode(data.mEMECodecType);
          } else {
            data.mSupportType->SetCanDecrypt(data.mEMECodecType);
          }
        }
      }
#else
      widevine.mMP4.SetCanDecryptAndDecode(EME_CODEC_H264);
      if (Preferences::GetBool("media.eme.vp9-in-mp4.enabled", false)) {
        widevine.mMP4.SetCanDecryptAndDecode(EME_CODEC_VP9);
      }
      widevine.mWebM.SetCanDecrypt(EME_CODEC_VORBIS);
      widevine.mWebM.SetCanDecrypt(EME_CODEC_OPUS);
      widevine.mWebM.SetCanDecryptAndDecode(EME_CODEC_VP8);
      widevine.mWebM.SetCanDecryptAndDecode(EME_CODEC_VP9);
#endif
      keySystemConfigs.AppendElement(Move(widevine));
    }
  }

  return keySystemConfigs;
}

static bool
GetKeySystemConfig(const nsAString& aKeySystem, KeySystemConfig& aOutKeySystemConfig)
{
  for (auto&& config : GetSupportedKeySystems()) {
    if (config.mKeySystem.Equals(aKeySystem)) {
      aOutKeySystemConfig = mozilla::Move(config);
      return true;
    }
  }
  // No matching key system found.
  return false;
}

/* static */
bool
MediaKeySystemAccess::KeySystemSupportsInitDataType(const nsAString& aKeySystem,
                                                    const nsAString& aInitDataType)
{
  KeySystemConfig implementation;
  return GetKeySystemConfig(aKeySystem, implementation) &&
         implementation.mInitDataTypes.Contains(aInitDataType);
}

enum CodecType
{
  Audio,
  Video,
  Invalid
};

static bool
CanDecryptAndDecode(const nsString& aKeySystem,
                    const nsString& aContentType,
                    CodecType aCodecType,
                    const KeySystemContainerSupport& aContainerSupport,
                    const nsTArray<EMECodecString>& aCodecs,
                    DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(aCodecType != Invalid);
  for (const EMECodecString& codec : aCodecs) {
    MOZ_ASSERT(!codec.IsEmpty());

    if (aContainerSupport.DecryptsAndDecodes(codec)) {
      // GMP can decrypt-and-decode this codec.
      continue;
    }

    if (aContainerSupport.Decrypts(codec) &&
        NS_SUCCEEDED(MediaSource::IsTypeSupported(aContentType, aDiagnostics))) {
      // GMP can decrypt and is allowed to return compressed samples to
      // Gecko to decode, and Gecko has a decoder.
      continue;
    }

    // Neither the GMP nor Gecko can both decrypt and decode. We don't
    // support this codec.

#if defined(XP_WIN)
    // Widevine CDM doesn't include an AAC decoder. So if WMF can't
    // decode AAC, and a codec wasn't specified, be conservative
    // and reject the MediaKeys request, since we assume Widevine
    // will be used with AAC.
    if (codec == EME_CODEC_AAC &&
        IsWidevineKeySystem(aKeySystem) &&
        !WMFDecoderModule::HasAAC()) {
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

static bool
ToSessionType(const nsAString& aSessionType, MediaKeySessionType& aOutType)
{
  if (aSessionType.Equals(ToString(MediaKeySessionType::Temporary))) {
    aOutType = MediaKeySessionType::Temporary;
    return true;
  }
  if (aSessionType.Equals(ToString(MediaKeySessionType::Persistent_license))) {
    aOutType = MediaKeySessionType::Persistent_license;
    return true;
  }
  return false;
}

// 5.2.1 Is persistent session type?
static bool
IsPersistentSessionType(MediaKeySessionType aSessionType)
{
  return aSessionType == MediaKeySessionType::Persistent_license;
}

CodecType
GetMajorType(const MediaMIMEType& aMIMEType)
{
  if (aMIMEType.HasAudioMajorType()) {
    return Audio;
  }
  if (aMIMEType.HasVideoMajorType()) {
    return Video;
  }
  return Invalid;
}

static CodecType
GetCodecType(const EMECodecString& aCodec)
{
  if (aCodec.Equals(EME_CODEC_AAC) ||
      aCodec.Equals(EME_CODEC_OPUS) ||
      aCodec.Equals(EME_CODEC_VORBIS)) {
    return Audio;
  }
  if (aCodec.Equals(EME_CODEC_H264) ||
      aCodec.Equals(EME_CODEC_VP8) ||
      aCodec.Equals(EME_CODEC_VP9)) {
    return Video;
  }
  return Invalid;
}

static bool
AllCodecsOfType(const nsTArray<EMECodecString>& aCodecs, const CodecType aCodecType)
{
  for (const EMECodecString& codec : aCodecs) {
    if (GetCodecType(codec) != aCodecType) {
      return false;
    }
  }
  return true;
}

static bool
IsParameterUnrecognized(const nsAString& aContentType)
{
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

// 3.1.2.3 Get Supported Capabilities for Audio/Video Type
static Sequence<MediaKeySystemMediaCapability>
GetSupportedCapabilities(const CodecType aCodecType,
                         const nsTArray<MediaKeySystemMediaCapability>& aRequestedCapabilities,
                         const MediaKeySystemConfiguration& aPartialConfig,
                         const KeySystemConfig& aKeySystem,
                         DecoderDoctorDiagnostics* aDiagnostics)
{
  // Let local accumulated configuration be a local copy of partial configuration.
  // (Note: It's not necessary for us to maintain a local copy, as we don't need
  // to test whether capabilites from previous calls to this algorithm work with
  // the capabilities currently being considered in this call. )

  // Let supported media capabilities be an empty sequence of
  // MediaKeySystemMediaCapability dictionaries.
  Sequence<MediaKeySystemMediaCapability> supportedCapabilities;

  // For each requested media capability in requested media capabilities:
  for (const MediaKeySystemMediaCapability& capabilities : aRequestedCapabilities) {
    // Let content type be requested media capability's contentType member.
    const nsString& contentTypeString = capabilities.mContentType;
    // Let robustness be requested media capability's robustness member.
    const nsString& robustness = capabilities.mRobustness;
    // If content type is the empty string, return null.
    if (contentTypeString.IsEmpty()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') rejected; "
              "audio or video capability has empty contentType.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentTypeString).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      return Sequence<MediaKeySystemMediaCapability>();
    }
    // If content type is an invalid or unrecognized MIME type, continue
    // to the next iteration.
    Maybe<MediaContainerType> maybeContainerType =
      MakeMediaContainerType(contentTypeString);
    if (!maybeContainerType) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "failed to parse contentTypeString as MIME type.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentTypeString).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    const MediaContainerType& containerType = *maybeContainerType;
    bool invalid = false;
    nsTArray<EMECodecString> codecs;
    for (const auto& codecString : containerType.ExtendedType().Codecs().Range()) {
      EMECodecString emeCodec = ToEMEAPICodecString(nsString(codecString));
      if (emeCodec.IsEmpty()) {
        invalid = true;
        EME_LOG("MediaKeySystemConfiguration (label='%s') "
                "MediaKeySystemMediaCapability('%s','%s') unsupported; "
                "'%s' is an invalid codec string.",
                NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
                NS_ConvertUTF16toUTF8(contentTypeString).get(),
                NS_ConvertUTF16toUTF8(robustness).get(),
                NS_ConvertUTF16toUTF8(codecString).get());
        break;
      }
      codecs.AppendElement(emeCodec);
    }
    if (invalid) {
      continue;
    }

    // If the user agent does not support container, continue to the next iteration.
    // The case-sensitivity of string comparisons is determined by the appropriate RFC.
    // (Note: Per RFC 6838 [RFC6838], "Both top-level type and subtype names are
    // case-insensitive."'. We're using nsContentTypeParser and that is
    // case-insensitive and converts all its parameter outputs to lower case.)
    const bool isMP4 =
      DecoderTraits::IsMP4SupportedType(containerType, aDiagnostics);
    if (isMP4 && !aKeySystem.mMP4.IsSupported()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "MP4 requested but unsupported.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentTypeString).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    const bool isWebM = WebMDecoder::IsSupportedType(containerType);
    if (isWebM && !aKeySystem.mWebM.IsSupported()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "WebM requested but unsupported.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentTypeString).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    if (!isMP4 && !isWebM) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "Unsupported or unrecognized container requested.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentTypeString).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
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
      // If container normatively implies a specific set of codecs and codec constraints:
      // Let parameters be that set.
      // Otherwise: Continue to the next iteration.
      continue;
    }

    // If container type is not strictly a audio/video type, continue to the next iteration.
    const auto majorType = GetMajorType(containerType.Type());
    if (majorType == Invalid) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "MIME type is not an audio or video MIME type.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentTypeString).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    if (majorType != aCodecType || !AllCodecsOfType(codecs, aCodecType)) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "MIME type mixes audio codecs in video capabilities "
              "or video codecs in audio capabilities.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentTypeString).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    // If robustness is not the empty string and contains an unrecognized
    // value or a value not supported by implementation, continue to the
    // next iteration. String comparison is case-sensitive.
    if (!robustness.IsEmpty()) {
      if (majorType == Audio && !aKeySystem.mAudioRobustness.Contains(robustness)) {
        EME_LOG("MediaKeySystemConfiguration (label='%s') "
                "MediaKeySystemMediaCapability('%s','%s') unsupported; "
                "unsupported robustness string.",
                NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
                NS_ConvertUTF16toUTF8(contentTypeString).get(),
                NS_ConvertUTF16toUTF8(robustness).get());
        continue;
      }
      if (majorType == Video && !aKeySystem.mVideoRobustness.Contains(robustness)) {
        EME_LOG("MediaKeySystemConfiguration (label='%s') "
                "MediaKeySystemMediaCapability('%s','%s') unsupported; "
                "unsupported robustness string.",
                NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
                NS_ConvertUTF16toUTF8(contentTypeString).get(),
                NS_ConvertUTF16toUTF8(robustness).get());
        continue;
      }
      // Note: specified robustness requirements are satisfied.
    }

    // If the user agent and implementation definitely support playback of
    // encrypted media data for the combination of container, media types,
    // robustness and local accumulated configuration in combination with
    // restrictions...
    const auto& containerSupport = isMP4 ? aKeySystem.mMP4 : aKeySystem.mWebM;
    if (!CanDecryptAndDecode(aKeySystem.mKeySystem,
                             contentTypeString,
                             majorType,
                             containerSupport,
                             codecs,
                             aDiagnostics)) {
        EME_LOG("MediaKeySystemConfiguration (label='%s') "
                "MediaKeySystemMediaCapability('%s','%s') unsupported; "
                "codec unsupported by CDM requested.",
                NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
                NS_ConvertUTF16toUTF8(contentTypeString).get(),
                NS_ConvertUTF16toUTF8(robustness).get());
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
  return Move(supportedCapabilities);
}

// "Get Supported Configuration and Consent" algorithm, steps 4-7 for
// distinctive identifier, and steps 8-11 for persistent state. The steps
// are the same for both requirements/features, so we factor them out into
// a single function.
static bool
CheckRequirement(const MediaKeysRequirement aRequirement,
                 const KeySystemFeatureSupport aFeatureSupport,
                 MediaKeysRequirement& aOutRequirement)
{
  // Let requirement be the value of candidate configuration's member.
  MediaKeysRequirement requirement = aRequirement;
  // If requirement is "optional" and feature is not allowed according to
  // restrictions, set requirement to "not-allowed".
  if (aRequirement == MediaKeysRequirement::Optional &&
      aFeatureSupport == KeySystemFeatureSupport::Prohibited) {
    requirement = MediaKeysRequirement::Not_allowed;
  }

  // Follow the steps for requirement from the following list:
  switch (requirement) {
    case MediaKeysRequirement::Required: {
      // If the implementation does not support use of requirement in combination
      // with accumulated configuration and restrictions, return NotSupported.
      if (aFeatureSupport == KeySystemFeatureSupport::Prohibited) {
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
      if (aFeatureSupport == KeySystemFeatureSupport::Required) {
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

// 3.1.2.2, step 12
// Follow the steps for the first matching condition from the following list:
// If the sessionTypes member is present in candidate configuration.
// Let session types be candidate configuration's sessionTypes member.
// Otherwise let session types be ["temporary"].
// Note: This returns an empty array on malloc failure.
static Sequence<nsString>
UnboxSessionTypes(const Optional<Sequence<nsString>>& aSessionTypes)
{
  Sequence<nsString> sessionTypes;
  if (aSessionTypes.WasPassed()) {
    sessionTypes = aSessionTypes.Value();
  } else {
    // Note: fallible. Results in an empty array.
    sessionTypes.AppendElement(ToString(MediaKeySessionType::Temporary),
                               mozilla::fallible);
  }
  return sessionTypes;
}

// 3.1.2.2 Get Supported Configuration and Consent
static bool
GetSupportedConfig(const KeySystemConfig& aKeySystem,
                   const MediaKeySystemConfiguration& aCandidate,
                   MediaKeySystemConfiguration& aOutConfig,
                   DecoderDoctorDiagnostics* aDiagnostics,
                   bool aInPrivateBrowsing)
{
  // Let accumulated configuration be a new MediaKeySystemConfiguration dictionary.
  MediaKeySystemConfiguration config;
  // Set the label member of accumulated configuration to equal the label member of
  // candidate configuration.
  config.mLabel = aCandidate.mLabel;
  // If the initDataTypes member of candidate configuration is non-empty, run the
  // following steps:
  if (!aCandidate.mInitDataTypes.IsEmpty()) {
    // Let supported types be an empty sequence of DOMStrings.
    nsTArray<nsString> supportedTypes;
    // For each value in candidate configuration's initDataTypes member:
    for (const nsString& initDataType : aCandidate.mInitDataTypes) {
      // Let initDataType be the value.
      // If the implementation supports generating requests based on initDataType,
      // add initDataType to supported types. String comparison is case-sensitive.
      // The empty string is never supported.
      if (aKeySystem.mInitDataTypes.Contains(initDataType)) {
        supportedTypes.AppendElement(initDataType);
      }
    }
    // If supported types is empty, return NotSupported.
    if (supportedTypes.IsEmpty()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
              "no supported initDataTypes provided.",
              NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // Set the initDataTypes member of accumulated configuration to supported types.
    if (!config.mInitDataTypes.Assign(supportedTypes)) {
      return false;
    }
  }

  if (!CheckRequirement(aCandidate.mDistinctiveIdentifier,
                        aKeySystem.mDistinctiveIdentifier,
                        config.mDistinctiveIdentifier)) {
    EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
            "distinctiveIdentifier requirement not satisfied.",
            NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
    return false;
  }

  if (!CheckRequirement(aCandidate.mPersistentState,
                        aKeySystem.mPersistentState,
                        config.mPersistentState)) {
    EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
            "persistentState requirement not satisfied.",
            NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
    return false;
  }

  if (config.mPersistentState == MediaKeysRequirement::Required &&
      aInPrivateBrowsing) {
    EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
            "persistentState requested in Private Browsing window.",
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
      EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
              "invalid session type specified.",
              NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // If accumulated configuration's persistentState value is "not-allowed"
    // and the Is persistent session type? algorithm returns true for session
    // type return NotSupported.
    if (config.mPersistentState == MediaKeysRequirement::Not_allowed &&
        IsPersistentSessionType(sessionType)) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
              "persistent session requested but keysystem doesn't"
              "support persistent state.",
              NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // If the implementation does not support session type in combination
    // with accumulated configuration and restrictions for other reasons,
    // return NotSupported.
    if (!aKeySystem.mSessionTypes.Contains(sessionType)) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
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
  config.mSessionTypes.Construct(Move(sessionTypes));

  // If the videoCapabilities and audioCapabilities members in candidate
  // configuration are both empty, return NotSupported.
  if (aCandidate.mAudioCapabilities.IsEmpty() &&
      aCandidate.mVideoCapabilities.IsEmpty()) {
    EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
            "no supported audio or video capabilities specified",
            NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
    return false;
  }

  // If the videoCapabilities member in candidate configuration is non-empty:
  if (!aCandidate.mVideoCapabilities.IsEmpty()) {
    // Let video capabilities be the result of executing the Get Supported
    // Capabilities for Audio/Video Type algorithm on Video, candidate
    // configuration's videoCapabilities member, accumulated configuration,
    // and restrictions.
    Sequence<MediaKeySystemMediaCapability> caps =
      GetSupportedCapabilities(Video,
                               aCandidate.mVideoCapabilities,
                               config,
                               aKeySystem,
                               aDiagnostics);
    // If video capabilities is null, return NotSupported.
    if (caps.IsEmpty()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
              "no supported video capabilities.",
              NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // Set the videoCapabilities member of accumulated configuration to video capabilities.
    config.mVideoCapabilities = Move(caps);
  } else {
    // Otherwise:
    // Set the videoCapabilities member of accumulated configuration to an empty sequence.
  }

  // If the audioCapabilities member in candidate configuration is non-empty:
  if (!aCandidate.mAudioCapabilities.IsEmpty()) {
    // Let audio capabilities be the result of executing the Get Supported Capabilities
    // for Audio/Video Type algorithm on Audio, candidate configuration's audioCapabilities
    // member, accumulated configuration, and restrictions.
    Sequence<MediaKeySystemMediaCapability> caps =
      GetSupportedCapabilities(Audio,
                               aCandidate.mAudioCapabilities,
                               config,
                               aKeySystem,
                               aDiagnostics);
    // If audio capabilities is null, return NotSupported.
    if (caps.IsEmpty()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
              "no supported audio capabilities.",
              NS_ConvertUTF16toUTF8(aCandidate.mLabel).get());
      return false;
    }
    // Set the audioCapabilities member of accumulated configuration to audio capabilities.
    config.mAudioCapabilities = Move(caps);
  } else {
    // Otherwise:
    // Set the audioCapabilities member of accumulated configuration to an empty sequence.
  }

  // If accumulated configuration's distinctiveIdentifier value is "optional", follow the
  // steps for the first matching condition from the following list:
  if (config.mDistinctiveIdentifier == MediaKeysRequirement::Optional) {
    // If the implementation requires use Distinctive Identifier(s) or
    // Distinctive Permanent Identifier(s) for any of the combinations
    // in accumulated configuration
    if (aKeySystem.mDistinctiveIdentifier == KeySystemFeatureSupport::Required) {
      // Change accumulated configuration's distinctiveIdentifier value to "required".
      config.mDistinctiveIdentifier = MediaKeysRequirement::Required;
    } else {
      // Otherwise, change accumulated configuration's distinctiveIdentifier
      // value to "not-allowed".
      config.mDistinctiveIdentifier = MediaKeysRequirement::Not_allowed;
    }
  }

  // If accumulated configuration's persistentState value is "optional", follow the
  // steps for the first matching condition from the following list:
  if (config.mPersistentState == MediaKeysRequirement::Optional) {
    // If the implementation requires persisting state for any of the combinations
    // in accumulated configuration
    if (aKeySystem.mPersistentState == KeySystemFeatureSupport::Required) {
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
  // and a codec wasn't specified, be conservative and reject the MediaKeys request.
  if (IsWidevineKeySystem(aKeySystem.mKeySystem) &&
      (aCandidate.mAudioCapabilities.IsEmpty() ||
       aCandidate.mVideoCapabilities.IsEmpty()) &&
     !WMFDecoderModule::HasAAC()) {
    if (aDiagnostics) {
      aDiagnostics->SetKeySystemIssue(
        DecoderDoctorDiagnostics::eWidevineWithNoWMF);
    }
    EME_LOG("MediaKeySystemConfiguration (label='%s') rejected; "
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
bool
MediaKeySystemAccess::GetSupportedConfig(
  const nsAString& aKeySystem,
  const Sequence<MediaKeySystemConfiguration>& aConfigs,
  MediaKeySystemConfiguration& aOutConfig,
  DecoderDoctorDiagnostics* aDiagnostics,
  bool aIsPrivateBrowsing)
{
  KeySystemConfig implementation;
  if (!GetKeySystemConfig(aKeySystem, implementation)) {
    return false;
  }
  for (const MediaKeySystemConfiguration& candidate : aConfigs) {
    if (mozilla::dom::GetSupportedConfig(implementation,
                                         candidate,
                                         aOutConfig,
                                         aDiagnostics,
                                         aIsPrivateBrowsing)) {
      return true;
    }
  }

  return false;
}


/* static */
void
MediaKeySystemAccess::NotifyObservers(nsPIDOMWindowInner* aWindow,
                                      const nsAString& aKeySystem,
                                      MediaKeySystemStatus aStatus)
{
  RequestMediaKeySystemAccessNotification data;
  data.mKeySystem = aKeySystem;
  data.mStatus = aStatus;
  nsAutoString json;
  data.ToJSON(json);
  EME_LOG("MediaKeySystemAccess::NotifyObservers() %s", NS_ConvertUTF16toUTF8(json).get());
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(aWindow, "mediakeys-request", json.get());
  }
}

static nsCString
ToCString(const nsString& aString)
{
  nsCString str("'");
  str.Append(NS_ConvertUTF16toUTF8(aString));
  str.AppendLiteral("'");
  return str;
}

static nsCString
ToCString(const MediaKeysRequirement aValue)
{
  nsCString str("'");
  str.Append(nsDependentCString(
    MediaKeysRequirementValues::strings[static_cast<uint32_t>(aValue)].value));
  str.AppendLiteral("'");
  return str;
}

static nsCString
ToCString(const MediaKeySystemMediaCapability& aValue)
{
  nsCString str;
  str.AppendLiteral("{contentType=");
  str.Append(ToCString(aValue.mContentType));
  str.AppendLiteral(", robustness=");
  str.Append(ToCString(aValue.mRobustness));
  str.AppendLiteral("}");
  return str;
}

template<class Type>
static nsCString
ToCString(const Sequence<Type>& aSequence)
{
  nsCString str;
  str.AppendLiteral("[");
  for (size_t i = 0; i < aSequence.Length(); i++) {
    if (i != 0) {
      str.AppendLiteral(",");
    }
    str.Append(ToCString(aSequence[i]));
  }
  str.AppendLiteral("]");
  return str;
}

template<class Type>
static nsCString
ToCString(const Optional<Sequence<Type>>& aOptional)
{
  nsCString str;
  if (aOptional.WasPassed()) {
    str.Append(ToCString(aOptional.Value()));
  } else {
    str.AppendLiteral("[]");
  }
  return str;
}

static nsCString
ToCString(const MediaKeySystemConfiguration& aConfig)
{
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
nsCString
MediaKeySystemAccess::ToCString(
  const Sequence<MediaKeySystemConfiguration>& aConfig)
{
  return mozilla::dom::ToCString(aConfig);
}

} // namespace dom
} // namespace mozilla
