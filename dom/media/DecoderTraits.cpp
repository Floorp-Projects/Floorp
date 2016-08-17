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
#include "mozilla/Telemetry.h"

#include "OggDecoder.h"
#include "OggReader.h"
#include "OggDemuxer.h"

#include "WebMDecoder.h"
#include "WebMDemuxer.h"

#ifdef MOZ_RAW
#include "RawDecoder.h"
#include "RawReader.h"
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

#include "WaveDecoder.h"
#include "WaveDemuxer.h"
#include "WaveReader.h"

#include "ADTSDecoder.h"
#include "ADTSDemuxer.h"

#include "nsPluginHost.h"

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

static char const *const gWaveCodecs[4] = {
  "1", // Microsoft PCM Format
  "6", // aLaw Encoding
  "7", // uLaw Encoding
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

static bool
IsWebMSupportedType(const nsACString& aType,
                    const nsAString& aCodecs = EmptyString())
{
  return WebMDecoder::CanHandleMediaType(aType, aCodecs);
}

/* static */ bool
DecoderTraits::IsWebMTypeAndEnabled(const nsACString& aType)
{
  return IsWebMSupportedType(aType);
}

/* static */ bool
DecoderTraits::IsWebMAudioType(const nsACString& aType)
{
  return aType.EqualsASCII("audio/webm");
}

static char const *const gHttpLiveStreamingTypes[] = {
  // For m3u8.
  // https://tools.ietf.org/html/draft-pantos-http-live-streaming-19#section-10
  "application/vnd.apple.mpegurl",
  // Some sites serve these as the informal m3u type.
  "application/x-mpegurl",
  "audio/x-mpegurl",
  nullptr
};

static bool
IsHttpLiveStreamingType(const nsACString& aType)
{
  return CodecListContains(gHttpLiveStreamingTypes, aType);
}

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

#ifdef MOZ_ANDROID_OMX
static bool
IsAndroidMediaType(const nsACString& aType)
{
  if (!MediaDecoder::IsAndroidMediaPluginEnabled()) {
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
                   DecoderDoctorDiagnostics* aDiagnostics,
                   const nsAString& aCodecs = EmptyString())
{
  return MP4Decoder::CanHandleMediaType(aType, aCodecs, aDiagnostics);
}
#endif

/* static */ bool
DecoderTraits::IsMP4TypeAndEnabled(const nsACString& aType,
                                   DecoderDoctorDiagnostics* aDiagnostics)
{
#ifdef MOZ_FMP4
  return IsMP4SupportedType(aType, aDiagnostics);
#else
  return false;
#endif
}

static bool
IsMP3SupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
#ifdef MOZ_OMX_DECODER
  return false;
#else
  return MP3Decoder::CanHandleMediaType(aType, aCodecs);
#endif
}

static bool
IsAACSupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return ADTSDecoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsWAVSupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return WaveDecoder::CanHandleMediaType(aType, aCodecs);
}

/* static */
bool DecoderTraits::ShouldHandleMediaType(const char* aMIMEType,
                                          DecoderDoctorDiagnostics* aDiagnostics)
{
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    // We should not return true for Wave types, since there are some
    // Wave codecs actually in use in the wild that we don't support, and
    // we should allow those to be handled by plugins or helper apps.
    // Furthermore people can play Wave files on most platforms by other
    // means.
    return false;
  }

  // If an external plugin which can handle quicktime video is available
  // (and not disabled), prefer it over native playback as there several
  // codecs found in the wild that we do not handle.
  if (nsDependentCString(aMIMEType).EqualsASCII("video/quicktime")) {
    RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
    if (pluginHost &&
        pluginHost->HavePluginForType(nsDependentCString(aMIMEType))) {
      return false;
    }
  }

  return CanHandleMediaType(aMIMEType, false, EmptyString(), aDiagnostics)
         != CANPLAY_NO;
}

/* static */
CanPlayStatus
DecoderTraits::CanHandleCodecsType(const char* aMIMEType,
                                   const nsAString& aRequestedCodecs,
                                   DecoderDoctorDiagnostics* aDiagnostics)
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
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    codecList = gWaveCodecs;
  }
