/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ProfilerScreenshots_h
#define mozilla_layers_ProfilerScreenshots_h

#include <functional>

#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"

#include "mozilla/gfx/Point.h"

class nsIThread;

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}

namespace layers {

/**
 * Can be used to submit screenshots from the compositor to the profiler.
 * Screenshots have a fixed bounding size. The user of this class will usually
 * scale down the window contents first, ideally on the GPU, then read back the
 * small scaled down image into main memory, and then call SubmitScreenshot to
 * pass the data to the profiler.
 * This class encodes each screenshot to a JPEG data URL, on a separate thread.
 * This class manages that thread and recycles memory buffers.
 */
class ProfilerScreenshots final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ProfilerScreenshots)

 public:
  ProfilerScreenshots();

  /**
   * Returns whether the profiler is currently active and is running with the
   * "screenshots" feature enabled.
   */
  static bool IsEnabled();

  /**
   * Returns a fixed size that all screenshots should be resized to fit into.
   */
  static gfx::IntSize ScreenshotSize() { return gfx::IntSize(350, 350); }

  /**
   * The main functionality provided by this class.
   * This method will synchronously invoke the supplied aPopulateSurface
   * callback function with a DataSourceSurface in which the callback should
   * store the pixel data. This surface may be larger than aScaledSize, but
   * only the data in the rectangle (0, 0, aScaledSize.width,
   * aScaledSize.height) will be read.
   * @param aWindowIdentifier A pointer-sized integer that can be used to match
   *   up multiple screenshots from the same window.
   * @param aOriginalSize The unscaled size of the snapshotted window.
   * @param aScaledSize The scaled size, aScaled <= ScreenshotSize(), which the
   *   snapshot has been resized to.
   * @param aTimeStamp The time at which the snapshot was taken. In
   *   asynchronous readback implementations, this is the time at which the
   *   readback / copy command was put into the command stream to the GPU, not
   *   the time at which the readback data was mapped into main memory.
   * @param aPopulateSurface A callback that the caller needs to implement,
   *   which needs to copy the screenshot pixel data into the surface that's
   *   supplied to the callback. Called zero or one times, synchronously.
   */
  void SubmitScreenshot(
      uintptr_t aWindowIdentifier, const gfx::IntSize& aOriginalSize,
      const gfx::IntSize& aScaledSize, const TimeStamp& aTimeStamp,
      const std::function<bool(gfx::DataSourceSurface*)>& aPopulateSurface);

 private:
  ~ProfilerScreenshots();

  /**
   * Recycle a surface from mAvailableSurfaces or create a new one if all
   * surfaces are currently in use, up to some maximum limit.
   * Returns null if the limit is reached.
   * Can be called on any thread.
   */
  already_AddRefed<DataSourceSurface> TakeNextSurface();

  /**
   * Return aSurface back into the mAvailableSurfaces pool. Can be called on
   * any thread.
   */
  void ReturnSurface(DataSourceSurface* aSurface);

  // The thread on which encoding happens.
  nsCOMPtr<nsIThread> mThread;
  // An array of surfaces ready to be recycled. Can be accessed from multiple
  // threads, protected by mMutex.
  nsTArray<RefPtr<DataSourceSurface>> mAvailableSurfaces;
  // Protects mAvailableSurfaces.
  Mutex mMutex;
  // The total number of surfaces created. If encoding is fast enough to happen
  // entirely in the time between two calls to SubmitScreenshot, this should
  // never exceed 1.
  uint32_t mLiveSurfaceCount;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_ProfilerScreenshots_h
