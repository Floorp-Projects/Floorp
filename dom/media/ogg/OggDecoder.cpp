/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPrefs.h"
#include "MediaContentType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "OggDemuxer.h"
#include "OggDecoder.h"
#include "nsContentTypeParser.h"

namespace mozilla {

MediaDecoderStateMachine* OggDecoder::CreateStateMachine()
{
  RefPtr<OggDemuxer> demuxer = new OggDemuxer(GetResource());
  RefPtr<MediaFormatReader> reader =
    new MediaFormatReader(this, demuxer, GetVideoFrameContainer());
  demuxer->SetChainingEvents(&reader->TimedMetadataProducer(),
                             &reader->MediaNotSeekableProducer());
  return new MediaDecoderStateMachine(this, reader);
}

/* static */
bool
OggDecoder::IsSupportedType(const MediaContentType& aContentType)
{
  if (!MediaPrefs::OggEnabled()) {
    return false;
  }

  if (aContentType.Type() != MEDIAMIMETYPE("audio/ogg") &&
      aContentType.Type() != MEDIAMIMETYPE("video/ogg") &&
      aContentType.Type() != MEDIAMIMETYPE("application/ogg")) {
    return false;
  }

  const bool isOggVideo = (aContentType.Type() != MEDIAMIMETYPE("audio/ogg"));

  const MediaCodecs& codecs = aContentType.ExtendedType().Codecs();
  if (codecs.IsEmpty()) {
    // WebM guarantees that the only codecs it contained are vp8, vp9, opus or vorbis.
    return true;
  }
  // Verify that all the codecs specified are ones that we expect that
  // we can play.
  for (const auto& codec : codecs.Range()) {
    if ((IsOpusEnabled() && codec.EqualsLiteral("opus")) ||
        codec.EqualsLiteral("vorbis") ||
        (MediaPrefs::FlacInOgg() && codec.EqualsLiteral("flac"))) {
      continue;
    }
    // Note: Only accept Theora in a video content type, not in an audio
    // content type.
    if (isOggVideo && codec.EqualsLiteral("theora")) {
      continue;
    }
    // Some unsupported codec.
    return false;
  }
  return true;
}

} // namespace mozilla
