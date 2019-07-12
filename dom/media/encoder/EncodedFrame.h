/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EncodedFrame_h_
#define EncodedFrame_h_

#include "nsISupportsImpl.h"

namespace mozilla {

// Represent an encoded frame emitted by an encoder
class EncodedFrame final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EncodedFrame)
 public:
  EncodedFrame() : mTimeStamp(0), mDuration(0), mFrameType(UNKNOWN) {}
  enum FrameType {
    VP8_I_FRAME,       // VP8 intraframe
    VP8_P_FRAME,       // VP8 predicted frame
    OPUS_AUDIO_FRAME,  // Opus audio frame
    UNKNOWN            // FrameType not set
  };
  void SwapInFrameData(nsTArray<uint8_t>& aData) {
    mFrameData.SwapElements(aData);
  }
  nsresult SwapOutFrameData(nsTArray<uint8_t>& aData) {
    if (mFrameType != UNKNOWN) {
      // Reset this frame type to UNKNOWN once the data is swapped out.
      mFrameData.SwapElements(aData);
      mFrameType = UNKNOWN;
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  const nsTArray<uint8_t>& GetFrameData() const { return mFrameData; }
  uint64_t GetTimeStamp() const { return mTimeStamp; }
  void SetTimeStamp(uint64_t aTimeStamp) { mTimeStamp = aTimeStamp; }

  uint64_t GetDuration() const { return mDuration; }
  void SetDuration(uint64_t aDuration) { mDuration = aDuration; }

  FrameType GetFrameType() const { return mFrameType; }
  void SetFrameType(FrameType aFrameType) { mFrameType = aFrameType; }

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~EncodedFrame() {}

  // Encoded data
  nsTArray<uint8_t> mFrameData;
  uint64_t mTimeStamp;
  // The playback duration of this packet in number of samples
  uint64_t mDuration;
  // Represent what is in the FrameData
  FrameType mFrameType;
};

}  // namespace mozilla

#endif  // EncodedFrame_h_
