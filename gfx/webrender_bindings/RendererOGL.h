/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_RENDEREROGL_H
#define MOZILLA_LAYERS_RENDEREROGL_H

#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/webrender/webrender_ffi.h"

namespace mozilla {

namespace gfx {
class DrawTarget;
}

namespace gl {
class GLContext;
}

namespace layers {
class CompositorBridgeParentBase;
}

namespace widget {
class CompositorWidget;
}

namespace wr {

/// Owns the WebRender renderer and GL context.
///
/// There is one renderer per window, all owned by the render thread.
/// This class is a similar abstraction to CompositorOGL except that it is used
/// on the render thread instead of the compositor thread.
class RendererOGL
{
  friend WrExternalImage LockExternalImage(void* aObj, WrExternalImageId aId);
  friend void UnlockExternalImage(void* aObj, WrExternalImageId aId);
  friend void ReleaseExternalImage(void* aObj, WrExternalImageId aId);

public:
  WrExternalImageHandler GetExternalImageHandler();

  /// This can be called on the render thread only.
  void Update();

  /// This can be called on the render thread only.
  bool Render();

  /// This can be called on the render thread only.
  bool RenderToTarget(gfx::DrawTarget& aTarget);

  /// This can be called on the render thread only.
  void SetProfilerEnabled(bool aEnabled);

  /// This can be called on the render thread only.
  ~RendererOGL();

  /// This can be called on the render thread only.
  RendererOGL(RefPtr<RenderThread>&& aThread,
              RefPtr<gl::GLContext>&& aGL,
              RefPtr<widget::CompositorWidget>&&,
              wr::WindowId aWindowId,
              WrRenderer* aWrRenderer,
              layers::CompositorBridgeParentBase* aBridge);

  layers::CompositorBridgeParentBase* GetCompositorBridge() { return mBridge; }

  WrRenderedEpochs* FlushRenderedEpochs();

protected:

  RefPtr<RenderThread> mThread;
  RefPtr<gl::GLContext> mGL;
  RefPtr<widget::CompositorWidget> mWidget;
  WrRenderer* mWrRenderer;
  layers::CompositorBridgeParentBase* mBridge;
  wr::WindowId mWindowId;
};

} // namespace wr
} // namespace mozilla

#endif
