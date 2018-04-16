/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

  static WebRenderBridgeParent* CreateDestroyed(const wr::PipelineId& aPipelineId);

  wr::PipelineId PipelineId() { return mPipelineId; }
  already_AddRefed<wr::WebRenderAPI> GetWebRenderAPI() { return do_AddRef(mApi); }
  wr::Epoch WrEpoch() const { return mWrEpoch; }
  AsyncImagePipelineManager* AsyncImageManager() { return mAsyncImageManager; }
  CompositorVsyncScheduler* CompositorScheduler() { return mCompositorScheduler.get(); }

  mozilla::ipc::IPCResult RecvNewCompositable(const CompositableHandle& aHandle,
                                              const TextureInfo& aInfo) override;
  mozilla::ipc::IPCResult RecvReleaseCompositable(const CompositableHandle& aHandle) override;

  mozilla::ipc::IPCResult RecvCreate(const gfx::IntSize& aSize) override;
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvShutdownSync() override;
  mozilla::ipc::IPCResult RecvDeleteCompositorAnimations(InfallibleTArray<uint64_t>&& aIds) override;
  mozilla::ipc::IPCResult RecvUpdateResources(nsTArray<OpUpdateResource>&& aUpdates,
                                              nsTArray<RefCountedShmem>&& aSmallShmems,
                                              nsTArray<ipc::Shmem>&& aLargeShmems) override;
  mozilla::ipc::IPCResult RecvSetDisplayList(const gfx::IntSize& aSize,
                                             InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                             InfallibleTArray<OpDestroy>&& aToDestroy,
                                             const uint64_t& aFwdTransactionId,
                                             const uint64_t& aTransactionId,
                                             const wr::LayoutSize& aContentSize,
                                             ipc::ByteBuf&& dl,
                                             const wr::BuiltDisplayListDescriptor& dlDesc,
                                             const WebRenderScrollData& aScrollData,
                                             nsTArray<OpUpdateResource>&& aResourceUpdates,
                                             nsTArray<RefCountedShmem>&& aSmallShmems,
                                             nsTArray<ipc::Shmem>&& aLargeShmems,
                                             const wr::IdNamespace& aIdNamespace,
                                             const TimeStamp& aTxnStartTime,
                                             const TimeStamp& aFwdTime) override;
  mozilla::ipc::IPCResult RecvEmptyTransaction(const FocusTarget& aFocusTarget,
                                               InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                               InfallibleTArray<OpDestroy>&& aToDestroy,
                                               const uint64_t& aFwdTransactionId,
                                               const uint64_t& aTransactionId,
                                               const wr::IdNamespace& aIdNamespace,
                                               const TimeStamp& aTxnStartTime,
                                               const TimeStamp& aFwdTime) override;
  mozilla::ipc::IPCResult RecvSetFocusTarget(const FocusTarget& aFocusTarget) override;
  mozilla::ipc::IPCResult RecvParentCommands(nsTArray<WebRenderParentCommand>&& commands) override;
  mozilla::ipc::IPCResult RecvGetSnapshot(PTextureParent* aTexture) override;

  mozilla::ipc::IPCResult RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch) override;

  mozilla::ipc::IPCResult RecvClearCachedResources() override;
  mozilla::ipc::IPCResult RecvScheduleComposite() override;
  mozilla::ipc::IPCResult RecvCapture() override;

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

  void HoldPendingTransactionId(const wr::Epoch& aWrEpoch,
                                uint64_t aTransactionId,
                                const TimeStamp& aTxnStartTime,
                                const TimeStamp& aFwdTime);
  uint64_t LastPendingTransactionId();
  uint64_t FlushPendingTransactionIds();
  uint64_t FlushTransactionIdsForEpoch(const wr::Epoch& aEpoch, const TimeStamp& aEndTime);

  TextureFactoryIdentifier GetTextureFactoryIdentifier();

  void ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotificationInfo>* aNotifications);

  wr::IdNamespace GetIdNamespace()
  {
    return mIdNamespace;
  }

  void FlushRendering();
  void FlushRenderingAsync();

  /**
   * Schedule generating WebRender frame definitely at next composite timing.
   *
   * WebRenderBridgeParent uses composite timing to check if there is an update
   * to AsyncImagePipelines. If there is no update, WebRenderBridgeParent skips
   * to generate frame. If we need to generate new frame at next composite timing,
   * call this method.
   *
   * Call CompositorVsyncScheduler::ScheduleComposition() directly, if we just
   * want to trigger AsyncImagePipelines update checks.
   */
  void ScheduleGenerateFrame();

  void UpdateWebRender(CompositorVsyncScheduler* aScheduler,
                       wr::WebRenderAPI* aApi,
                       AsyncImagePipelineManager* aImageMgr,
                       CompositorAnimationStorage* aAnimStorage);

