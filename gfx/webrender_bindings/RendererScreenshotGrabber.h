/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_RendererScreenshotGrabber_h
#define mozilla_layers_RendererScreenshotGrabber_h

#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/ProfilerScreenshots.h"
#include "nsTArray.h"

namespace mozilla {
namespace wr {

/**
 * Used by |RendererOGL| to grab screenshots from WebRender and submit them to
 * the Gecko profiler.
 *
 * If the profiler is not running or the screenshots feature is disabled, no
 * work will be done.
 */
class RendererScreenshotGrabber final {
 public:
  RendererScreenshotGrabber();

  /**
   * Grab a screenshot from WebRender if we are profiling and screenshots are
   * enabled.
   *
   * The captured screenshot will not be mapped until the second call to
   * |MaybeProcessQueue| after this call to |MaybeGrabScreenshot|.
   */
  void MaybeGrabScreenshot(Renderer* aRenderer,
                           const gfx::IntSize& aWindowSize);

  /**
   * Process the screenshots pending in the queue if we are profiling and
   * screenshots are enabled.
   */
  void MaybeProcessQueue(Renderer* aRenderer);

 private:
  /**
   * Drop all our allocated memory when we are no longer profiling.
   *
   * This will also instruct WebRender to drop all its Gecko profiler
   * associated memory.
   */
  void Destroy(Renderer* aRenderer);

  /**
   * Actually grab a screenshot from WebRender.
   */
  void GrabScreenshot(Renderer* aRenderer, const gfx::IntSize& aWindowSize);

  /**
   * Process the screenshots pending in the queue.
   */
  void ProcessQueue(Renderer* aRenderer);

  struct QueueItem {
    mozilla::TimeStamp mTimeStamp;
    AsyncScreenshotHandle mHandle;
    gfx::IntSize mScreenshotSize;
    gfx::IntSize mWindowSize;
    uintptr_t mWindowIdentifier;
  };

  /**
   * The maximum size for screenshots, as dictated by
   * |ProfilerScrenshots::ScreenshotSize|.
   */
  gfx::IntSize mMaxScreenshotSize;

  /**
   * The queue of screenshots waiting to be processed and submitted.
   */
  nsTArray<QueueItem> mQueue;

  /**
   * The queue item for the current frame. This will be inserted into the queue
   * after a call to |MaybeProcessQueue| so it will be not be processed until
   * the next frame.
   */
  Maybe<QueueItem> mCurrentFrameQueueItem;

  /**
   * Our handle to the profiler screenshots object.
   */
  RefPtr<mozilla::layers::ProfilerScreenshots> mProfilerScreenshots;
};

}  // namespace wr
}  // namespace mozilla

#endif  // mozilla_layers_RendererScreenshotGrabber_h
