/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RendererScreenshotGrabber.h"

#include "RendererOGL.h"

using mozilla::layers::ProfilerScreenshots;

namespace mozilla {
namespace wr {

RendererScreenshotGrabber::RendererScreenshotGrabber() {
  mMaxScreenshotSize = ProfilerScreenshots::ScreenshotSize();
}

void RendererScreenshotGrabber::MaybeGrabScreenshot(
    RendererOGL* aRendererOGL, const gfx::IntSize& aWindowSize) {
  bool isEnabled =
      ProfilerScreenshots::IsEnabled() && aRendererOGL->EnsureAsyncScreenshot();

  if (isEnabled) {
    if (!mProfilerScreenshots) {
      mProfilerScreenshots = new ProfilerScreenshots();
    }

    GrabScreenshot(aRendererOGL->GetRenderer(), aWindowSize);
  } else if (mProfilerScreenshots) {
    Destroy(aRendererOGL->GetRenderer());
  }
}

void RendererScreenshotGrabber::MaybeProcessQueue(RendererOGL* aRendererOGL) {
  bool isEnabled =
      ProfilerScreenshots::IsEnabled() && aRendererOGL->EnsureAsyncScreenshot();

  if (isEnabled) {
    if (!mProfilerScreenshots) {
      mProfilerScreenshots = new ProfilerScreenshots();
    }

    ProcessQueue(aRendererOGL->GetRenderer());
  } else if (mProfilerScreenshots) {
    Destroy(aRendererOGL->GetRenderer());
  }
}

void RendererScreenshotGrabber::Destroy(Renderer* aRenderer) {
  mQueue.Clear();
  mCurrentFrameQueueItem = Nothing();
  mProfilerScreenshots = nullptr;

  wr_renderer_release_profiler_structures(aRenderer);
}

void RendererScreenshotGrabber::GrabScreenshot(
    Renderer* aRenderer, const gfx::IntSize& aWindowSize) {
  gfx::IntSize screenshotSize;

  AsyncScreenshotHandle handle = wr_renderer_get_screenshot_async(
      aRenderer, 0, 0, aWindowSize.width, aWindowSize.height,
      mMaxScreenshotSize.width, mMaxScreenshotSize.height, ImageFormat::BGRA8,
      &screenshotSize.width, &screenshotSize.height);

  mCurrentFrameQueueItem = Some(QueueItem{
      TimeStamp::Now(),
      handle,
      screenshotSize,
      aWindowSize,
      reinterpret_cast<uintptr_t>(this),
  });
}

void RendererScreenshotGrabber::ProcessQueue(Renderer* aRenderer) {
  for (const auto& item : mQueue) {
    mProfilerScreenshots->SubmitScreenshot(
        item.mWindowIdentifier, item.mWindowSize, item.mScreenshotSize,
        item.mTimeStamp,
        [&item, aRenderer](gfx::DataSourceSurface* aTargetSurface) {
          gfx::DataSourceSurface::ScopedMap map(aTargetSurface,
                                                gfx::DataSourceSurface::WRITE);
          int32_t destStride = map.GetStride();

          bool success = wr_renderer_map_and_recycle_screenshot(
              aRenderer, item.mHandle, map.GetData(),
              destStride * aTargetSurface->GetSize().height, destStride);

          return success;
        });
  }

  mQueue.Clear();

  if (mCurrentFrameQueueItem) {
    mQueue.AppendElement(*mCurrentFrameQueueItem);
    mCurrentFrameQueueItem = Nothing();
  }
}

}  // namespace wr
}  // namespace mozilla
