/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderTraits.h"
#include "MediaDecoder.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsMimeTypes.h"
#include "mozilla/Preferences.h"

#ifdef MOZ_ANDROID_OMX
#include "AndroidMediaPluginHost.h"
#endif

#include "OggDecoder.h"
#include "OggReader.h"
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
#ifdef MOZ_ANDROID_OMX
#include "AndroidMediaPluginHost.h"
#include "AndroidMediaDecoder.h"
#include "AndroidMediaReader.h"
#include "AndroidMediaPluginHost.h"
#endif
#ifdef MOZ_OMX_DECODER
#include "MediaOmxDecoder.h"
#include "MediaOmxReader.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/HTMLMediaElement.h"
#if ANDROID_VERSION >= 18
#include "MediaCodecDecoder.h"
#include "MediaCodecReader.h"
#endif
#endif
#ifdef NECKO_PROTOCOL_rtsp
#if ANDROID_VERSION >= 18
#include "RtspMediaCodecDecoder.h"
#include "RtspMediaCodecReader.h"
#endif
#include "RtspOmxDecoder.h"
#include "RtspOmxReader.h"
#endif
#ifdef MOZ_WMF
#include "WMFDecoder.h"
#include "WMFReader.h"
#endif
#ifdef MOZ_DIRECTSHOW
#include "DirectShowDecoder.h"
#include "DirectShowReader.h"
#endif
#ifdef MOZ_APPLEMEDIA
#include "AppleDecoder.h"
#include "AppleMP3Reader.h"
#endif
#ifdef MOZ_FMP4
#include "MP4Reader.h"
#include "MP4Decoder.h"
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

static char const *const gWebMCodecs[7] = {
  "vp8",
  "vp8.0",
  "vp9",
  "vp9.0",
  "vorbis",
  "opus",
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
  if (IsOggType(aMimeType) && !Preferences::GetBool("media.prefer-gstreamer", false))
    return false;

  return GStreamerDecoder::CanHandleMediaType(aMimeType, nullptr);
}
#endif

#ifdef MOZ_OMX_DECODER
static const char* const gOmxTypes[] = {
  "audio/mpeg",
  "audio/mp4",
  "audio/amr",
  "audio/3gpp",
  "video/mp4",
  "video/3gpp",
  "video/3gpp2",
  "video/quicktime",
#ifdef MOZ_OMX_WEBM_DECODER
  "video/webm",
  "audio/webm",
#endif
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

#ifdef MOZ_OMX_WEBM_DECODER
static char const *const gOMXWebMCodecs[4] = {
  "vorbis",
  "vp8",
  "vp8.0",
  nullptr
};
#endif //MOZ_OMX_WEBM_DECODER

#endif

#ifdef NECKO_PROTOCOL_rtsp
static const char* const gRtspTypes[2] = {
    "RTSP",
    nullptr
};

static bool
IsRtspSupportedType(const nsACString& aMimeType)
{
  return MediaDecoder::IsRtspEnabled() &&
    CodecListContains(gRtspTypes, aMimeType);
}
#endif

/* static */
bool DecoderTraits::DecoderWaitsForOnConnected(const nsACString& aMimeType) {
#ifdef NECKO_PROTOCOL_rtsp
  return CodecListContains(gRtspTypes, aMimeType);
#else
  return false;
#endif
}

#ifdef MOZ_ANDROID_OMX
static bool
IsAndroidMediaType(const nsACString& aType)
{
  if (!MediaDecoder::IsAndroidMediaEnabled()) {
    return false;
  }

  static const char* supportedTypes[] = {
    "audio/mpeg", "audio/mp4", "video/mp4", nullptr
  };
  return CodecListContains(supportedTypes, aType);
}
#endif

#ifdef MOZ_WMF
static bool
IsWMFSupportedType(const nsACString& aType)
{
  return WMFDecoder::CanPlayType(aType, NS_LITERAL_STRING(""));
}
#endif

#ifdef MOZ_DIRECTSHOW
static bool
IsDirectShowSupportedType(const nsACString& aType)
{
  return DirectShowDecoder::GetSupportedCodecs(aType, nullptr);
}
#endif

#ifdef MOZ_FMP4
static bool
IsMP4SupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
// Currently on B2G, FMP4 is only working for MSE playback.
// For other normal MP4, it still uses current omx decoder.
// Bug 1061034 is a follow-up bug to enable all MP4s with MOZ_FMP4
#ifdef MOZ_OMX_DECODER
  return false;
#else
  return Preferences::GetBool("media.fragmented-mp4.exposed", false) &&
         MP4Decoder::CanHandleMediaType(aType, aCodecs);
#endif
}
#endif

#ifdef MOZ_APPLEMEDIA
static const char * const gAppleMP3Types[] = {
  "audio/mp3",
  "audio/mpeg",
  nullptr,
};

static const char * const gAppleMP3Codecs[] = {
  "mp3",
  nullptr
};

