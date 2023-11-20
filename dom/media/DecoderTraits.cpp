/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderTraits.h"
#include "MediaContainerType.h"
#include "mozilla/Preferences.h"

#include "OggDecoder.h"
#include "OggDemuxer.h"

#include "WebMDecoder.h"
#include "WebMDemuxer.h"

#ifdef MOZ_ANDROID_HLS_SUPPORT
#  include "HLSDecoder.h"
#endif
#include "MP4Decoder.h"
#include "MP4Demuxer.h"
#include "MediaFormatReader.h"

#include "MP3Decoder.h"
#include "MP3Demuxer.h"

#include "WaveDecoder.h"
#include "WaveDemuxer.h"

#include "ADTSDecoder.h"
#include "ADTSDemuxer.h"

#include "FlacDecoder.h"
#include "FlacDemuxer.h"

namespace mozilla {

/* static */
bool DecoderTraits::IsHttpLiveStreamingType(const MediaContainerType& aType) {
  const auto& mimeType = aType.Type();
  return  // For m3u8.
          // https://tools.ietf.org/html/draft-pantos-http-live-streaming-19#section-10
      mimeType == MEDIAMIMETYPE("application/vnd.apple.mpegurl") ||
      // Some sites serve these as the informal m3u type.
      mimeType == MEDIAMIMETYPE("application/x-mpegurl") ||
      mimeType == MEDIAMIMETYPE("audio/mpegurl") ||
      mimeType == MEDIAMIMETYPE("audio/x-mpegurl");
}

/* static */
bool DecoderTraits::IsMatroskaType(const MediaContainerType& aType) {
  const auto& mimeType = aType.Type();
  // https://matroska.org/technical/specs/notes.html
  return mimeType == MEDIAMIMETYPE("audio/x-matroska") ||
         mimeType == MEDIAMIMETYPE("video/x-matroska");
}

static CanPlayStatus CanHandleCodecsType(
    const MediaContainerType& aType, DecoderDoctorDiagnostics* aDiagnostics) {
  // We should have been given a codecs string, though it may be empty.
  MOZ_ASSERT(aType.ExtendedType().HaveCodecs());

  // Container type with the MIME type, no codecs.
  const MediaContainerType mimeType(aType.Type());

  if (OggDecoder::IsSupportedType(mimeType)) {
    if (OggDecoder::IsSupportedType(aType)) {
      return CANPLAY_YES;
    }
    // We can only reach this position if a particular codec was requested,
    // ogg is supported and working: the codec must be invalid.
    return CANPLAY_NO;
  }
  if (WaveDecoder::IsSupportedType(MediaContainerType(mimeType))) {
    if (WaveDecoder::IsSupportedType(aType)) {
      return CANPLAY_YES;
    }
    // We can only reach this position if a particular codec was requested, wave
    // is supported and working: the codec must be invalid or not supported.
    return CANPLAY_NO;
  }
  if (WebMDecoder::IsSupportedType(mimeType)) {
    if (WebMDecoder::IsSupportedType(aType)) {
      return CANPLAY_YES;
    }
    // We can only reach this position if a particular codec was requested,
    // webm is supported and working: the codec must be invalid.
    return CANPLAY_NO;
  }
  if (MP4Decoder::IsSupportedType(mimeType,
                                  /* DecoderDoctorDiagnostics* */ nullptr)) {
    if (MP4Decoder::IsSupportedType(aType, aDiagnostics)) {
      return CANPLAY_YES;
    }
    // We can only reach this position if a particular codec was requested,
    // fmp4 is supported and working: the codec must be invalid.
    return CANPLAY_NO;
  }
  if (MP3Decoder::IsSupportedType(mimeType)) {
    if (MP3Decoder::IsSupportedType(aType)) {
      return CANPLAY_YES;
    }
    // We can only reach this position if a particular codec was requested,
    // mp3 is supported and working: the codec must be invalid.
    return CANPLAY_NO;
  }
  if (ADTSDecoder::IsSupportedType(mimeType)) {
    if (ADTSDecoder::IsSupportedType(aType)) {
      return CANPLAY_YES;
    }
    // We can only reach this position if a particular codec was requested,
    // adts is supported and working: the codec must be invalid.
    return CANPLAY_NO;
  }
  if (FlacDecoder::IsSupportedType(mimeType)) {
    if (FlacDecoder::IsSupportedType(aType)) {
      return CANPLAY_YES;
    }
    // We can only reach this position if a particular codec was requested,
    // flac is supported and working: the codec must be invalid.
    return CANPLAY_NO;
  }

  return CANPLAY_MAYBE;
}

static CanPlayStatus CanHandleMediaType(
    const MediaContainerType& aType, DecoderDoctorDiagnostics* aDiagnostics) {
  if (DecoderTraits::IsHttpLiveStreamingType(aType)) {
    Telemetry::Accumulate(Telemetry::MEDIA_HLS_CANPLAY_REQUESTED, true);
  }
#ifdef MOZ_ANDROID_HLS_SUPPORT
  if (HLSDecoder::IsSupportedType(aType)) {
    Telemetry::Accumulate(Telemetry::MEDIA_HLS_CANPLAY_SUPPORTED, true);
    return CANPLAY_MAYBE;
  }
#endif

  if (DecoderTraits::IsMatroskaType(aType)) {
    Telemetry::Accumulate(Telemetry::MEDIA_MKV_CANPLAY_REQUESTED, true);
  }

  if (aType.ExtendedType().HaveCodecs()) {
    CanPlayStatus result = CanHandleCodecsType(aType, aDiagnostics);
    if (result == CANPLAY_NO || result == CANPLAY_YES) {
      return result;
    }
  }

  // Container type with just the MIME type/subtype, no codecs.
  const MediaContainerType mimeType(aType.Type());

  if (OggDecoder::IsSupportedType(mimeType)) {
    return CANPLAY_MAYBE;
  }
  if (WaveDecoder::IsSupportedType(mimeType)) {
    return CANPLAY_MAYBE;
  }
  if (MP4Decoder::IsSupportedType(mimeType, aDiagnostics)) {
    return CANPLAY_MAYBE;
  }
  if (WebMDecoder::IsSupportedType(mimeType)) {
    return CANPLAY_MAYBE;
  }
  if (MP3Decoder::IsSupportedType(mimeType)) {
    return CANPLAY_MAYBE;
  }
  if (ADTSDecoder::IsSupportedType(mimeType)) {
    return CANPLAY_MAYBE;
  }
  if (FlacDecoder::IsSupportedType(mimeType)) {
    return CANPLAY_MAYBE;
  }
  return CANPLAY_NO;
}

/* static */
CanPlayStatus DecoderTraits::CanHandleContainerType(
    const MediaContainerType& aContainerType,
    DecoderDoctorDiagnostics* aDiagnostics) {
  return CanHandleMediaType(aContainerType, aDiagnostics);
}

/* static */
bool DecoderTraits::ShouldHandleMediaType(
    const char* aMIMEType, DecoderDoctorDiagnostics* aDiagnostics) {
  Maybe<MediaContainerType> containerType = MakeMediaContainerType(aMIMEType);
  if (!containerType) {
    return false;
  }

  if (WaveDecoder::IsSupportedType(*containerType)) {
    // We should not return true for Wave types, since there are some
    // Wave codecs actually in use in the wild that we don't support, and
    // we should allow those to be handled by plugins or helper apps.
    // Furthermore people can play Wave files on most platforms by other
    // means.
    return false;
  }

  return CanHandleMediaType(*containerType, aDiagnostics) != CANPLAY_NO;
}

/* static */
already_AddRefed<MediaDataDemuxer> DecoderTraits::CreateDemuxer(
    const MediaContainerType& aType, MediaResource* aResource) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<MediaDataDemuxer> demuxer;

