/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorScreenshotGrabber.h"

#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/layers/ProfilerScreenshots.h"
#include "mozilla/gfx/Point.h"
#include "nsTArray.h"

namespace mozilla {

using namespace gfx;

namespace layers {

/**
 * The actual implementation of screenshot grabbing.
 * The CompositorScreenshotGrabberImpl object is destroyed if the profiler is
 * disabled and MaybeGrabScreenshot notices it.
 */
class CompositorScreenshotGrabberImpl final
{
public:
  explicit CompositorScreenshotGrabberImpl(const IntSize& aBufferSize);
  ~CompositorScreenshotGrabberImpl();

  void GrabScreenshot(Compositor* aCompositor);
  void ProcessQueue();

private:
  struct QueueItem final
  {
    mozilla::TimeStamp mTimeStamp;
    RefPtr<AsyncReadbackBuffer> mScreenshotBuffer;
    gfx::IntSize mScreenshotSize;
    gfx::IntSize mWindowSize;
    uintptr_t mWindowIdentifier;
  };

  RefPtr<CompositingRenderTarget>
  ScaleDownWindowTargetToSize(Compositor* aCompositor,
                              const gfx::IntSize& aDestSize,
                              CompositingRenderTarget* aWindowTarget,
                              size_t aLevel);

  already_AddRefed<AsyncReadbackBuffer> TakeNextBuffer(Compositor* aCompositor);
  void ReturnBuffer(AsyncReadbackBuffer* aBuffer);

