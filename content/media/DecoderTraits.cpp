/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderTraits.h"
#include "MediaDecoder.h"
#ifdef MOZ_MEDIA_PLUGINS
#include "MediaPluginHost.h"
#endif

namespace mozilla
{

#ifdef MOZ_RAW
static const char gRawTypes[2][16] = {
  "video/x-raw",
  "video/x-raw-yuv"
};

static const char* gRawCodecs[1] = {
  nullptr
};

/* static */
bool
DecoderTraits::IsRawType(const nsACString& aType)
{
  if (!MediaDecoder::IsRawEnabled()) {
    return false;
  }

  for (uint32_t i = 0; i < ArrayLength(gRawTypes); ++i) {
    if (aType.EqualsASCII(gRawTypes[i])) {
      return true;
    }
  }

  return false;
}
#endif

#ifdef MOZ_OGG
// See http://www.rfc-editor.org/rfc/rfc5334.txt for the definitions
// of Ogg media types and codec types
static const char gOggTypes[3][16] = {
  "video/ogg",
  "audio/ogg",
  "application/ogg"
};

static char const *const gOggCodecs[3] = {
  "vorbis",
  "theora",
  nullptr
};

static char const *const gOggCodecsWithOpus[4] = {
  "vorbis",
  "opus",
  "theora",
  nullptr
};

bool
DecoderTraits::IsOggType(const nsACString& aType)
{
  if (!MediaDecoder::IsOggEnabled()) {
    return false;
  }

  for (uint32_t i = 0; i < ArrayLength(gOggTypes); ++i) {
    if (aType.EqualsASCII(gOggTypes[i])) {
      return true;
    }
  }

  return false;
}
#endif

#ifdef MOZ_WAVE
// See http://www.rfc-editor.org/rfc/rfc2361.txt for the definitions
// of WAVE media types and codec types. However, the audio/vnd.wave
// MIME type described there is not used.
static const char gWaveTypes[4][15] = {
  "audio/x-wav",
  "audio/wav",
  "audio/wave",
  "audio/x-pn-wav"
};

static char const *const gWaveCodecs[2] = {
  "1", // Microsoft PCM Format
  nullptr
};

bool
DecoderTraits::IsWaveType(const nsACString& aType)
{
  if (!MediaDecoder::IsWaveEnabled()) {
    return false;
  }

  for (uint32_t i = 0; i < ArrayLength(gWaveTypes); ++i) {
    if (aType.EqualsASCII(gWaveTypes[i])) {
      return true;
    }
  }

  return false;
}
#endif

#ifdef MOZ_WEBM
static const char gWebMTypes[2][11] = {
  "video/webm",
  "audio/webm"
};

static char const *const gWebMCodecs[4] = {
  "vp8",
  "vp8.0",
  "vorbis",
  nullptr
};

bool
DecoderTraits::IsWebMType(const nsACString& aType)
{
  if (!MediaDecoder::IsWebMEnabled()) {
    return false;
  }

  for (uint32_t i = 0; i < ArrayLength(gWebMTypes); ++i) {
    if (aType.EqualsASCII(gWebMTypes[i])) {
      return true;
    }
  }

  return false;
}
#endif

#ifdef MOZ_GSTREAMER
static const char gH264Types[3][16] = {
  "video/mp4",
  "video/3gpp",
  "video/quicktime",
};

bool
DecoderTraits::IsGStreamerSupportedType(const nsACString& aMimeType)
{
  if (!MediaDecoder::IsGStreamerEnabled())
    return false;
  if (IsH264Type(aMimeType))
    return true;
  if (!Preferences::GetBool("media.prefer-gstreamer", false))
    return false;
#ifdef MOZ_WEBM
  if (IsWebMType(aMimeType))
    return true;
#endif
#ifdef MOZ_OGG
  if (IsOggType(aMimeType))
    return true;
#endif
  return false;
}

bool
DecoderTraits::IsH264Type(const nsACString& aType)
{
  for (uint32_t i = 0; i < ArrayLength(gH264Types); ++i) {
    if (aType.EqualsASCII(gH264Types[i])) {
      return true;
    }
  }
  return false;
}
#endif

#ifdef MOZ_WIDGET_GONK
static const char gOmxTypes[5][16] = {
  "audio/mpeg",
  "audio/mp4",
  "video/mp4",
  "video/3gpp",
  "video/quicktime",
};

bool
DecoderTraits::IsOmxSupportedType(const nsACString& aType)
{
  if (!MediaDecoder::IsOmxEnabled()) {
    return false;
  }

  for (uint32_t i = 0; i < ArrayLength(gOmxTypes); ++i) {
    if (aType.EqualsASCII(gOmxTypes[i])) {
      return true;
    }
  }

  return false;
}
#endif

#if defined(MOZ_GSTREAMER) || defined(MOZ_WIDGET_GONK)
static char const *const gH264Codecs[9] = {
  "avc1.42E01E",  // H.264 Constrained Baseline Profile Level 3.0
  "avc1.42001E",  // H.264 Baseline Profile Level 3.0
  "avc1.58A01E",  // H.264 Extended Profile Level 3.0
  "avc1.4D401E",  // H.264 Main Profile Level 3.0
  "avc1.64001E",  // H.264 High Profile Level 3.0
  "avc1.64001F",  // H.264 High Profile Level 3.1
  "mp4v.20.3",    // 3GPP
  "mp4a.40.2",    // AAC-LC
  nullptr
};
#endif

#ifdef MOZ_MEDIA_PLUGINS
bool
DecoderTraits::IsMediaPluginsType(const nsACString& aType)
{
  if (!MediaDecoder::IsMediaPluginsEnabled()) {
    return false;
  }

  static const char* supportedTypes[] = {
    "audio/mpeg", "audio/mp4", "video/mp4"
  };
  for (uint32_t i = 0; i < ArrayLength(supportedTypes); ++i) {
    if (aType.EqualsASCII(supportedTypes[i])) {
      return true;
    }
  }
  return false;
}
#endif

#ifdef MOZ_DASH
/* static */
static const char gDASHMPDTypes[1][21] = {
  "application/dash+xml"
};

/* static */
bool
DecoderTraits::IsDASHMPDType(const nsACString& aType)
{
  if (!MediaDecoder::IsDASHEnabled()) {
    return false;
  }

  for (uint32_t i = 0; i < ArrayLength(gDASHMPDTypes); ++i) {
    if (aType.EqualsASCII(gDASHMPDTypes[i])) {
      return true;
    }
  }

  return false;
}
#endif

/* static */
bool DecoderTraits::ShouldHandleMediaType(const char* aMIMEType)
{
#ifdef MOZ_RAW
  if (IsRawType(nsDependentCString(aMIMEType)))
    return true;
#endif
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType)))
    return true;
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(nsDependentCString(aMIMEType)))
    return true;