  if (MP4Decoder::IsSupportedType(aType,
                                  /* DecoderDoctorDiagnostics* */ nullptr)) {
    demuxer = new MP4Demuxer(aResource);
  } else if (MP3Decoder::IsSupportedType(aType)) {
    demuxer = new MP3Demuxer(aResource);
  } else if (ADTSDecoder::IsSupportedType(aType)) {
    demuxer = new ADTSDemuxer(aResource);
  } else if (WaveDecoder::IsSupportedType(aType)) {
    demuxer = new WAVDemuxer(aResource);
  } else if (FlacDecoder::IsSupportedType(aType)) {
    demuxer = new FlacDemuxer(aResource);
  } else if (OggDecoder::IsSupportedType(aType)) {
    demuxer = new OggDemuxer(aResource);
  } else if (WebMDecoder::IsSupportedType(aType)) {
    demuxer = new WebMDemuxer(aResource);
  }

  return demuxer.forget();
}

/* static */
MediaFormatReader* DecoderTraits::CreateReader(const MediaContainerType& aType,
                                               MediaFormatReaderInit& aInit) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<MediaDataDemuxer> demuxer = CreateDemuxer(aType, aInit.mResource);
  if (!demuxer) {
    return nullptr;
  }

  MediaFormatReader* decoderReader = new MediaFormatReader(aInit, demuxer);

  if (OggDecoder::IsSupportedType(aType)) {
    static_cast<OggDemuxer*>(demuxer.get())
        ->SetChainingEvents(&decoderReader->TimedMetadataProducer(),
                            &decoderReader->MediaNotSeekableProducer());
  }

  return decoderReader;
}