private:
  explicit WebRenderBridgeParent(const wr::PipelineId& aPipelineId);
  virtual ~WebRenderBridgeParent();

  void UpdateAPZFocusState(const FocusTarget& aFocus);
  void UpdateAPZScrollData(const wr::Epoch& aEpoch, WebRenderScrollData&& aData);

  bool UpdateResources(const nsTArray<OpUpdateResource>& aResourceUpdates,
                       const nsTArray<RefCountedShmem>& aSmallShmems,
                       const nsTArray<ipc::Shmem>& aLargeShmems,
                       wr::TransactionBuilder& aUpdates);
  bool AddExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                        wr::TransactionBuilder& aResources);

  void AddPipelineIdForCompositable(const wr::PipelineId& aPipelineIds,
                                    const CompositableHandle& aHandle,
                                    const bool& aAsync);
  void RemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId);

  void AddExternalImageIdForCompositable(const ExternalImageId& aImageId,
                                         const CompositableHandle& aHandle);
  void RemoveExternalImageId(const ExternalImageId& aImageId);

  LayersId GetLayersId() const;
  void ProcessWebRenderParentCommands(const InfallibleTArray<WebRenderParentCommand>& aCommands);

  void ClearResources();
  uint64_t GetChildLayerObserverEpoch() const { return mChildLayerObserverEpoch; }
  bool ShouldParentObserveEpoch();
  mozilla::ipc::IPCResult HandleShutdown();

  void AdvanceAnimations();
  void SampleAnimations(nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                        nsTArray<wr::WrTransformProperty>& aTransformArray);

  CompositorBridgeParent* GetRootCompositorBridgeParent() const;

  // Tell APZ what the subsequent sampling's timestamp should be.
  void SetAPZSampleTime();

  wr::Epoch GetNextWrEpoch();

private:
  struct PendingTransactionId {
    PendingTransactionId(const wr::Epoch& aEpoch, uint64_t aId, const TimeStamp& aTxnStartTime, const TimeStamp& aFwdTime)
      : mEpoch(aEpoch)
      , mId(aId)
      , mTxnStartTime(aTxnStartTime)
      , mFwdTime(aFwdTime)
    {}
    wr::Epoch mEpoch;
    uint64_t mId;
    TimeStamp mTxnStartTime;
    TimeStamp mFwdTime;
  };

  CompositorBridgeParentBase* MOZ_NON_OWNING_REF mCompositorBridge;
  wr::PipelineId mPipelineId;
  RefPtr<widget::CompositorWidget> mWidget;
  RefPtr<wr::WebRenderAPI> mApi;
  RefPtr<AsyncImagePipelineManager> mAsyncImageManager;
  RefPtr<CompositorVsyncScheduler> mCompositorScheduler;
  RefPtr<CompositorAnimationStorage> mAnimStorage;
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
  wr::Epoch mWrEpoch;
  wr::IdNamespace mIdNamespace;

  bool mPaused;
  bool mDestroyed;
  bool mForceRendering;
  bool mReceivedDisplayList;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeParent_h
