/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"
#include "WMFDecoder.h"
#include "WMFReader.h"
#include "WMFUtils.h"
#include "MediaDecoderStateMachine.h"
#include "mozilla/Preferences.h"
#include "WinUtils.h"

#ifdef MOZ_DIRECTSHOW
#include "DirectShowDecoder.h"
#endif

using namespace mozilla::widget;

namespace mozilla {

MediaDecoderStateMachine* WMFDecoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(this, new WMFReader(this));
}

/* static */
bool
WMFDecoder::IsMP3Supported()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
#ifdef MOZ_DIRECTSHOW
  if (DirectShowDecoder::IsEnabled()) {
    // DirectShowDecoder is enabled, we use that in preference to the WMF
    // backend.
    return false;
  }
#endif
 if (!MediaDecoder::IsWMFEnabled()) {
    return false;
  }
  if (WinUtils::GetWindowsVersion() != WinUtils::WIN7_VERSION) {
    return true;
  }
  // We're on Windows 7. MP3 support is disabled if no service pack
  // is installed, as it's crashy on Win7 SP0.
  UINT spMajorVer = 0, spMinorVer = 0;
  if (!WinUtils::GetWindowsServicePackVersion(spMajorVer, spMinorVer)) {
    // Um... We can't determine the service pack version... Just block
    // MP3 as a precaution...
    return false;
  }
  return spMajorVer != 0;
}

bool
WMFDecoder::GetSupportedCodecs(const nsACString& aType,
                               char const *const ** aCodecList)
{
  if (!MediaDecoder::IsWMFEnabled() ||
      NS_FAILED(LoadDLLs()))
    return false;

  // Assume that if LoadDLLs() didn't fail, we can playback the types that
  // we know should be supported by Windows Media Foundation.
  static char const *const mp3AudioCodecs[] = {
    "mp3",
    nullptr
  };
  if ((aType.EqualsASCII("audio/mpeg") || aType.EqualsASCII("audio/mp3")) &&
      IsMP3Supported()) {
    // Note: We block MP3 playback on Window 7 SP0 since it seems to crash
    // in some circumstances.
    if (aCodecList) {
      *aCodecList = mp3AudioCodecs;
    }
    return true;
  }

  // AAC in M4A.
  static char const *const aacAudioCodecs[] = {
    "mp4a.40.2",    // AAC-LC
    nullptr
  };
  if (aType.EqualsASCII("audio/mp4") ||
      aType.EqualsASCII("audio/x-m4a")) {
    if (aCodecList) {
      *aCodecList = aacAudioCodecs;
    }
    return true;
  }

  // H.264 + AAC in MP4.
  static char const *const H264Codecs[] = {
    "avc1.42E01E",  // H.264 Constrained Baseline Profile Level 3.0
    "avc1.42001E",  // H.264 Baseline Profile Level 3.0
    "avc1.58A01E",  // H.264 Extended Profile Level 3.0
    "avc1.4D401E",  // H.264 Main Profile Level 3.0
    "avc1.64001E",  // H.264 High Profile Level 3.0
    "avc1.64001F",  // H.264 High Profile Level 3.1
    "mp4a.40.2",    // AAC-LC
    nullptr
  };
  if (aType.EqualsASCII("video/mp4")) {
    if (aCodecList) {
      *aCodecList = H264Codecs;
    }
    return true;
  }

  return false;
}

nsresult
WMFDecoder::LoadDLLs()
{
  return SUCCEEDED(wmf::LoadDLLs()) ? NS_OK : NS_ERROR_FAILURE;
}

void
WMFDecoder::UnloadDLLs()
{
  wmf::UnloadDLLs();
}

/* static */
bool
WMFDecoder::IsEnabled()
{
  // We only use WMF on Windows Vista and up
  return WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
         Preferences::GetBool("media.windows-media-foundation.enabled");
}

} // namespace mozilla

