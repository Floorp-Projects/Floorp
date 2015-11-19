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

#include "OggDecoder.h"
#include "OggReader.h"
#ifdef MOZ_WAVE
#include "WaveDecoder.h"
#include "WaveReader.h"
#endif
#ifdef MOZ_WEBM
#include "WebMDecoder.h"
#include "WebMReader.h"
#include "WebMDemuxer.h"
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
#ifdef MOZ_DIRECTSHOW
#include "DirectShowDecoder.h"
#include "DirectShowReader.h"
#endif
#ifdef MOZ_FMP4
#include "MP4Decoder.h"
#include "MP4Demuxer.h"
#endif
#include "MediaFormatReader.h"

#include "MP3Decoder.h"
#include "MP3Demuxer.h"

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
#endif

/* static */ bool
DecoderTraits::IsWebMTypeAndEnabled(const nsACString& aType)
{
#ifdef MOZ_WEBM
  if (!MediaDecoder::IsWebMEnabled()) {
    return false;
  }

  return CodecListContains(gWebMTypes, aType);
#endif
  return false;
}

#ifdef MOZ_GSTREAMER
static bool
IsGStreamerSupportedType(const nsACString& aMimeType)
{
  if (DecoderTraits::IsWebMTypeAndEnabled(aMimeType))
    return false;

  if (!MediaDecoder::IsGStreamerEnabled())
    return false;

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
  "audio/flac",
  "video/mp4",
  "video/x-m4v",
  "video/3gpp",
  "video/3gpp2",
  "video/quicktime",
#ifdef MOZ_OMX_WEBM_DECODER
  "video/webm",
  "audio/webm",
#endif
  "audio/x-matroska",
  "video/mp2t",
  "video/avi",
  "video/x-matroska",
  nullptr
};

static const char* const gB2GOnlyTypes[] = {
  "audio/3gpp",
  "audio/amr",
  "audio/x-matroska",
  "video/mp2t",
  "video/avi",
  "video/x-matroska",
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

static bool
IsB2GSupportOnlyType(const nsACString& aType)
{
  return CodecListContains(gB2GOnlyTypes, aType);
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
static char const *const gOMXWebMCodecs[] = {
  "vorbis",
  "vp8",
  "vp8.0",
  // Since Android KK, VP9 SW decoder is supported.
  // http://developer.android.com/guide/appendix/media-formats.html
#if ANDROID_VERSION > 18
  "vp9",
  "vp9.0",
#endif
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
    "audio/mpeg", "audio/mp4", "video/mp4", "video/x-m4v", nullptr
  };
  return CodecListContains(supportedTypes, aType);
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
  return MP4Decoder::CanHandleMediaType(aType, aCodecs);
}
#endif

/* static */ bool
DecoderTraits::IsMP4TypeAndEnabled(const nsACString& aType)
{
#ifdef MOZ_FMP4
  return IsMP4SupportedType(aType);
#endif
  return false;
}

static bool
IsMP3SupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
#ifdef MOZ_OMX_DECODER
  return false;
#endif
  return MP3Decoder::CanHandleMediaType(aType, aCodecs);
}

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
DecoderTraits::CanHandleCodecsType(const char* aMIMEType,
                                   const nsAString& aRequestedCodecs)
{
  char const* const* codecList = nullptr;
#ifdef MOZ_RAW
  if (IsRawType(nsDependentCString(aMIMEType))) {
    codecList = gRawCodecs;
  }
#endif
  if (IsOggType(nsDependentCString(aMIMEType))) {
    codecList = MediaDecoder::IsOpusEnabled() ? gOggCodecsWithOpus : gOggCodecs;
  }
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    codecList = gWaveCodecs;
  }
#endif
#if !defined(MOZ_OMX_WEBM_DECODER)
  if (IsWebMTypeAndEnabled(nsDependentCString(aMIMEType))) {
    codecList = gWebMCodecs;
  }
#endif
#ifdef MOZ_FMP4
  if (IsMP4TypeAndEnabled(nsDependentCString(aMIMEType))) {
    if (IsMP4SupportedType(nsDependentCString(aMIMEType), aRequestedCodecs)) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // fmp4 is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
#endif
  if (IsMP3SupportedType(nsDependentCString(aMIMEType), aRequestedCodecs)) {
    return CANPLAY_YES;
  }
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(nsDependentCString(aMIMEType))) {
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
  DirectShowDecoder::GetSupportedCodecs(nsDependentCString(aMIMEType), &codecList);
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaEnabled()) {
    EnsureAndroidMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), &codecList);
  }
#endif
  if (!codecList) {
    return CANPLAY_MAYBE;
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

/* static */
CanPlayStatus
DecoderTraits::CanHandleMediaType(const char* aMIMEType,
                                  bool aHaveRequestedCodecs,
                                  const nsAString& aRequestedCodecs)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aHaveRequestedCodecs) {
    CanPlayStatus result = CanHandleCodecsType(aMIMEType, aRequestedCodecs);
    if (result == CANPLAY_NO || result == CANPLAY_YES) {
      return result;
    }
  }
