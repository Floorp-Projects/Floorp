/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPrefs.h"
#include "MediaContainerType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "OggDemuxer.h"
#include "OggDecoder.h"

namespace mozilla {

MediaDecoderStateMachine* OggDecoder::CreateStateMachine()
{
  RefPtr<OggDemuxer> demuxer = new OggDemuxer(mResource);
  MediaDecoderReaderInit init(this);
  init.mVideoFrameContainer = GetVideoFrameContainer();
  mReader = new MediaFormatReader(init, demuxer);
  demuxer->SetChainingEvents(&mReader->TimedMetadataProducer(),
                             &mReader->MediaNotSeekableProducer());
  return new MediaDecoderStateMachine(this, mReader);
}

/* static */
bool
OggDecoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  if (!MediaPrefs::OggEnabled()) {
    return false;
  }

  if (aContainerType.Type() != MEDIAMIMETYPE("audio/ogg") &&
      aContainerType.Type() != MEDIAMIMETYPE("video/ogg") &&
      aContainerType.Type() != MEDIAMIMETYPE("application/ogg")) {
    return false;
  }

  const bool isOggVideo = (aContainerType.Type() != MEDIAMIMETYPE("audio/ogg"));

  const MediaCodecs& codecs = aContainerType.ExtendedType().Codecs();
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
    // Note: Only accept Theora in a video container type, not in an audio
    // container type.
    if (isOggVideo && codec.EqualsLiteral("theora")) {
      continue;
    }
    // Some unsupported codec.
    return false;
  }
  return true;
}

} // namespace mozilla
