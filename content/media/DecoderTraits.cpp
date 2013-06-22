/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderTraits.h"
#include "MediaDecoder.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/Preferences.h"

#ifdef MOZ_MEDIA_PLUGINS
#include "MediaPluginHost.h"
#endif

#ifdef MOZ_OGG
#include "OggDecoder.h"
#include "OggReader.h"
#endif
#ifdef MOZ_WAVE
#include "WaveDecoder.h"
#include "WaveReader.h"
#endif
#ifdef MOZ_WEBM
#include "WebMDecoder.h"
#include "WebMReader.h"
#endif
#ifdef MOZ_RAW
#include "RawDecoder.h"
#include "RawReader.h"
#endif
#ifdef MOZ_GSTREAMER
#include "GStreamerDecoder.h"
#include "GStreamerReader.h"
#endif
#ifdef MOZ_MEDIA_PLUGINS
#include "MediaPluginHost.h"
#include "MediaPluginDecoder.h"
#include "MediaPluginReader.h"
#include "MediaPluginHost.h"
#endif
#ifdef MOZ_OMX_DECODER
#include "MediaOmxDecoder.h"
#include "MediaOmxReader.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/HTMLMediaElement.h"
#endif
#ifdef MOZ_DASH
#include "DASHDecoder.h"
#endif
#ifdef MOZ_WMF
#include "WMFDecoder.h"
#include "WMFReader.h"
#endif

namespace mozilla
{

template <class String>
static bool
CodecListContains(char const *const * aCodecs, const String& aCodec)
{
  for (int32_t i = 0; aCodecs[i]; ++i) {
    if (aCodec.EqualsASCII(aCodecs[i]))
      return true;
  }
  return false;
}

#ifdef MOZ_RAW
static const char* gRawTypes[3] = {
  "video/x-raw",
  "video/x-raw-yuv",
  nullptr
};

static const char* gRawCodecs[1] = {
  nullptr
};

static bool
IsRawType(const nsACString& aType)
{
  if (!MediaDecoder::IsRawEnabled()) {
    return false;
  }

  return CodecListContains(gRawTypes, aType);
}
#endif

#ifdef MOZ_OGG
// See http://www.rfc-editor.org/rfc/rfc5334.txt for the definitions
// of Ogg media types and codec types
static const char* const gOggTypes[4] = {
  "video/ogg",
  "audio/ogg",
  "application/ogg",
  nullptr
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

static bool
IsOggType(const nsACString& aType)
{
  if (!MediaDecoder::IsOggEnabled()) {
    return false;
  }

  return CodecListContains(gOggTypes, aType);
}
#endif

#ifdef MOZ_WAVE
// See http://www.rfc-editor.org/rfc/rfc2361.txt for the definitions
// of WAVE media types and codec types. However, the audio/vnd.wave
// MIME type described there is not used.
static const char* const gWaveTypes[5] = {
  "audio/x-wav",
  "audio/wav",
  "audio/wave",
  "audio/x-pn-wav",
  nullptr
};

static char const *const gWaveCodecs[2] = {
  "1", // Microsoft PCM Format
  nullptr
};

static bool
IsWaveType(const nsACString& aType)
{
  if (!MediaDecoder::IsWaveEnabled()) {
    return false;
  }

  return CodecListContains(gWaveTypes, aType);
}
#endif

#ifdef MOZ_WEBM
static const char* const gWebMTypes[3] = {
  "video/webm",
  "audio/webm",
  nullptr
};

static char const *const gWebMCodecs[4] = {
  "vp8",
  "vp8.0",
  "vorbis",
  nullptr
};

static bool
IsWebMType(const nsACString& aType)
{
  if (!MediaDecoder::IsWebMEnabled()) {
    return false;
  }

  return CodecListContains(gWebMTypes, aType);
}
#endif

#ifdef MOZ_GSTREAMER
static bool
IsGStreamerSupportedType(const nsACString& aMimeType)
{
  if (!MediaDecoder::IsGStreamerEnabled())
    return false;

#ifdef MOZ_WEBM
  if (IsWebMType(aMimeType) && !Preferences::GetBool("media.prefer-gstreamer", false))
    return false;
#endif
#ifdef MOZ_OGG
  if (IsOggType(aMimeType) && !Preferences::GetBool("media.prefer-gstreamer", false))
    return false;
#endif

  return GStreamerDecoder::CanHandleMediaType(aMimeType, nullptr);
}
#endif

#ifdef MOZ_OMX_DECODER
static const char* const gOmxTypes[7] = {
  "audio/mpeg",
  "audio/mp4",
  "audio/amr",
  "video/mp4",
  "video/3gpp",
  "video/quicktime",
  nullptr
};

static bool
IsOmxSupportedType(const nsACString& aType)
{
  if (!MediaDecoder::IsOmxEnabled()) {
    return false;
  }

  return CodecListContains(gOmxTypes, aType);
}

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

static char const *const gMpegAudioCodecs[2] = {
  "mp3",          // MP3
  nullptr
};
#endif

#ifdef MOZ_MEDIA_PLUGINS
static bool
IsMediaPluginsType(const nsACString& aType)
{
  if (!MediaDecoder::IsMediaPluginsEnabled()) {
    return false;
  }

  static const char* supportedTypes[] = {
    "audio/mpeg", "audio/mp4", "video/mp4", nullptr
  };
  return CodecListContains(supportedTypes, aType);
}
#endif

#ifdef MOZ_DASH
/* static */
static const char* const gDASHMPDTypes[2] = {
  "application/dash+xml",
  nullptr
};

static bool
IsDASHMPDType(const nsACString& aType)
{
  if (!MediaDecoder::IsDASHEnabled()) {
    return false;
  }

  return CodecListContains(gDASHMPDTypes, aType);
}
#endif

#ifdef MOZ_WMF
static bool
IsWMFSupportedType(const nsACString& aType)
{
  return WMFDecoder::GetSupportedCodecs(aType, nullptr);
}
#endif

/* static */
bool DecoderTraits::ShouldHandleMediaType(const char* aMIMEType)
{
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    // We should not return true for Wave types, since there are some
    // Wave codecs actually in use in the wild that we don't support, and
    // we should allow those to be handled by plugins or helper apps.
    // Furthermore people can play Wave files on most platforms by other
    // means.
    return false;
  }
#endif
  return CanHandleMediaType(aMIMEType, false, EmptyString()) != CANPLAY_NO;
}

/* static */
CanPlayStatus
DecoderTraits::CanHandleMediaType(const char* aMIMEType,
                                  bool aHaveRequestedCodecs,
                                  const nsAString& aRequestedCodecs)
{
  char const* const* codecList = nullptr;
  CanPlayStatus result = CANPLAY_NO;
#ifdef MOZ_RAW
  if (IsRawType(nsDependentCString(aMIMEType))) {
    codecList = gRawCodecs;
    result = CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType))) {
    codecList = MediaDecoder::IsOpusEnabled() ? gOggCodecsWithOpus : gOggCodecs;
    result = CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    codecList = gWaveCodecs;
    result = CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(nsDependentCString(aMIMEType))) {
    codecList = gWebMCodecs;
    result = CANPLAY_YES;
  }
