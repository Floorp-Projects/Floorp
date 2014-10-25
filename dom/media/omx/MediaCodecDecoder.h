/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_CODEC_DECODER_H
#define MEDIA_CODEC_DECODER_H

#include "MediaOmxCommonDecoder.h"

namespace mozilla {

// MediaDecoder that uses MediaCodecReader.
class MediaCodecDecoder : public MediaOmxCommonDecoder
{
public:

  virtual MediaDecoder* Clone();

  virtual MediaOmxCommonReader* CreateReader();

  virtual MediaDecoderStateMachine* CreateStateMachine(MediaOmxCommonReader* aReader);
};

} // namespace mozilla

#endif // MEDIA_CODEC_DECODER_H
