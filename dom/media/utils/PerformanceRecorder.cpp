/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceRecorder.h"

#include "mozilla/Logging.h"

namespace mozilla {

const char* StageToStr(MediaStage aStage) {
  switch (aStage) {
    case MediaStage::RequestData:
      return "RequestData";
    case MediaStage::RequestDemux:
      return "RequestDemux";
    case MediaStage::CopyDemuxedData:
      return "CopyDemuxedData";
    case MediaStage::RequestDecode:
      return "RequestDecode";
    case MediaStage::CopyDecodedVideo:
      return "CopyDecodedVideo";
    default:
      return "InvalidStage";
  }
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

/* static */
const char* FindMediaResolution(int32_t aHeight) {
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
bool PerformanceRecorderBase::IsMeasurementEnabled() {
  return profiler_thread_is_being_profiled_for_markers() ||
         PerformanceRecorderBase::sEnableMeasurementForTesting;
}

/* static */
TimeStamp PerformanceRecorderBase::GetCurrentTimeForMeasurement() {
  // The system call to get the clock is rather expensive on Windows. As we
  // only report the measurement report via markers, if the marker isn't enabled
  // then we won't do any measurement in order to save CPU time.
  return IsMeasurementEnabled() ? TimeStamp::Now() : TimeStamp();
}

ProfilerString8View PlaybackStage::Name() const {
  if (!mName) {
    mName.emplace(StageToStr(mStage));
    mName->Append(":");
    mName->Append(FindMediaResolution(mHeight));
    mName->Append(":");
    AppendMediaInfoFlagToName(*mName, mFlag);
  }
  return *mName;
}

}  // namespace mozilla
