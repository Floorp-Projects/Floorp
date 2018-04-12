/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorScreenshotGrabber_h
#define mozilla_layers_CompositorScreenshotGrabber_h

#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace layers {

class CompositorScreenshotGrabberImpl;

/**
 * Used by LayerManagerComposite to grab snapshots from the compositor and
 * submit them to the Gecko profiler.
 * Doesn't do any work if the profiler is not running or the "screenshots"
 * feature is not enabled.
 * Screenshots are scaled down to fit within a fixed size, and read back to
 * main memory using async readback. Scaling is done in multiple scale-by-0.5x
 * steps using CompositingRenderTargets and Compositor::BlitFromRenderTarget,
 * and readback is done using AsyncReadbackBuffers.
 */
class CompositorScreenshotGrabber final
{
public:
  CompositorScreenshotGrabber();
  ~CompositorScreenshotGrabber();

  // Scale the contents of aCompositor's current render target into an
  // approapriately sized CompositingRenderTarget and read its contents into an
  // AsyncReadbackBuffer. The AsyncReadbackBuffer is not mapped into main
  // memory until the second call to MaybeProcessQueue() after this call to
  // MaybeGrabScreenshot().
  void MaybeGrabScreenshot(Compositor* aCompositor);

  // Map the contents of any outstanding AsyncReadbackBuffers from previous
  // composites into main memory and submit each screenshot to the profiler.
  void MaybeProcessQueue();

  // Insert a special profiler marker for a composite that didn't do any actual
  // compositing, so that the profiler knows why no screenshot was taken for
  // this frame.
  void NotifyEmptyFrame();

  // Destroy all Compositor-related resources that this class is holding on to.
  void Destroy();

private:
  // non-null while ProfilerScreenshots::IsEnabled() returns true
  UniquePtr<CompositorScreenshotGrabberImpl> mImpl;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorScreenshotGrabber_h
