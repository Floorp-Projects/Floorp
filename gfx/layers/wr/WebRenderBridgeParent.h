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
class WebRenderImageHost;

class WebRenderBridgeParent final : public PWebRenderBridgeParent
                                  , public CompositorVsyncSchedulerOwner
                                  , public CompositableParentManager
{
public:
  WebRenderBridgeParent(CompositorBridgeParentBase* aCompositorBridge,
                        const wr::PipelineId& aPipelineId,
                        widget::CompositorWidget* aWidget,
                        CompositorVsyncScheduler* aScheduler,
                        RefPtr<wr::WebRenderAPI>&& aApi,
                        RefPtr<WebRenderCompositableHolder>&& aHolder);

  wr::PipelineId PipelineId() { return mPipelineId; }
  wr::WebRenderAPI* GetWebRenderAPI() { return mApi; }
  WebRenderCompositableHolder* CompositableHolder() { return mCompositableHolder; }
  CompositorVsyncScheduler* CompositorScheduler() { return mCompositorScheduler.get(); }

  mozilla::ipc::IPCResult RecvNewCompositable(const CompositableHandle& aHandle,
                                              const TextureInfo& aInfo) override;
  mozilla::ipc::IPCResult RecvReleaseCompositable(const CompositableHandle& aHandle) override;

  mozilla::ipc::IPCResult RecvInitReadLocks(ReadLockArray&& aReadLocks) override;

  mozilla::ipc::IPCResult RecvCreate(const gfx::IntSize& aSize) override;
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvAddImage(const wr::ImageKey& aImageKey,
                                       const gfx::IntSize& aSize,
                                       const uint32_t& aStride,
                                       const gfx::SurfaceFormat& aFormat,
                                       const ByteBuffer& aBuffer) override;
  mozilla::ipc::IPCResult RecvAddBlobImage(const wr::ImageKey& aImageKey,
                                           const gfx::IntSize& aSize,
                                           const uint32_t& aStride,
                                           const gfx::SurfaceFormat& aFormat,
                                           const ByteBuffer& aBuffer) override;
  mozilla::ipc::IPCResult RecvUpdateImage(const wr::ImageKey& aImageKey,
                                          const gfx::IntSize& aSize,
                                          const gfx::SurfaceFormat& aFormat,
                                          const ByteBuffer& aBuffer) override;
  mozilla::ipc::IPCResult RecvDeleteImage(const wr::ImageKey& a1) override;
  mozilla::ipc::IPCResult RecvAddRawFont(const wr::FontKey& aFontKey,
                                         const ByteBuffer& aBuffer,
                                         const uint32_t& aFontIndex) override;
  mozilla::ipc::IPCResult RecvDeleteFont(const wr::FontKey& aFontKey) override;
  mozilla::ipc::IPCResult RecvDPBegin(const gfx::IntSize& aSize) override;
  mozilla::ipc::IPCResult RecvDPEnd(const gfx::IntSize& aSize,
                                    InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                    InfallibleTArray<OpDestroy>&& aToDestroy,
                                    const uint64_t& aFwdTransactionId,
                                    const uint64_t& aTransactionId,
                                    const ByteBuffer& dl,
                                    const WrBuiltDisplayListDescriptor& dlDesc,
                                    const ByteBuffer& aux,
                                    const WrAuxiliaryListsDescriptor& auxDesc,
                                    const WebRenderScrollData& aScrollData) override;
  mozilla::ipc::IPCResult RecvDPSyncEnd(const gfx::IntSize& aSize,
                                        InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                        InfallibleTArray<OpDestroy>&& aToDestroy,
                                        const uint64_t& aFwdTransactionId,
                                        const uint64_t& aTransactionId,
                                        const ByteBuffer& dl,
                                        const WrBuiltDisplayListDescriptor& dlDesc,
                                        const ByteBuffer& aux,
                                        const WrAuxiliaryListsDescriptor& auxDesc,
                                        const WebRenderScrollData& aScrollData) override;
  mozilla::ipc::IPCResult RecvDPGetSnapshot(PTextureParent* aTexture) override;

  mozilla::ipc::IPCResult RecvAddExternalImageId(const ExternalImageId& aImageId,
                                                 const CompositableHandle& aHandle) override;
  mozilla::ipc::IPCResult RecvAddExternalImageIdForCompositable(const ExternalImageId& aImageId,
                                                                const CompositableHandle& aHandle) override;
  mozilla::ipc::IPCResult RecvRemoveExternalImageId(const ExternalImageId& aImageId) override;
  mozilla::ipc::IPCResult RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch) override;

  mozilla::ipc::IPCResult RecvClearCachedResources() override;
  mozilla::ipc::IPCResult RecvForceComposite() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void SetWebRenderProfilerEnabled(bool aEnabled);

  void Pause();
  bool Resume();

  void Destroy();

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

  void HoldPendingTransactionId(uint32_t aWrEpoch, uint64_t aTransactionId);
  uint64_t LastPendingTransactionId();
  uint64_t FlushPendingTransactionIds();
  uint64_t FlushTransactionIdsForEpoch(const wr::Epoch& aEpoch);

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

  uint32_t GetIdNameSpace()
  {
    return mIdNameSpace;
  }

  void UpdateAPZ();
  const WebRenderScrollData& GetScrollData() const;

