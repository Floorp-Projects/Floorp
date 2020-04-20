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
#include "Layers.h"
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/CompositorVsyncSchedulerOwner.h"
#include "mozilla/layers/PWebRenderBridgeParent.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/layers/WebRenderCompositionRecorder.h"
#include "mozilla/HashTable.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
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

class PipelineIdAndEpochHashEntry : public PLDHashEntryHdr {
 public:
  typedef const std::pair<wr::PipelineId, wr::Epoch>& KeyType;
  typedef const std::pair<wr::PipelineId, wr::Epoch>* KeyTypePointer;
  enum { ALLOW_MEMMOVE = true };

  explicit PipelineIdAndEpochHashEntry(wr::PipelineId aPipelineId,
                                       wr::Epoch aEpoch)
      : mValue(aPipelineId, aEpoch) {}

  PipelineIdAndEpochHashEntry(PipelineIdAndEpochHashEntry&& aOther) = default;

  explicit PipelineIdAndEpochHashEntry(KeyTypePointer aKey)
      : mValue(aKey->first, aKey->second) {}

  ~PipelineIdAndEpochHashEntry() {}

  KeyType GetKey() const { return mValue; }

  bool KeyEquals(KeyTypePointer aKey) const {
    return mValue.first.mHandle == aKey->first.mHandle &&
           mValue.first.mNamespace == aKey->first.mNamespace &&
           mValue.second.mHandle == aKey->second.mHandle;
  };

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashGeneric(aKey->first.mHandle, aKey->first.mNamespace,
                                aKey->second.mHandle);
  }

 private:
  std::pair<wr::PipelineId, wr::Epoch> mValue;
};

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

  void UpdateQualitySettings();
  void UpdateDebugFlags();
  void UpdateMultithreading();
  void UpdateBatchingParameters();

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
      nsTArray<uint64_t>&& aIds) override;
  mozilla::ipc::IPCResult RecvUpdateResources(
      nsTArray<OpUpdateResource>&& aUpdates,
      nsTArray<RefCountedShmem>&& aSmallShmems,
      nsTArray<ipc::Shmem>&& aLargeShmems,
      const wr::RenderRoot& aRenderRoot) override;
  mozilla::ipc::IPCResult RecvSetDisplayList(
      nsTArray<RenderRootDisplayListData>&& aDisplayLists,
      nsTArray<OpDestroy>&& aToDestroy, const uint64_t& aFwdTransactionId,
      const TransactionId& aTransactionId, const bool& aContainsSVGGroup,
      const VsyncId& aVsyncId, const TimeStamp& aVsyncStartTime,
      const TimeStamp& aRefreshStartTime, const TimeStamp& aTxnStartTime,
      const nsCString& aTxnURL, const TimeStamp& aFwdTime,
      nsTArray<CompositionPayload>&& aPayloads) override;
  mozilla::ipc::IPCResult RecvEmptyTransaction(
      const FocusTarget& aFocusTarget,
      nsTArray<RenderRootUpdates>&& aRenderRootUpdates,
      nsTArray<OpDestroy>&& aToDestroy, const uint64_t& aFwdTransactionId,
      const TransactionId& aTransactionId, const VsyncId& aVsyncId,
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
  mozilla::ipc::IPCResult RecvInvalidateRenderedFrame() override;
  mozilla::ipc::IPCResult RecvScheduleComposite() override;
  mozilla::ipc::IPCResult RecvCapture() override;
  mozilla::ipc::IPCResult RecvToggleCaptureSequence() override;
  mozilla::ipc::IPCResult RecvSetTransactionLogging(const bool&) override;
  mozilla::ipc::IPCResult RecvSyncWithCompositor() override;

  mozilla::ipc::IPCResult RecvSetConfirmedTargetAPZC(
      const uint64_t& aBlockId,
      nsTArray<ScrollableLayerGuid>&& aTargets) override;

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
      const nsTArray<AsyncParentMessageData>& aMessage) override;
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
   * Invalidate rendered frame.
   *
   * WebRender could skip frame rendering if there is no update.
   * This function is used to force invalidating even when there is no update.
   */
  void InvalidateRenderedFrame();

  /**
   * Schedule forced frame rendering at next composite timing.
   *
   * WebRender could skip frame rendering if there is no update.
   * This function is used to force rendering even when there is no update.
   */
  void ScheduleForcedGenerateFrame();

  void NotifyDidSceneBuild(const nsTArray<wr::RenderRoot>& aRenderRoots,
                           RefPtr<const wr::WebRenderPipelineInfo> aInfo);

  wr::Epoch UpdateWebRender(
      CompositorVsyncScheduler* aScheduler,
      nsTArray<RefPtr<wr::WebRenderAPI>>&& aApis,
      AsyncImagePipelineManager* aImageMgr,
      CompositorAnimationStorage* aAnimStorage,
      const TextureFactoryIdentifier& aTextureFactoryIdentifier);

  void RemoveEpochDataPriorTo(const wr::Epoch& aRenderedEpoch);

  void PushDeferredPipelineData(RenderRootDeferredData&& aDeferredData);

  /**
   * If we attempt to process information for a particular pipeline before we
   * can determine what RenderRoot it belongs to, then we defer that data until
   * we can. This handles processing that deferred data.
   */
  bool MaybeHandleDeferredPipelineData(
      wr::RenderRoot aRenderRoot, const nsTArray<wr::PipelineId>& aPipelineIds,
      const TimeStamp& aTxnStartTime);

  /**
   * See MaybeHandleDeferredPipelineData - this is the implementation of that
   * for a single pipeline.
   */
  bool MaybeHandleDeferredPipelineDataForPipeline(
      wr::RenderRoot aRenderRoot, wr::PipelineId aPipelineId,
      const TimeStamp& aTxnStartTime);

  bool HandleDeferredPipelineData(
      nsTArray<RenderRootDeferredData>& aDeferredData,
      const TimeStamp& aTxnStartTime);

  bool IsRootWebRenderBridgeParent() const;
  LayersId GetLayersId() const;

  void SetCompositionRecorder(
      UniquePtr<layers::WebRenderCompositionRecorder> aRecorder);

  /**
   * Write the frames collected by the |WebRenderCompositionRecorder| to disk.
   *
   * If there is not currently a recorder, this is a no-op.
   */
  RefPtr<wr::WebRenderAPI::WriteCollectedFramesPromise> WriteCollectedFrames();