#ifdef MOZ_RAW
  if (IsRawType(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
#endif
  if (IsOggType(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
#endif
  if (IsMP4TypeAndEnabled(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
#if !defined(MOZ_OMX_WEBM_DECODER)
  if (IsWebMTypeAndEnabled(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
#endif
  if (IsMP3SupportedType(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
#ifdef MOZ_GSTREAMER
  if (GStreamerDecoder::CanHandleMediaType(nsDependentCString(aMIMEType),
                                           aHaveRequestedCodecs ? &aRequestedCodecs : nullptr)) {
    return aHaveRequestedCodecs ? CANPLAY_YES : CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_DIRECTSHOW
  if (DirectShowDecoder::GetSupportedCodecs(nsDependentCString(aMIMEType), nullptr)) {
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaEnabled() &&
      EnsureAndroidMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), nullptr)) {
    return CANPLAY_MAYBE;
  }
#endif
#ifdef NECKO_PROTOCOL_rtsp
  if (IsRtspSupportedType(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
#endif
  return CANPLAY_NO;
}

// Instantiates but does not initialize decoder.
static
already_AddRefed<MediaDecoder>
InstantiateDecoder(const nsACString& aType, MediaDecoderOwner* aOwner)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<MediaDecoder> decoder;

#ifdef MOZ_FMP4
  if (IsMP4SupportedType(aType)) {
    decoder = new MP4Decoder(aOwner);
    return decoder.forget();
  }
#endif
  if (IsMP3SupportedType(aType)) {
    decoder = new MP3Decoder(aOwner);
    return decoder.forget();
  }
#ifdef MOZ_GSTREAMER
  if (IsGStreamerSupportedType(aType)) {
    decoder = new GStreamerDecoder(aOwner);
    return decoder.forget();
  }
#endif
#ifdef MOZ_RAW
  if (IsRawType(aType)) {
    decoder = new RawDecoder(aOwner);
    return decoder.forget();
  }
#endif
  if (IsOggType(aType)) {
    decoder = new OggDecoder(aOwner);
    return decoder.forget();
  }
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    decoder = new WaveDecoder(aOwner);
    return decoder.forget();
  }
#endif
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(aType)) {
    // we are discouraging Web and App developers from using those formats in
    // gB2GOnlyTypes, thus we only allow them to be played on WebApps.
    if (IsB2GSupportOnlyType(aType)) {
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
      ? static_cast<MediaDecoder*>(new MediaCodecDecoder(aOwner))
      : static_cast<MediaDecoder*>(new MediaOmxDecoder(aOwner));
#else
    decoder = new MediaOmxDecoder(aOwner);
#endif
    return decoder.forget();
  }
#endif
#ifdef NECKO_PROTOCOL_rtsp
  if (IsRtspSupportedType(aType)) {
#if ANDROID_VERSION >= 18
    decoder = MediaDecoder::IsOmxAsyncEnabled()
      ? static_cast<MediaDecoder*>(new RtspMediaCodecDecoder(aOwner))
      : static_cast<MediaDecoder*>(new RtspOmxDecoder(aOwner));
#else
    decoder = new RtspOmxDecoder(aOwner);
#endif
    return decoder.forget();
  }
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaEnabled() &&
      EnsureAndroidMediaPluginHost()->FindDecoder(aType, nullptr)) {
    decoder = new AndroidMediaDecoder(aOwner, aType);
    return decoder.forget();
  }
#endif
  if (DecoderTraits::IsWebMTypeAndEnabled(aType)) {
    decoder = new WebMDecoder(aOwner);
    return decoder.forget();
  }
#ifdef MOZ_DIRECTSHOW
  // Note: DirectShow should come before WMF, so that we prefer DirectShow's
  // MP3 support over WMF's.
  if (IsDirectShowSupportedType(aType)) {
    decoder = new DirectShowDecoder(aOwner);
    return decoder.forget();
  }
#endif

  return nullptr;
}

/* static */
already_AddRefed<MediaDecoder>
DecoderTraits::CreateDecoder(const nsACString& aType, MediaDecoderOwner* aOwner)
{
  MOZ_ASSERT(NS_IsMainThread());
  return InstantiateDecoder(aType, aOwner);
}

/* static */
MediaDecoderReader* DecoderTraits::CreateReader(const nsACString& aType, AbstractMediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaDecoderReader* decoderReader = nullptr;

  if (!aDecoder) {
    return decoderReader;
  }
#ifdef MOZ_FMP4
  if (IsMP4SupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new MP4Demuxer(aDecoder->GetResource()));
  } else
#endif
  if (IsMP3SupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new mp3::MP3Demuxer(aDecoder->GetResource()));
  } else
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
      EnsureAndroidMediaPluginHost()->FindDecoder(aType, nullptr)) {
    decoderReader = new AndroidMediaReader(aDecoder, aType);
  } else
#endif
  if (IsWebMTypeAndEnabled(aType)) {
    decoderReader = Preferences::GetBool("media.format-reader.webm", true) ?
      static_cast<MediaDecoderReader*>(new MediaFormatReader(aDecoder, new WebMDemuxer(aDecoder->GetResource()))) :
      new WebMReader(aDecoder);
  } else
#ifdef MOZ_DIRECTSHOW
  if (IsDirectShowSupportedType(aType)) {
    decoderReader = new DirectShowReader(aDecoder);
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
    // We support the formats in gB2GOnlyTypes only inside WebApps on firefoxOS
    // but not in general web content. Ensure we dont create a VideoDocument
    // when accessing those format URLs directly.
    (IsOmxSupportedType(aType) &&
     !IsB2GSupportOnlyType(aType)) ||
#endif
    IsWebMTypeAndEnabled(aType) ||
#ifdef MOZ_GSTREAMER
    IsGStreamerSupportedType(aType) ||
#endif
#ifdef MOZ_ANDROID_OMX
    (MediaDecoder::IsAndroidMediaEnabled() && IsAndroidMediaType(aType)) ||
#endif
#ifdef MOZ_FMP4
    IsMP4SupportedType(aType) ||
#endif
    IsMP3SupportedType(aType) ||
#ifdef MOZ_DIRECTSHOW
    IsDirectShowSupportedType(aType) ||
#endif
#ifdef NECKO_PROTOCOL_rtsp
    IsRtspSupportedType(aType) ||
#endif
    false;
}

} // namespace mozilla
