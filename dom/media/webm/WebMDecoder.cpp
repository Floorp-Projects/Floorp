/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#ifdef MOZ_AV1
#include "AOMDecoder.h"
#endif
#include "MediaContainerType.h"
#include "MediaDecoderStateMachine.h"
#include "WebMDemuxer.h"
#include "WebMDecoder.h"
#include "VideoUtils.h"

namespace mozilla {

MediaDecoderStateMachine* WebMDecoder::CreateStateMachine()
{
  MediaFormatReaderInit init(this);
  init.mVideoFrameContainer = GetVideoFrameContainer();
  init.mKnowsCompositor = GetCompositor();
  init.mCrashHelper = GetOwner()->CreateGMPCrashHelper();
  mReader = new MediaFormatReader(init, new WebMDemuxer(mResource));
  return new MediaDecoderStateMachine(this, mReader);
}

/* static */
bool
WebMDecoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  if (!Preferences::GetBool("media.webm.enabled")) {
    return false;
  }

  bool isVideo = aContainerType.Type() == MEDIAMIMETYPE("video/webm");
  if (aContainerType.Type() != MEDIAMIMETYPE("audio/webm") && !isVideo) {
    return false;
  }

  const MediaCodecs& codecs = aContainerType.ExtendedType().Codecs();
  if (codecs.IsEmpty()) {
    // WebM guarantees that the only codecs it contained are vp8, vp9, opus or vorbis.
    return true;
  }
  // Verify that all the codecs specified are ones that we expect that
  // we can play.
  for (const auto& codec : codecs.Range()) {
    if (codec.EqualsLiteral("opus") || codec.EqualsLiteral("vorbis")) {
      continue;
    }
    // Note: Only accept VP8/VP9 in a video container type, not in an audio
    // container type.
    if (isVideo &&
        (codec.EqualsLiteral("vp8") || codec.EqualsLiteral("vp8.0") ||
         codec.EqualsLiteral("vp9") || codec.EqualsLiteral("vp9.0"))) {
      continue;
    }
#ifdef MOZ_AV1
    if (isVideo && AOMDecoder::IsSupportedCodec(codec)) {
      continue;
    }
#endif
    // Some unsupported codec.
    return false;
  }
  return true;
}

void
WebMDecoder::GetMozDebugReaderData(nsACString& aString)
{
  if (mReader) {
    mReader->GetMozDebugReaderData(aString);
  }
}

} // namespace mozilla

