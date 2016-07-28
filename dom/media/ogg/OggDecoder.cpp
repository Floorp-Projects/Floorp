/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "OggDemuxer.h"
#include "OggReader.h"
#include "OggDecoder.h"

namespace mozilla {

MediaDecoderStateMachine* OggDecoder::CreateStateMachine()
{
  bool useFormatDecoder =
    Preferences::GetBool("media.format-reader.ogg", true);
  RefPtr<OggDemuxer> demuxer =
    useFormatDecoder ? new OggDemuxer(GetResource()) : nullptr;
  RefPtr<MediaDecoderReader> reader = useFormatDecoder
    ? static_cast<MediaDecoderReader*>(new MediaFormatReader(this, demuxer, GetVideoFrameContainer()))
    : new OggReader(this);
  if (useFormatDecoder) {
    demuxer->SetChainingEvents(&reader->TimedMetadataProducer(),
                               &reader->MediaNotSeekableProducer());
  }
  return new MediaDecoderStateMachine(this, reader);
}

} // namespace mozilla
