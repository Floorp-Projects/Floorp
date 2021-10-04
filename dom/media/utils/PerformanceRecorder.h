/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PerformanceRecorder_h
#define mozilla_PerformanceRecorder_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsStringFwd.h"

namespace mozilla {

/**
 * This class is used to record the passed time on the different stages in the
 * media playback pipeline. It needs to call `Start()` and `End()` explicitly
 * in order to record the passed time between these two calls.
 */
class PerformanceRecorder {
 public:
  /**
   * This represents the different stages that a media data will go through
   * within the playback journey.
   *
   *           |---|           |---|                 |------|
   *            Copy Demuxed    Copy Demuxed          Copy Decoded
   *            Data            Data                  Video
   *   |------------- |      |-----------------------------------|
   *     Request Demux         Request Decode
   * |-----------------------------------------------------------|
   *   Request Data
   *
   * RequestData : Record the time where MediaDecoderStateMachine(MDSM) starts
   * asking for a decoded data to MDSM receives a decoded data.
   *
   * RequestDemux : Record the time where MediaFormatReader(MFR) starts asking
   * a demuxed sample to MFR received a demuxed sample. This stage is a sub-
   * stage of RequestData.
   *
   * CopyDemuxedData : On some situations, we will need to copy the demuxed
   * data, which is still not decoded yet so its size is still small. This
   * records the time which we spend on copying data. This stage could happen
   * multiple times, either being a sub-stage of RequestDemux (in MSE case), or
   * being a sub-stage of RequestDecode (when sending data via IPC).
   *
   * RequestDecode : Record the time where MFR starts asking decoder to return
   * a decoded data to MFR receives a decoded data. As the decoder might be
   * remote, this stage might include the time spending on IPC trips. This stage
   * is a sub-stage of RequestData.
   *
   * CopyDecodedVideo : If we can't reuse same decoder texture to the
   * compositor, then we have to copy video data to to another sharable texture.
   * This records the time which we spend on copying data. This stage is a sub-
   * stage of RequestDecode.
   */
  enum class Stage : uint8_t {
    Invalid,
    RequestData,
    RequestDemux,
    CopyDemuxedData,
    RequestDecode,
    CopyDecodedVideo,
  };

  explicit PerformanceRecorder(Stage aStage, int32_t aHeight = 0)
      : mStage(aStage), mHeight(aHeight) {}
  ~PerformanceRecorder() = default;

  PerformanceRecorder(PerformanceRecorder&& aRhs) noexcept {
    mStage = aRhs.mStage;
    mHeight = aRhs.mHeight;
    mStartTime = std::move(aRhs.mStartTime);
    aRhs.mStage = Stage::Invalid;
  }

  PerformanceRecorder& operator=(PerformanceRecorder&& aRhs) noexcept {
    MOZ_ASSERT(&aRhs != this, "self-moves are prohibited");
    mStage = aRhs.mStage;
    mHeight = aRhs.mHeight;
    mStartTime = std::move(aRhs.mStartTime);
    aRhs.mStage = Stage::Invalid;
    return *this;
  }

  PerformanceRecorder(const PerformanceRecorder&) = delete;
  PerformanceRecorder& operator=(const PerformanceRecorder&) = delete;

  void Start();
  void End();

 private:
  void Reset();

  Stage mStage = Stage::Invalid;
  int32_t mHeight;
  Maybe<TimeStamp> mStartTime;
};

}  // namespace mozilla

#endif  // mozilla_PerformanceRecorder_h