static bool
IsAppleMediaSupportedType(const nsACString& aType,
                     const char * const ** aCodecs = nullptr)
{
  if (MediaDecoder::IsAppleMP3Enabled()
      && CodecListContains(gAppleMP3Types, aType)) {

    if (aCodecs) {
      *aCodecs = gAppleMP3Codecs;
    }

    return true;
  }

  // TODO MP4

  return false;
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
  if (IsOggType(nsDependentCString(aMIMEType))) {
    codecList = MediaDecoder::IsOpusEnabled() ? gOggCodecsWithOpus : gOggCodecs;
    result = CANPLAY_MAYBE;
  }
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    codecList = gWaveCodecs;
    result = CANPLAY_MAYBE;
  }
#endif
#if defined(MOZ_WEBM) && !defined(MOZ_OMX_WEBM_DECODER)
  if (IsWebMType(nsDependentCString(aMIMEType))) {
    codecList = gWebMCodecs;
    result = CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_FMP4
  if (IsMP4SupportedType(nsDependentCString(aMIMEType),
                                     aRequestedCodecs)) {
    return aHaveRequestedCodecs ? CANPLAY_YES : CANPLAY_MAYBE;
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
#ifdef MOZ_OMX_WEBM_DECODER
    } else if (nsDependentCString(aMIMEType).EqualsASCII("audio/webm") ||
               nsDependentCString(aMIMEType).EqualsASCII("video/webm")) {
      codecList = gOMXWebMCodecs;
#endif
    } else {
      codecList = gH264Codecs;
    }
  }
#endif
#ifdef MOZ_DIRECTSHOW
  // Note: DirectShow should come before WMF, so that we prefer DirectShow's
  // MP3 support over WMF's.
  if (DirectShowDecoder::GetSupportedCodecs(nsDependentCString(aMIMEType), &codecList)) {
    result = CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_WMF
  if (!Preferences::GetBool("media.fragmented-mp4.exposed", false) &&
      IsWMFSupportedType(nsDependentCString(aMIMEType))) {
    if (!aHaveRequestedCodecs) {
      return CANPLAY_MAYBE;
    }
    return WMFDecoder::CanPlayType(nsDependentCString(aMIMEType),
                                   aRequestedCodecs)
           ? CANPLAY_YES : CANPLAY_NO;
  }
#endif
#ifdef MOZ_APPLEMEDIA
  if (IsAppleMediaSupportedType(nsDependentCString(aMIMEType), &codecList)) {
    result = CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaEnabled() &&
      GetAndroidMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), &codecList))
    result = CANPLAY_MAYBE;
#endif
#ifdef NECKO_PROTOCOL_rtsp
  if (IsRtspSupportedType(nsDependentCString(aMIMEType))) {
    result = CANPLAY_MAYBE;
  }
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
    expectMoreTokens = tokenizer.separatorAfterCurrentToken();
  }
  if (expectMoreTokens) {
    // Last codec name was empty
    return CANPLAY_NO;
  }
  return CANPLAY_YES;
}

// Instantiates but does not initialize decoder.
static
already_AddRefed<MediaDecoder>
InstantiateDecoder(const nsACString& aType, MediaDecoderOwner* aOwner)
{
  nsRefPtr<MediaDecoder> decoder;

#ifdef MOZ_FMP4
  if (IsMP4SupportedType(aType)) {
    decoder = new MP4Decoder();
    return decoder.forget();
  }
#endif
#ifdef MOZ_GSTREAMER
  if (IsGStreamerSupportedType(aType)) {
    decoder = new GStreamerDecoder();
    return decoder.forget();
  }
#endif
#ifdef MOZ_RAW
  if (IsRawType(aType)) {
    decoder = new RawDecoder();
    return decoder.forget();
  }
#endif
  if (IsOggType(aType)) {
    decoder = new OggDecoder();
    return decoder.forget();
  }
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    decoder = new WaveDecoder();
    return decoder.forget();
  }
#endif
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(aType)) {
    // AMR audio is enabled for MMS, but we are discouraging Web and App
    // developers from using AMR, thus we only allow AMR to be played on WebApps.
    if (aType.EqualsLiteral(AUDIO_AMR) || aType.EqualsLiteral(AUDIO_3GPP)) {
      dom::HTMLMediaElement* element = aOwner->GetMediaElement();
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
#if ANDROID_VERSION >= 18
    decoder = MediaDecoder::IsOmxAsyncEnabled()
      ? static_cast<MediaDecoder*>(new MediaCodecDecoder())
      : static_cast<MediaDecoder*>(new MediaOmxDecoder());
#else
    decoder = new MediaOmxDecoder();
#endif
    return decoder.forget();
  }
#endif
#ifdef NECKO_PROTOCOL_rtsp
  if (IsRtspSupportedType(aType)) {
#if ANDROID_VERSION >= 18
    decoder = MediaDecoder::IsOmxAsyncEnabled()
      ? static_cast<MediaDecoder*>(new RtspMediaCodecDecoder())
      : static_cast<MediaDecoder*>(new RtspOmxDecoder());
#else
    decoder = new RtspOmxDecoder();
#endif
    return decoder.forget();
  }
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaEnabled() &&
      GetAndroidMediaPluginHost()->FindDecoder(aType, nullptr)) {
    decoder = new AndroidMediaDecoder(aType);
    return decoder.forget();
  }
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(aType)) {
    decoder = new WebMDecoder();
    return decoder.forget();
  }
