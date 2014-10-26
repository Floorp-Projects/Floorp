/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoDecoderProxy_h_
#define GMPVideoDecoderProxy_h_

#include "nsTArray.h"
#include "gmp-video-decode.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"

#include "GMPCallbackBase.h"
#include "mozilla/UniquePtr.h"

class GMPVideoDecoderCallbackProxy : public GMPCallbackBase,
                                     public GMPVideoDecoderCallback
{
public:
  virtual ~GMPVideoDecoderCallbackProxy() {}
};

// A proxy to GMPVideoDecoder in the child process.
// GMPVideoDecoderParent exposes this to users the GMP.
// This enables Gecko to pass nsTArrays to the child GMP and avoid
// an extra copy when doing so.

// The consumer must call Close() when done with the codec, or when
// Terminated() is called by the GMP plugin indicating an abnormal shutdown
// of the underlying plugin.  After calling Close(), the consumer must
// not access this again.

// This interface is not thread-safe and must only be used from GMPThread.
class GMPVideoDecoderProxy {
public:
  virtual nsresult InitDecode(const GMPVideoCodec& aCodecSettings,
                              const nsTArray<uint8_t>& aCodecSpecific,
                              GMPVideoDecoderCallbackProxy* aCallback,
                              int32_t aCoreCount) = 0;
  virtual nsresult Decode(mozilla::UniquePtr<GMPVideoEncodedFrame> aInputFrame,
                          bool aMissingFrames,
                          const nsTArray<uint8_t>& aCodecSpecificInfo,
                          int64_t aRenderTimeMs = -1) = 0;
  virtual nsresult Reset() = 0;
  virtual nsresult Drain() = 0;
  virtual const uint64_t ParentID() = 0;

  // Call to tell GMP/plugin the consumer will no longer use this
  // interface/codec.
  virtual void Close() = 0;
};

namespace mozilla {

template<>
struct DefaultDelete<GMPVideoEncodedFrame>
{
  void operator()(GMPVideoEncodedFrame* aFrame) const
  {
    aFrame->Destroy();
  }
};

} // namespace mozilla

#endif
