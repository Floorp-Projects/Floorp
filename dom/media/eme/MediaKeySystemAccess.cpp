/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeySystemAccess.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "mozilla/Preferences.h"
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

MediaKeySystemAccess::MediaKeySystemAccess(nsPIDOMWindow* aParent,
                                           const nsAString& aKeySystem)
  : mParent(aParent)
  , mKeySystem(aKeySystem)
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

nsPIDOMWindow*
MediaKeySystemAccess::GetParentObject() const
{
  return mParent;
}

void
MediaKeySystemAccess::GetKeySystem(nsString& aRetVal) const
{
  aRetVal = mKeySystem;
}

already_AddRefed<Promise>
MediaKeySystemAccess::CreateMediaKeys(ErrorResult& aRv)
{
  nsRefPtr<MediaKeys> keys(new MediaKeys(mParent, mKeySystem));
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
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    NS_WARNING("AdobePluginFileExists() lying because it doesn't work with e10s");
    return true;
  }

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

static MediaKeySystemStatus
EnsureMinCDMVersion(mozIGeckoMediaPluginService* aGMPService,
                    const nsAString& aKeySystem,
                    int32_t aMinCdmVersion,
                    nsACString& aOutMessage)
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

  if (!hasPlugin) {
    aOutMessage = NS_LITERAL_CSTRING("CDM is not installed");
    return MediaKeySystemStatus::Cdm_not_installed;
  }

#ifdef XP_WIN
  if (aKeySystem.EqualsLiteral("com.adobe.access") ||
      aKeySystem.EqualsLiteral("com.adobe.primetime")) {
    // Verify that anti-virus hasn't "helpfully" deleted the Adobe GMP DLL,
    // as we suspect may happen (Bug 1160382).
    bool somethingMissing = false;
    if (!AdobePluginDLLExists(versionStr)) {
      aOutMessage = NS_LITERAL_CSTRING("Adobe DLL was expected to be on disk but was not");
      somethingMissing = true;
    }
    if (!AdobePluginVoucherExists(versionStr)) {
      aOutMessage = NS_LITERAL_CSTRING("Adobe plugin voucher was expected to be on disk but was not");
      somethingMissing = true;
    }
    if (somethingMissing) {
      NS_WARNING("Adobe EME plugin or voucher disappeared from disk!");
      // Reset the prefs that Firefox's GMP downloader sets, so that
      // Firefox will try to download the plugin next time the updater runs.
      Preferences::ClearUser("media.gmp-eme-adobe.lastUpdate");
      Preferences::ClearUser("media.gmp-eme-adobe.version");
      return MediaKeySystemStatus::Cdm_not_installed;
    }
  }
#endif

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
                                         nsACString& aOutMessage)
{
  MOZ_ASSERT(Preferences::GetBool("media.eme.enabled", false));
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
    return EnsureMinCDMVersion(mps, aKeySystem, aMinCdmVersion, aOutMessage);
  }

#ifdef XP_WIN
  if ((aKeySystem.EqualsLiteral("com.adobe.access") ||
       aKeySystem.EqualsLiteral("com.adobe.primetime"))) {
    // Win Vista and later only.
    if (!IsVistaOrLater()) {
      aOutMessage = NS_LITERAL_CSTRING("Minimum Windows version not met for Adobe EME");
      return MediaKeySystemStatus::Cdm_not_supported;
    }
    if (!Preferences::GetBool("media.gmp-eme-adobe.enabled", false)) {
      aOutMessage = NS_LITERAL_CSTRING("Adobe EME disabled");
      return MediaKeySystemStatus::Cdm_disabled;
    }
    if (!EMEVoucherFileExists()) {
      // The system doesn't have the codecs that Adobe EME relies
      // on installed, or doesn't have a voucher for the plugin-container.
      // Adobe EME isn't going to work, so don't advertise that it will.
      aOutMessage = NS_LITERAL_CSTRING("Plugin-container voucher not present");
      return MediaKeySystemStatus::Cdm_not_supported;
    }
    return EnsureMinCDMVersion(mps, aKeySystem, aMinCdmVersion, aOutMessage);
  }
#endif

  return MediaKeySystemStatus::Cdm_not_supported;
}

static bool
IsPlayableWithGMP(mozIGeckoMediaPluginService* aGMPS,
                  const nsAString& aKeySystem,
                  const nsAString& aContentType)
{
#ifdef MOZ_FMP4
  nsContentTypeParser parser(aContentType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (!mimeType.EqualsLiteral("audio/mp4") &&
      !mimeType.EqualsLiteral("audio/x-m4a") &&
      !mimeType.EqualsLiteral("video/mp4")) {
    return false;
  }

  nsAutoString codecs;
  parser.GetParameter("codecs", codecs);

  NS_ConvertUTF16toUTF8 mimeTypeUTF8(mimeType);
  bool hasAAC = false;
  bool hasH264 = false;
  bool hasMP3 = false;
  if (!MP4Decoder::CanHandleMediaType(mimeTypeUTF8,
                                      codecs,
                                      hasAAC,
                                      hasH264,
                                      hasMP3) ||
      hasMP3) {
    return false;
  }
  return (!hasAAC ||
          !HaveGMPFor(aGMPS,
                      NS_ConvertUTF16toUTF8(aKeySystem),
                      NS_LITERAL_CSTRING(GMP_API_DECRYPTOR),
                      NS_LITERAL_CSTRING("aac"))) &&
         (!hasH264 ||
          !HaveGMPFor(aGMPS,
                     NS_ConvertUTF16toUTF8(aKeySystem),
                     NS_LITERAL_CSTRING(GMP_API_DECRYPTOR),
                     NS_LITERAL_CSTRING("h264")));
#else
  return false;
#endif
}

/* static */
bool
MediaKeySystemAccess::IsSupported(const nsAString& aKeySystem,
                                  const Sequence<MediaKeySystemOptions>& aOptions)
{
  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (NS_WARN_IF(!mps)) {
    return false;
  }

  for (size_t i = 0; i < aOptions.Length(); i++) {
    const MediaKeySystemOptions& options = aOptions[i];
    if (!options.mInitDataType.EqualsLiteral("cenc")) {
      continue;
    }
    if (!options.mAudioCapability.IsEmpty() ||
        !options.mVideoCapability.IsEmpty()) {
      // Don't support any capabilites until we know we have a CDM with
      // capabilities...
      continue;
    }
    if (!options.mAudioType.IsEmpty() &&
        !IsPlayableWithGMP(mps, aKeySystem, options.mAudioType)) {
      continue;
    }
    if (!options.mVideoType.IsEmpty() &&
        !IsPlayableWithGMP(mps, aKeySystem, options.mVideoType)) {
      continue;
    }

    // Our sandbox provides an origin specific unique identifier, and the
    // ability to persist data. We don't yet have a way to turn those off
    // and on for specific GMPs/CDMs, so we don't check the uniqueidentifier
    // and stateful attributes here.

    return true;
  }
  return false;
}

/* static */
void
MediaKeySystemAccess::NotifyObservers(nsIDOMWindow* aWindow,
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
