/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 * Copyright (c) 2014, Mozilla
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 ** Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 ** Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 ** Neither the name of Google nor the names of its contributors may
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GMP_VIDEO_ENCODE_h_
#define GMP_VIDEO_ENCODE_h_

#include <vector>
#include <stdint.h>

#include "gmp-video-errors.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"
#include "gmp-video-codec.h"

// ALL METHODS MUST BE CALLED ON THE MAIN THREAD
class GMPEncoderCallback
{
public:
  virtual ~GMPEncoderCallback() {}

  virtual void Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                       const GMPCodecSpecificInfo& aCodecSpecificInfo) = 0;
};

// ALL METHODS MUST BE CALLED ON THE MAIN THREAD
class GMPVideoEncoder
{
public:
  virtual ~GMPVideoEncoder() {}

  // Initialize the encoder with the information from the VideoCodec.
  //
  // Input:
  // - codecSettings : Codec settings
  // - aCallback: Subclass should retain reference to it until EncodingComplete
  //              is called. Do not attempt to delete it, host retains ownership.
  // - numberOfCores : Number of cores available for the encoder
  // - maxPayloadSize : The maximum size each payload is allowed
  //                    to have. Usually MTU - overhead.
  virtual GMPVideoErr InitEncode(const GMPVideoCodec& aCodecSettings,
                                 GMPEncoderCallback* aCallback,
                                 int32_t aNumberOfCores,
                                 uint32_t aMaxPayloadSize) = 0;

  // Encode an I420 frame (as a part of a video stream). The encoded frame
  // will be returned to the user through the encode complete callback.
  //
  // Input:
  // - inputFrame : Frame to be encoded
  // - codecSpecificInfo : Pointer to codec specific data
  // - frame_types : The frame type to encode
  virtual GMPVideoErr Encode(GMPVideoi420Frame* aInputFrame,
                             const GMPCodecSpecificInfo& aCodecSpecificInfo,
                             const std::vector<GMPVideoFrameType>& aFrameTypes) = 0;

  // Inform the encoder about the packet loss and round trip time on the
  // network used to decide the best pattern and signaling.
  //
  // - packetLoss : Fraction lost (loss rate in percent =
  // 100 * packetLoss / 255)
  // - rtt : Round-trip time in milliseconds
  virtual GMPVideoErr SetChannelParameters(uint32_t aPacketLoss, uint32_t aRTT) = 0;

  // Inform the encoder about the new target bit rate.
  //
  // - newBitRate : New target bit rate
  // - frameRate : The target frame rate
  virtual GMPVideoErr SetRates(uint32_t aNewBitRate, uint32_t aFrameRate) = 0;

  // Use this function to enable or disable periodic key frames. Can be useful for codecs
  // which have other ways of stopping error propagation.
  //
  // - enable : Enable or disable periodic key frames
  virtual GMPVideoErr SetPeriodicKeyFrames(bool aEnable) = 0;

  // May free Encoder memory.
  virtual void EncodingComplete() = 0;
};

#endif // GMP_VIDEO_ENCODE_h_