#endif
#ifdef MOZ_DASH
  if (IsDASHMPDType(nsDependentCString(aMIMEType))) {
    // DASH manifest uses WebM codecs only.
    codecList = gWebMCodecs;
    result = CANPLAY_YES;
  }
#endif
#ifdef MOZ_GSTREAMER
  if (GStreamerDecoder::CanHandleMediaType(nsDependentCString(aMIMEType),
                                           aHaveRequestedCodecs ? &aRequestedCodecs : nullptr)) {
    if (aHaveRequestedCodecs)
      return CANPLAY_YES;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(nsDependentCString(aMIMEType))) {
    result = CANPLAY_MAYBE;
    if (nsDependentCString(aMIMEType).EqualsASCII("audio/mpeg")) {
      codecList = gMpegAudioCodecs;
    } else {
      codecList = gH264Codecs;
    }
  }
#endif
#ifdef MOZ_WMF
  if (WMFDecoder::GetSupportedCodecs(nsDependentCString(aMIMEType), &codecList)) {
    result = CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (MediaDecoder::IsMediaPluginsEnabled() &&
      GetMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), &codecList))
    result = CANPLAY_MAYBE;
#endif
  if (result == CANPLAY_NO || !aHaveRequestedCodecs || !codecList) {
    return result;
  }

  // See http://www.rfc-editor.org/rfc/rfc4281.txt for the description
  // of the 'codecs' parameter
  nsCharSeparatedTokenizer tokenizer(aRequestedCodecs, ',');
  bool expectMoreTokens = false;
  while (tokenizer.hasMoreTokens()) {
    const nsSubstring& token = tokenizer.nextToken();

    if (!CodecListContains(codecList, token)) {
      // Totally unsupported codec
      return CANPLAY_NO;
    }
    expectMoreTokens = tokenizer.lastTokenEndedWithSeparator();
  }
  if (expectMoreTokens) {
    // Last codec name was empty
    return CANPLAY_NO;
  }
  return CANPLAY_YES;
}

