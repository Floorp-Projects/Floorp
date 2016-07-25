/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/MediaKeySystemAccess.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "mozilla/Preferences.h"
#include "MediaPrefs.h"
#include "nsContentTypeParser.h"
#ifdef MOZ_FMP4
#include "MP4Decoder.h"
#endif
#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
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
#include "gmp-audio-decode.h"
#include "gmp-video-decode.h"
#include "DecoderDoctorDiagnostics.h"
#include "WebMDecoder.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsUnicharUtils.h"
#include "mozilla/dom/MediaSource.h"

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

MediaKeySystemAccess::MediaKeySystemAccess(nsPIDOMWindowInner* aParent,
                                           const nsAString& aKeySystem,
                                           const nsAString& aCDMVersion,
                                           const MediaKeySystemConfiguration& aConfig)
  : mParent(aParent)
  , mKeySystem(aKeySystem)
  , mCDMVersion(aCDMVersion)
  , mConfig(aConfig)
{
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
                                       mCDMVersion,
                                       mConfig.mDistinctiveIdentifier == MediaKeysRequirement::Required,
                                       mConfig.mPersistentState == MediaKeysRequirement::Required));
  return keys->Init(aRv);
}

static bool
HaveGMPFor(mozIGeckoMediaPluginService* aGMPService,
           const nsCString& aKeySystem,
           const nsCString& aAPI,
           const nsCString& aTag = EmptyCString())
{
  nsTArray<nsCString> tags;
  tags.AppendElement(aKeySystem);
  if (!aTag.IsEmpty()) {
    tags.AppendElement(aTag);
  }
  bool hasPlugin = false;
  if (NS_FAILED(aGMPService->HasPluginForAPI(aAPI,
                                             &tags,
                                             &hasPlugin))) {
    return false;
  }
  return hasPlugin;
}

