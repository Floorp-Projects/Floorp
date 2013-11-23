/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Decoder.h"
#include "MP4Reader.h"
#include "MediaDecoderStateMachine.h"
#include "mozilla/Preferences.h"

#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#endif

namespace mozilla {

MediaDecoderStateMachine* MP4Decoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(this, new MP4Reader(this));
}

bool
MP4Decoder::GetSupportedCodecs(const nsACString& aType,
                               char const *const ** aCodecList)
{
  if (!IsEnabled()) {
    return false;
  }

  // AAC in M4A.
  static char const *const aacAudioCodecs[] = {
    "mp4a.40.2",    // AAC-LC
    // TODO: AAC-HE ?
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
  static char const *const h264Codecs[] = {
    "avc1.42E01E",  // H.264 Constrained Baseline Profile Level 3.0
    "avc1.42001E",  // H.264 Baseline Profile Level 3.0
    "avc1.58A01E",  // H.264 Extended Profile Level 3.0
    "avc1.4D401E",  // H.264 Main Profile Level 3.0
    "avc1.64001E",  // H.264 High Profile Level 3.0
    "avc1.64001F",  // H.264 High Profile Level 3.1
    "mp4a.40.2",    // AAC-LC
    // TODO: There must be more profiles here?
    nullptr
  };
  if (aType.EqualsASCII("video/mp4")) {
    if (aCodecList) {
      *aCodecList = h264Codecs;
    }
    return true;
  }

  return false;
}

static bool
HavePlatformMPEGDecoders()
{
  return
    Preferences::GetBool("media.fragmented-mp4.use-blank-decoder") ||
#ifdef XP_WIN
    // We have H.264/AAC platform decoders on Windows Vista and up.
    IsVistaOrLater() ||
#endif
    // TODO: Other platforms...
    false;
}

/* static */
bool
MP4Decoder::IsEnabled()
{
  return HavePlatformMPEGDecoders() &&
         Preferences::GetBool("media.fragmented-mp4.enabled");
}

} // namespace mozilla

