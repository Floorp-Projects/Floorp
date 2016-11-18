/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderBridgeParent_h
#define mozilla_layers_WebRenderBridgeParent_h

#include "GLContextProvider.h"
#include "mozilla/layers/PWebRenderBridgeParent.h"
#include "mozilla/layers/WebRenderTypes.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace widget {
class CompositorWidget;
}

namespace layers {

class Compositor;

class WebRenderBridgeParent final : public PWebRenderBridgeParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderBridgeParent)

public:
  WebRenderBridgeParent(const uint64_t& aPipelineId,
                        const nsString* aResourcePath,
                        widget::CompositorWidget* aWidget,
                        gl::GLContext* aGlContext,
                        wrwindowstate* aWrWindowState,
                        layers::Compositor* aCompositor);
  uint64_t PipelineId() { return mPipelineId; }
  gl::GLContext* GLContext() { return mGLContext.get(); }
  wrwindowstate* WindowState() { return mWRWindowState; }
  layers::Compositor* Compositor() { return mCompositor.get(); }

  mozilla::ipc::IPCResult RecvCreate(const uint32_t& aWidth,
                                     const uint32_t& aHeight) override;
  mozilla::ipc::IPCResult RecvDestroy() override;
  mozilla::ipc::IPCResult RecvAddImage(const uint32_t& aWidth,
                                       const uint32_t& aHeight,
                                       const uint32_t& aStride,
                                       const WRImageFormat& aFormat,
                                       const ByteBuffer& aBuffer,
                                       WRImageKey* aOutImageKey) override;
  mozilla::ipc::IPCResult RecvUpdateImage(const WRImageKey& aImageKey,
                                          const uint32_t& aWidth,
                                          const uint32_t& aHeight,
                                          const WRImageFormat& aFormat,
                                          const ByteBuffer& aBuffer) override;
  mozilla::ipc::IPCResult RecvDeleteImage(const WRImageKey& aImageKey) override;
  mozilla::ipc::IPCResult RecvDPBegin(const uint32_t& aWidth,
                                      const uint32_t& aHeight,
                                      bool* aOutSuccess) override;
  mozilla::ipc::IPCResult RecvDPEnd(InfallibleTArray<WebRenderCommand>&& commands) override;

  void ActorDestroy(ActorDestroyReason aWhy) override {}

protected:
  virtual ~WebRenderBridgeParent();
  void DeleteOldImages();

private:
  uint64_t mPipelineId;
  RefPtr<widget::CompositorWidget> mWidget;
  wrstate* mWRState;
  RefPtr<gl::GLContext> mGLContext;
  wrwindowstate* mWRWindowState;
  RefPtr<layers::Compositor> mCompositor;
  std::vector<WRImageKey> mKeysToDelete;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeParent_h