/* static */
already_AddRefed<MediaDecoder>
DecoderTraits::CreateDecoder(const nsACString& aType, MediaDecoderOwner* aOwner)
{
  nsRefPtr<MediaDecoder> decoder;

#ifdef MOZ_GSTREAMER
  if (IsGStreamerSupportedType(aType)) {
    decoder = new GStreamerDecoder();
  }
#endif
#ifdef MOZ_RAW
  if (IsRawType(aType)) {
    decoder = new RawDecoder();
  }
#endif
#ifdef MOZ_OGG
  if (IsOggType(aType)) {
    decoder = new OggDecoder();
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    decoder = new WaveDecoder();
  }
#endif
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(aType)) {
    // AMR audio is enabled for MMS, but we are discouraging Web and App
    // developers from using AMR, thus we only allow AMR to be played on WebApps.
    if (aType.EqualsASCII("audio/amr")) {
      HTMLMediaElement* element = aOwner->GetMediaElement();
      if (!element) {
        return nullptr;
      }
      nsIPrincipal* principal = element->NodePrincipal();
      if (!principal) {
        return nullptr;
      }
      if (principal->GetAppStatus() < nsIPrincipal::APP_STATUS_PRIVILEGED) {
        return nullptr;
      }
    }
    decoder = new MediaOmxDecoder();
  }
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (MediaDecoder::IsMediaPluginsEnabled() && GetMediaPluginHost()->FindDecoder(aType, NULL)) {
    decoder = new MediaPluginDecoder(aType);
  }
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(aType)) {
    decoder = new WebMDecoder();
  }
#endif
#ifdef MOZ_DASH
  if (IsDASHMPDType(aType)) {
    decoder = new DASHDecoder();
  }
#endif
#ifdef MOZ_WMF
  if (IsWMFSupportedType(aType)) {
    decoder = new WMFDecoder();
  }
#endif

  NS_ENSURE_TRUE(decoder != nullptr, nullptr);
  NS_ENSURE_TRUE(decoder->Init(aOwner), nullptr);

  return decoder.forget();
}

/* static */
MediaDecoderReader* DecoderTraits::CreateReader(const nsACString& aType, AbstractMediaDecoder* aDecoder)
{
  MediaDecoderReader* decoderReader = nullptr;

#ifdef MOZ_GSTREAMER
  if (IsGStreamerSupportedType(aType)) {
    decoderReader = new GStreamerReader(aDecoder);
  } else
#endif
#ifdef MOZ_RAW
  if (IsRawType(aType)) {
    decoderReader = new RawReader(aDecoder);
  } else
#endif
#ifdef MOZ_OGG
  if (IsOggType(aType)) {
    decoderReader = new OggReader(aDecoder);
  } else
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    decoderReader = new WaveReader(aDecoder);
  } else
#endif
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(aType)) {
    decoderReader = new MediaOmxReader(aDecoder);
  } else
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (MediaDecoder::IsMediaPluginsEnabled() &&
      GetMediaPluginHost()->FindDecoder(aType, nullptr)) {
    decoderReader = new MediaPluginReader(aDecoder, aType);
  } else
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(aType)) {
    decoderReader = new WebMReader(aDecoder);
  } else
#endif
#ifdef MOZ_WMF
  if (IsWMFSupportedType(aType)) {
    decoderReader = new WMFReader(aDecoder);
  } else
#endif
#ifdef MOZ_DASH
  // The DASH decoder is not supported.
#endif
  if (false) {} // dummy if to take care of the dangling else

  return decoderReader;
}

/* static */
bool DecoderTraits::IsSupportedInVideoDocument(const nsACString& aType)
{
  return
#ifdef MOZ_OGG
    IsOggType(aType) ||
#endif
#ifdef MOZ_OMX_DECODER
    IsOmxSupportedType(aType) ||
#endif
#ifdef MOZ_WEBM
    IsWebMType(aType) ||
#endif
#ifdef MOZ_DASH
    IsDASHMPDType(aType) ||
#endif
#ifdef MOZ_GSTREAMER
    IsGStreamerSupportedType(aType) ||
#endif
#ifdef MOZ_MEDIA_PLUGINS
    (MediaDecoder::IsMediaPluginsEnabled() && IsMediaPluginsType(aType)) ||
#endif
#ifdef MOZ_WMF
    (IsWMFSupportedType(aType) &&
     Preferences::GetBool("media.windows-media-foundation.play-stand-alone", true)) ||
#endif
    false;
}

}
