/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceRecorder.h"

#include "mozilla/Logging.h"
#include "mozilla/ProfilerMarkers.h"
#include "nsString.h"

namespace mozilla {

const char* StageToStr(PerformanceRecorder::Stage aStage) {
  switch (aStage) {
    case PerformanceRecorder::Stage::RequestData:
      return "RequestData";
    case PerformanceRecorder::Stage::RequestDemux:
      return "RequestDemux";
    case PerformanceRecorder::Stage::CopyDemuxedData:
      return "CopyDemuxedData";
    case PerformanceRecorder::Stage::RequestDecode:
      return "RequestDecode";
    case PerformanceRecorder::Stage::CopyDecodedVideo:
      return "CopyDecodedVideo";
    default:
      return "InvalidStage";
  }
}

/* static */
const char* PerformanceRecorder::FindMediaResolution(int32_t aHeight) {
  static const struct {
    const int32_t mH;
    const nsCString mRes;
  } sResolutions[] = {{0, "A:0"_ns},  // other followings are for video
                      {240, "V:0<h<=240"_ns},
                      {480, "V:240<h<=480"_ns},
                      {576, "V:480<h<=576"_ns},
                      {720, "V:576<h<=720"_ns},
                      {1080, "V:720<h<=1080"_ns},
                      {1440, "V:1080<h<=1440"_ns},
                      {2160, "V:1440<h<=2160"_ns},
                      {INT_MAX, "V:h>2160"_ns}};
  const char* resolution = sResolutions[0].mRes.get();
  for (auto&& res : sResolutions) {
    if (aHeight <= res.mH) {
      resolution = res.mRes.get();
      break;
    }
  }
  return resolution;
}

/* static */
bool PerformanceRecorder::IsMeasurementEnabled() {
  return profiler_thread_is_being_profiled_for_markers() ||
         PerformanceRecorder::sEnableMeasurementForTesting;
}

/* static */
TimeStamp PerformanceRecorder::GetCurrentTimeForMeasurement() {
  // The system call to get the clock is rather expensive on Windows. As we
  // only report the measurement report via markers, if the marker isn't enabled
  // then we won't do any measurement in order to save CPU time.
  return IsMeasurementEnabled() ? TimeStamp::Now() : TimeStamp();
}

void PerformanceRecorder::Start() {
  MOZ_ASSERT(mStage != Stage::Invalid);
  MOZ_ASSERT(!mStartTime);
  mStartTime = Some(GetCurrentTimeForMeasurement());
}

void PerformanceRecorder::Reset() {
  mStartTime.reset();
  mStage = Stage::Invalid;
}

void AppendMediaInfoFlagToName(nsCString& aName, MediaInfoFlag aFlag) {
  if (aFlag & MediaInfoFlag::KeyFrame) {
    aName.Append("kf,");
  }
  // Decoding
  if (aFlag & MediaInfoFlag::SoftwareDecoding) {
    aName.Append("sw,");
  } else if (aFlag & MediaInfoFlag::HardwareDecoding) {
    aName.Append("hw,");
  }
  // Codec type
  if (aFlag & MediaInfoFlag::VIDEO_AV1) {
    aName.Append("av1,");
  } else if (aFlag & MediaInfoFlag::VIDEO_H264) {
    aName.Append("h264,");
  } else if (aFlag & MediaInfoFlag::VIDEO_VP8) {
    aName.Append("vp8,");
  } else if (aFlag & MediaInfoFlag::VIDEO_VP9) {
    aName.Append("vp9,");
  }
}

float PerformanceRecorder::End() {
  double elapsedTimeUs = 0.0;
  if (mStartTime && !mStartTime->IsNull()) {
    MOZ_ASSERT(mStage != Stage::Invalid);
    if (IsMeasurementEnabled()) {
      const auto now = TimeStamp::Now();
      elapsedTimeUs = (now - *mStartTime).ToMicroseconds();
      MOZ_ASSERT(elapsedTimeUs >= 0, "Elapsed time can't be less than 0!");
      nsAutoCString name(StageToStr(mStage));
      name.Append(":");
      name.Append(FindMediaResolution(mHeight));
      name.Append(":");
      AppendMediaInfoFlagToName(name, mFlag);
      PROFILER_MARKER_UNTYPED(
          name, MEDIA_PLAYBACK,
          MarkerOptions(MarkerTiming::Interval(*mStartTime, now)));
    }
    Reset();
  }
  return elapsedTimeUs;
}

}  // namespace mozilla
