/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenshotGrabber.h"

#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/ProfilerScreenshots.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/gfx/Point.h"
#include "nsTArray.h"

namespace mozilla {

using namespace gfx;

namespace layers {
namespace profiler_screenshots {

/**
 * The actual implementation of screenshot grabbing.
 * The ScreenshotGrabberImpl object is destroyed if the profiler is
 * disabled and MaybeGrabScreenshot notices it.
 */
class ScreenshotGrabberImpl final {
 public:
  explicit ScreenshotGrabberImpl(const IntSize& aBufferSize);
  ~ScreenshotGrabberImpl();

  void GrabScreenshot(Window& aWindow);
  void ProcessQueue();

 private:
  struct QueueItem final {
    mozilla::TimeStamp mTimeStamp;
    RefPtr<AsyncReadbackBuffer> mScreenshotBuffer;
    IntSize mScreenshotSize;
    IntSize mWindowSize;
    uintptr_t mWindowIdentifier;
  };

  RefPtr<RenderSource> ScaleDownWindowRenderSourceToSize(
      Window& aWindow, const IntSize& aDestSize,
      RenderSource* aWindowRenderSource, size_t aLevel);

  already_AddRefed<AsyncReadbackBuffer> TakeNextBuffer(Window& aWindow);
  void ReturnBuffer(AsyncReadbackBuffer* aBuffer);

