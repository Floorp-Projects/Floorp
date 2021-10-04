/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceRecorder.h"

#include "mozilla/Logging.h"
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

void PerformanceRecorder::Start() {
  MOZ_ASSERT(mStage != Stage::Invalid);
  MOZ_ASSERT(!mStartTime);
  mStartTime = Some(TimeStamp::Now());
}

void PerformanceRecorder::Reset() {
  mStartTime.reset();
  mStage = Stage::Invalid;
}

void PerformanceRecorder::End() {
  if (mStartTime) {
    MOZ_ASSERT(mStage != Stage::Invalid);
    const double passedTimeUs =
        (TimeStamp::Now() - *mStartTime).ToMicroseconds();
    MOZ_ASSERT(passedTimeUs > 0, "Passed time can't be less than 0!");
    // TODO : report the time in following patches.
    Reset();
  }
}

}  // namespace mozilla
