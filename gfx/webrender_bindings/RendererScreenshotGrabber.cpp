#include "RendererScreenshotGrabber.h"

using mozilla::layers::ProfilerScreenshots;

namespace mozilla {
namespace wr {

RendererScreenshotGrabber::RendererScreenshotGrabber() {
  mMaxScreenshotSize = ProfilerScreenshots::ScreenshotSize();
}

void RendererScreenshotGrabber::MaybeGrabScreenshot(
    Renderer* aRenderer, const gfx::IntSize& aWindowSize) {
  if (ProfilerScreenshots::IsEnabled()) {
    if (!mProfilerScreenshots) {
      mProfilerScreenshots = new ProfilerScreenshots();
    }

    GrabScreenshot(aRenderer, aWindowSize);
  } else if (mProfilerScreenshots) {
    Destroy(aRenderer);
  }
}

void RendererScreenshotGrabber::MaybeProcessQueue(Renderer* aRenderer) {
  if (ProfilerScreenshots::IsEnabled()) {
    if (!mProfilerScreenshots) {
      mProfilerScreenshots = new ProfilerScreenshots();
    }

    ProcessQueue(aRenderer);
  } else if (mProfilerScreenshots) {
    Destroy(aRenderer);
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
        item.mTimeStamp, [&item, aRenderer](DataSourceSurface* aTargetSurface) {
          DataSourceSurface::ScopedMap map(aTargetSurface,
                                           DataSourceSurface::WRITE);
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
