/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PerformanceRecorder_h
#define mozilla_PerformanceRecorder_h

#include <type_traits>

#include "mozilla/Attributes.h"
#include "mozilla/BaseProfilerMarkersPrerequisites.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypedEnumBits.h"
#include "nsPrintfCString.h"
#include "nsStringFwd.h"
#include "nsTPriorityQueue.h"
#include "mozilla/ProfilerMarkers.h"

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
 * multiple times, either being a sub-stage of RequestDemux (in MSE case),
 * or being a sub-stage of RequestDecode (when sending data via IPC).
 *
 * RequestDecode : Record the time where MFR starts asking decoder to return
 * a decoded data to MFR receives a decoded data. As the decoder might be
 * remote, this stage might include the time spending on IPC trips. This
 * stage is a sub-stage of RequestData.
 *
 * CopyDecodedVideo : If we can't reuse same decoder texture to the
 * compositor, then we have to copy video data to to another sharable
 * texture. This records the time which we spend on copying data. This stage
 * is a sub- stage of RequestDecode.
 */
enum class MediaStage : uint8_t {
  Invalid,
  RequestData,
  RequestDemux,
  CopyDemuxedData,
  RequestDecode,
  CopyDecodedVideo,
};

class PlaybackStage {
 public:
  explicit PlaybackStage(MediaStage aStage, int32_t aHeight = 0,
                         MediaInfoFlag aFlag = MediaInfoFlag::None)
      : mStage(aStage), mHeight(aHeight), mFlag(aFlag) {
    MOZ_ASSERT(aStage != MediaStage::Invalid);
  }

  ProfilerString8View Name() const;
  const MarkerCategory& Category() const {
    return baseprofiler::category::MEDIA_PLAYBACK;
  }

  MediaStage mStage;
  int32_t mHeight;
  MediaInfoFlag mFlag;

 private:
  mutable Maybe<nsCString> mName;
};

class CopyVideoStage {
 public:
  CopyVideoStage(nsCString aSource, int32_t aWidth, int32_t aHeight)
      : mSource(std::move(aSource)), mWidth(aWidth), mHeight(aHeight) {}

  ProfilerString8View Name() const;
  const MarkerCategory& Category() const {
    return baseprofiler::category::MEDIA_RT;
  }

  // The name of the source that performs this stage.
  nsCString mSource;
  int32_t mWidth;
  int32_t mHeight;

 private:
  mutable Maybe<nsCString> mName;
};

class PerformanceRecorderBase {
 public:
  static bool IsMeasurementEnabled();
  static TimeStamp GetCurrentTimeForMeasurement();

  // Return the resolution range for the given height. Eg. V:1080<h<=1440.
  static const char* FindMediaResolution(int32_t aHeight);

 protected:
  // We would enable the measurement on testing.
  static inline bool sEnableMeasurementForTesting = false;
};

template <typename StageType>
class PerformanceRecorderImpl : public PerformanceRecorderBase {
 public:
  ~PerformanceRecorderImpl() = default;

  PerformanceRecorderImpl(PerformanceRecorderImpl&& aRhs) noexcept
      : mStages(std::move(aRhs.mStages)) {}
  PerformanceRecorderImpl& operator=(PerformanceRecorderImpl&&) = delete;
  PerformanceRecorderImpl(const PerformanceRecorderImpl&) = delete;
  PerformanceRecorderImpl& operator=(const PerformanceRecorderImpl&) = delete;

 protected:
  PerformanceRecorderImpl() = default;

  // Stores the stage with the current time as its start time, associated with
  // aId.
  template <typename... Args>
  void Start(int64_t aId, Args... aArgs) {
    if (IsMeasurementEnabled()) {
      mStages.Push(MakeTuple(aId, GetCurrentTimeForMeasurement(),
                             StageType(std::move(aArgs)...)));
    }
  }

  // Return the passed time since creation of the aId stage in microseconds if
  // it has not yet been recorded. Other stages with lower ids will be
  // discarded. Otherwise, return 0.
  template <typename F>
  float Record(int64_t aId, F&& aStageMutator) {
    while (!mStages.IsEmpty() && Get<0>(mStages.Top()) < aId) {
      mStages.Pop();
    }
    if (mStages.IsEmpty()) {
      return 0.0;
    }
    if (Get<0>(mStages.Top()) != aId) {
      return 0.0;
    }
    Entry entry = mStages.Pop();
    const auto& startTime = Get<1>(entry);
    auto& stage = Get<2>(entry);
    MOZ_ASSERT(Get<0>(entry) == aId);
    double elapsedTimeUs = 0.0;
    if (!startTime.IsNull() && IsMeasurementEnabled()) {
      const auto now = TimeStamp::Now();
      elapsedTimeUs = (now - startTime).ToMicroseconds();
      MOZ_ASSERT(elapsedTimeUs >= 0, "Elapsed time can't be less than 0!");
      aStageMutator(stage);
      AUTO_PROFILER_STATS(PROFILER_MARKER_UNTYPED);
      ::profiler_add_marker(
          stage.Name(), stage.Category(),
          MarkerOptions(MarkerTiming::Interval(startTime, now)));
    }
    return static_cast<float>(elapsedTimeUs);
  }
  float Record(int64_t aId) {
    return Record(aId, [](auto&) {});
  }

 protected:
  using Entry = Tuple<int64_t, TimeStamp, StageType>;

  struct IdComparator {
    bool LessThan(const Entry& aTupleA, const Entry& aTupleB) {
      return Get<0>(aTupleA) < Get<0>(aTupleB);
    }
  };

  nsTPriorityQueue<Entry, IdComparator> mStages;
};

/**
 * This class is used to record the time spent on different stages in the media
 * pipeline. `Record()` needs to be called explicitly to record a profiler
 * marker registering the time passed since creation. A stage may be mutated in
 * `Record()` in case data has become available since the recorder started.
 *
 * This variant is intended to be created on the stack when a stage starts, then
 * recorded with `Record()` when the stage is finished.
 */
template <typename StageType>
class PerformanceRecorder : public PerformanceRecorderImpl<StageType> {
  using Super = PerformanceRecorderImpl<StageType>;

 public:
  template <typename... Args>
  explicit PerformanceRecorder(Args... aArgs) {
    Start(std::move(aArgs)...);
  };

 private:
  template <typename... Args>
  void Start(Args... aArgs) {
    Super::Start(0, std::move(aArgs)...);
  }

 public:
  template <typename F>
  float Record(F&& aStageMutator) {
    return Super::Record(0, std::forward<F>(aStageMutator));
  }
  float Record() { return Super::Record(0); }
};

/**
 * This class is used to record the time spent on different stages in the media
 * pipeline. `Start()` and `Record()` needs to be called explicitly to record a
 * profiler marker registering the time passed since creation. A stage may be
 * mutated in `Record()` in case data has become available since the recorder
 * started.
 *
 * This variant is intended to be kept as a member in a class and supports async
 * stages. The async stages may overlap each other. To distinguish different
 * stages from each other, an int64_t is used as identifier. This is often a
 * timestamp in microseconds, see TimeUnit::ToMicroseconds.
 */
template <typename StageType>
class PerformanceRecorderMulti : public PerformanceRecorderImpl<StageType> {
  using Super = PerformanceRecorderImpl<StageType>;

 public:
  PerformanceRecorderMulti() = default;

  using Super::Record;
  using Super::Start;
};

}  // namespace mozilla

#endif  // mozilla_PerformanceRecorder_h
