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

namespace mozilla {

MediaDecoderStateMachine* WMFDecoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(this, new WMFReader(this));
}

bool
WMFDecoder::GetSupportedCodecs(const nsACString& aType,
                               char const *const ** aCodecList)
{
  if (!MediaDecoder::IsWMFEnabled() ||
      NS_FAILED(LoadDLLs()))
    return false;

  // MP3 is specified to have no codecs in its "type" param:
  // http://wiki.whatwg.org/wiki/Video_type_parameters#MPEG
  // So specify an empty codecs list, so that if script specifies 
  // a "type" param with codecs, it will be reported as not supported
  // as per the spec.
  static char const *const mp3AudioCodecs[] = {
    nullptr
  };
  if (aType.EqualsASCII("audio/mpeg")) {
    if (aCodecList) {
      *aCodecList = mp3AudioCodecs;
    }
    // Assume that if LoadDLLs() didn't fail, we can decode MP3.
    return true;
  }

  // AAC in M4A.
  static char const *const aacAudioCodecs[] = {
    "mp4a.40.2",    // AAC-LC
    nullptr
  };
  if (aType.EqualsASCII("audio/mp4")) {
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

bool IsWindows7OrLater()
{
  OSVERSIONINFO versionInfo;
  BOOL isWin7OrLater = FALSE;
  versionInfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
  if (!GetVersionEx(&versionInfo)) {
    return false;
  }
  // Note: Win Vista = 6.0
  //       Win 7     = 6.1
  //       Win 8     = 6.2
  return versionInfo.dwMajorVersion > 6 ||
        (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion >= 1);
}

/* static */
bool
WMFDecoder::IsEnabled()
{
  // We only use WMF on Windows 7 and up, until we can properly test Vista
  // and how it responds with and without the Platform Update installed.
  return IsWindows7OrLater() &&
         Preferences::GetBool("media.windows-media-foundation.enabled");
}

} // namespace mozilla