#endif
#ifdef MOZ_DIRECTSHOW
  // Note: DirectShow should come before WMF, so that we prefer DirectShow's
  // MP3 support over WMF's.
  if (IsDirectShowSupportedType(aType)) {
    decoder = new DirectShowDecoder();
    return decoder.forget();
  }
#endif
#ifdef MOZ_WMF
  if (IsWMFSupportedType(aType)) {
    decoder = new WMFDecoder();
    return decoder.forget();
  }
#endif
#ifdef MOZ_APPLEMEDIA
  if (IsAppleMediaSupportedType(aType)) {
    decoder = new AppleDecoder();
    return decoder.forget();
  }
#endif

  NS_ENSURE_TRUE(decoder != nullptr, nullptr);
  NS_ENSURE_TRUE(decoder->Init(aOwner), nullptr);
  return nullptr;
}

/* static */
already_AddRefed<MediaDecoder>
DecoderTraits::CreateDecoder(const nsACString& aType, MediaDecoderOwner* aOwner)
{
  nsRefPtr<MediaDecoder> decoder(InstantiateDecoder(aType, aOwner));
  NS_ENSURE_TRUE(decoder != nullptr, nullptr);
  NS_ENSURE_TRUE(decoder->Init(aOwner), nullptr);

  return decoder.forget();
}

/* static */
MediaDecoderReader* DecoderTraits::CreateReader(const nsACString& aType, AbstractMediaDecoder* aDecoder)
{
  MediaDecoderReader* decoderReader = nullptr;

#ifdef MOZ_FMP4
  if (IsMP4SupportedType(aType)) {
    decoderReader = new MP4Reader(aDecoder);
  } else
#endif
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
  if (IsOggType(aType)) {
    decoderReader = new OggReader(aDecoder);
  } else
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    decoderReader = new WaveReader(aDecoder);
  } else
#endif
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(aType)) {
#if ANDROID_VERSION >= 18
    decoderReader = MediaDecoder::IsOmxAsyncEnabled()
      ? static_cast<MediaDecoderReader*>(new MediaCodecReader(aDecoder))
      : static_cast<MediaDecoderReader*>(new MediaOmxReader(aDecoder));
#else
    decoderReader = new MediaOmxReader(aDecoder);
#endif
  } else
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaEnabled() &&
      GetAndroidMediaPluginHost()->FindDecoder(aType, nullptr)) {
    decoderReader = new AndroidMediaReader(aDecoder, aType);
  } else
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(aType)) {
    decoderReader = new WebMReader(aDecoder);
  } else
#endif
#ifdef MOZ_DIRECTSHOW
  // Note: DirectShowReader is preferred for MP3, but if it's disabled we
  // fallback to the WMFReader.
  if (IsDirectShowSupportedType(aType)) {
    decoderReader = new DirectShowReader(aDecoder);
  } else
#endif
#ifdef MOZ_WMF
  if (IsWMFSupportedType(aType)) {
    decoderReader = new WMFReader(aDecoder);
  } else
#endif
#ifdef MOZ_APPLEMEDIA
  if (IsAppleMediaSupportedType(aType)) {
    decoderReader = new AppleMP3Reader(aDecoder);
  } else
#endif
  if (false) {} // dummy if to take care of the dangling else

  return decoderReader;
}

/* static */
bool DecoderTraits::IsSupportedInVideoDocument(const nsACString& aType)
{
  // Forbid playing media in video documents if the user has opted
  // not to, using either the legacy WMF specific pref, or the newer
  // catch-all pref.
  if (!Preferences::GetBool("media.windows-media-foundation.play-stand-alone", true) ||
      !Preferences::GetBool("media.play-stand-alone", true)) {
    return false;
  }

  return
    IsOggType(aType) ||
#ifdef MOZ_OMX_DECODER
    // We support amr inside WebApps on firefoxOS but not in general web content.
    // Ensure we dont create a VideoDocument when accessing amr URLs directly.
    (IsOmxSupportedType(aType) &&
     (!aType.EqualsLiteral(AUDIO_AMR) && !aType.EqualsLiteral(AUDIO_3GPP))) ||
#endif
#ifdef MOZ_WEBM
    IsWebMType(aType) ||
#endif
#ifdef MOZ_GSTREAMER
    IsGStreamerSupportedType(aType) ||
#endif
#ifdef MOZ_ANDROID_OMX
    (MediaDecoder::IsAndroidMediaEnabled() && IsAndroidMediaType(aType)) ||
#endif
#ifdef MOZ_FMP4
    IsMP4SupportedType(aType) ||
#endif
#ifdef MOZ_WMF
    IsWMFSupportedType(aType) ||
#endif
#ifdef MOZ_DIRECTSHOW
    IsDirectShowSupportedType(aType) ||
#endif
#ifdef MOZ_APPLEMEDIA
    IsAppleMediaSupportedType(aType) ||
#endif
#ifdef NECKO_PROTOCOL_rtsp
    IsRtspSupportedType(aType) ||
#endif
    false;
}

}
