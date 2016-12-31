/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderTraits.h"
#include "MediaContentType.h"
#include "MediaDecoder.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsMimeTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

#include "OggDecoder.h"
#include "OggDemuxer.h"

#include "WebMDecoder.h"
#include "WebMDemuxer.h"

#ifdef MOZ_ANDROID_OMX
#include "AndroidMediaDecoder.h"
#include "AndroidMediaReader.h"
#include "AndroidMediaPluginHost.h"
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

#include "ADTSDecoder.h"
#include "ADTSDemuxer.h"

#include "FlacDecoder.h"
#include "FlacDemuxer.h"

#include "nsPluginHost.h"
#include "MediaPrefs.h"

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

static bool
IsOggSupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return OggDecoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsOggTypeAndEnabled(const nsACString& aType)
{
  return IsOggSupportedType(aType);
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
IsMP4SupportedType(const MediaContentType& aParsedType,
                   DecoderDoctorDiagnostics* aDiagnostics)
{
  return MP4Decoder::CanHandleMediaType(aParsedType, aDiagnostics);
}
static bool
IsMP4SupportedType(const nsACString& aType,
                   DecoderDoctorDiagnostics* aDiagnostics)
{
  Maybe<MediaContentType> contentType = MakeMediaContentType(aType);
  if (!contentType) {
    return false;
  }
  return IsMP4SupportedType(*contentType, aDiagnostics);
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
  return MP3Decoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsAACSupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return ADTSDecoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsWaveSupportedType(const nsACString& aType,
                    const nsAString& aCodecs = EmptyString())
{
  return WaveDecoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsFlacSupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return FlacDecoder::CanHandleMediaType(aType, aCodecs);
}

static
CanPlayStatus
CanHandleCodecsType(const MediaContentType& aType,
                    DecoderDoctorDiagnostics* aDiagnostics)
{
  // We should have been given a codecs string, though it may be empty.
  MOZ_ASSERT(aType.ExtendedType().HaveCodecs());

  // Content type with the the MIME type, no codecs.
  const MediaContentType mimeType(aType.Type());

  char const* const* codecList = nullptr;
  if (IsOggTypeAndEnabled(mimeType.Type().AsString())) {
    if (IsOggSupportedType(aType.Type().AsString(),
                           aType.ExtendedType().Codecs().AsString())) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // ogg is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
  if (IsWaveSupportedType(mimeType.Type().AsString())) {
    if (IsWaveSupportedType(aType.Type().AsString(),
                            aType.ExtendedType().Codecs().AsString())) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // ogg is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
#if !defined(MOZ_OMX_WEBM_DECODER)
  if (DecoderTraits::IsWebMTypeAndEnabled(mimeType.Type().AsString())) {
    if (IsWebMSupportedType(aType.Type().AsString(),
                            aType.ExtendedType().Codecs().AsString())) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // webm is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
#endif
#ifdef MOZ_FMP4
  if (DecoderTraits::IsMP4TypeAndEnabled(mimeType.Type().AsString(), aDiagnostics)) {
    if (IsMP4SupportedType(aType, aDiagnostics)) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // fmp4 is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
#endif
  if (IsMP3SupportedType(mimeType.Type().AsString(),
                         aType.ExtendedType().Codecs().AsString())) {
    return CANPLAY_YES;
  }
  if (IsAACSupportedType(mimeType.Type().AsString(),
                         aType.ExtendedType().Codecs().AsString())) {
    return CANPLAY_YES;
  }
  if (IsFlacSupportedType(mimeType.Type().AsString(),
                          aType.ExtendedType().Codecs().AsString())) {
    return CANPLAY_YES;
  }
#ifdef MOZ_DIRECTSHOW
  DirectShowDecoder::GetSupportedCodecs(aType.Type().AsString(), &codecList);
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaPluginEnabled()) {
    EnsureAndroidMediaPluginHost()->FindDecoder(aType.Type().AsString(),
                                                &codecList);
  }
#endif
  if (!codecList) {
    return CANPLAY_MAYBE;
  }

  // See http://www.rfc-editor.org/rfc/rfc4281.txt for the description
  // of the 'codecs' parameter
  nsCharSeparatedTokenizer
    tokenizer(aType.ExtendedType().Codecs().AsString(), ',');
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

static
CanPlayStatus
CanHandleMediaType(const MediaContentType& aType,
                   DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (IsHttpLiveStreamingType(aType.Type().AsString())) {
    Telemetry::Accumulate(Telemetry::MEDIA_HLS_CANPLAY_REQUESTED, true);
  }

  if (aType.ExtendedType().HaveCodecs()) {
    CanPlayStatus result = CanHandleCodecsType(aType, aDiagnostics);
    if (result == CANPLAY_NO || result == CANPLAY_YES) {
      return result;
    }
  }

  // Content type with just the MIME type/subtype, no codecs.
  const MediaContentType mimeType(aType.Type());

  if (IsOggTypeAndEnabled(mimeType.Type().AsString())) {
    return CANPLAY_MAYBE;
  }
  if (IsWaveSupportedType(mimeType.Type().AsString())) {
    return CANPLAY_MAYBE;
  }
  if (DecoderTraits::IsMP4TypeAndEnabled(mimeType.Type().AsString(), aDiagnostics)) {
    return CANPLAY_MAYBE;
  }
#if !defined(MOZ_OMX_WEBM_DECODER)
  if (DecoderTraits::IsWebMTypeAndEnabled(mimeType.Type().AsString())) {
    return CANPLAY_MAYBE;
  }
#endif
  if (IsMP3SupportedType(mimeType.Type().AsString())) {
    return CANPLAY_MAYBE;
  }
  if (IsAACSupportedType(mimeType.Type().AsString())) {
    return CANPLAY_MAYBE;
  }
  if (IsFlacSupportedType(mimeType.Type().AsString())) {
    return CANPLAY_MAYBE;
  }
#ifdef MOZ_DIRECTSHOW
  if (DirectShowDecoder::GetSupportedCodecs(mimeType.Type().AsString(), nullptr)) {
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_ANDROID_OMX
  if (MediaDecoder::IsAndroidMediaPluginEnabled() &&
      EnsureAndroidMediaPluginHost()->FindDecoder(mimeType.Type().AsString(), nullptr)) {
    return CANPLAY_MAYBE;
  }
#endif
  return CANPLAY_NO;
}

/* static */
CanPlayStatus
DecoderTraits::CanHandleContentType(const MediaContentType& aContentType,
                                    DecoderDoctorDiagnostics* aDiagnostics)
{
  return CanHandleMediaType(aContentType, aDiagnostics);
}

/* static */
bool DecoderTraits::ShouldHandleMediaType(const char* aMIMEType,
                                          DecoderDoctorDiagnostics* aDiagnostics)
{
  if (IsWaveSupportedType(nsDependentCString(aMIMEType))) {
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

  Maybe<MediaContentType> parsed = MakeMediaContentType(aMIMEType);
  if (!parsed) {
    return false;
  }
  return CanHandleMediaType(*parsed, aDiagnostics)
         != CANPLAY_NO;
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
  if (IsOggSupportedType(aType)) {
    decoder = new OggDecoder(aOwner);
    return decoder.forget();
  }
  if (IsWaveSupportedType(aType)) {
    decoder = new WaveDecoder(aOwner);
    return decoder.forget();
  }
  if (IsFlacSupportedType(aType)) {
    decoder = new FlacDecoder(aOwner);
    return decoder.forget();
  }
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
  if (IsWaveSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new WAVDemuxer(aDecoder->GetResource()));
  } else
  if (IsFlacSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new FlacDemuxer(aDecoder->GetResource()));
  } else
  if (IsOggSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new OggDemuxer(aDecoder->GetResource()));
  } else
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
    IsOggSupportedType(aType) ||
    IsWebMSupportedType(aType) ||
#ifdef MOZ_ANDROID_OMX
    (MediaDecoder::IsAndroidMediaPluginEnabled() && IsAndroidMediaType(aType)) ||
#endif
#ifdef MOZ_FMP4
    IsMP4SupportedType(aType, /* DecoderDoctorDiagnostics* */ nullptr) ||
#endif
    IsMP3SupportedType(aType) ||
    IsAACSupportedType(aType) ||
    IsFlacSupportedType(aType) ||
#ifdef MOZ_DIRECTSHOW
    IsDirectShowSupportedType(aType) ||
#endif
    false;
}

} // namespace mozilla
