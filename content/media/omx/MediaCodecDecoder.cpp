/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCodecDecoder.h"

#include "MediaCodecReader.h"
#include "MediaDecoderStateMachine.h"

namespace mozilla {

MediaDecoder*
MediaCodecDecoder::Clone()
{
  return new MediaCodecDecoder();
}

MediaOmxCommonReader*
MediaCodecDecoder::CreateReader()
{
  return new MediaCodecReader(this);
}

MediaDecoderStateMachine*
MediaCodecDecoder::CreateStateMachine(MediaOmxCommonReader* aReader)
{
  return new MediaDecoderStateMachine(this, aReader);
}

} // namespace mozilla
