/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"
#include "WMFDecoderModule.h"
#include "WMFDecoder.h"
#include "WMFVideoMFTManager.h"
#include "WMFAudioMFTManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"
#include "WMFMediaDataDecoder.h"
#include "nsIWindowsRegKey.h"
#include "nsComponentManagerUtils.h"
#include "nsIGfxInfo.h"
#include "GfxDriverInfo.h"

namespace mozilla {

bool WMFDecoderModule::sIsWMFEnabled = false;
bool WMFDecoderModule::sDXVAEnabled = false;

WMFDecoderModule::WMFDecoderModule()
{
}

WMFDecoderModule::~WMFDecoderModule()
{
}

/* static */
void
WMFDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  sIsWMFEnabled = Preferences::GetBool("media.windows-media-foundation.enabled", false);
  if (!sIsWMFEnabled) {
    return;
  }
  if (NS_FAILED(WMFDecoder::LoadDLLs())) {
    sIsWMFEnabled = false;
  }
  sDXVAEnabled = Preferences::GetBool("media.windows-media-foundation.use-dxva", false);
}

nsresult
WMFDecoderModule::Startup()
{
  if (!sIsWMFEnabled) {
    return NS_ERROR_FAILURE;
  }
  if (FAILED(wmf::MFStartup())) {
    NS_WARNING("Failed to initialize Windows Media Foundation");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
WMFDecoderModule::Shutdown()
{
  DebugOnly<HRESULT> hr = wmf::MFShutdown();
  NS_ASSERTION(SUCCEEDED(hr), "MFShutdown failed");
  return NS_OK;
}

already_AddRefed<MediaDataDecoder>
WMFDecoderModule::CreateVideoDecoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                                     layers::LayersBackend aLayersBackend,
                                     layers::ImageContainer* aImageContainer,
                                     FlushableMediaTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> decoder =
    new WMFMediaDataDecoder(new WMFVideoMFTManager(aConfig,
                                                   aLayersBackend,
                                                   aImageContainer,
                                                   sDXVAEnabled && ShouldUseDXVA(aConfig)),
                            aVideoTaskQueue,
                            aCallback);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
WMFDecoderModule::CreateAudioDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                                     FlushableMediaTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> decoder =
    new WMFMediaDataDecoder(new WMFAudioMFTManager(aConfig),
                            aAudioTaskQueue,
                            aCallback);
  return decoder.forget();
}

bool
WMFDecoderModule::ShouldUseDXVA(const mp4_demuxer::VideoDecoderConfig& aConfig) const
{
  static bool isAMD = false;
  static bool initialized = false;
  if (!initialized) {
    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    nsAutoString vendor;
    gfxInfo->GetAdapterVendorID(vendor);
    isAMD = vendor.Equals(widget::GfxDriverInfo::GetDeviceVendor(widget::VendorAMD), nsCaseInsensitiveStringComparator()) ||
            vendor.Equals(widget::GfxDriverInfo::GetDeviceVendor(widget::VendorATI), nsCaseInsensitiveStringComparator());
    initialized = true;
  }
  if (!isAMD) {
    return true;
  }
  // Don't use DXVA for 4k videos or above, since it seems to perform poorly.
  return aConfig.display_height <= 1920 && aConfig.display_height <= 1200;
}

bool
WMFDecoderModule::SupportsSharedDecoders(const mp4_demuxer::VideoDecoderConfig& aConfig) const
{
  // If DXVA is enabled, but we're not going to use it for this specific config, then
  // we can't use the shared decoder.
  return !sDXVAEnabled || ShouldUseDXVA(aConfig);
}

bool
WMFDecoderModule::SupportsVideoMimeType(const char* aMimeType)
{
  return !strcmp(aMimeType, "video/mp4") ||
         !strcmp(aMimeType, "video/avc") ||
         !strcmp(aMimeType, "video/webm; codecs=vp8") ||
         !strcmp(aMimeType, "video/webm; codecs=vp9");
}

bool
WMFDecoderModule::SupportsAudioMimeType(const char* aMimeType)
{
  return !strcmp(aMimeType, "audio/mp4a-latm") ||
         !strcmp(aMimeType, "audio/mpeg");
}

static bool
ClassesRootRegKeyExists(const nsAString& aRegKeyPath)
{
  nsresult rv;

  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                    aRegKeyPath,
                    nsIWindowsRegKey::ACCESS_READ);
  if (NS_FAILED(rv)) {
    return false;
  }

  regKey->Close();

  return true;
}

/* static */ bool
WMFDecoderModule::HasH264()
{
  // CLSID_CMSH264DecoderMFT
  return ClassesRootRegKeyExists(
    NS_LITERAL_STRING("CLSID\\{32D186A7-218F-4C75-8876-DD77273A8999}"));
}

/* static */ bool
WMFDecoderModule::HasAAC()
{
  // CLSID_CMSAACDecMFT
  return ClassesRootRegKeyExists(
    NS_LITERAL_STRING("CLSID\\{62CE7E72-4C71-4D20-B15D-452831A87D9D}"));
}

} // namespace mozilla