#if defined(MOZ_WIDGET_ANDROID)
  /**
   * Request a screengrab for android
   */
  void RequestScreenPixels(UiCompositorControllerParent* aController);
  void MaybeCaptureScreenPixels();
#endif
  /**
   * Return the frames collected by the |WebRenderCompositionRecorder| encoded
   * as data URIs.
   *
   * If there is not currently a recorder, this is a no-op and the promise will
   * be rejected.
   */
  RefPtr<wr::WebRenderAPI::GetCollectedFramesPromise> GetCollectedFrames();

  void DisableNativeCompositor();
  void AddPendingScrollPayload(
      CompositionPayload& aPayload,
      const std::pair<wr::PipelineId, wr::Epoch>& aKey);

  nsTArray<CompositionPayload>* GetPendingScrollPayload(
      const std::pair<wr::PipelineId, wr::Epoch>& aKey);

  bool RemovePendingScrollPayload(
      const std::pair<wr::PipelineId, wr::Epoch>& aKey);

 private:
  class ScheduleSharedSurfaceRelease;

  explicit WebRenderBridgeParent(const wr::PipelineId& aPipelineId);
  virtual ~WebRenderBridgeParent();

  wr::WebRenderAPI* Api(wr::RenderRoot aRenderRoot) {
    if (IsRootWebRenderBridgeParent()) {
      return mApis[aRenderRoot];
    } else {
      MOZ_ASSERT(aRenderRoot == wr::RenderRoot::Default);
      return mApis[*mRenderRoot];
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
      return *mRenderRoot;
    }
  }

  // Returns whether a given render root is valid for this WRBP to receive as
  // input from the WRBC.
  bool RenderRootIsValid(wr::RenderRoot aRenderRoot);

  void RemoveDeferredPipeline(wr::PipelineId aPipelineId);

  bool ProcessEmptyTransactionUpdates(RenderRootUpdates& aUpdates,
                                      bool* aScheduleComposite);

  bool ProcessRenderRootDisplayListData(RenderRootDisplayListData& aDisplayList,
                                        wr::Epoch aWrEpoch,
                                        const TimeStamp& aTxnStartTime,
                                        bool aValidTransaction,
                                        bool aObserveLayersUpdate);

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
  bool AddPrivateExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                               wr::ImageDescriptor aDesc,
                               wr::TransactionBuilder& aResources);
  bool AddSharedExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                              wr::TransactionBuilder& aResources);
  bool UpdateSharedExternalImage(
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
      const nsTArray<WebRenderParentCommand>& aCommands,
      wr::TransactionBuilder& aTxn, wr::RenderRoot aRenderRoot);

  void ClearResources();
  bool ShouldParentObserveEpoch();
  mozilla::ipc::IPCResult HandleShutdown();

  // Returns true if there is any animation (including animations in delay
  // phase).
  bool AdvanceAnimations();

  struct WrAnimations {
    wr::RenderRootArray<nsTArray<wr::WrOpacityProperty>> mOpacityArrays;
    wr::RenderRootArray<nsTArray<wr::WrTransformProperty>> mTransformArrays;
    wr::RenderRootArray<nsTArray<wr::WrColorProperty>> mColorArrays;
  };
  bool SampleAnimations(WrAnimations& aAnimations);

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
                                   nsTArray<uint64_t>&& aIds)
        : mEpoch(aEpoch), mIds(std::move(aIds)) {}

    wr::Epoch mEpoch;
    nsTArray<uint64_t> mIds;
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
  // This is a map from pipeline id to render root, that tracks the render
  // roots of all subpipelines (including nested subpipelines, e.g. in the
  // Fission case) attached to this WebRenderBridgeParent. This is only
  // populated on the root WRBP. It is used to resolve the render root for the
  // subpipelines, since they may not know where they are attached in the
  // parent display list and therefore may not know their render root.
  nsDataHashtable<nsUint64HashKey, wr::RenderRoot> mPipelineRenderRoots;
  // This is a hashset of child pipelines for this WRBP. This allows us to
  // iterate through all the children of a non-root WRBP and add them to
  // the root's mPipelineRenderRoots, and potentially resolve any of their
  // deferred updates.
  nsTHashtable<nsUint64HashKey> mChildPipelines;
  // This is a map from pipeline id to a list of deferred data. This is only
  // populated on the root WRBP. The data contained within is deferred because
  // the sub-WRBP that received it did not know which renderroot it belonged
  // to. Once that is resolved by the root WRBP getting the right display list
  // update, the deferred data is processed.
  nsDataHashtable<nsUint64HashKey, nsTArray<RenderRootDeferredData>>
      mPipelineDeferredUpdates;
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

  Maybe<wr::RenderRoot> mRenderRoot;
#if defined(MOZ_WIDGET_ANDROID)
  UiCompositorControllerParent* mScreenPixelsTarget;
#endif
  bool mPaused;
  bool mDestroyed;
  bool mReceivedDisplayList;
  bool mIsFirstPaint;
  bool mSkippedComposite;
  bool mDisablingNativeCompositor;
  // These payloads are being used for SCROLL_PRESENT_LATENCY telemetry
  DataMutex<nsClassHashtable<PipelineIdAndEpochHashEntry,
                             nsTArray<CompositionPayload>>>
      mPendingScrollPayloads;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_WebRenderBridgeParent_h