#endif
#ifdef MOZ_GSTREAMER
  if (IsH264Type(nsDependentCString(aMIMEType)))
    return true;
#endif
#ifdef MOZ_WIDGET_GONK
  if (IsOmxSupportedType(nsDependentCString(aMIMEType))) {
    return true;
  }
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (MediaDecoder::IsMediaPluginsEnabled() && GetMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), NULL))
    return true;
#endif
  // We should not return true for Wave types, since there are some
  // Wave codecs actually in use in the wild that we don't support, and
  // we should allow those to be handled by plugins or helper apps.
  // Furthermore people can play Wave files on most platforms by other
  // means.
  return false;
}

/* static */
CanPlayStatus
DecoderTraits::CanHandleMediaType(const char* aMIMEType,
                                       char const *const ** aCodecList)
{
#ifdef MOZ_RAW
  if (IsRawType(nsDependentCString(aMIMEType))) {
    *aCodecList = gRawCodecs;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType))) {
    *aCodecList = MediaDecoder::IsOpusEnabled() ? gOggCodecsWithOpus : gOggCodecs;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    *aCodecList = gWaveCodecs;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(nsDependentCString(aMIMEType))) {
    *aCodecList = gWebMCodecs;
    return CANPLAY_YES;
  }
#endif
#ifdef MOZ_DASH
  if (IsDASHMPDType(nsDependentCString(aMIMEType))) {
    // DASH manifest uses WebM codecs only.
    *aCodecList = gWebMCodecs;
    return CANPLAY_YES;
  }
#endif

#ifdef MOZ_GSTREAMER
  if (IsH264Type(nsDependentCString(aMIMEType))) {
    *aCodecList = gH264Codecs;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_WIDGET_GONK
  if (IsOmxSupportedType(nsDependentCString(aMIMEType))) {
    *aCodecList = gH264Codecs;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (MediaDecoder::IsMediaPluginsEnabled() && GetMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), aCodecList))
    return CANPLAY_MAYBE;
#endif
  return CANPLAY_NO;
}

}

