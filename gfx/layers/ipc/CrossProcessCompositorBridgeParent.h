/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CrossProcessCompositorBridgeParent_h
#define mozilla_layers_CrossProcessCompositorBridgeParent_h

#include "mozilla/layers/CompositorBridgeParent.h"

namespace mozilla {
namespace layers {

/**
 * This class handles layer updates pushed directly from child processes to
 * the compositor thread. It's associated with a CompositorBridgeParent on the
 * compositor thread. While it uses the PCompositorBridge protocol to manage
 * these updates, it doesn't actually drive compositing itself. For that it
 * hands off work to the CompositorBridgeParent it's associated with.
 */
class CrossProcessCompositorBridgeParent final : public CompositorBridgeParentBase
{
  friend class CompositorBridgeParent;

public:
  explicit CrossProcessCompositorBridgeParent()
    : mNotifyAfterRemotePaint(false)
    , mDestroyCalled(false)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void Bind(Endpoint<PCompositorBridgeParent>&& aEndpoint) {
    if (!aEndpoint.Bind(this)) {
      return;
    }
    mSelfRef = this;
  }

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // FIXME/bug 774388: work out what shutdown protocol we need.
  virtual mozilla::ipc::IPCResult RecvInitialize(const uint64_t& aRootLayerTreeId) override { return IPC_FAIL_NO_REASON(this); }
  virtual mozilla::ipc::IPCResult RecvReset(nsTArray<LayersBackend>&& aBackendHints,
                         const uint64_t& aSeqNo,
                         bool* aResult,
                         TextureFactoryIdentifier* aOutIdentifier) override
  { return IPC_FAIL_NO_REASON(this); }
  virtual mozilla::ipc::IPCResult RecvRequestOverfill() override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvWillClose() override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvPause() override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvResume() override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvNotifyChildCreated(const uint64_t& child) override;
  virtual mozilla::ipc::IPCResult RecvNotifyChildRecreated(const uint64_t& child) override { return IPC_FAIL_NO_REASON(this); }
  virtual mozilla::ipc::IPCResult RecvAdoptChild(const uint64_t& child) override { return IPC_FAIL_NO_REASON(this); }
  virtual mozilla::ipc::IPCResult RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                const gfx::IntRect& aRect) override
  { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvFlushRendering() override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvForcePresent() override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvNotifyRegionInvalidated(const nsIntRegion& aRegion) override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex) override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvStopFrameTimeRecording(const uint32_t& aStartIndex, InfallibleTArray<float>* intervals) override  { return IPC_OK(); }

  virtual mozilla::ipc::IPCResult RecvClearApproximatelyVisibleRegions(const uint64_t& aLayersId,
                                                    const uint32_t& aPresShellId) override;

  virtual mozilla::ipc::IPCResult RecvNotifyApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                                    const CSSIntRegion& aRegion) override;

  virtual mozilla::ipc::IPCResult RecvAllPluginsCaptured() override { return IPC_OK(); }

  virtual mozilla::ipc::IPCResult RecvGetFrameUniformity(FrameUniformityData* aOutData) override
  {
    // Don't support calculating frame uniformity on the child process and
    // this is just a stub for now.
    MOZ_ASSERT(false);
    return IPC_OK();
  }

  /**
   * Tells this CompositorBridgeParent to send a message when the compositor has received the transaction.
   */
  virtual mozilla::ipc::IPCResult RecvRequestNotifyAfterRemotePaint() override;

  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool *aSuccess) override;

  virtual bool DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers) override;

  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const uint64_t& aTransactionId,
                                   const TargetConfig& aTargetConfig,
                                   const InfallibleTArray<PluginWindowData>& aPlugins,
                                   bool aIsFirstPaint,
                                   bool aScheduleComposite,
                                   uint32_t aPaintSequenceNumber,
                                   bool aIsRepeatTransaction,
                                   int32_t /*aPaintSyncId: unused*/,
                                   bool aHitTestUpdate) override;
  virtual void ForceComposite(LayerTransactionParent* aLayerTree) override;
  virtual void NotifyClearCachedResources(LayerTransactionParent* aLayerTree) override;
  virtual bool SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                 const TimeStamp& aTime) override;
  virtual void LeaveTestMode(LayerTransactionParent* aLayerTree) override;
  virtual void ApplyAsyncProperties(LayerTransactionParent* aLayerTree)
               override;
  virtual void FlushApzRepaints(const LayerTransactionParent* aLayerTree) override;
  virtual void GetAPZTestData(const LayerTransactionParent* aLayerTree,
                              APZTestData* aOutData) override;
  virtual void SetConfirmedTargetAPZC(const LayerTransactionParent* aLayerTree,
                                      const uint64_t& aInputBlockId,
                                      const nsTArray<ScrollableLayerGuid>& aTargets) override;

  virtual AsyncCompositionManager* GetCompositionManager(LayerTransactionParent* aParent) override;
  virtual mozilla::ipc::IPCResult RecvRemotePluginsReady()  override { return IPC_FAIL_NO_REASON(this); }
  virtual mozilla::ipc::IPCResult RecvAcknowledgeCompositorUpdate(const uint64_t& aLayersId) override;

  void DidComposite(uint64_t aId,
                    TimeStamp& aCompositeStart,
                    TimeStamp& aCompositeEnd);

  virtual PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                              const LayersBackend& aLayersBackend,
                                              const TextureFlags& aFlags,
                                              const uint64_t& aId,
                                              const uint64_t& aSerial) override;

  virtual bool DeallocPTextureParent(PTextureParent* actor) override;

  virtual bool IsSameProcess() const override;

  PCompositorWidgetParent* AllocPCompositorWidgetParent(const CompositorWidgetInitData& aInitData) override {
    // Not allowed.
    return nullptr;
  }
  bool DeallocPCompositorWidgetParent(PCompositorWidgetParent* aActor) override {
    // Not allowed.
    return false;
  }

  virtual mozilla::ipc::IPCResult RecvAsyncPanZoomEnabled(const uint64_t& aLayersId, bool* aHasAPZ) override;

  virtual PAPZCTreeManagerParent* AllocPAPZCTreeManagerParent(const uint64_t& aLayersId) override;
  virtual bool DeallocPAPZCTreeManagerParent(PAPZCTreeManagerParent* aActor) override;

  virtual PAPZParent* AllocPAPZParent(const uint64_t& aLayersId) override;
  virtual bool DeallocPAPZParent(PAPZParent* aActor) override;

  virtual void UpdatePaintTime(LayerTransactionParent* aLayerTree, const TimeDuration& aPaintTime) override;

  PWebRenderBridgeParent* AllocPWebRenderBridgeParent(const uint64_t& aPipelineId) override;
  bool DeallocPWebRenderBridgeParent(PWebRenderBridgeParent* aActor) override;

protected:
  void OnChannelConnected(int32_t pid) override {
    mCompositorThreadHolder = CompositorThreadHolder::GetSingleton();
  }
private:
  // Private destructor, to discourage deletion outside of Release():
  virtual ~CrossProcessCompositorBridgeParent();

  void DeferredDestroy();

  // There can be many CPCPs, and IPDL-generated code doesn't hold a
  // reference to top-level actors.  So we hold a reference to
  // ourself.  This is released (deferred) in ActorDestroy().
  RefPtr<CrossProcessCompositorBridgeParent> mSelfRef;

  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;
  // If true, we should send a RemotePaintIsReady message when the layer transaction
  // is received
  bool mNotifyAfterRemotePaint;
  bool mDestroyCalled;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CrossProcessCompositorBridgeParent_h
