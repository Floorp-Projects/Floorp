/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ScreenshotGrabber_h
#define mozilla_layers_ScreenshotGrabber_h

#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace layers {

namespace profiler_screenshots {
class Window;
class RenderSource;
class DownscaleTarget;
class AsyncReadbackBuffer;

class ScreenshotGrabberImpl;
}  // namespace profiler_screenshots

/**
 * Used by various renderers / layer managers to grab snapshots from the window
 * and submit them to the Gecko profiler.
 * Doesn't do any work if the profiler is not running or the "screenshots"
 * feature is not enabled.
 * Screenshots are scaled down to fit within a fixed size, and read back to
 * main memory using async readback. Scaling is done in multiple scale-by-0.5x
 * steps using DownscaleTarget::CopyFrom, and readback is done using
 * AsyncReadbackBuffers.
 */
class ScreenshotGrabber final {
 public:
  ScreenshotGrabber();
  ~ScreenshotGrabber();

  // Scale the contents of aWindow's current render target into an
  // appropriately sized DownscaleTarget and read its contents into an
  // AsyncReadbackBuffer. The AsyncReadbackBuffer is not mapped into main
  // memory until the second call to MaybeProcessQueue() after this call to
  // MaybeGrabScreenshot().
  void MaybeGrabScreenshot(profiler_screenshots::Window& aWindow);

  // Map the contents of any outstanding AsyncReadbackBuffers from previous
  // composites into main memory and submit each screenshot to the profiler.
  void MaybeProcessQueue();

  // Insert a special profiler marker for a composite that didn't do any actual
  // compositing, so that the profiler knows why no screenshot was taken for
  // this frame.
  void NotifyEmptyFrame();

  // Destroy all Window-related resources that this class is holding on to.
  void Destroy();

 private:
  // non-null while ProfilerScreenshots::IsEnabled() returns true
  UniquePtr<profiler_screenshots::ScreenshotGrabberImpl> mImpl;
};

// Interface definitions.

namespace profiler_screenshots {

class Window {
 public:
  virtual already_AddRefed<RenderSource> GetWindowContents() = 0;
  virtual already_AddRefed<DownscaleTarget> CreateDownscaleTarget(
      const gfx::IntSize& aSize) = 0;
  virtual already_AddRefed<AsyncReadbackBuffer> CreateAsyncReadbackBuffer(
      const gfx::IntSize& aSize) = 0;

 protected:
  virtual ~Window() {}
};

class RenderSource {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RenderSource)

  const auto& Size() const { return mSize; }

 protected:
  explicit RenderSource(const gfx::IntSize& aSize) : mSize(aSize) {}
  virtual ~RenderSource() {}

  const gfx::IntSize mSize;
};

class DownscaleTarget {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DownscaleTarget)

  virtual already_AddRefed<RenderSource> AsRenderSource() = 0;

  const auto& Size() const { return mSize; }
  virtual bool DownscaleFrom(RenderSource* aSource,
                             const gfx::IntRect& aSourceRect,
                             const gfx::IntRect& aDestRect) = 0;

 protected:
  explicit DownscaleTarget(const gfx::IntSize& aSize) : mSize(aSize) {}
  virtual ~DownscaleTarget() {}

  const gfx::IntSize mSize;
};

class AsyncReadbackBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(
      mozilla::layers::profiler_screenshots::AsyncReadbackBuffer)

  const auto& Size() const { return mSize; }
  virtual void CopyFrom(RenderSource* aSource) = 0;
  virtual bool MapAndCopyInto(gfx::DataSourceSurface* aSurface,
                              const gfx::IntSize& aReadSize) = 0;

 protected:
  explicit AsyncReadbackBuffer(const gfx::IntSize& aSize) : mSize(aSize) {}
  virtual ~AsyncReadbackBuffer() {}

  const gfx::IntSize mSize;
};

}  // namespace profiler_screenshots

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_ScreenshotGrabber_h