/* static */
bool DecoderTraits::IsSupportedInVideoDocument(const nsACString& aType) {
  // Forbid playing media in video documents if the user has opted
  // not to, using either the legacy WMF specific pref, or the newer
  // catch-all pref.
  if (!Preferences::GetBool("media.wmf.play-stand-alone", true) ||
      !Preferences::GetBool("media.play-stand-alone", true)) {
    return false;
  }

  Maybe<MediaContainerType> type = MakeMediaContainerType(aType);
  if (!type) {
    return false;
  }

  return OggDecoder::IsSupportedType(*type) ||
         WebMDecoder::IsSupportedType(*type) ||
         MP4Decoder::IsSupportedType(*type,
                                     /* DecoderDoctorDiagnostics* */ nullptr) ||
         MP3Decoder::IsSupportedType(*type) ||
         ADTSDecoder::IsSupportedType(*type) ||
         FlacDecoder::IsSupportedType(*type) ||
#ifdef MOZ_ANDROID_HLS_SUPPORT
         HLSDecoder::IsSupportedType(*type) ||
#endif
         false;
}

/* static */
nsTArray<UniquePtr<TrackInfo>> DecoderTraits::GetTracksInfo(
    const MediaContainerType& aType) {
  // Container type with just the MIME type/subtype, no codecs.
  const MediaContainerType mimeType(aType.Type());

  if (OggDecoder::IsSupportedType(mimeType)) {
    return OggDecoder::GetTracksInfo(aType);
  }
  if (WaveDecoder::IsSupportedType(mimeType)) {
    return WaveDecoder::GetTracksInfo(aType);
  }
  if (MP4Decoder::IsSupportedType(mimeType, nullptr)) {
    return MP4Decoder::GetTracksInfo(aType);
  }
  if (WebMDecoder::IsSupportedType(mimeType)) {
    return WebMDecoder::GetTracksInfo(aType);
  }
  if (MP3Decoder::IsSupportedType(mimeType)) {
    return MP3Decoder::GetTracksInfo(aType);
  }
  if (ADTSDecoder::IsSupportedType(mimeType)) {
    return ADTSDecoder::GetTracksInfo(aType);
  }
  if (FlacDecoder::IsSupportedType(mimeType)) {
    return FlacDecoder::GetTracksInfo(aType);
  }
  return nsTArray<UniquePtr<TrackInfo>>();
}

}  // namespace mozilla
