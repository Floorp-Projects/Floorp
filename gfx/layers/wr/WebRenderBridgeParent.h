/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderBridgeParent_h
#define mozilla_layers_WebRenderBridgeParent_h

#include <unordered_set>

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
class CompositorAnimationStorage;
class CompositorBridgeParentBase;
class CompositorVsyncScheduler;
class AsyncImagePipelineManager;
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
                        RefPtr<AsyncImagePipelineManager>&& aImageMgr,
                        RefPtr<CompositorAnimationStorage>&& aAnimStorage);

  static WebRenderBridgeParent* CreateDestroyed();

  wr::PipelineId PipelineId() { return mPipelineId; }
  wr::WebRenderAPI* GetWebRenderAPI() { return mApi; }
  wr::Epoch WrEpoch() { return wr::NewEpoch(mWrEpoch); }
  AsyncImagePipelineManager* AsyncImageManager() { return mAsyncImageManager; }
  CompositorVsyncScheduler* CompositorScheduler() { return mCompositorScheduler.get(); }

  mozilla::ipc::IPCResult RecvNewCompositable(const CompositableHandle& aHandle,
                                              const TextureInfo& aInfo) override;
  mozilla::ipc::IPCResult RecvReleaseCompositable(const CompositableHandle& aHandle) override;

  mozilla::ipc::IPCResult RecvInitReadLocks(ReadLockArray&& aReadLocks) override;

  mozilla::ipc::IPCResult RecvCreate(const gfx::IntSize& aSize) override;
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvShutdownSync() override;
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
  mozilla::ipc::IPCResult RecvDeleteCompositorAnimations(InfallibleTArray<uint64_t>&& aIds) override;
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
                                    const wr::LayoutSize& aContentSize,
                                    const wr::ByteBuffer& dl,
                                    const wr::BuiltDisplayListDescriptor& dlDesc,
                                    const WebRenderScrollData& aScrollData,
                                    const uint32_t& aIdNameSpace,
                                    const TimeStamp& aFwdTime) override;
  mozilla::ipc::IPCResult RecvDPSyncEnd(const gfx::IntSize& aSize,
                                        InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                        InfallibleTArray<OpDestroy>&& aToDestroy,
                                        const uint64_t& aFwdTransactionId,
                                        const uint64_t& aTransactionId,
                                        const wr::LayoutSize& aContentSize,
                                        const wr::ByteBuffer& dl,
                                        const wr::BuiltDisplayListDescriptor& dlDesc,
                                        const WebRenderScrollData& aScrollData,
                                        const uint32_t& aIdNameSpace,
                                        const TimeStamp& aFwdTime) override;
  mozilla::ipc::IPCResult RecvParentCommands(nsTArray<WebRenderParentCommand>&& commands) override;
  mozilla::ipc::IPCResult RecvDPGetSnapshot(PTextureParent* aTexture) override;

  mozilla::ipc::IPCResult RecvAddPipelineIdForAsyncCompositable(const wr::PipelineId& aPipelineIds,
                                                                const CompositableHandle& aHandle) override;
  mozilla::ipc::IPCResult RecvRemovePipelineIdForAsyncCompositable(const wr::PipelineId& aPipelineId) override;

  mozilla::ipc::IPCResult RecvAddExternalImageIdForCompositable(const ExternalImageId& aImageId,
                                                                const CompositableHandle& aHandle) override;
  mozilla::ipc::IPCResult RecvRemoveExternalImageId(const ExternalImageId& aImageId) override;
  mozilla::ipc::IPCResult RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch) override;

  mozilla::ipc::IPCResult RecvClearCachedResources() override;
  mozilla::ipc::IPCResult RecvForceComposite() override;

  mozilla::ipc::IPCResult RecvSetConfirmedTargetAPZC(const uint64_t& aBlockId,
                                                     nsTArray<ScrollableLayerGuid>&& aTargets) override;

  mozilla::ipc::IPCResult RecvSetTestSampleTime(const TimeStamp& aTime) override;
  mozilla::ipc::IPCResult RecvLeaveTestMode() override;
  mozilla::ipc::IPCResult RecvGetAnimationOpacity(const uint64_t& aCompositorAnimationsId,
                                                  float* aOpacity,
                                                  bool* aHasAnimationOpacity) override;
  mozilla::ipc::IPCResult RecvGetAnimationTransform(const uint64_t& aCompositorAnimationsId,
                                                    MaybeTransform* aTransform) override;
  mozilla::ipc::IPCResult RecvSetAsyncScrollOffset(const FrameMetrics::ViewID& aScrollId,
                                                   const float& aX,
                                                   const float& aY) override;
  mozilla::ipc::IPCResult RecvSetAsyncZoom(const FrameMetrics::ViewID& aScrollId,
                                           const float& aZoom) override;
  mozilla::ipc::IPCResult RecvFlushApzRepaints() override;
  mozilla::ipc::IPCResult RecvGetAPZTestData(APZTestData* data) override;

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

  void HoldPendingTransactionId(uint32_t aWrEpoch, uint64_t aTransactionId, const TimeStamp& aFwdTime);
  uint64_t LastPendingTransactionId();
  uint64_t FlushPendingTransactionIds();
  uint64_t FlushTransactionIdsForEpoch(const wr::Epoch& aEpoch, const TimeStamp& aEndTime);

  TextureFactoryIdentifier GetTextureFactoryIdentifier();

  void ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotificationInfo>* aNotifications);

  uint32_t GetIdNameSpace()
  {
    return mIdNameSpace;
  }

  void UpdateAPZ();
  const WebRenderScrollData& GetScrollData() const;

  static uint32_t AllocIdNameSpace() {
    return ++sIdNameSpace;
  }

  void FlushRendering(bool aIsSync);

  void ScheduleComposition();

  void UpdateWebRender(CompositorVsyncScheduler* aScheduler,
                       wr::WebRenderAPI* aApi,
                       AsyncImagePipelineManager* aImageMgr,
                       CompositorAnimationStorage* aAnimStorage);

