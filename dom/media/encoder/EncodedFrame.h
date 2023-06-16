/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EncodedFrame_h_
#define EncodedFrame_h_

#include "nsISupportsImpl.h"
#include "mozilla/media/MediaUtils.h"
#include "TimeUnits.h"
#include "VideoUtils.h"

namespace mozilla {

// Represent an encoded frame emitted by an encoder
class EncodedFrame final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EncodedFrame)
 public:
  enum FrameType {
    VP8_I_FRAME,       // VP8 intraframe
    VP8_P_FRAME,       // VP8 predicted frame
    OPUS_AUDIO_FRAME,  // Opus audio frame
    UNKNOWN            // FrameType not set
  };
  using ConstFrameData = const media::Refcountable<nsTArray<uint8_t>>;
  using FrameData = media::Refcountable<nsTArray<uint8_t>>;
  EncodedFrame(const media::TimeUnit& aTime, uint64_t aDuration,
               uint64_t aDurationBase, FrameType aFrameType,
               RefPtr<ConstFrameData> aData)
      : mTime(aTime),
        mDuration(aDuration),
        mDurationBase(aDurationBase),
        mFrameType(aFrameType),
        mFrameData(std::move(aData)) {
    MOZ_ASSERT(mFrameData);
    MOZ_ASSERT_IF(mFrameType == VP8_I_FRAME, mDurationBase == PR_USEC_PER_SEC);
    MOZ_ASSERT_IF(mFrameType == VP8_P_FRAME, mDurationBase == PR_USEC_PER_SEC);
    MOZ_ASSERT_IF(mFrameType == OPUS_AUDIO_FRAME, mDurationBase == 48000);
  }
  // Timestamp in microseconds
  const media::TimeUnit mTime;
  // The playback duration of this packet in mDurationBase.
  const uint64_t mDuration;
  // The time base of mDuration.
  const uint64_t mDurationBase;
  // Represent what is in the FrameData
  const FrameType mFrameType;
  // Encoded data
  const RefPtr<ConstFrameData> mFrameData;

  // The end time of the frame in microseconds.
  media::TimeUnit GetEndTime() const {
    return mTime + media::TimeUnit(mDuration, mDurationBase);
  }

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~EncodedFrame() = default;
};

}  // namespace mozilla

#endif  // EncodedFrame_h_