  nsTArray<RefPtr<CompositingRenderTarget>> mTargets;
  nsTArray<RefPtr<AsyncReadbackBuffer>> mAvailableBuffers;
  Maybe<QueueItem> mCurrentFrameQueueItem;
  nsTArray<QueueItem> mQueue;
  UniquePtr<ProfilerScreenshots> mProfilerScreenshots;
  const IntSize mBufferSize;
};

CompositorScreenshotGrabber::CompositorScreenshotGrabber()
{
}

CompositorScreenshotGrabber::~CompositorScreenshotGrabber()
{
}

void
CompositorScreenshotGrabber::MaybeGrabScreenshot(Compositor* aCompositor)
{
  if (ProfilerScreenshots::IsEnabled()) {
    if (!mImpl) {
      mImpl = MakeUnique<CompositorScreenshotGrabberImpl>(ProfilerScreenshots::ScreenshotSize());
    }
    mImpl->GrabScreenshot(aCompositor);
  } else if (mImpl) {
    Destroy();
  }
}

void
CompositorScreenshotGrabber::MaybeProcessQueue()
{
  if (ProfilerScreenshots::IsEnabled()) {
    if (!mImpl) {
      mImpl = MakeUnique<CompositorScreenshotGrabberImpl>(ProfilerScreenshots::ScreenshotSize());
    }
    mImpl->ProcessQueue();
  } else if (mImpl) {
    Destroy();
  }
}

void
CompositorScreenshotGrabber::NotifyEmptyFrame()
{
#ifdef MOZ_GECKO_PROFILER
  profiler_add_marker("NoCompositorScreenshot because nothing changed");
#endif
}

void
CompositorScreenshotGrabber::Destroy()
{
  mImpl = nullptr;
}

CompositorScreenshotGrabberImpl::CompositorScreenshotGrabberImpl(const IntSize& aBufferSize)
  : mBufferSize(aBufferSize)
{
}

CompositorScreenshotGrabberImpl::~CompositorScreenshotGrabberImpl()
{
  // Any queue items in mQueue or mCurrentFrameQueueItem will be lost.
  // That's ok: Either the profiler has stopped and we don't care about these
  // screenshots, or the window is closing and we don't really need the last
  // few frames from the window.
}

// Scale down aWindowTarget into a CompositingRenderTarget of size
// mBufferSize * (1 << aLevel) and return that CompositingRenderTarget.
// Don't scale down by more than a factor of 2 with a single scaling operation,
// because it'll look bad. If higher scales are needed, use another
// intermediate target by calling this function recursively with aLevel + 1.
RefPtr<CompositingRenderTarget>
CompositorScreenshotGrabberImpl::ScaleDownWindowTargetToSize(Compositor* aCompositor,
                                                             const IntSize& aDestSize,
                                                             CompositingRenderTarget* aWindowTarget,
                                                             size_t aLevel)
{
  if (aLevel == mTargets.Length()) {
    mTargets.AppendElement(aCompositor->CreateRenderTarget(
      IntRect(IntPoint(), mBufferSize * (1 << aLevel)), INIT_MODE_NONE));
  }
  MOZ_RELEASE_ASSERT(aLevel < mTargets.Length());

  RefPtr<CompositingRenderTarget> sourceTarget = aWindowTarget;
  IntSize sourceSize = aWindowTarget->GetSize();
  if (aWindowTarget->GetSize().width > aDestSize.width * 2) {
    sourceSize = aDestSize * 2;
    sourceTarget = ScaleDownWindowTargetToSize(aCompositor, sourceSize,
                                               aWindowTarget, aLevel + 1);
  }

  if (sourceTarget) {
    aCompositor->SetRenderTarget(mTargets[aLevel]);
    if (aCompositor->BlitRenderTarget(sourceTarget, sourceSize, aDestSize)) {
      return mTargets[aLevel];
    }
  }
  return nullptr;
}

void
CompositorScreenshotGrabberImpl::GrabScreenshot(Compositor* aCompositor)
{
  RefPtr<CompositingRenderTarget> previousTarget =
    aCompositor->GetCurrentRenderTarget();

  RefPtr<CompositingRenderTarget> windowTarget =
    aCompositor->GetWindowRenderTarget();

  if (!windowTarget) {
    PROFILER_ADD_MARKER("NoCompositorScreenshot because of unsupported compositor configuration");
    return;
  }

  Size windowSize(windowTarget->GetSize());
  float scale = std::min(mBufferSize.width / windowSize.width,
                         mBufferSize.height / windowSize.height);
  IntSize scaledSize = IntSize::Round(windowSize * scale);
  RefPtr<CompositingRenderTarget> scaledTarget =
    ScaleDownWindowTargetToSize(aCompositor, scaledSize, windowTarget, 0);

  // Restore the old render target.
  aCompositor->SetRenderTarget(previousTarget);

  if (!scaledTarget) {
    PROFILER_ADD_MARKER("NoCompositorScreenshot because ScaleDownWindowTargetToSize failed");
    return;
  }

  RefPtr<AsyncReadbackBuffer> buffer = TakeNextBuffer(aCompositor);
  if (!buffer) {
    PROFILER_ADD_MARKER("NoCompositorScreenshot because AsyncReadbackBuffer creation failed");
    return;
  }

  aCompositor->ReadbackRenderTarget(scaledTarget, buffer);

  // This QueueItem will be added to the queue at the end of the next call to
  // ProcessQueue(). This ensures that the buffer isn't mapped into main memory
  // until the next frame. If we did it in this frame, we'd block on the GPU.
  mCurrentFrameQueueItem = Some(QueueItem{
    TimeStamp::Now(), buffer.forget(), scaledSize, windowTarget->GetSize(),
    reinterpret_cast<uintptr_t>(static_cast<void*>(this))
  });
}

already_AddRefed<AsyncReadbackBuffer>
CompositorScreenshotGrabberImpl::TakeNextBuffer(Compositor* aCompositor)
{
  if (!mAvailableBuffers.IsEmpty()) {
    RefPtr<AsyncReadbackBuffer> buffer = mAvailableBuffers[0];
    mAvailableBuffers.RemoveElementAt(0);
    return buffer.forget();
  }
  return aCompositor->CreateAsyncReadbackBuffer(mBufferSize);
}

void
CompositorScreenshotGrabberImpl::ReturnBuffer(AsyncReadbackBuffer* aBuffer)
{
  mAvailableBuffers.AppendElement(aBuffer);
}

void
CompositorScreenshotGrabberImpl::ProcessQueue()
{
  if (!mQueue.IsEmpty()) {
    if (!mProfilerScreenshots) {
      mProfilerScreenshots = MakeUnique<ProfilerScreenshots>();
    }
    for (const auto& item : mQueue) {
      mProfilerScreenshots->SubmitScreenshot(
        item.mWindowIdentifier, item.mWindowSize,
        item.mScreenshotSize, item.mTimeStamp,
        [&item](DataSourceSurface* aTargetSurface) {
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

} // namespace layers
} // namespace mozilla
