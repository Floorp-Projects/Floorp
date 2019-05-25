/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositionRecorder.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "gfxPrefs.h"

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

CompositionRecorder::~CompositionRecorder() {}

void CompositionRecorder::RecordFrame(RecordedFrame* aFrame) {
  mCollectedFrames.AppendElement(aFrame);
}

void CompositionRecorder::WriteCollectedFrames() {
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);
  std::stringstream str;
  str << gfxPrefs::LayersWindowRecordingPath() << "windowrecording-"
      << std::put_time(&tm, "%Y%m%d%H%M%S");

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

}  // namespace layers
}  // namespace mozilla