#if !defined(MOZ_OMX_WEBM_DECODER)
  if (IsWebMTypeAndEnabled(nsDependentCString(aMIMEType))) {
    if (IsWebMSupportedType(nsDependentCString(aMIMEType), aRequestedCodecs)) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // webm is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
#endif
#ifdef MOZ_FMP4
  if (IsMP4TypeAndEnabled(nsDependentCString(aMIMEType), aDiagnostics)) {
    if (IsMP4SupportedType(nsDependentCString(aMIMEType), aDiagnostics, aRequestedCodecs)) {
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
  if (IsAACSupportedType(nsDependentCString(aMIMEType), aRequestedCodecs)) {
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
  if (MediaDecoder::IsAndroidMediaPluginEnabled()) {
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
                                  const nsAString& aRequestedCodecs,
                                  DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (IsHttpLiveStreamingType(nsDependentCString(aMIMEType))) {
    Telemetry::Accumulate(Telemetry::MEDIA_HLS_CANPLAY_REQUESTED, true);
  }

  if (aHaveRequestedCodecs) {
    CanPlayStatus result = CanHandleCodecsType(aMIMEType,
                                               aRequestedCodecs,
                                               aDiagnostics);
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
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
  if (IsMP4TypeAndEnabled(nsDependentCString(aMIMEType), aDiagnostics)) {
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
  if (IsAACSupportedType(nsDependentCString(aMIMEType))) {
    return CANPLAY_MAYBE;
  }
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
  if (MediaDecoder::IsAndroidMediaPluginEnabled() &&
      EnsureAndroidMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), nullptr)) {
    return CANPLAY_MAYBE;
  }
#endif
  return CANPLAY_NO;
}

// Instantiates but does not initialize decoder.
static
already_AddRefed<MediaDecoder>
InstantiateDecoder(const nsACString& aType,
                   MediaDecoderOwner* aOwner,
                   DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<MediaDecoder> decoder;

#ifdef MOZ_FMP4
  if (IsMP4SupportedType(aType, aDiagnostics)) {
    decoder = new MP4Decoder(aOwner);
    return decoder.forget();
  }
#endif
  if (IsMP3SupportedType(aType)) {
    decoder = new MP3Decoder(aOwner);
    return decoder.forget();
  }
  if (IsAACSupportedType(aType)) {
    decoder = new ADTSDecoder(aOwner);
    return decoder.forget();
  }
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
  if (IsWaveType(aType)) {
    decoder = new WaveDecoder(aOwner);
    return decoder.forget();
  }
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
    decoder = new MediaOmxDecoder(aOwner);
    return decoder.forget();
  }
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaPluginEnabled() &&
      EnsureAndroidMediaPluginHost()->FindDecoder(aType, nullptr)) {
    decoder = new AndroidMediaDecoder(aOwner, aType);
    return decoder.forget();
  }
#endif

  if (IsWebMSupportedType(aType)) {
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

  if (IsHttpLiveStreamingType(aType)) {
    // We don't have an HLS decoder.
    Telemetry::Accumulate(Telemetry::MEDIA_HLS_DECODER_SUCCESS, false);
  }

  return nullptr;
}

/* static */
already_AddRefed<MediaDecoder>
DecoderTraits::CreateDecoder(const nsACString& aType,
                             MediaDecoderOwner* aOwner,
                             DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());
  return InstantiateDecoder(aType, aOwner, aDiagnostics);
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
  if (IsMP4SupportedType(aType, /* DecoderDoctorDiagnostics* */ nullptr)) {
    decoderReader = new MediaFormatReader(aDecoder, new MP4Demuxer(aDecoder->GetResource()));
  } else
#endif
  if (IsMP3SupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new mp3::MP3Demuxer(aDecoder->GetResource()));
  } else
  if (IsAACSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new ADTSDemuxer(aDecoder->GetResource()));
  } else
  if (IsWAVSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new WAVDemuxer(aDecoder->GetResource()));
  } else
#ifdef MOZ_RAW
  if (IsRawType(aType)) {
    decoderReader = new RawReader(aDecoder);
  } else
#endif
  if (IsOggType(aType)) {
    decoderReader = Preferences::GetBool("media.format-reader.ogg", true) ?
      static_cast<MediaDecoderReader*>(new MediaFormatReader(aDecoder, new OggDemuxer(aDecoder->GetResource()))) :
      new OggReader(aDecoder);
  } else
  if (IsWaveType(aType)) {
    decoderReader = new WaveReader(aDecoder);
  } else
#ifdef MOZ_OMX_DECODER
  if (IsOmxSupportedType(aType)) {
    decoderReader = new MediaOmxReader(aDecoder);
  } else
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaPluginEnabled() &&
      EnsureAndroidMediaPluginHost()->FindDecoder(aType, nullptr)) {
    decoderReader = new AndroidMediaReader(aDecoder, aType);
  } else
#endif
  if (IsWebMSupportedType(aType)) {
    decoderReader =
      new MediaFormatReader(aDecoder, new WebMDemuxer(aDecoder->GetResource()));
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
    IsWebMSupportedType(aType) ||
#ifdef MOZ_ANDROID_OMX
    (MediaDecoder::IsAndroidMediaPluginEnabled() && IsAndroidMediaType(aType)) ||
#endif
#ifdef MOZ_FMP4
    IsMP4SupportedType(aType, /* DecoderDoctorDiagnostics* */ nullptr) ||
#endif
    IsMP3SupportedType(aType) ||
    IsAACSupportedType(aType) ||
#ifdef MOZ_DIRECTSHOW
    IsDirectShowSupportedType(aType) ||
#endif
    false;
}

} // namespace mozilla
