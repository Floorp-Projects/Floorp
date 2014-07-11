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

#include "gmp-errors.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"
#include "gmp-video-codec.h"
#include <stdint.h>

// ALL METHODS MUST BE CALLED ON THE MAIN THREAD
class GMPVideoDecoderCallback
{
public:
  virtual ~GMPVideoDecoderCallback() {}

  virtual void Decoded(GMPVideoi420Frame* aDecodedFrame) = 0;

  virtual void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) = 0;

  virtual void ReceivedDecodedFrame(const uint64_t aPictureId) = 0;

  virtual void InputDataExhausted() = 0;

  virtual void DrainComplete() = 0;

  virtual void ResetComplete() = 0;
};

// ALL METHODS MUST BE CALLED ON THE MAIN THREAD
class GMPVideoDecoder
{
public:
  virtual ~GMPVideoDecoder() {}

  // - aCodecSettings: Details of decoder to create.
  // - aCodecSpecific: codec specific data, cast to a GMPVideoCodecXXX struct
  //                   to get codec specific config data.
  // - aCodecSpecificLength: number of bytes in aCodecSpecific.
  // - aCallback: Subclass should retain reference to it until DecodingComplete
  //              is called. Do not attempt to delete it, host retains ownership.
  // aCoreCount: number of CPU cores.
  virtual GMPErr InitDecode(const GMPVideoCodec& aCodecSettings,
                            const uint8_t* aCodecSpecific,
                            uint32_t aCodecSpecificLength,
                            GMPVideoDecoderCallback* aCallback,
                            int32_t aCoreCount) = 0;

  // Decode encoded frame (as a part of a video stream). The decoded frame
  // will be returned to the user through the decode complete callback.
  //
  // - aInputFrame: Frame to decode. Call Destroy() on frame when it's decoded.
  // - aMissingFrames: True if one or more frames have been lost since the
  //                   previous decode call.
  // - aCodecSpecificInfo : codec specific data, pointer to a
  //                        GMPCodecSpecificInfo structure appropriate for
  //                        this codec type.
  // - aCodecSpecificInfoLength : number of bytes in aCodecSpecificInfo
  // - renderTimeMs : System time to render in milliseconds. Only used by
  //                  decoders with internal rendering.
  virtual GMPErr Decode(GMPVideoEncodedFrame* aInputFrame,
                        bool aMissingFrames,
                        const uint8_t* aCodecSpecificInfo,
                        uint32_t aCodecSpecificInfoLength,
                        int64_t aRenderTimeMs = -1) = 0;

  // Reset decoder state and prepare for a new call to Decode(...).
  // Flushes the decoder pipeline.
  // The decoder should enqueue a task to run ResetComplete() on the main
  // thread once the reset has finished.
  virtual GMPErr Reset() = 0;

  // Output decoded frames for any data in the pipeline, regardless of ordering.
  // All remaining decoded frames should be immediately returned via callback.
  // The decoder should enqueue a task to run DrainComplete() on the main
  // thread once the reset has finished.
  virtual GMPErr Drain() = 0;

  // May free decoder memory.
  virtual void DecodingComplete() = 0;
};

#endif // GMP_VIDEO_DECODE_h_
