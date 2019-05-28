/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderBridgeParent_h
#define mozilla_layers_WebRenderBridgeParent_h

#include <unordered_map>
#include <unordered_set>

#include "CompositableHost.h"  // for CompositableHost, ImageCompositeNotificationInfo
#include "GLContextProvider.h"
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/CompositorVsyncSchedulerOwner.h"
#include "mozilla/layers/PWebRenderBridgeParent.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/RenderThread.h"
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

class WebRenderBridgeParent final
    : public PWebRenderBridgeParent,
      public CompositorVsyncSchedulerOwner,
      public CompositableParentManager,
      public layers::FrameRecorder,
      public SupportsWeakPtr<WebRenderBridgeParent> {
 public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(WebRenderBridgeParent)
  WebRenderBridgeParent(CompositorBridgeParentBase* aCompositorBridge,
                        const wr::PipelineId& aPipelineId,
                        widget::CompositorWidget* aWidget,
                        CompositorVsyncScheduler* aScheduler,
                        nsTArray<RefPtr<wr::WebRenderAPI>>&& aApis,
                        RefPtr<AsyncImagePipelineManager>&& aImageMgr,
                        RefPtr<CompositorAnimationStorage>&& aAnimStorage,
                        TimeDuration aVsyncRate);

  static WebRenderBridgeParent* CreateDestroyed(
      const wr::PipelineId& aPipelineId);

  wr::PipelineId PipelineId() { return mPipelineId; }

  bool CloneWebRenderAPIs(nsTArray<RefPtr<wr::WebRenderAPI>>& aOutAPIs) {
    for (auto& api : mApis) {
      if (api) {
        RefPtr<wr::WebRenderAPI> clone = api->Clone();
        if (!clone) {
          return false;
        }
        aOutAPIs.AppendElement(clone);
      }
    }
    return true;
  }
  already_AddRefed<wr::WebRenderAPI> GetWebRenderAPIAtPoint(
      const ScreenPoint& aPoint);
  already_AddRefed<wr::WebRenderAPI> GetWebRenderAPI(
      wr::RenderRoot aRenderRoot) {
    if (aRenderRoot > wr::kHighestRenderRoot) {
      return nullptr;
    }
    return do_AddRef(mApis[aRenderRoot]);
  }
  AsyncImagePipelineManager* AsyncImageManager() { return mAsyncImageManager; }
  CompositorVsyncScheduler* CompositorScheduler() {
    return mCompositorScheduler.get();
  }
  CompositorBridgeParentBase* GetCompositorBridge() {
    return mCompositorBridge;
  }

  mozilla::ipc::IPCResult RecvEnsureConnected(
      TextureFactoryIdentifier* aTextureFactoryIdentifier,
      MaybeIdNamespace* aMaybeIdNamespace) override;

  mozilla::ipc::IPCResult RecvNewCompositable(
      const CompositableHandle& aHandle, const TextureInfo& aInfo) override;
  mozilla::ipc::IPCResult RecvReleaseCompositable(
      const CompositableHandle& aHandle) override;

  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvShutdownSync() override;
  mozilla::ipc::IPCResult RecvDeleteCompositorAnimations(
      InfallibleTArray<uint64_t>&& aIds) override;
  mozilla::ipc::IPCResult RecvUpdateResources(
      nsTArray<OpUpdateResource>&& aUpdates,
      nsTArray<RefCountedShmem>&& aSmallShmems,
      nsTArray<ipc::Shmem>&& aLargeShmems,
      const wr::RenderRoot& aRenderRoot) override;
  mozilla::ipc::IPCResult RecvSetDisplayList(
      nsTArray<RenderRootDisplayListData>&& aDisplayLists,
      InfallibleTArray<OpDestroy>&& aToDestroy,
      const uint64_t& aFwdTransactionId, const TransactionId& aTransactionId,
      const wr::IdNamespace& aIdNamespace, const bool& aContainsSVGGroup,
      const VsyncId& aVsyncId, const TimeStamp& aVsyncStartTime,
      const TimeStamp& aRefreshStartTime, const TimeStamp& aTxnStartTime,
      const nsCString& aTxnURL, const TimeStamp& aFwdTime,
      nsTArray<CompositionPayload>&& aPayloads) override;
  mozilla::ipc::IPCResult RecvEmptyTransaction(
      const FocusTarget& aFocusTarget, const uint32_t& aPaintSequenceNumber,
      nsTArray<RenderRootUpdates>&& aRenderRootUpdates,
      InfallibleTArray<OpDestroy>&& aToDestroy,
      const uint64_t& aFwdTransactionId, const TransactionId& aTransactionId,
      const wr::IdNamespace& aIdNamespace, const VsyncId& aVsyncId,
      const TimeStamp& aVsyncStartTime, const TimeStamp& aRefreshStartTime,
      const TimeStamp& aTxnStartTime, const nsCString& aTxnURL,
      const TimeStamp& aFwdTime,
      nsTArray<CompositionPayload>&& aPayloads) override;
  mozilla::ipc::IPCResult RecvSetFocusTarget(
      const FocusTarget& aFocusTarget) override;
  mozilla::ipc::IPCResult RecvParentCommands(
      nsTArray<WebRenderParentCommand>&& commands,
      const wr::RenderRoot& aRenderRoot) override;
  mozilla::ipc::IPCResult RecvGetSnapshot(PTextureParent* aTexture) override;

  mozilla::ipc::IPCResult RecvSetLayersObserverEpoch(
      const LayersObserverEpoch& aChildEpoch) override;

  mozilla::ipc::IPCResult RecvClearCachedResources() override;
  mozilla::ipc::IPCResult RecvScheduleComposite() override;
  mozilla::ipc::IPCResult RecvCapture() override;
  mozilla::ipc::IPCResult RecvSyncWithCompositor() override;

  mozilla::ipc::IPCResult RecvSetConfirmedTargetAPZC(
      const uint64_t& aBlockId,
      nsTArray<SLGuidAndRenderRoot>&& aTargets) override;

  mozilla::ipc::IPCResult RecvSetTestSampleTime(
      const TimeStamp& aTime) override;
  mozilla::ipc::IPCResult RecvLeaveTestMode() override;
  mozilla::ipc::IPCResult RecvGetAnimationValue(
      const uint64_t& aCompositorAnimationsId, OMTAValue* aValue) override;
  mozilla::ipc::IPCResult RecvSetAsyncScrollOffset(
      const ScrollableLayerGuid::ViewID& aScrollId, const float& aX,
      const float& aY) override;
  mozilla::ipc::IPCResult RecvSetAsyncZoom(
      const ScrollableLayerGuid::ViewID& aScrollId,
      const float& aZoom) override;
  mozilla::ipc::IPCResult RecvFlushApzRepaints() override;
  mozilla::ipc::IPCResult RecvGetAPZTestData(APZTestData* data) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void Pause();
  bool Resume();

  void Destroy();

  // CompositorVsyncSchedulerOwner
  bool IsPendingComposite() override { return false; }
  void FinishPendingComposite() override {}
  void CompositeToTarget(VsyncId aId, gfx::DrawTarget* aTarget,
                         const gfx::IntRect* aRect = nullptr) override;
  TimeDuration GetVsyncInterval() const override;

  // CompositableParentManager
  bool IsSameProcess() const override;
  base::ProcessId GetChildProcessId() override;
  void NotifyNotUsed(PTextureParent* aTexture,
                     uint64_t aTransactionId) override;
  void SendAsyncMessage(
      const InfallibleTArray<AsyncParentMessageData>& aMessage) override;
  void SendPendingAsyncMessages() override;
  void SetAboutToSendAsyncMessages() override;

  void HoldPendingTransactionId(
      const wr::Epoch& aWrEpoch, TransactionId aTransactionId,
      bool aContainsSVGGroup, const VsyncId& aVsyncId,
      const TimeStamp& aVsyncStartTime, const TimeStamp& aRefreshStartTime,
      const TimeStamp& aTxnStartTime, const nsCString& aTxnURL,
      const TimeStamp& aFwdTime, const bool aIsFirstPaint,
      nsTArray<CompositionPayload>&& aPayloads,
      const bool aUseForTelemetry = true);
  TransactionId LastPendingTransactionId();
  TransactionId FlushTransactionIdsForEpoch(
      const wr::Epoch& aEpoch, const VsyncId& aCompositeStartId,
      const TimeStamp& aCompositeStartTime, const TimeStamp& aRenderStartTime,
      const TimeStamp& aEndTime, UiCompositorControllerParent* aUiController,
      wr::RendererStats* aStats = nullptr,
      nsTArray<FrameStats>* aOutputStats = nullptr);
  void NotifySceneBuiltForEpoch(const wr::Epoch& aEpoch,
                                const TimeStamp& aEndTime);

  void CompositeIfNeeded();

  TextureFactoryIdentifier GetTextureFactoryIdentifier();

  void ExtractImageCompositeNotifications(
      nsTArray<ImageCompositeNotificationInfo>* aNotifications);

  wr::Epoch GetCurrentEpoch() const { return mWrEpoch; }
  wr::IdNamespace GetIdNamespace() { return mIdNamespace; }

  void FlushRendering(bool aWaitForPresent = true);

  /**
   * Schedule generating WebRender frame definitely at next composite timing.
   *
   * WebRenderBridgeParent uses composite timing to check if there is an update
   * to AsyncImagePipelines. If there is no update, WebRenderBridgeParent skips
   * to generate frame. If we need to generate new frame at next composite
   * timing, call this method.
   *
   * Call CompositorVsyncScheduler::ScheduleComposition() directly, if we just
   * want to trigger AsyncImagePipelines update checks.
   */
  void ScheduleGenerateFrame(const Maybe<wr::RenderRoot>& aRenderRoot);
  void ScheduleGenerateFrame(const wr::RenderRootSet& aRenderRoots);
  void ScheduleGenerateFrameAllRenderRoots();

  /**
   * Schedule forced frame rendering at next composite timing.
   *
   * WebRender could skip frame rendering if there is no update.
   * This function is used to force rendering even when there is not update.
   */
  void ScheduleForcedGenerateFrame();

  void NotifyDidSceneBuild(const nsTArray<wr::RenderRoot>& aRenderRoots,
                           RefPtr<wr::WebRenderPipelineInfo> aInfo);

  wr::Epoch UpdateWebRender(
      CompositorVsyncScheduler* aScheduler,
      nsTArray<RefPtr<wr::WebRenderAPI>>&& aApis,
      AsyncImagePipelineManager* aImageMgr,
      CompositorAnimationStorage* aAnimStorage,
      const TextureFactoryIdentifier& aTextureFactoryIdentifier);

  void RemoveEpochDataPriorTo(const wr::Epoch& aRenderedEpoch);

  /**
   * This sets the is-first-paint flag to true for the next received
   * display list. This is intended to be called by the widget code when it
   * loses its viewport information (or for whatever reason wants to refresh
   * the viewport information). The message will sent back to the widget code
   * via UiCompositorControllerParent::NotifyFirstPaint() when the corresponding
   * transaction is flushed.
   */
  void ForceIsFirstPaint() { mIsFirstPaint = true; }

  bool IsRootWebRenderBridgeParent() const;
  LayersId GetLayersId() const;
  WRRootId GetWRRootId() const;

 private:
  class ScheduleSharedSurfaceRelease;

  explicit WebRenderBridgeParent(const wr::PipelineId& aPipelineId);
  virtual ~WebRenderBridgeParent() = default;

  wr::WebRenderAPI* Api(wr::RenderRoot aRenderRoot) {
    if (IsRootWebRenderBridgeParent()) {
      return mApis[aRenderRoot];
    } else {
      MOZ_ASSERT(aRenderRoot == wr::RenderRoot::Default);
      return mApis[mRenderRoot];
    }
  }

  // Within WebRenderBridgeParent, we can use wr::RenderRoot::Default to
  // refer to DefaultApi(), which can be the content API if this
  // WebRenderBridgeParent is for a content WebRenderBridgeChild. However,
  // different WebRenderBridgeParents use the same AsyncImagePipelineManager,
  // for example, which doesn't have this distinction, so we need to
  // convert out our RenderRoot.
  wr::RenderRoot RenderRootForExternal(wr::RenderRoot aRenderRoot) {
    if (IsRootWebRenderBridgeParent()) {
      return aRenderRoot;
    } else {
      MOZ_ASSERT(aRenderRoot == wr::RenderRoot::Default);
      return mRenderRoot;
    }
  }

  bool SetDisplayList(wr::RenderRoot aRenderRoot, const LayoutDeviceRect& aRect,
                      const wr::LayoutSize& aContentSize, ipc::ByteBuf&& aDL,
                      const wr::BuiltDisplayListDescriptor& aDLDesc,
                      const nsTArray<OpUpdateResource>& aResourceUpdates,
                      const nsTArray<RefCountedShmem>& aSmallShmems,
                      const nsTArray<ipc::Shmem>& aLargeShmems,
                      const TimeStamp& aTxnStartTime,
                      wr::TransactionBuilder& aTxn, wr::Epoch aWrEpoch,
                      bool aValidTransaction, bool aObserveLayersUpdate);

  void UpdateAPZFocusState(const FocusTarget& aFocus);
  void UpdateAPZScrollData(const wr::Epoch& aEpoch, WebRenderScrollData&& aData,
                           wr::RenderRoot aRenderRoot);
  void UpdateAPZScrollOffsets(ScrollUpdatesMap&& aUpdates,
                              uint32_t aPaintSequenceNumber,
                              wr::RenderRoot aRenderRoot);

  bool UpdateResources(const nsTArray<OpUpdateResource>& aResourceUpdates,
                       const nsTArray<RefCountedShmem>& aSmallShmems,
                       const nsTArray<ipc::Shmem>& aLargeShmems,
                       wr::TransactionBuilder& aUpdates);
  bool AddExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                        wr::TransactionBuilder& aResources);
  bool UpdateExternalImage(
      wr::ExternalImageId aExtId, wr::ImageKey aKey,
      const ImageIntRect& aDirtyRect, wr::TransactionBuilder& aResources,
      UniquePtr<ScheduleSharedSurfaceRelease>& aScheduleRelease);
  void ObserveSharedSurfaceRelease(
      const nsTArray<wr::ExternalImageKeyPair>& aPairs);

  bool PushExternalImageForTexture(wr::ExternalImageId aExtId,
                                   wr::ImageKey aKey, TextureHost* aTexture,
                                   bool aIsUpdate,
                                   wr::TransactionBuilder& aResources);

  void AddPipelineIdForCompositable(const wr::PipelineId& aPipelineIds,
                                    const CompositableHandle& aHandle,
                                    const bool& aAsync,
                                    wr::TransactionBuilder& aTxn,
                                    wr::TransactionBuilder& aTxnForImageBridge,
                                    const wr::RenderRoot& aRenderRoot);
  void RemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                       wr::TransactionBuilder& aTxn,
                                       wr::RenderRoot aRenderRoot);

  void DeleteImage(const wr::ImageKey& aKey, wr::TransactionBuilder& aUpdates);
  void ReleaseTextureOfImage(const wr::ImageKey& aKey);

  bool ProcessWebRenderParentCommands(
      const InfallibleTArray<WebRenderParentCommand>& aCommands,
      wr::TransactionBuilder& aTxn, wr::RenderRoot aRenderRoot);

  void ClearResources();
  bool ShouldParentObserveEpoch();
  mozilla::ipc::IPCResult HandleShutdown();

  // Returns true if there is any animation (including animations in delay
  // phase).
  bool AdvanceAnimations();
  bool SampleAnimations(
      wr::RenderRootArray<nsTArray<wr::WrOpacityProperty>>& aOpacityArrays,
      wr::RenderRootArray<nsTArray<wr::WrTransformProperty>>& aTransformArrays);

  CompositorBridgeParent* GetRootCompositorBridgeParent() const;

  RefPtr<WebRenderBridgeParent> GetRootWebRenderBridgeParent() const;

  // Tell APZ what the subsequent sampling's timestamp should be.
  void SetAPZSampleTime();

  wr::Epoch GetNextWrEpoch();
  // This function is expected to be used when GetNextWrEpoch() is called,
  // but TransactionBuilder does not have resource updates nor display list.
  // In this case, ScheduleGenerateFrame is not triggered via SceneBuilder.
  // Then we want to rollback WrEpoch. See Bug 1490117.
  void RollbackWrEpoch();

  void FlushSceneBuilds();
  void FlushFrameGeneration();
  void FlushFramePresentation();

  void MaybeGenerateFrame(VsyncId aId, bool aForceGenerateFrame);

  VsyncId GetVsyncIdForEpoch(const wr::Epoch& aEpoch) {
    for (auto& id : mPendingTransactionIds) {
      if (id.mEpoch.mHandle == aEpoch.mHandle) {
        return id.mVsyncId;
      }
    }
    return VsyncId();
  }

 private:
  struct PendingTransactionId {
    PendingTransactionId(const wr::Epoch& aEpoch, TransactionId aId,
                         bool aContainsSVGGroup, const VsyncId& aVsyncId,
                         const TimeStamp& aVsyncStartTime,
                         const TimeStamp& aRefreshStartTime,
                         const TimeStamp& aTxnStartTime,
                         const nsCString& aTxnURL, const TimeStamp& aFwdTime,
                         const bool aIsFirstPaint, const bool aUseForTelemetry,
                         nsTArray<CompositionPayload>&& aPayloads)
        : mEpoch(aEpoch),
          mId(aId),
          mVsyncId(aVsyncId),
          mVsyncStartTime(aVsyncStartTime),
          mRefreshStartTime(aRefreshStartTime),
          mTxnStartTime(aTxnStartTime),
          mTxnURL(aTxnURL),
          mFwdTime(aFwdTime),
          mSkippedComposites(0),
          mContainsSVGGroup(aContainsSVGGroup),
          mIsFirstPaint(aIsFirstPaint),
          mUseForTelemetry(aUseForTelemetry),
          mPayloads(std::move(aPayloads)) {}
    wr::Epoch mEpoch;
    TransactionId mId;
    VsyncId mVsyncId;
    TimeStamp mVsyncStartTime;
    TimeStamp mRefreshStartTime;
    TimeStamp mTxnStartTime;
    nsCString mTxnURL;
    TimeStamp mFwdTime;
    TimeStamp mSceneBuiltTime;
    uint32_t mSkippedComposites;
    bool mContainsSVGGroup;
    bool mIsFirstPaint;
    bool mUseForTelemetry;
    nsTArray<CompositionPayload> mPayloads;
  };

  struct CompositorAnimationIdsForEpoch {
    CompositorAnimationIdsForEpoch(const wr::Epoch& aEpoch,
                                   InfallibleTArray<uint64_t>&& aIds)
        : mEpoch(aEpoch), mIds(std::move(aIds)) {}

    wr::Epoch mEpoch;
    InfallibleTArray<uint64_t> mIds;
  };

  CompositorBridgeParentBase* MOZ_NON_OWNING_REF mCompositorBridge;
  wr::PipelineId mPipelineId;
  RefPtr<widget::CompositorWidget> mWidget;
  // The RenderRootArray means there will always be a fixed number of apis,
  // one for each RenderRoot, even if renderroot splitting isn't enabled.
  // In this case, the unused apis will be nullptrs. Also, if this is not
  // the root WebRenderBridgeParent, there should only be one api in this
  // list. We avoid using a dynamically sized array for this because we
  // need to be able to null these out in a thread-safe way from
  // ClearResources, and there's no way to do that with an nsTArray.
  wr::RenderRootArray<RefPtr<wr::WebRenderAPI>> mApis;
  RefPtr<AsyncImagePipelineManager> mAsyncImageManager;
  RefPtr<CompositorVsyncScheduler> mCompositorScheduler;
  RefPtr<CompositorAnimationStorage> mAnimStorage;
  // mActiveAnimations is used to avoid leaking animations when
  // WebRenderBridgeParent is destroyed abnormally and Tab move between
  // different windows.
  std::unordered_map<uint64_t, wr::Epoch> mActiveAnimations;
  wr::RenderRootArray<std::unordered_map<uint64_t, RefPtr<WebRenderImageHost>>>
      mAsyncCompositables;
  std::unordered_map<uint64_t, CompositableTextureHostRef> mTextureHosts;
  std::unordered_map<uint64_t, wr::ExternalImageId> mSharedSurfaceIds;

  TimeDuration mVsyncRate;
  TimeStamp mPreviousFrameTimeStamp;
  // These fields keep track of the latest layer observer epoch values in the
  // child and the parent. mChildLayersObserverEpoch is the latest epoch value
  // received from the child. mParentLayersObserverEpoch is the latest epoch
  // value that we have told BrowserParent about (via ObserveLayerUpdate).
  LayersObserverEpoch mChildLayersObserverEpoch;
  LayersObserverEpoch mParentLayersObserverEpoch;

  std::deque<PendingTransactionId> mPendingTransactionIds;
  std::queue<CompositorAnimationIdsForEpoch> mCompositorAnimationsToDelete;
  wr::Epoch mWrEpoch;
  wr::IdNamespace mIdNamespace;

  VsyncId mSkippedCompositeId;
  TimeStamp mMostRecentComposite;

  // Kind of clunky, but I can't sort out a more elegant way of getting this to
  // work.
  Mutex mRenderRootRectMutex;
  wr::NonDefaultRenderRootArray<ScreenRect> mRenderRootRects;

  wr::RenderRoot mRenderRoot;
  bool mPaused;
  bool mDestroyed;
  bool mReceivedDisplayList;
  bool mIsFirstPaint;
  bool mSkippedComposite;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_WebRenderBridgeParent_h
