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

#ifndef GMP_VIDEO_DECODE_h_
#define GMP_VIDEO_DECODE_h_

#include "gmp-video-errors.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"
#include "gmp-video-codec.h"
#include <stdint.h>

// ALL METHODS MUST BE CALLED ON THE MAIN THREAD
class GMPDecoderCallback
{
public:
  virtual ~GMPDecoderCallback() {}

  virtual void Decoded(GMPVideoi420Frame* aDecodedFrame) = 0;

  virtual void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) = 0;

  virtual void ReceivedDecodedFrame(const uint64_t aPictureId) = 0;

  virtual void InputDataExhausted() = 0;
};

// ALL METHODS MUST BE CALLED ON THE MAIN THREAD
class GMPVideoDecoder
{
public:
  virtual ~GMPVideoDecoder() {}

  // aCallback: Subclass should retain reference to it until DecodingComplete
  //            is called. Do not attempt to delete it, host retains ownership.
  virtual GMPVideoErr InitDecode(const GMPVideoCodec& aCodecSettings,
                                 GMPDecoderCallback* aCallback,
                                 int32_t aCoreCount) = 0;

  // Decode encoded frame (as a part of a video stream). The decoded frame
  // will be returned to the user through the decode complete callback.
  //
  // inputFrame:        Frame to decode.
  //
  // missingFrames:     True if one or more frames have been lost since the previous decode call.
  //
  // fragmentation:     Specifies where the encoded frame can be split into separate fragments.
  //                    The meaning of fragment is codec specific, but often means that each
  //                    fragment is decodable by itself.
  //
  // codecSpecificInfo: Codec-specific data
  //
  // renderTimeMs :     System time to render in milliseconds. Only used by decoders with internal
  //                    rendering.
  virtual GMPVideoErr Decode(GMPVideoEncodedFrame* aInputFrame,
                             bool aMissingFrames,
                             const GMPCodecSpecificInfo& aCodecSpecificInfo,
                             int64_t aRenderTimeMs = -1) = 0;

  // Reset decoder state and prepare for a new call to Decode(...). Flushes the decoder pipeline.
  virtual GMPVideoErr Reset() = 0;

  // Output decoded frames for any data in the pipeline, regardless of ordering.
  virtual GMPVideoErr Drain() = 0;

  // May free decoder memory.
  virtual void DecodingComplete() = 0;
};

#endif // GMP_VIDEO_DECODE_h_