private:
  WebRenderBridgeParent();
  virtual ~WebRenderBridgeParent();

  uint64_t GetLayersId() const;
  void DeleteOldImages();
  void ProcessWebRenderParentCommands(InfallibleTArray<WebRenderParentCommand>& aCommands);
  void ProcessWebRenderCommands(const gfx::IntSize &aSize,
                                InfallibleTArray<WebRenderParentCommand>& commands,
                                const wr::Epoch& aEpoch,
                                const wr::LayoutSize& aContentSize,
                                const wr::ByteBuffer& dl,
                                const wr::BuiltDisplayListDescriptor& dlDesc,
                                const uint32_t& aIdNameSpace);
  void ClearResources();
  uint64_t GetChildLayerObserverEpoch() const { return mChildLayerObserverEpoch; }
  bool ShouldParentObserveEpoch();
  void HandleDPEnd(const gfx::IntSize& aSize,
                   InfallibleTArray<WebRenderParentCommand>&& aCommands,
                   InfallibleTArray<OpDestroy>&& aToDestroy,
                   const uint64_t& aFwdTransactionId,
                   const uint64_t& aTransactionId,
                   const wr::LayoutSize& aContentSize,
                   const wr::ByteBuffer& dl,
                   const wr::BuiltDisplayListDescriptor& dlDesc,
                   const WebRenderScrollData& aScrollData,
                   const uint32_t& aIdNameSpace,
                   const TimeStamp& aFwdTime);
  mozilla::ipc::IPCResult HandleShutdown();

  void AdvanceAnimations();
  void SampleAnimations(nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                        nsTArray<wr::WrTransformProperty>& aTransformArray);

  CompositorBridgeParent* GetRootCompositorBridgeParent() const;

  // Have APZ push the async scroll state to WR. Returns true if an APZ
  // animation is in effect and we need to schedule another composition.
  // If scrollbars need their transforms updated, the provided aTransformArray
  // is populated with the property update details.
  bool PushAPZStateToWR(nsTArray<wr::WrTransformProperty>& aTransformArray);

  // Helper method to get an APZC reference from a scroll id. Uses the layers
  // id of this bridge, and may return null if the APZC wasn't found.
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const FrameMetrics::ViewID& aId);

  uint32_t GetNextWrEpoch();

private:
  struct PendingTransactionId {
    PendingTransactionId(wr::Epoch aEpoch, uint64_t aId, const TimeStamp& aFwdTime)
      : mEpoch(aEpoch)
      , mId(aId)
      , mFwdTime(aFwdTime)
    {}
    wr::Epoch mEpoch;
    uint64_t mId;
    TimeStamp mFwdTime;
  };

  CompositorBridgeParentBase* MOZ_NON_OWNING_REF mCompositorBridge;
  wr::PipelineId mPipelineId;
  RefPtr<widget::CompositorWidget> mWidget;
  RefPtr<wr::WebRenderAPI> mApi;
  RefPtr<AsyncImagePipelineManager> mAsyncImageManager;
  RefPtr<CompositorVsyncScheduler> mCompositorScheduler;
  RefPtr<CompositorAnimationStorage> mAnimStorage;
  std::vector<wr::ImageKey> mKeysToDelete;
  // mActiveImageKeys and mFontKeys are used to avoid leaking animations when
  // WebRenderBridgeParent is destroyed abnormally and Tab move between different windows.
  std::unordered_set<uint64_t> mActiveImageKeys;
  std::unordered_set<uint64_t> mFontKeys;
  // mActiveAnimations is used to avoid leaking animations when WebRenderBridgeParent is
  // destroyed abnormally and Tab move between different windows.
  std::unordered_set<uint64_t> mActiveAnimations;
  nsDataHashtable<nsUint64HashKey, RefPtr<WebRenderImageHost>> mAsyncCompositables;
  nsDataHashtable<nsUint64HashKey, RefPtr<WebRenderImageHost>> mExternalImageIds;

  TimeStamp mPreviousFrameTimeStamp;
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
  bool mForceRendering;

  // Can only be accessed on the compositor thread.
  WebRenderScrollData mScrollData;

  static uint32_t sIdNameSpace;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeParent_h
