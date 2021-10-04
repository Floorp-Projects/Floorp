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
#include "mozilla/TypedEnumBits.h"
#include "nsStringFwd.h"

namespace mozilla {

enum class MediaInfoFlag : uint16_t {
  None = (0 << 0),
  NonKeyFrame = (1 << 0),
  KeyFrame = (1 << 1),
  SoftwareDecoding = (1 << 2),
  HardwareDecoding = (1 << 3),
  VIDEO_AV1 = (1 << 4),
  VIDEO_H264 = (1 << 5),
  VIDEO_VP8 = (1 << 6),
  VIDEO_VP9 = (1 << 7),
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MediaInfoFlag)

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

  explicit PerformanceRecorder(Stage aStage, int32_t aHeight = 0,
                               MediaInfoFlag aFlag = MediaInfoFlag::None)
      : mStage(aStage), mHeight(aHeight), mFlag(aFlag) {}
  ~PerformanceRecorder() = default;

  PerformanceRecorder(PerformanceRecorder&& aRhs) noexcept {
    mStage = aRhs.mStage;
    mHeight = aRhs.mHeight;
    mStartTime = std::move(aRhs.mStartTime);
    mFlag = aRhs.mFlag;
    aRhs.mStage = Stage::Invalid;
  }

  PerformanceRecorder& operator=(PerformanceRecorder&& aRhs) noexcept {
    MOZ_ASSERT(&aRhs != this, "self-moves are prohibited");
    mStage = aRhs.mStage;
    mHeight = aRhs.mHeight;
    mStartTime = std::move(aRhs.mStartTime);
    mFlag = aRhs.mFlag;
    aRhs.mStage = Stage::Invalid;
    return *this;
  }

  PerformanceRecorder(const PerformanceRecorder&) = delete;
  PerformanceRecorder& operator=(const PerformanceRecorder&) = delete;

  void Start();

  // Return the passed time if it has started and still valid. Otherwise,
  // return 0.
  float End();

 protected:
  void Reset();

  static bool IsMeasurementEnabled();
  static TimeStamp GetCurrentTimeForMeasurement();

  // Return the resolution range for the given height. Eg. V:1080<h<=1440.
  static const char* FindMediaResolution(int32_t aHeight);

  Stage mStage = Stage::Invalid;
  int32_t mHeight;
  MediaInfoFlag mFlag = MediaInfoFlag::None;
  Maybe<TimeStamp> mStartTime;

  // We would enable the measurement on testing.
  static inline bool sEnableMeasurementForTesting = false;
};

}  // namespace mozilla

#endif  // mozilla_PerformanceRecorder_h
