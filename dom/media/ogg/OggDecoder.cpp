/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPrefs.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "OggDemuxer.h"
#include "OggReader.h"
#include "OggDecoder.h"
#include "nsContentTypeParser.h"

namespace mozilla {

MediaDecoderStateMachine* OggDecoder::CreateStateMachine()
{
  bool useFormatDecoder = MediaPrefs::OggFormatReader();
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

/* static */
bool
OggDecoder::IsEnabled()
{
  return MediaPrefs::OggEnabled();
}

/* static */
bool
OggDecoder::CanHandleMediaType(const nsACString& aMIMETypeExcludingCodecs,
                               const nsAString& aCodecs)
{
  if (!IsEnabled()) {
    return false;
  }

  const bool isOggAudio = aMIMETypeExcludingCodecs.EqualsASCII("audio/ogg");
  const bool isOggVideo =
    aMIMETypeExcludingCodecs.EqualsASCII("video/ogg") ||
    aMIMETypeExcludingCodecs.EqualsASCII("application/ogg");

  if (!isOggAudio && !isOggVideo) {
    return false;
  }

  nsTArray<nsCString> codecMimes;
  if (aCodecs.IsEmpty()) {
    // WebM guarantees that the only codecs it contained are vp8, vp9, opus or vorbis.
    return true;
  }
  // Verify that all the codecs specified are ones that we expect that
  // we can play.
  nsTArray<nsString> codecs;
  if (!ParseCodecsString(aCodecs, codecs)) {
    return false;
  }
  for (const nsString& codec : codecs) {
    if ((IsOpusEnabled() && codec.EqualsLiteral("opus")) ||
        codec.EqualsLiteral("vorbis")) {
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