  nsTArray<RefPtr<DownscaleTarget>> mCachedLevels;
  nsTArray<RefPtr<AsyncReadbackBuffer>> mAvailableBuffers;
  Maybe<QueueItem> mCurrentFrameQueueItem;
  nsTArray<QueueItem> mQueue;
  RefPtr<ProfilerScreenshots> mProfilerScreenshots;
  const IntSize mBufferSize;
};

}  // namespace profiler_screenshots

ScreenshotGrabber::ScreenshotGrabber() = default;

ScreenshotGrabber::~ScreenshotGrabber() = default;

void ScreenshotGrabber::MaybeGrabScreenshot(
    profiler_screenshots::Window& aWindow) {
  if (ProfilerScreenshots::IsEnabled()) {
    if (!mImpl) {
      mImpl = MakeUnique<profiler_screenshots::ScreenshotGrabberImpl>(
          ProfilerScreenshots::ScreenshotSize());
    }
    mImpl->GrabScreenshot(aWindow);
  } else if (mImpl) {
    Destroy();
  }
}

void ScreenshotGrabber::MaybeProcessQueue() {
  if (ProfilerScreenshots::IsEnabled()) {
    if (!mImpl) {
      mImpl = MakeUnique<profiler_screenshots::ScreenshotGrabberImpl>(
          ProfilerScreenshots::ScreenshotSize());
    }
    mImpl->ProcessQueue();
  } else if (mImpl) {
    Destroy();
  }
}

void ScreenshotGrabber::NotifyEmptyFrame() {
#ifdef MOZ_GECKO_PROFILER
  PROFILER_ADD_MARKER("NoCompositorScreenshot because nothing changed",
                      GRAPHICS);
#endif
}

void ScreenshotGrabber::Destroy() { mImpl = nullptr; }

namespace profiler_screenshots {

ScreenshotGrabberImpl::ScreenshotGrabberImpl(const IntSize& aBufferSize)
    : mBufferSize(aBufferSize) {}

ScreenshotGrabberImpl::~ScreenshotGrabberImpl() {
  // Any queue items in mQueue or mCurrentFrameQueueItem will be lost.
  // That's ok: Either the profiler has stopped and we don't care about these
  // screenshots, or the window is closing and we don't really need the last
  // few frames from the window.
}

// Scale down aWindowRenderSource into a RenderSource of size
// mBufferSize * (1 << aLevel) and return that RenderSource.
// Don't scale down by more than a factor of 2 with a single scaling operation,
// because it'll look bad. If higher scales are needed, use another
// intermediate target by calling this function recursively with aLevel + 1.
RefPtr<RenderSource> ScreenshotGrabberImpl::ScaleDownWindowRenderSourceToSize(
    Window& aWindow, const IntSize& aDestSize,
    RenderSource* aWindowRenderSource, size_t aLevel) {
  if (aLevel == mCachedLevels.Length()) {
    mCachedLevels.AppendElement(
        aWindow.CreateDownscaleTarget(mBufferSize * (1 << aLevel)));
  }
  MOZ_RELEASE_ASSERT(aLevel < mCachedLevels.Length());

  RefPtr<RenderSource> renderSource = aWindowRenderSource;
  IntSize sourceSize = aWindowRenderSource->Size();
  if (sourceSize.width > aDestSize.width * 2) {
    sourceSize = aDestSize * 2;
    renderSource = ScaleDownWindowRenderSourceToSize(
        aWindow, sourceSize, aWindowRenderSource, aLevel + 1);
  }

  if (renderSource) {
    if (mCachedLevels[aLevel]->DownscaleFrom(
            renderSource, IntRect({}, sourceSize), IntRect({}, aDestSize))) {
      return mCachedLevels[aLevel]->AsRenderSource();
    }
  }
  return nullptr;
}

void ScreenshotGrabberImpl::GrabScreenshot(Window& aWindow) {
  RefPtr<RenderSource> windowRenderSource = aWindow.GetWindowContents();

  if (!windowRenderSource) {
    PROFILER_ADD_MARKER(
        "NoCompositorScreenshot because of unsupported compositor "
        "configuration",
        GRAPHICS);
    return;
  }

  Size windowSize(windowRenderSource->Size());
  float scale = std::min(mBufferSize.width / windowSize.width,
                         mBufferSize.height / windowSize.height);
  IntSize scaledSize = IntSize::Round(windowSize * scale);
  RefPtr<RenderSource> scaledTarget = ScaleDownWindowRenderSourceToSize(
      aWindow, scaledSize, windowRenderSource, 0);

  if (!scaledTarget) {
    PROFILER_ADD_MARKER(
        "NoCompositorScreenshot because ScaleDownWindowRenderSourceToSize "
        "failed",
        GRAPHICS);
    return;
  }

  RefPtr<AsyncReadbackBuffer> buffer = TakeNextBuffer(aWindow);
  if (!buffer) {
    PROFILER_ADD_MARKER(
        "NoCompositorScreenshot because AsyncReadbackBuffer creation failed",
        GRAPHICS);
    return;
  }

  buffer->CopyFrom(scaledTarget);

  // This QueueItem will be added to the queue at the end of the next call to
  // ProcessQueue(). This ensures that the buffer isn't mapped into main memory
  // until the next frame. If we did it in this frame, we'd block on the GPU.
  mCurrentFrameQueueItem =
      Some(QueueItem{TimeStamp::Now(), std::move(buffer), scaledSize,
                     windowRenderSource->Size(),
                     reinterpret_cast<uintptr_t>(static_cast<void*>(this))});
}

already_AddRefed<AsyncReadbackBuffer> ScreenshotGrabberImpl::TakeNextBuffer(
    Window& aWindow) {
  if (!mAvailableBuffers.IsEmpty()) {
    RefPtr<AsyncReadbackBuffer> buffer = mAvailableBuffers[0];
    mAvailableBuffers.RemoveElementAt(0);
    return buffer.forget();
  }
  return aWindow.CreateAsyncReadbackBuffer(mBufferSize);
}

void ScreenshotGrabberImpl::ReturnBuffer(AsyncReadbackBuffer* aBuffer) {
  mAvailableBuffers.AppendElement(aBuffer);
}

void ScreenshotGrabberImpl::ProcessQueue() {
  if (!mQueue.IsEmpty()) {
    if (!mProfilerScreenshots) {
      mProfilerScreenshots = new ProfilerScreenshots();
    }
    for (const auto& item : mQueue) {
      mProfilerScreenshots->SubmitScreenshot(
          item.mWindowIdentifier, item.mWindowSize, item.mScreenshotSize,
          item.mTimeStamp, [&item](DataSourceSurface* aTargetSurface) {
            return item.mScreenshotBuffer->MapAndCopyInto(aTargetSurface,
                                                          item.mScreenshotSize);
          });
      ReturnBuffer(item.mScreenshotBuffer);
    }
  }
  mQueue.Clear();

  if (mCurrentFrameQueueItem) {
    mQueue.AppendElement(std::move(*mCurrentFrameQueueItem));
    mCurrentFrameQueueItem = Nothing();
  }
}

}  // namespace profiler_screenshots
}  // namespace layers
}  // namespace mozilla
