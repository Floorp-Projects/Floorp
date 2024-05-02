/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PerformanceRecorder_h
#define mozilla_PerformanceRecorder_h

#include <type_traits>
#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/BaseProfilerMarkersPrerequisites.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/ProfilerMarkerTypes.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypedEnumBits.h"
#include "nsStringFwd.h"
#include "nsTPriorityQueue.h"
#include "mozilla/ProfilerMarkers.h"

namespace mozilla {
namespace gfx {
enum class YUVColorSpace : uint8_t;
enum class ColorDepth : uint8_t;
enum class ColorRange : uint8_t;
}  // namespace gfx

struct TrackingId {
  enum class Source : uint8_t {
    Unimplemented,
    AudioDestinationNode,
    Camera,
    Canvas,
    ChannelDecoder,
    HLSDecoder,
    MediaCapabilities,
    MediaElementDecoder,
    MediaElementStream,
    MSEDecoder,
    RTCRtpReceiver,
    Screen,
    Tab,
    Window,
    LAST
  };
  enum class TrackAcrossProcesses : uint8_t {
    Yes,
    No,
  };
  TrackingId();
  TrackingId(Source aSource, uint32_t aUniqueInProcId,
             TrackAcrossProcesses aTrack = TrackAcrossProcesses::No);

  nsCString ToString() const;

  Source mSource;
  uint32_t mUniqueInProcId;
  Maybe<uint32_t> mProcId;
};

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
  VIDEO_THEORA = (1 << 8),
  VIDEO_HEVC = (1 << 9),
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

class StageBase {
 public:
  virtual void AddMarker(MarkerOptions&& aOption) {
    profiler_add_marker(Name(), Category(),
                        std::forward<MarkerOptions&&>(aOption));
  }

 protected:
  virtual ProfilerString8View Name() const = 0;
  virtual const MarkerCategory& Category() const = 0;
};

class PlaybackStage : public StageBase {
 public:
  explicit PlaybackStage(MediaStage aStage, int32_t aHeight = 0,
                         MediaInfoFlag aFlag = MediaInfoFlag::None)
      : mStage(aStage), mHeight(aHeight), mFlag(aFlag) {
    MOZ_ASSERT(aStage != MediaStage::Invalid);
  }

  ProfilerString8View Name() const override;
  const MarkerCategory& Category() const override {
    return baseprofiler::category::MEDIA_PLAYBACK;
  }
  void AddMarker(MarkerOptions&& aOption) override;

  void SetStartTimeAndEndTime(uint64_t aStartTime, uint64_t aEndTime) {
    mStartAndEndTimeUs =
        Some(std::pair<uint64_t, uint64_t>{aStartTime, aEndTime});
  }

  MediaStage mStage;
  int32_t mHeight;
  MediaInfoFlag mFlag;

  Maybe<std::pair<uint64_t, uint64_t>> mStartAndEndTimeUs;

 private:
  mutable Maybe<nsCString> mName;
};

class CaptureStage : public StageBase {
 public:
  enum class ImageType : uint8_t {
    Unknown,
    I420,
    YUY2,
    YV12,
    UYVY,
    NV12,
    NV21,
    MJPEG,
  };

  CaptureStage(nsCString aSource, TrackingId aTrackingId, int32_t aWidth,
               int32_t aHeight, ImageType aImageType)
      : mSource(std::move(aSource)),
        mTrackingId(std::move(aTrackingId)),
        mWidth(aWidth),
        mHeight(aHeight),
        mImageType(aImageType) {}

  ProfilerString8View Name() const override;
  const MarkerCategory& Category() const override {
    return baseprofiler::category::MEDIA_RT;
  }

  nsCString mSource;
  TrackingId mTrackingId;
  int32_t mWidth;
  int32_t mHeight;
  ImageType mImageType;

 private:
  mutable Maybe<nsCString> mName;
};

class CopyVideoStage : public StageBase {
 public:
  CopyVideoStage(nsCString aSource, TrackingId aTrackingId, int32_t aWidth,
                 int32_t aHeight)
      : mSource(std::move(aSource)),
        mTrackingId(std::move(aTrackingId)),
        mWidth(aWidth),
        mHeight(aHeight) {}

  ProfilerString8View Name() const override;
  const MarkerCategory& Category() const override {
    return baseprofiler::category::MEDIA_RT;
  }

  // The name of the source that performs this stage.
  nsCString mSource;
  // A unique id identifying the source of the video frame this stage is
  // performed for.
  TrackingId mTrackingId;
  int32_t mWidth;
  int32_t mHeight;

