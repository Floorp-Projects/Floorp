/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderBridgeParent_h
#define mozilla_layers_WebRenderBridgeParent_h

#include "CompositableHost.h"           // for CompositableHost, ImageCompositeNotificationInfo
#include "GLContextProvider.h"
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/CompositorVsyncSchedulerOwner.h"
#include "mozilla/layers/PWebRenderBridgeParent.h"
#include "mozilla/Maybe.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace widget {
class CompositorWidget;
}

namespace wr {
class WebRenderAPI;
}

namespace layers {

class Compositor;
class CompositorBridgeParentBase;
class CompositorVsyncScheduler;
class WebRenderCompositableHolder;

class WebRenderBridgeParent final : public PWebRenderBridgeParent
                                  , public CompositorVsyncSchedulerOwner
                                  , public CompositableParentManager
{
public:
  WebRenderBridgeParent(CompositorBridgeParentBase* aCompositorBridge,
                        const wr::PipelineId& aPipelineId,
                        widget::CompositorWidget* aWidget,
                        RefPtr<wr::WebRenderAPI>&& aApi,
                        RefPtr<WebRenderCompositableHolder>&& aHolder);

  wr::PipelineId PipelineId() { return mPipelineId; }
  wr::WebRenderAPI* GetWebRenderAPI() { return mApi; }
  WebRenderCompositableHolder* CompositableHolder() { return mCompositableHolder; }
  CompositorVsyncScheduler* CompositorScheduler() { return mCompositorScheduler.get(); }

  mozilla::ipc::IPCResult RecvNewCompositable(const CompositableHandle& aHandle,
                                              const TextureInfo& aInfo) override;
  mozilla::ipc::IPCResult RecvReleaseCompositable(const CompositableHandle& aHandle) override;

  mozilla::ipc::IPCResult RecvCreate(const gfx::IntSize& aSize) override;
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvAddImage(const gfx::IntSize& aSize,
                                       const uint32_t& aStride,
                                       const gfx::SurfaceFormat& aFormat,
                                       const ByteBuffer& aBuffer,
                                       wr::ImageKey* aOutImageKey) override;
  mozilla::ipc::IPCResult RecvUpdateImage(const wr::ImageKey& aImageKey,
                                          const gfx::IntSize& aSize,
                                          const gfx::SurfaceFormat& aFormat,
                                          const ByteBuffer& aBuffer) override;
  mozilla::ipc::IPCResult RecvDeleteImage(const wr::ImageKey& a1) override;
  mozilla::ipc::IPCResult RecvDPBegin(const gfx::IntSize& aSize,
                                      bool* aOutSuccess) override;
  mozilla::ipc::IPCResult RecvDPEnd(InfallibleTArray<WebRenderCommand>&& aCommands,
                                    InfallibleTArray<OpDestroy>&& aToDestroy,
                                    const uint64_t& aFwdTransactionId,
                                    const uint64_t& aTransactionId) override;
  mozilla::ipc::IPCResult RecvDPSyncEnd(InfallibleTArray<WebRenderCommand>&& aCommands,
                                        InfallibleTArray<OpDestroy>&& aToDestroy,
                                        const uint64_t& aFwdTransactionId,
                                        const uint64_t& aTransactionId) override;
  mozilla::ipc::IPCResult RecvDPGetSnapshot(PTextureParent* aTexture) override;

  mozilla::ipc::IPCResult RecvAddExternalImageId(const uint64_t& aImageId,
                                                 const CompositableHandle& aHandle) override;
  mozilla::ipc::IPCResult RecvAddExternalImageIdForCompositable(const uint64_t& aImageId,
                                                                const CompositableHandle& aHandle) override;
  mozilla::ipc::IPCResult RecvRemoveExternalImageId(const uint64_t& aImageId) override;
  mozilla::ipc::IPCResult RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch) override;

  mozilla::ipc::IPCResult RecvClearCachedResources() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void SetWebRenderProfilerEnabled(bool aEnabled);

  void Destroy();

  const uint64_t& GetPendingTransactionId() { return mPendingTransactionId; }
  void SetPendingTransactionId(uint64_t aId) { mPendingTransactionId = aId; }

  // CompositorVsyncSchedulerOwner
  bool IsPendingComposite() override { return false; }
  void FinishPendingComposite() override { }
  void CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr) override;

  // CompositableParentManager
  bool IsSameProcess() const override;
  base::ProcessId GetChildProcessId() override;
  void NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId) override;
  void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;
  void SendPendingAsyncMessages() override;
  void SetAboutToSendAsyncMessages() override;

  void DidComposite(uint64_t aTransactionId, TimeStamp aStart, TimeStamp aEnd);

  TextureFactoryIdentifier GetTextureFactoryIdentifier();

  void AppendImageCompositeNotification(const ImageCompositeNotificationInfo& aNotification)
  {
    MOZ_ASSERT(mWidget);
    mImageCompositeNotifications.AppendElement(aNotification);
  }

  void ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotificationInfo>* aNotifications)
  {
    MOZ_ASSERT(mWidget);
    aNotifications->AppendElements(Move(mImageCompositeNotifications));
  }

private:
  virtual ~WebRenderBridgeParent();

  void DeleteOldImages();
  void ProcessWebrenderCommands(InfallibleTArray<WebRenderCommand>& commands, const wr::Epoch& aEpoch);
  void ScheduleComposition();
  void ClearResources();
  uint64_t GetChildLayerObserverEpoch() const { return mChildLayerObserverEpoch; }
  bool ShouldParentObserveEpoch();
  void HandleDPEnd(InfallibleTArray<WebRenderCommand>&& aCommands,
                   InfallibleTArray<OpDestroy>&& aToDestroy,
                   const uint64_t& aFwdTransactionId,
                   const uint64_t& aTransactionId);

private:
  CompositorBridgeParentBase* MOZ_NON_OWNING_REF mCompositorBridge;
  wr::PipelineId mPipelineId;
  RefPtr<widget::CompositorWidget> mWidget;
  Maybe<wr::DisplayListBuilder> mBuilder;
  RefPtr<wr::WebRenderAPI> mApi;
  RefPtr<WebRenderCompositableHolder> mCompositableHolder;
  RefPtr<CompositorVsyncScheduler> mCompositorScheduler;
  std::vector<wr::ImageKey> mKeysToDelete;
  nsDataHashtable<nsUint64HashKey, RefPtr<CompositableHost>> mExternalImageIds;
  nsTArray<ImageCompositeNotificationInfo> mImageCompositeNotifications;

  // These fields keep track of the latest layer observer epoch values in the child and the
  // parent. mChildLayerObserverEpoch is the latest epoch value received from the child.
  // mParentLayerObserverEpoch is the latest epoch value that we have told TabParent about
  // (via ObserveLayerUpdate).
  uint64_t mChildLayerObserverEpoch;
  uint64_t mParentLayerObserverEpoch;

  uint64_t mPendingTransactionId;

  bool mDestroyed;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeParent_h
