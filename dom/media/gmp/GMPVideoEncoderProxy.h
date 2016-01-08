/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoEncoderProxy_h_
#define GMPVideoEncoderProxy_h_

#include "nsTArray.h"
#include "gmp-video-encode.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"

#include "GMPCallbackBase.h"
#include "GMPUtils.h"

class GMPVideoEncoderCallbackProxy : public GMPCallbackBase {
public:
  virtual ~GMPVideoEncoderCallbackProxy() {}
  virtual void Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                       const nsTArray<uint8_t>& aCodecSpecificInfo) = 0;
  virtual void Error(GMPErr aError) = 0;
};

// A proxy to GMPVideoEncoder in the child process.
// GMPVideoEncoderParent exposes this to users the GMP.
// This enables Gecko to pass nsTArrays to the child GMP and avoid
// an extra copy when doing so.

// The consumer must call Close() when done with the codec, or when
// Terminated() is called by the GMP plugin indicating an abnormal shutdown
// of the underlying plugin.  After calling Close(), the consumer must
// not access this again.

// This interface is not thread-safe and must only be used from GMPThread.
class GMPVideoEncoderProxy {
public:
  virtual GMPErr InitEncode(const GMPVideoCodec& aCodecSettings,
                            const nsTArray<uint8_t>& aCodecSpecific,
                            GMPVideoEncoderCallbackProxy* aCallback,
                            int32_t aNumberOfCores,
                            uint32_t aMaxPayloadSize) = 0;
  virtual GMPErr Encode(mozilla::GMPUniquePtr<GMPVideoi420Frame> aInputFrame,
                        const nsTArray<uint8_t>& aCodecSpecificInfo,
                        const nsTArray<GMPVideoFrameType>& aFrameTypes) = 0;
  virtual GMPErr SetChannelParameters(uint32_t aPacketLoss, uint32_t aRTT) = 0;
  virtual GMPErr SetRates(uint32_t aNewBitRate, uint32_t aFrameRate) = 0;
  virtual GMPErr SetPeriodicKeyFrames(bool aEnable) = 0;
  virtual uint32_t GetPluginId() const = 0;

  // Call to tell GMP/plugin the consumer will no longer use this
  // interface/codec.
  virtual void Close() = 0;
};

#endif // GMPVideoEncoderProxy_h_
