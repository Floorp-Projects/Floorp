/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EncodedFrame_h_
#define EncodedFrame_h_

#include "nsISupportsImpl.h"
#include "VideoUtils.h"

namespace mozilla {

// Represent an encoded frame emitted by an encoder
class EncodedFrame final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EncodedFrame)
 public:
  EncodedFrame() : mTime(0), mDuration(0), mFrameType(UNKNOWN) {}
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
  // Timestamp in microseconds
  uint64_t mTime;
  // The time base of mDuration.
  uint64_t mDurationBase;
  // The playback duration of this packet in mDurationBase.
  uint64_t mDuration;
  // Represent what is in the FrameData
  FrameType mFrameType;

  // The end time of the frame in microseconds.
  uint64_t GetEndTime() const {
    return mTime + FramesToUsecs(mDuration, mDurationBase).value();
  }

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~EncodedFrame() = default;

  // Encoded data
  nsTArray<uint8_t> mFrameData;
};

}  // namespace mozilla

#endif  // EncodedFrame_h_