#ifdef XP_WIN
static bool
AdobePluginFileExists(const nsACString& aVersionStr,
                      const nsAString& aFilename)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  nsCOMPtr<nsIFile> path;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = path->Append(NS_LITERAL_STRING("gmp-eme-adobe"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = path->AppendNative(aVersionStr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = path->Append(aFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  bool exists = false;
  return NS_SUCCEEDED(path->Exists(&exists)) && exists;
}

static bool
AdobePluginDLLExists(const nsACString& aVersionStr)
{
  return AdobePluginFileExists(aVersionStr, NS_LITERAL_STRING("eme-adobe.dll"));
}

static bool
AdobePluginVoucherExists(const nsACString& aVersionStr)
{
  return AdobePluginFileExists(aVersionStr, NS_LITERAL_STRING("eme-adobe.voucher"));
}
#endif

/* static */ bool
MediaKeySystemAccess::IsGMPPresentOnDisk(const nsAString& aKeySystem,
                                         const nsACString& aVersion,
                                         nsACString& aOutMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    // We need to be able to access the filesystem, so call this in the
    // main process via ContentChild.
    ContentChild* contentChild = ContentChild::GetSingleton();
    if (NS_WARN_IF(!contentChild)) {
      return false;
    }

    nsCString message;
    bool result = false;
    bool ok = contentChild->SendIsGMPPresentOnDisk(nsString(aKeySystem), nsCString(aVersion),
                                                   &result, &message);
    aOutMessage = message;
    return ok && result;
  }

  bool isPresent = true;

#if XP_WIN
  if (aKeySystem.EqualsLiteral("com.adobe.primetime")) {
    if (!AdobePluginDLLExists(aVersion)) {
      NS_WARNING("Adobe EME plugin disappeared from disk!");
      aOutMessage = NS_LITERAL_CSTRING("Adobe DLL was expected to be on disk but was not");
      isPresent = false;
    }
    if (!AdobePluginVoucherExists(aVersion)) {
      NS_WARNING("Adobe EME voucher disappeared from disk!");
      aOutMessage = NS_LITERAL_CSTRING("Adobe plugin voucher was expected to be on disk but was not");
      isPresent = false;
    }

    if (!isPresent) {
      // Reset the prefs that Firefox's GMP downloader sets, so that
      // Firefox will try to download the plugin next time the updater runs.
      Preferences::ClearUser("media.gmp-eme-adobe.lastUpdate");
      Preferences::ClearUser("media.gmp-eme-adobe.version");
    } else if (!EMEVoucherFileExists()) {
      // Gecko doesn't have a voucher file for the plugin-container.
      // Adobe EME isn't going to work, so don't advertise that it will.
      aOutMessage = NS_LITERAL_CSTRING("Plugin-container voucher not present");
      isPresent = false;
    }
  }
#endif

  return isPresent;
}

static MediaKeySystemStatus
EnsureMinCDMVersion(mozIGeckoMediaPluginService* aGMPService,
                    const nsAString& aKeySystem,
                    int32_t aMinCdmVersion,
                    nsACString& aOutMessage,
                    nsACString& aOutCdmVersion)
{
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_ConvertUTF16toUTF8(aKeySystem));
  bool hasPlugin;
  nsAutoCString versionStr;
  if (NS_FAILED(aGMPService->GetPluginVersionForAPI(NS_LITERAL_CSTRING(GMP_API_DECRYPTOR),
                                                    &tags,
                                                    &hasPlugin,
                                                    versionStr))) {
    aOutMessage = NS_LITERAL_CSTRING("GetPluginVersionForAPI failed");
    return MediaKeySystemStatus::Error;
  }

  aOutCdmVersion = versionStr;

  if (!hasPlugin) {
    aOutMessage = NS_LITERAL_CSTRING("CDM is not installed");
    return MediaKeySystemStatus::Cdm_not_installed;
  }

  if (!MediaKeySystemAccess::IsGMPPresentOnDisk(aKeySystem, versionStr, aOutMessage)) {
    return MediaKeySystemStatus::Cdm_not_installed;
  }

  nsresult rv;
  int32_t version = versionStr.ToInteger(&rv);
  if (aMinCdmVersion != NO_CDM_VERSION &&
      (NS_FAILED(rv) || version < 0 || aMinCdmVersion > version)) {
    aOutMessage = NS_LITERAL_CSTRING("Installed CDM version insufficient");
    return MediaKeySystemStatus::Cdm_insufficient_version;
  }

  return MediaKeySystemStatus::Available;
}

/* static */
MediaKeySystemStatus
MediaKeySystemAccess::GetKeySystemStatus(const nsAString& aKeySystem,
                                         int32_t aMinCdmVersion,
                                         nsACString& aOutMessage,
                                         nsACString& aOutCdmVersion)
{
  MOZ_ASSERT(MediaPrefs::EMEEnabled());
  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (NS_WARN_IF(!mps)) {
    aOutMessage = NS_LITERAL_CSTRING("Failed to get GMP service");
    return MediaKeySystemStatus::Error;
  }

  if (aKeySystem.EqualsLiteral("org.w3.clearkey")) {
    if (!Preferences::GetBool("media.eme.clearkey.enabled", true)) {
      aOutMessage = NS_LITERAL_CSTRING("ClearKey was disabled");
      return MediaKeySystemStatus::Cdm_disabled;
    }
    return EnsureMinCDMVersion(mps, aKeySystem, aMinCdmVersion, aOutMessage, aOutCdmVersion);
  }

  if (Preferences::GetBool("media.gmp-eme-adobe.visible", false)) {
    if (aKeySystem.EqualsLiteral("com.adobe.primetime")) {
      if (!Preferences::GetBool("media.gmp-eme-adobe.enabled", false)) {
        aOutMessage = NS_LITERAL_CSTRING("Adobe EME disabled");
        return MediaKeySystemStatus::Cdm_disabled;
      }
#ifdef XP_WIN
      // Win Vista and later only.
      if (!IsVistaOrLater()) {
        aOutMessage = NS_LITERAL_CSTRING("Minimum Windows version (Vista) not met for Adobe EME");
        return MediaKeySystemStatus::Cdm_not_supported;
      }
#endif
      return EnsureMinCDMVersion(mps, aKeySystem, aMinCdmVersion, aOutMessage, aOutCdmVersion);
    }
  }

  if (Preferences::GetBool("media.gmp-widevinecdm.visible", false)) {
    if (aKeySystem.EqualsLiteral("com.widevine.alpha")) {
#ifdef XP_WIN
      // Win Vista and later only.
      if (!IsVistaOrLater()) {
        aOutMessage = NS_LITERAL_CSTRING("Minimum Windows version (Vista) not met for Widevine EME");
        return MediaKeySystemStatus::Cdm_not_supported;
      }
#endif
      if (!Preferences::GetBool("media.gmp-widevinecdm.enabled", false)) {
        aOutMessage = NS_LITERAL_CSTRING("Widevine EME disabled");
        return MediaKeySystemStatus::Cdm_disabled;
      }
      return EnsureMinCDMVersion(mps, aKeySystem, aMinCdmVersion, aOutMessage, aOutCdmVersion);
    }
  }

  return MediaKeySystemStatus::Cdm_not_supported;
}

typedef nsCString GMPCodecString;

#define GMP_CODEC_AAC NS_LITERAL_CSTRING("aac")
#define GMP_CODEC_OPUS NS_LITERAL_CSTRING("opus")
#define GMP_CODEC_VORBIS NS_LITERAL_CSTRING("vorbis")
#define GMP_CODEC_H264 NS_LITERAL_CSTRING("h264")
#define GMP_CODEC_VP8 NS_LITERAL_CSTRING("vp8")
#define GMP_CODEC_VP9 NS_LITERAL_CSTRING("vp9")

GMPCodecString
ToGMPAPICodecString(const nsString& aCodec)
{
  if (IsAACCodecString(aCodec)) {
    return GMP_CODEC_AAC;
  }
  if (aCodec.EqualsLiteral("opus")) {
    return GMP_CODEC_OPUS;
  }
  if (aCodec.EqualsLiteral("vorbis")) {
    return GMP_CODEC_VORBIS;
  }
  if (IsH264CodecString(aCodec)) {
    return GMP_CODEC_H264;
  }
  if (IsVP8CodecString(aCodec)) {
    return GMP_CODEC_VP8;
  }
  if (IsVP9CodecString(aCodec)) {
    return GMP_CODEC_VP9;
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
  bool DecryptsAndDecodes(GMPCodecString aCodec) const
  {
    return mCodecsDecoded.Contains(aCodec);
  }

  // CDM decrypts and passes the decrypted samples back to Gecko for decoding.
  bool Decrypts(GMPCodecString aCodec) const
  {
    return mCodecsDecrypted.Contains(aCodec);
  }

  void SetCanDecryptAndDecode(GMPCodecString aCodec)
  {
    // Can't both decrypt and decrypt-and-decode a codec.
    MOZ_ASSERT(!Decrypts(aCodec));
    // Prevent duplicates.
    MOZ_ASSERT(!DecryptsAndDecodes(aCodec));
    mCodecsDecoded.AppendElement(aCodec);
  }

  void SetCanDecrypt(GMPCodecString aCodec)
  {
    // Prevent duplicates.
    MOZ_ASSERT(!Decrypts(aCodec));
    // Can't both decrypt and decrypt-and-decode a codec.
    MOZ_ASSERT(!DecryptsAndDecodes(aCodec));
    mCodecsDecrypted.AppendElement(aCodec);
  }

private:
  nsTArray<GMPCodecString> mCodecsDecoded;
  nsTArray<GMPCodecString> mCodecsDecrypted;
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

StaticAutoPtr<nsTArray<KeySystemConfig>> sKeySystemConfigs;

static const nsTArray<KeySystemConfig>&
GetSupportedKeySystems()
{
  if (!sKeySystemConfigs) {
    sKeySystemConfigs = new nsTArray<KeySystemConfig>();
    ClearOnShutdown(&sKeySystemConfigs);

    {
      KeySystemConfig clearkey;
      clearkey.mKeySystem = NS_LITERAL_STRING("org.w3.clearkey");
      clearkey.mInitDataTypes.AppendElement(NS_LITERAL_STRING("cenc"));
      clearkey.mInitDataTypes.AppendElement(NS_LITERAL_STRING("keyids"));
      clearkey.mInitDataTypes.AppendElement(NS_LITERAL_STRING("webm"));
      clearkey.mPersistentState = KeySystemFeatureSupport::Requestable;
      clearkey.mDistinctiveIdentifier = KeySystemFeatureSupport::Prohibited;
      clearkey.mSessionTypes.AppendElement(MediaKeySessionType::Temporary);
      clearkey.mSessionTypes.AppendElement(MediaKeySessionType::Persistent_license);
#if defined(XP_WIN)
      // Clearkey CDM uses WMF decoders on Windows.
      if (WMFDecoderModule::HasAAC()) {
        clearkey.mMP4.SetCanDecryptAndDecode(GMP_CODEC_AAC);
      } else {
        clearkey.mMP4.SetCanDecrypt(GMP_CODEC_AAC);
      }
      if (WMFDecoderModule::HasH264()) {
        clearkey.mMP4.SetCanDecryptAndDecode(GMP_CODEC_H264);
      } else {
        clearkey.mMP4.SetCanDecrypt(GMP_CODEC_H264);
      }
#else
      clearkey.mMP4.SetCanDecrypt(GMP_CODEC_AAC);
      clearkey.mMP4.SetCanDecrypt(GMP_CODEC_H264);
#endif
      clearkey.mWebM.SetCanDecrypt(GMP_CODEC_VORBIS);
      clearkey.mWebM.SetCanDecrypt(GMP_CODEC_OPUS);
      clearkey.mWebM.SetCanDecrypt(GMP_CODEC_VP8);
      clearkey.mWebM.SetCanDecrypt(GMP_CODEC_VP9);
      sKeySystemConfigs->AppendElement(Move(clearkey));
    }
    {
      KeySystemConfig widevine;
      widevine.mKeySystem = NS_LITERAL_STRING("com.widevine.alpha");
      widevine.mInitDataTypes.AppendElement(NS_LITERAL_STRING("cenc"));
      widevine.mInitDataTypes.AppendElement(NS_LITERAL_STRING("keyids"));
      widevine.mInitDataTypes.AppendElement(NS_LITERAL_STRING("webm"));
      widevine.mPersistentState = KeySystemFeatureSupport::Requestable;
      widevine.mDistinctiveIdentifier = KeySystemFeatureSupport::Prohibited;
      widevine.mSessionTypes.AppendElement(MediaKeySessionType::Temporary);
      widevine.mAudioRobustness.AppendElement(NS_LITERAL_STRING("SW_SECURE_CRYPTO"));
      widevine.mVideoRobustness.AppendElement(NS_LITERAL_STRING("SW_SECURE_DECODE"));
#if defined(XP_WIN)
      // Widevine CDM doesn't include an AAC decoder. So if WMF can't
      // decode AAC, and a codec wasn't specified, be conservative
      // and reject the MediaKeys request, since our policy is to prevent
      //  the Adobe GMP's unencrypted AAC decoding path being used to
      // decode content decrypted by the Widevine CDM.
      if (WMFDecoderModule::HasAAC()) {
        widevine.mMP4.SetCanDecrypt(GMP_CODEC_AAC);
      }
#else
      widevine.mMP4.SetCanDecrypt(GMP_CODEC_AAC);
#endif
      widevine.mMP4.SetCanDecryptAndDecode(GMP_CODEC_H264);
      // TODO: Enable Widevine/WebM once bug 1279077 lands.
      //widevine.mWebM.SetCanDecrypt(GMP_CODEC_VORBIS);
      //widevine.mWebM.SetCanDecrypt(GMP_CODEC_OPUS);
      //widevine.mWebM.SetCanDecryptAndDecode(GMP_CODEC_VP8);
      //widevine.mWebM.SetCanDecryptAndDecode(GMP_CODEC_VP9);
      sKeySystemConfigs->AppendElement(Move(widevine));
    }
    {
      KeySystemConfig primetime;
      primetime.mKeySystem = NS_LITERAL_STRING("com.adobe.primetime");
      primetime.mInitDataTypes.AppendElement(NS_LITERAL_STRING("cenc"));
      primetime.mPersistentState = KeySystemFeatureSupport::Required;
      primetime.mDistinctiveIdentifier = KeySystemFeatureSupport::Required;
      primetime.mSessionTypes.AppendElement(MediaKeySessionType::Temporary);
      primetime.mMP4.SetCanDecryptAndDecode(GMP_CODEC_AAC);
      primetime.mMP4.SetCanDecryptAndDecode(GMP_CODEC_H264);
      sKeySystemConfigs->AppendElement(Move(primetime));
    }
  }
  return *sKeySystemConfigs;
}

static const KeySystemConfig*
GetKeySystemConfig(const nsAString& aKeySystem)
{
  for (const KeySystemConfig& config : GetSupportedKeySystems()) {
    if (config.mKeySystem.Equals(aKeySystem)) {
      return &config;
    }
  }
  return nullptr;
}

enum CodecType
{
  Audio,
  Video,
  Invalid
};

static bool
CanDecryptAndDecode(mozIGeckoMediaPluginService* aGMPService,
                    const nsString& aKeySystem,
                    const nsString& aContentType,
                    CodecType aCodecType,
                    const KeySystemContainerSupport& aContainerSupport,
                    const nsTArray<GMPCodecString>& aCodecs,
                    DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(aCodecType != Invalid);
  MOZ_ASSERT(HaveGMPFor(aGMPService,
                        NS_ConvertUTF16toUTF8(aKeySystem),
                        NS_LITERAL_CSTRING(GMP_API_DECRYPTOR)));
  for (const GMPCodecString& codec : aCodecs) {
    MOZ_ASSERT(!codec.IsEmpty());

    nsCString api = (aCodecType == Audio) ? NS_LITERAL_CSTRING(GMP_API_AUDIO_DECODER)
                                          : NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER);
    if (aContainerSupport.DecryptsAndDecodes(codec) &&
        HaveGMPFor(aGMPService,
                   NS_ConvertUTF16toUTF8(aKeySystem),
                   api,
                   codec)) {
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
    // and reject the MediaKeys request, since our policy is to prevent
    //  the Adobe GMP's unencrypted AAC decoding path being used to
    // decode content decrypted by the Widevine CDM.
    if (codec == GMP_CODEC_AAC &&
        aKeySystem.EqualsLiteral("com.widevine.alpha") &&
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
  using MediaKeySessionTypeValues::strings;
  const char* temporary =
    strings[static_cast<uint32_t>(MediaKeySessionType::Temporary)].value;
  if (aSessionType.EqualsASCII(temporary)) {
    aOutType = MediaKeySessionType::Temporary;
    return true;
  }
  const char* persistentLicense =
    strings[static_cast<uint32_t>(MediaKeySessionType::Persistent_license)].value;
  if (aSessionType.EqualsASCII(persistentLicense)) {
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
GetMajorType(const nsAString& aContentType)
{
  if (CaseInsensitiveFindInReadable(NS_LITERAL_STRING("audio/"), aContentType)) {
    return Audio;
  }
  if (CaseInsensitiveFindInReadable(NS_LITERAL_STRING("video/"), aContentType)) {
    return Video;
  }
  return Invalid;
}

static CodecType
GetCodecType(const GMPCodecString& aCodec)
{
  if (aCodec.Equals(GMP_CODEC_AAC) ||
      aCodec.Equals(GMP_CODEC_OPUS) ||
      aCodec.Equals(GMP_CODEC_VORBIS)) {
    return Audio;
  }
  if (aCodec.Equals(GMP_CODEC_H264) ||
      aCodec.Equals(GMP_CODEC_VP8) ||
      aCodec.Equals(GMP_CODEC_VP9)) {
    return Video;
  }
  return Invalid;
}

static bool
AllCodecsOfType(const nsTArray<GMPCodecString>& aCodecs, const CodecType aCodecType)
{
  for (const GMPCodecString& codec : aCodecs) {
    if (GetCodecType(codec) != aCodecType) {
      return false;
    }
  }
  return true;
}

// 3.1.2.3 Get Supported Capabilities for Audio/Video Type
static Sequence<MediaKeySystemMediaCapability>
GetSupportedCapabilities(const CodecType aCodecType,
                         mozIGeckoMediaPluginService* aGMPService,
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
    const nsString& contentType = capabilities.mContentType;
    // Let robustness be requested media capability's robustness member.
    const nsString& robustness = capabilities.mRobustness;
    // If content type is the empty string, return null.
    if (contentType.IsEmpty()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') rejected; "
              "audio or video capability has empty contentType.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentType).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      return Sequence<MediaKeySystemMediaCapability>();
    }
    // If content type is an invalid or unrecognized MIME type, continue
    // to the next iteration.
    nsAutoString container;
    nsTArray<nsString> codecStrings;
    if (!ParseMIMETypeString(contentType, container, codecStrings)) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "failed to parse contentType as MIME type.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentType).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    bool invalid = false;
    nsTArray<GMPCodecString> codecs;
    for (const nsString& codecString : codecStrings) {
      GMPCodecString gmpCodec = ToGMPAPICodecString(codecString);
      if (gmpCodec.IsEmpty()) {
        invalid = true;
        EME_LOG("MediaKeySystemConfiguration (label='%s') "
                "MediaKeySystemMediaCapability('%s','%s') unsupported; "
                "'%s' is an invalid codec string.",
                NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
                NS_ConvertUTF16toUTF8(contentType).get(),
                NS_ConvertUTF16toUTF8(robustness).get(),
                NS_ConvertUTF16toUTF8(codecString).get());
        break;
      }
      codecs.AppendElement(gmpCodec);
    }
    if (invalid) {
      continue;
    }

    // If the user agent does not support container, continue to the next iteration.
    // The case-sensitivity of string comparisons is determined by the appropriate RFC.
    // (Note: Per RFC 6838 [RFC6838], "Both top-level type and subtype names are
    // case-insensitive."'. We're using nsContentTypeParser and that is
    // case-insensitive and converts all its parameter outputs to lower case.)
    NS_ConvertUTF16toUTF8 container_utf8(container);
    const bool isMP4 = DecoderTraits::IsMP4TypeAndEnabled(container_utf8, aDiagnostics);
    if (isMP4 && !aKeySystem.mMP4.IsSupported()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "MP4 requested but unsupported.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentType).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    const bool isWebM = DecoderTraits::IsWebMTypeAndEnabled(container_utf8);
    if (isWebM && !aKeySystem.mWebM.IsSupported()) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "WebM requested but unsupported.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentType).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    if (!isMP4 && !isWebM) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "Unsupported or unrecognized container requested.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentType).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }

    // Let parameters be the RFC 6381[RFC6381] parameters, if any, specified by
    // content type.
    // If the user agent does not recognize one or more parameters, continue to
    // the next iteration.
    // Let media types be the set of codecs and codec constraints specified by
    // parameters. The case-sensitivity of string comparisons is determined by
    // the appropriate RFC or other specification.
    // (Note: codecs array is 'parameter').

    // If media types is empty:
    if (codecs.IsEmpty()) {
      // If container normatively implies a specific set of codecs and codec constraints:
      // Let parameters be that set.
      if (isMP4) {
        if (aCodecType == Audio) {
          codecs.AppendElement(GMP_CODEC_AAC);
        } else if (aCodecType == Video) {
          codecs.AppendElement(GMP_CODEC_H264);
        }
      } else if (isWebM) {
        if (aCodecType == Audio) {
          codecs.AppendElement(GMP_CODEC_VORBIS);
        } else if (aCodecType == Video) {
          codecs.AppendElement(GMP_CODEC_VP8);
        }
      }
      // Otherwise: Continue to the next iteration.
      // (Note: all containers we support have implied codecs, so don't continue here.)
    }

    // If content type is not strictly a audio/video type, continue to the next iteration.
    const auto majorType = GetMajorType(container);
    if (majorType == Invalid) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "MIME type is not an audio or video MIME type.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentType).get(),
              NS_ConvertUTF16toUTF8(robustness).get());
      continue;
    }
    if (majorType != aCodecType || !AllCodecsOfType(codecs, aCodecType)) {
      EME_LOG("MediaKeySystemConfiguration (label='%s') "
              "MediaKeySystemMediaCapability('%s','%s') unsupported; "
              "MIME type mixes audio codecs in video capabilities "
              "or video codecs in audio capabilities.",
              NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
              NS_ConvertUTF16toUTF8(contentType).get(),
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
                NS_ConvertUTF16toUTF8(contentType).get(),
                NS_ConvertUTF16toUTF8(robustness).get());
        continue;
      }
      if (majorType == Video && !aKeySystem.mVideoRobustness.Contains(robustness)) {
        EME_LOG("MediaKeySystemConfiguration (label='%s') "
                "MediaKeySystemMediaCapability('%s','%s') unsupported; "
                "unsupported robustness string.",
                NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
                NS_ConvertUTF16toUTF8(contentType).get(),
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
    if (!CanDecryptAndDecode(aGMPService,
                             aKeySystem.mKeySystem,
                             contentType,
                             majorType,
                             containerSupport,
                             codecs,
                             aDiagnostics)) {
        EME_LOG("MediaKeySystemConfiguration (label='%s') "
                "MediaKeySystemMediaCapability('%s','%s') unsupported; "
                "codec unsupported by CDM requested.",
                NS_ConvertUTF16toUTF8(aPartialConfig.mLabel).get(),
                NS_ConvertUTF16toUTF8(contentType).get(),
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
    using MediaKeySessionTypeValues::strings;
    const char* temporary = strings[static_cast<uint32_t>(MediaKeySessionType::Temporary)].value;
    // Note: fallible. Results in an empty array.
    sessionTypes.AppendElement(NS_ConvertUTF8toUTF16(nsDependentCString(temporary)), mozilla::fallible);
  }
  return sessionTypes;
}

// 3.1.2.2 Get Supported Configuration and Consent
static bool
GetSupportedConfig(mozIGeckoMediaPluginService* aGMPService,
                   const KeySystemConfig& aKeySystem,
                   const MediaKeySystemConfiguration& aCandidate,
                   MediaKeySystemConfiguration& aOutConfig,
                   DecoderDoctorDiagnostics* aDiagnostics)
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
  // TODO: Most sites using EME still don't pass capabilities, so we
  // can't reject on it yet without breaking them. So add this later.

  // If the videoCapabilities member in candidate configuration is non-empty:
  if (!aCandidate.mVideoCapabilities.IsEmpty()) {
    // Let video capabilities be the result of executing the Get Supported
    // Capabilities for Audio/Video Type algorithm on Video, candidate
    // configuration's videoCapabilities member, accumulated configuration,
    // and restrictions.
    Sequence<MediaKeySystemMediaCapability> caps =
      GetSupportedCapabilities(Video,
                               aGMPService,
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
                               aGMPService,
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
  if (aKeySystem.mKeySystem.EqualsLiteral("com.widevine.alpha") &&
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
MediaKeySystemAccess::GetSupportedConfig(const nsAString& aKeySystem,
                                         const Sequence<MediaKeySystemConfiguration>& aConfigs,
                                         MediaKeySystemConfiguration& aOutConfig,
                                         DecoderDoctorDiagnostics* aDiagnostics)
{
  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (NS_WARN_IF(!mps)) {
    return false;
  }
  const KeySystemConfig* implementation = nullptr;
  if (!HaveGMPFor(mps,
                  NS_ConvertUTF16toUTF8(aKeySystem),
                  NS_LITERAL_CSTRING(GMP_API_DECRYPTOR)) ||
      !(implementation = GetKeySystemConfig(aKeySystem))) {
    return false;
  }
  for (const MediaKeySystemConfiguration& candidate : aConfigs) {
    if (mozilla::dom::GetSupportedConfig(mps,
                                         *implementation,
                                         candidate,
                                         aOutConfig,
                                         aDiagnostics)) {
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

} // namespace dom
} // namespace mozilla