private:
  virtual ~WebRenderBridgeParent();

  void DeleteOldImages();
  void ProcessWebRenderCommands(const gfx::IntSize &aSize, InfallibleTArray<WebRenderParentCommand>& commands, const wr::Epoch& aEpoch,
                                    const ByteBuffer& dl,
                                    const WrBuiltDisplayListDescriptor& dlDesc,
                                    const ByteBuffer& aux,
                                    const WrAuxiliaryListsDescriptor& auxDesc);
  void ScheduleComposition();
  void ClearResources();
  uint64_t GetChildLayerObserverEpoch() const { return mChildLayerObserverEpoch; }
  bool ShouldParentObserveEpoch();
  void HandleDPEnd(const gfx::IntSize& aSize,
                   InfallibleTArray<WebRenderParentCommand>&& aCommands,
                   InfallibleTArray<OpDestroy>&& aToDestroy,
                   const uint64_t& aFwdTransactionId,
                   const uint64_t& aTransactionId,
                   const ByteBuffer& dl,
                   const WrBuiltDisplayListDescriptor& dlDesc,
                   const ByteBuffer& aux,
                   const WrAuxiliaryListsDescriptor& auxDesc,
                   const WebRenderScrollData& aScrollData);

  void SampleAnimations(nsTArray<WrOpacityProperty>& aOpacityArray,
                        nsTArray<WrTransformProperty>& aTransformArray);

private:
  struct PendingTransactionId {
    PendingTransactionId(wr::Epoch aEpoch, uint64_t aId)
      : mEpoch(aEpoch)
      , mId(aId)
    {}
    wr::Epoch mEpoch;
    uint64_t mId;
  };

  CompositorBridgeParentBase* MOZ_NON_OWNING_REF mCompositorBridge;
  wr::PipelineId mPipelineId;
  RefPtr<widget::CompositorWidget> mWidget;
  RefPtr<wr::WebRenderAPI> mApi;
  RefPtr<WebRenderCompositableHolder> mCompositableHolder;
  RefPtr<CompositorVsyncScheduler> mCompositorScheduler;
  std::vector<wr::ImageKey> mKeysToDelete;
  // XXX How to handle active keys of non-ExternalImages?
  nsDataHashtable<nsUint64HashKey, wr::ImageKey> mActiveKeys;
  nsDataHashtable<nsUint64HashKey, RefPtr<WebRenderImageHost>> mExternalImageIds;
  nsTArray<ImageCompositeNotificationInfo> mImageCompositeNotifications;

  // These fields keep track of the latest layer observer epoch values in the child and the
  // parent. mChildLayerObserverEpoch is the latest epoch value received from the child.
  // mParentLayerObserverEpoch is the latest epoch value that we have told TabParent about
  // (via ObserveLayerUpdate).
  uint64_t mChildLayerObserverEpoch;
  uint64_t mParentLayerObserverEpoch;

  std::queue<PendingTransactionId> mPendingTransactionIds;
  uint32_t mWrEpoch;
  uint32_t mIdNameSpace;

  bool mPaused;
  bool mDestroyed;

  // Can only be accessed on the compositor thread.
  WebRenderScrollData mScrollData;

  static uint32_t sIdNameSpace;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeParent_h
