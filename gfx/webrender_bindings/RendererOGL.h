/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_RENDEREROGL_H
#define MOZILLA_LAYERS_RENDEREROGL_H

#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/RendererScreenshotGrabber.h"

namespace mozilla {

namespace gfx {
class DrawTarget;
}

namespace gl {
class GLContext;
}

namespace layers {
class CompositorBridgeParent;
class SyncObjectHost;
}  // namespace layers

namespace widget {
class CompositorWidget;
}

namespace wr {

class RenderCompositor;
class RenderTextureHost;

/// Owns the WebRender renderer and GL context.
///
/// There is one renderer per window, all owned by the render thread.
/// This class is a similar abstraction to CompositorOGL except that it is used
/// on the render thread instead of the compositor thread.
class RendererOGL {
  friend wr::WrExternalImage LockExternalImage(void* aObj,
                                               wr::WrExternalImageId aId,
                                               uint8_t aChannelIndex,
                                               wr::ImageRendering);
  friend void UnlockExternalImage(void* aObj, wr::WrExternalImageId aId,
                                  uint8_t aChannelIndex);

 public:
  wr::WrExternalImageHandler GetExternalImageHandler();

  /// This can be called on the render thread only.
  void Update();

  /// This can be called on the render thread only.
  bool UpdateAndRender(const Maybe<gfx::IntSize>& aReadbackSize,
                       const Maybe<wr::ImageFormat>& aReadbackFormat,
                       const Maybe<Range<uint8_t>>& aReadbackBuffer,
                       bool aHadSlowFrame, RendererStats* aOutStats);

  /// This can be called on the render thread only.
  void WaitForGPU();

  /// This can be called on the render thread only.
  void SetProfilerEnabled(bool aEnabled);

  /// This can be called on the render thread only.
  void SetFrameStartTime(const TimeStamp& aTime);

  /// This can be called on the render thread only.
  ~RendererOGL();

  /// This can be called on the render thread only.
  RendererOGL(RefPtr<RenderThread>&& aThread,
              UniquePtr<RenderCompositor> aCompositor, wr::WindowId aWindowId,
              wr::Renderer* aRenderer, layers::CompositorBridgeParent* aBridge);

  /// This can be called on the render thread only.
  void Pause();

  /// This can be called on the render thread only.
  bool Resume();

  /// This can be called on the render thread only.
  void CheckGraphicsResetStatus();

  layers::SyncObjectHost* GetSyncObject() const;

  layers::CompositorBridgeParent* GetCompositorBridge() { return mBridge; }

  RefPtr<WebRenderPipelineInfo> FlushPipelineInfo();

  RenderTextureHost* GetRenderTexture(wr::WrExternalImageId aExternalImageId);

  void AccumulateMemoryReport(MemoryReport* aReport);

  wr::Renderer* GetRenderer() { return mRenderer; }

  gl::GLContext* gl() const;

 protected:
  void NotifyWebRenderError(WebRenderError aError);

  RefPtr<RenderThread> mThread;
  UniquePtr<RenderCompositor> mCompositor;
  wr::Renderer* mRenderer;
  layers::CompositorBridgeParent* mBridge;
  wr::WindowId mWindowId;
  TimeStamp mFrameStartTime;

  RendererScreenshotGrabber mScreenshotGrabber;
};

}  // namespace wr
}  // namespace mozilla

#endif
