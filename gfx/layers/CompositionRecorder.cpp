/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositionRecorder.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"

#include <ctime>
#include <iomanip>
#include "stdio.h"
#ifdef XP_WIN
#  include "direct.h"
#else
#  include <sys/types.h>
#  include "sys/stat.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

CompositionRecorder::CompositionRecorder(TimeStamp aRecordingStart)
    : mRecordingStart(aRecordingStart) {}

void CompositionRecorder::RecordFrame(RecordedFrame* aFrame) {
  mCollectedFrames.AppendElement(aFrame);
}

void CompositionRecorder::WriteCollectedFrames() {
  // The directory has the format of
  // "[LayersWindowRecordingPath]/windowrecording-[mRecordingStartTime as unix
  // timestamp]". We need mRecordingStart as a unix timestamp here because we
  // want the consumer of these files to be able to compute an absolute
  // timestamp of each screenshot, so that we can align screenshots with timed
  // data from other sources, such as Gecko profiler information. The time of
  // each screenshot is part of the screenshot's filename, expressed as
  // milliseconds *relative to mRecordingStart*. We want to compute the number
  // of milliseconds between midnight 1 January 1970 UTC and mRecordingStart,
  // unfortunately, mozilla::TimeStamp does not have a built-in way of doing
  // that. However, PR_Now() returns the number of microseconds since midnight 1
  // January 1970 UTC. We call PR_Now() and TimeStamp::NowUnfuzzed() very
  // closely to each other so that they return their representation of "the same
  // time", and then compute (Now - (Now - mRecordingStart)).
  std::stringstream str;
  nsCString recordingStartTime;
  TimeDuration delta = TimeStamp::NowUnfuzzed() - mRecordingStart;
  recordingStartTime.AppendFloat(
      static_cast<double>(PR_Now() / 1000.0 - delta.ToMilliseconds()));
  str << gfxVars::LayersWindowRecordingPath() << "windowrecording-"
      << recordingStartTime;

#ifdef XP_WIN
  _mkdir(str.str().c_str());
#else
  mkdir(str.str().c_str(), 0777);
#endif

  uint32_t i = 1;
  for (RefPtr<RecordedFrame>& frame : mCollectedFrames) {
    RefPtr<DataSourceSurface> surf = frame->GetSourceSurface();
    std::stringstream filename;
    filename << str.str() << "/frame-" << i << "-"
             << uint32_t(
                    (frame->GetTimeStamp() - mRecordingStart).ToMilliseconds())
             << ".png";
    gfxUtils::WriteAsPNG(surf, filename.str().c_str());
    i++;
  }
  mCollectedFrames.Clear();
}

void CompositionRecorder::ClearCollectedFrames() { mCollectedFrames.Clear(); }

}  // namespace layers
}  // namespace mozilla
