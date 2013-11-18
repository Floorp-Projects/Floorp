/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_opengl_FPSCounter_h_
#define mozilla_layers_opengl_FPSCounter_h_

#include <stddef.h>                     // for size_t
#include <algorithm>                    // for min
#include "GLDefs.h"                     // for GLuint
#include "mozilla/TimeStamp.h"          // for TimeStamp, TimeDuration
#include "nsTArray.h"                   // for nsAutoTArray, nsTArray_Impl, etc
#include "VBOArena.h"                   // for gl::VBOArena

namespace mozilla {
namespace gl {
class GLContext;
}
namespace layers {

class ShaderProgramOGL;

const double kFpsWindowMs = 250.0;
const size_t kNumFrameTimeStamps = 16;
struct FPSCounter {
  FPSCounter() : mCurrentFrameIndex(0) {
      mFrames.SetLength(kNumFrameTimeStamps);
  }

  // We keep a circular buffer of the time points at which the last K
  // frames were drawn.  To estimate FPS, we count the number of
  // frames we've drawn within the last kFPSWindowMs milliseconds and
  // divide by the amount time since the first of those frames.
  nsAutoTArray<TimeStamp, kNumFrameTimeStamps> mFrames;
  size_t mCurrentFrameIndex;

  void AddFrame(TimeStamp aNewFrame) {
    mFrames[mCurrentFrameIndex] = aNewFrame;
    mCurrentFrameIndex = (mCurrentFrameIndex + 1) % kNumFrameTimeStamps;
  }

  double AddFrameAndGetFps(TimeStamp aCurrentFrame) {
    AddFrame(aCurrentFrame);
    return EstimateFps(aCurrentFrame);
  }

  double GetFpsAt(TimeStamp aNow) {
    return EstimateFps(aNow);
  }

private:
  double EstimateFps(TimeStamp aNow) {
    TimeStamp beginningOfWindow =
      (aNow - TimeDuration::FromMilliseconds(kFpsWindowMs));
    TimeStamp earliestFrameInWindow = aNow;
    size_t numFramesDrawnInWindow = 0;
    for (size_t i = 0; i < kNumFrameTimeStamps; ++i) {
      const TimeStamp& frame = mFrames[i];
      if (!frame.IsNull() && frame > beginningOfWindow) {
        ++numFramesDrawnInWindow;
        earliestFrameInWindow = std::min(earliestFrameInWindow, frame);
      }
    }
    double realWindowSecs = (aNow - earliestFrameInWindow).ToSeconds();
    if (realWindowSecs == 0.0 || numFramesDrawnInWindow == 1) {
      return 0.0;
    }
    return double(numFramesDrawnInWindow - 1) / realWindowSecs;
  }
};

struct FPSState {
  GLuint mTexture;
  FPSCounter mCompositionFps;
  FPSCounter mTransactionFps;
  gl::VBOArena mVBOs;

  FPSState() : mTexture(0) { }

  void DrawFPS(TimeStamp, unsigned, gl::GLContext*, ShaderProgramOGL*);

  void NotifyShadowTreeTransaction() {
    mTransactionFps.AddFrame(TimeStamp::Now());
  }
};

}
}

#endif // mozilla_layers_opengl_FPSCounter_h_
