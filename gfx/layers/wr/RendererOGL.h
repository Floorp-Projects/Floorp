/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_RENDEREROGL_H
#define MOZILLA_LAYERS_RENDEREROGL_H

#include "mozilla/layers/RenderThread.h"
#include "mozilla/gfx/webrender.h"

namespace mozilla {

namespace gfx {
class DrawTarget;
}

namespace gl {
class GLContext;
}

namespace widget {
class CompositorWidget;
}

namespace layers {

class WebRenderBridgeParent;

class RendererOGL
{
public:

  static RendererOGL* Create(already_AddRefed<RenderThread> aThread,
                             already_AddRefed<widget::CompositorWidget> aWidget,
                             WrRenderer* aWrRenderer,
                             uint64_t aPipelineId,
                             WebRenderBridgeParent* aBridge);

  void Update();

  bool Render(uint64_t aTransactionId);

  bool RenderToTarget(gfx::DrawTarget& aTarget);

  ~RendererOGL();

protected:
  RendererOGL(already_AddRefed<RenderThread> aThread,
              already_AddRefed<gl::GLContext> aGL,
              already_AddRefed<widget::CompositorWidget>,
              uint64_t aPipelineId,
              WrRenderer* aWrRenderer,
              WebRenderBridgeParent* aBridge);

  RefPtr<RenderThread> mThread;
  RefPtr<gl::GLContext> mGL;
  RefPtr<widget::CompositorWidget> mWidget;
  WrRenderer* mWrRenderer;
  WebRenderBridgeParent* mBridge;
  uint64_t mPipelineId;
};

}
}


#endif
