/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "WebMDemuxer.h"
#include "WebMReader.h"
#include "WebMDecoder.h"
#include "VideoUtils.h"

namespace mozilla {

MediaDecoderStateMachine* WebMDecoder::CreateStateMachine()
{
  bool useFormatDecoder =
    Preferences::GetBool("media.format-reader.webm", true);
  RefPtr<MediaDecoderReader> reader = useFormatDecoder ?
      static_cast<MediaDecoderReader*>(new MediaFormatReader(this, new WebMDemuxer(GetResource()), GetVideoFrameContainer())) :
      new WebMReader(this);
  return new MediaDecoderStateMachine(this, reader);
}

/* static */
bool
WebMDecoder::IsEnabled()
{
  return Preferences::GetBool("media.webm.enabled");
}

/* static */
bool
WebMDecoder::CanHandleMediaType(const nsACString& aMIMETypeExcludingCodecs,
                                const nsAString& aCodecs)
{
  if (!IsEnabled()) {
    return false;
  }

  const bool isWebMAudio = aMIMETypeExcludingCodecs.EqualsASCII("audio/webm");
  const bool isWebMVideo = aMIMETypeExcludingCodecs.EqualsASCII("video/webm");
  if (!isWebMAudio && !isWebMVideo) {
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
    if (codec.EqualsLiteral("opus") || codec.EqualsLiteral("vorbis")) {
      continue;
    }
    // Note: Only accept VP8/VP9 in a video content type, not in an audio
    // content type.
    if (isWebMVideo &&
        (codec.EqualsLiteral("vp8") || codec.EqualsLiteral("vp8.0") ||
         codec.EqualsLiteral("vp9") || codec.EqualsLiteral("vp9.0"))) {

      continue;
    }
    // Some unsupported codec.
    return false;
  }
  return true;
}

} // namespace mozilla

