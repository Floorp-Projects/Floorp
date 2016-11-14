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
  virtual bool RecvInitialize(const uint64_t& aRootLayerTreeId) override { return false; }
  virtual bool RecvReset(nsTArray<LayersBackend>&& aBackendHints,
                         const uint64_t& aSeqNo,
                         bool* aResult,
                         TextureFactoryIdentifier* aOutIdentifier) override
  { return false; }
  virtual bool RecvRequestOverfill() override { return true; }
  virtual bool RecvWillClose() override { return true; }
  virtual bool RecvPause() override { return true; }
  virtual bool RecvResume() override { return true; }
  virtual bool RecvNotifyChildCreated(const uint64_t& child) override;
  virtual bool RecvNotifyChildRecreated(const uint64_t& child) override { return false; }
  virtual bool RecvAdoptChild(const uint64_t& child) override { return false; }
  virtual bool RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                const gfx::IntRect& aRect) override
  { return true; }
  virtual bool RecvFlushRendering() override { return true; }
  virtual bool RecvForcePresent() override { return true; }
  virtual bool RecvNotifyRegionInvalidated(const nsIntRegion& aRegion) override { return true; }
  virtual bool RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex) override { return true; }
  virtual bool RecvStopFrameTimeRecording(const uint32_t& aStartIndex, InfallibleTArray<float>* intervals) override  { return true; }

  virtual bool RecvClearApproximatelyVisibleRegions(const uint64_t& aLayersId,
                                                    const uint32_t& aPresShellId) override;

  virtual bool RecvNotifyApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                                    const CSSIntRegion& aRegion) override;

  virtual bool RecvAllPluginsCaptured() override { return true; }

  virtual bool RecvGetFrameUniformity(FrameUniformityData* aOutData) override
  {
    // Don't support calculating frame uniformity on the child process and
    // this is just a stub for now.
    MOZ_ASSERT(false);
    return true;
  }

  /**
   * Tells this CompositorBridgeParent to send a message when the compositor has received the transaction.
   */
  virtual bool RecvRequestNotifyAfterRemotePaint() override;

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
  virtual bool RecvRemotePluginsReady()  override { return false; }
  virtual bool RecvAcknowledgeCompositorUpdate(const uint64_t& aLayersId) override;

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

  virtual bool RecvAsyncPanZoomEnabled(const uint64_t& aLayersId, bool* aHasAPZ) override;

  virtual PAPZCTreeManagerParent* AllocPAPZCTreeManagerParent(const uint64_t& aLayersId) override;
  virtual bool DeallocPAPZCTreeManagerParent(PAPZCTreeManagerParent* aActor) override;

  virtual PAPZParent* AllocPAPZParent(const uint64_t& aLayersId) override;
  virtual bool DeallocPAPZParent(PAPZParent* aActor) override;

  virtual void UpdatePaintTime(LayerTransactionParent* aLayerTree, const TimeDuration& aPaintTime) override;

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
