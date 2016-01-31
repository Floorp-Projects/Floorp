/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_opengl_FPSCounter_h_
#define mozilla_layers_opengl_FPSCounter_h_

#include <algorithm>                    // for min
#include <stddef.h>                     // for size_t
#include <map>                          // for std::map
#include "GLDefs.h"                     // for GLuint
#include "mozilla/RefPtr.h"             // for already_AddRefed, RefCounted
#include "mozilla/TimeStamp.h"          // for TimeStamp, TimeDuration
#include "nsTArray.h"                   // for nsAutoTArray, nsTArray_Impl, etc
#include "prio.h"                       // for NSPR file i/o

namespace mozilla {
namespace layers {

class DataTextureSource;
class Compositor;

// Dump the FPS histogram every 10 seconds or kMaxFrameFPS
const int kFpsDumpInterval = 10;

// On desktop, we can have 240 hz monitors, so 10 seconds
// times 240 frames = 2400
const int kMaxFrames = 2400;

/**
 * The FPSCounter tracks how often we composite or have a layer transaction.
 * At each composite / layer transaction, we record the timestamp.
 * After kFpsDumpInterval number of composites / transactions, we calculate
 * the average and standard deviation of frames composited. We dump a histogram,
 * which allows for more statistically significant measurements. We also dump
 * absolute frame composite times to a file on the device.
 * The FPS counters displayed on screen are based on how many frames we
 * composited within the last ~1 second. The more accurate measurement is to
 * grab the histogram from stderr or grab the FPS timestamp dumps written to file.
 *
 * To enable dumping to file, enable
 * layers.acceleration.draw-fps.write-to-file pref.

  double AddFrameAndGetFps(TimeStamp aCurrentFrame) {
    AddFrame(aCurrentFrame);
    return EstimateFps(aCurrentFrame);
  }
 * To enable printing histogram data to logcat,
 * enable layers.acceleration.draw-fps.print-histogram
 *
 * Use the HasNext(), GetNextTimeStamp() like an iterator to read the data,
 * backwards in time. This abstracts away the mechanics of reading the data.
 */
class FPSCounter {
public:
  explicit FPSCounter(const char* aName);
  ~FPSCounter();

  void AddFrame(TimeStamp aTimestamp);
  double AddFrameAndGetFps(TimeStamp aTimestamp);
  double GetFPS(TimeStamp aTimestamp);

private:
  void      Init();
  bool      CapturedFullInterval(TimeStamp aTimestamp);

  // Used while iterating backwards over the data
  void      ResetReverseIterator();
  bool      HasNext(TimeStamp aTimestamp, double aDuration = kFpsDumpInterval);
  TimeStamp GetNextTimeStamp();
  int       GetLatestReadIndex();
  TimeStamp GetLatestTimeStamp();
  void      WriteFrameTimeStamps(PRFileDesc* fd);
  bool      IteratedFullInterval(TimeStamp aTimestamp, double aDuration);

  void      PrintFPS();
  int       BuildHistogram(std::map<int, int>& aHistogram);
  void      PrintHistogram(std::map<int, int>& aHistogram);
  double    GetMean(std::map<int,int> aHistogram);
  double    GetStdDev(std::map<int, int> aHistogram);
  nsresult  WriteFrameTimeStamps();

  /***
   * mFrameTimestamps is a psuedo circular buffer
   * Since we have a constant write time and don't
   * read at an offset except our latest write
   * we don't need an explicit read pointer.
   */
  nsAutoTArray<TimeStamp, kMaxFrames> mFrameTimestamps;
  int mWriteIndex;      // points to next open write slot
  int mIteratorIndex;   // used only when iterating
  const char* mFPSName;
  TimeStamp mLastInterval;
};

struct FPSState {
  FPSState();
  void DrawFPS(TimeStamp, int offsetX, int offsetY, unsigned, Compositor* aCompositor);
  void NotifyShadowTreeTransaction() {
    mTransactionFps.AddFrame(TimeStamp::Now());
  }

  FPSCounter mCompositionFps;
  FPSCounter mTransactionFps;

private:
  RefPtr<DataTextureSource> mFPSTextureSource;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_opengl_FPSCounter_h_