 private:
  mutable Maybe<nsCString> mName;
};

class DecodeStage : public StageBase {
 public:
  enum ImageFormat : uint8_t {
    YUV420P,
    YUV422P,
    YUV444P,
    NV12,
    YV12,
    NV21,
    P010,
    P016,
    RGBA32,
    RGB24,
    GBRP,
    ANDROID_SURFACE,
    VAAPI_SURFACE,
  };

  DecodeStage(nsCString aSource, TrackingId aTrackingId, MediaInfoFlag aFlag)
      : mSource(std::move(aSource)),
        mTrackingId(std::move(aTrackingId)),
        mFlag(aFlag) {}
  ProfilerString8View Name() const override;
  const MarkerCategory& Category() const override {
    return baseprofiler::category::MEDIA_PLAYBACK;
  }

  void SetResolution(int aWidth, int aHeight) {
    mWidth = Some(aWidth);
    mHeight = Some(aHeight);
  }
  void SetImageFormat(ImageFormat aFormat) { mImageFormat = Some(aFormat); }
  void SetYUVColorSpace(gfx::YUVColorSpace aColorSpace) {
    mYUVColorSpace = Some(aColorSpace);
  }
  void SetColorRange(gfx::ColorRange aColorRange) {
    mColorRange = Some(aColorRange);
  }
  void SetColorDepth(gfx::ColorDepth aColorDepth) {
    mColorDepth = Some(aColorDepth);
  }
  void SetStartTimeAndEndTime(uint64_t aStartTime, uint64_t aEndTime) {
    mStartAndEndTimeUs =
        Some(std::pair<uint64_t, uint64_t>{aStartTime, aEndTime});
  }
  void AddMarker(MarkerOptions&& aOption) override;

  // The name of the source that performs this stage.
  nsCString mSource;
  // A unique id identifying the source of the video frame this stage is
  // performed for.
  TrackingId mTrackingId;
  MediaInfoFlag mFlag;
  Maybe<int> mWidth;
  Maybe<int> mHeight;
  Maybe<ImageFormat> mImageFormat;
  Maybe<gfx::YUVColorSpace> mYUVColorSpace;
  Maybe<gfx::ColorRange> mColorRange;
  Maybe<gfx::ColorDepth> mColorDepth;
  mutable Maybe<nsCString> mName;
  Maybe<std::pair<uint64_t, uint64_t>> mStartAndEndTimeUs;
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
      MutexAutoLock lock(mMutex);
      mStages.Push(std::make_tuple(aId, GetCurrentTimeForMeasurement(),
                                   StageType(std::move(aArgs)...)));
    }
  }

  // Return the passed time since creation of the aId stage in microseconds if
  // it has not yet been recorded. Other stages with lower ids will be
  // discarded. Otherwise, return 0.
  template <typename F>
  float Record(int64_t aId, F&& aStageMutator) {
    Maybe<Entry> entry;
    {
      MutexAutoLock lock(mMutex);
      while (!mStages.IsEmpty() && std::get<0>(mStages.Top()) < aId) {
        mStages.Pop();
      }
      if (mStages.IsEmpty()) {
        return 0.0;
      }
      if (std::get<0>(mStages.Top()) != aId) {
        return 0.0;
      }
      entry = Some(mStages.Pop());
    }
    const auto& startTime = std::get<1>(*entry);
    auto& stage = std::get<2>(*entry);
    MOZ_ASSERT(std::get<0>(*entry) == aId);
    double elapsedTimeUs = 0.0;
    if (!startTime.IsNull() && IsMeasurementEnabled()) {
      const auto now = TimeStamp::Now();
      elapsedTimeUs = (now - startTime).ToMicroseconds();
      MOZ_ASSERT(elapsedTimeUs >= 0, "Elapsed time can't be less than 0!");
      aStageMutator(stage);
      AUTO_PROFILER_STATS(PROFILER_MARKER_UNTYPED);
      stage.AddMarker(MarkerOptions(MarkerTiming::Interval(startTime, now)));
    }
    return static_cast<float>(elapsedTimeUs);
  }
  float Record(int64_t aId) {
    return Record(aId, [](auto&) {});
  }

 protected:
  using Entry = std::tuple<int64_t, TimeStamp, StageType>;

  struct IdComparator {
    bool LessThan(const Entry& aTupleA, const Entry& aTupleB) {
      return std::get<0>(aTupleA) < std::get<0>(aTupleB);
    }
  };

  Mutex mMutex{"PerformanceRecorder::mMutex"};
  nsTPriorityQueue<Entry, IdComparator> mStages MOZ_GUARDED_BY(mMutex);
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
