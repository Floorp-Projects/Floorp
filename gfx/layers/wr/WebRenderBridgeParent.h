/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderBridgeParent_h
#define mozilla_layers_WebRenderBridgeParent_h

#include "GLContextProvider.h"
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/CompositorVsyncSchedulerOwner.h"
#include "mozilla/layers/PWebRenderBridgeParent.h"
#include "mozilla/layers/WebRenderTypes.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace widget {
class CompositorWidget;
}

namespace layers {

class Compositor;
class CompositorBridgeParentBase;
class CompositorVsyncScheduler;

class WebRenderBridgeParent final : public PWebRenderBridgeParent
                                  , public CompositorVsyncSchedulerOwner
                                  , public CompositableParentManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderBridgeParent)

public:
  WebRenderBridgeParent(CompositorBridgeParentBase* aCompositorBridge,
                        const uint64_t& aPipelineId,
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
  mozilla::ipc::IPCResult RecvShutdown() override;
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
  mozilla::ipc::IPCResult RecvDPSyncEnd(InfallibleTArray<WebRenderCommand>&& commands) override;
  mozilla::ipc::IPCResult RecvDPGetSnapshot(PTextureParent* aTexture,
                                            const gfx::IntRect& aRect) override;

  mozilla::ipc::IPCResult RecvAddExternalImageId(const uint64_t& aImageId,
                                                 const uint64_t& aAsyncContainerId) override;
  mozilla::ipc::IPCResult RecvRemoveExternalImageId(const uint64_t& aImageId) override;

  mozilla::ipc::IPCResult RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch) override;

  mozilla::ipc::IPCResult RecvClearCachedResources() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void Destroy();

  // CompositorVsyncSchedulerOwner
  bool IsPendingComposite() override { return false; }
  void FinishPendingComposite() override { }
  void CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr) override;

private:
  virtual ~WebRenderBridgeParent();

  virtual PCompositableParent* AllocPCompositableParent(const TextureInfo& aInfo) override;
  bool DeallocPCompositableParent(PCompositableParent* aActor) override;

  // CompositableParentManager
  bool IsSameProcess() const override;
  base::ProcessId GetChildProcessId() override;
  void NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId) override;
  void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;

  void DeleteOldImages();
  void ProcessWebrenderCommands(InfallibleTArray<WebRenderCommand>& commands);
  void ScheduleComposition();
  void ClearResources();
  uint64_t GetChildLayerObserverEpoch() const { return mChildLayerObserverEpoch; }
  bool ShouldParentObserveEpoch();

private:
  CompositorBridgeParentBase* mCompositorBridge;
  uint64_t mPipelineId;
  RefPtr<widget::CompositorWidget> mWidget;
  wrstate* mWRState;
  RefPtr<gl::GLContext> mGLContext;
  wrwindowstate* mWRWindowState;
  RefPtr<layers::Compositor> mCompositor;
  RefPtr<CompositorVsyncScheduler> mCompositorScheduler;
  std::vector<WRImageKey> mKeysToDelete;
  nsDataHashtable<nsUint64HashKey, uint64_t> mExternalImageIds;

  // These fields keep track of the latest layer observer epoch values in the child and the
  // parent. mChildLayerObserverEpoch is the latest epoch value received from the child.
  // mParentLayerObserverEpoch is the latest epoch value that we have told TabParent about
  // (via ObserveLayerUpdate).
  uint64_t mChildLayerObserverEpoch;
  uint64_t mParentLayerObserverEpoch;

  bool mDestroyed;
  uint32_t mWREpoch;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeParent_h
