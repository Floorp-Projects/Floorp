/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_LAYERTRANSACTIONPARENT_H
#define MOZILLA_LAYERS_LAYERTRANSACTIONPARENT_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint64_t, uint32_t
#include "CompositableTransactionParent.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/PLayerTransactionParent.h"
#include "nsRefPtrHashtable.h"
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray

namespace mozilla {

namespace ipc {
class Shmem;
} // namespace ipc

namespace layout {
class RenderFrameParent;
} // namespace layout

namespace layers {

class Layer;
class HostLayerManager;
class ShadowLayerParent;
class CompositableParent;
class CompositorAnimationStorage;
class CompositorBridgeParentBase;

class LayerTransactionParent final : public PLayerTransactionParent,
                                     public CompositableParentManager,
                                     public ShmemAllocator
{
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;
  typedef InfallibleTArray<Edit> EditArray;
  typedef InfallibleTArray<OpDestroy> OpDestroyArray;
  typedef InfallibleTArray<PluginWindowData> PluginsArray;

public:
  LayerTransactionParent(HostLayerManager* aManager,
                         CompositorBridgeParentBase* aBridge,
                         CompositorAnimationStorage* aAnimStorage,
                         LayersId aId,
                         TimeDuration aVsyncRate);

protected:
  ~LayerTransactionParent();

public:
  void Destroy();

  void SetLayerManager(HostLayerManager* aLayerManager, CompositorAnimationStorage* aAnimStorage);

  LayersId GetId() const { return mId; }
  Layer* GetRoot() const { return mRoot; }

  uint64_t GetChildEpoch() const { return mChildEpoch; }
  bool ShouldParentObserveEpoch();

  ShmemAllocator* AsShmemAllocator() override { return this; }

  bool AllocShmem(size_t aSize,
                  ipc::SharedMemory::SharedMemoryType aType,
                  ipc::Shmem* aShmem) override;

  bool AllocUnsafeShmem(size_t aSize,
                        ipc::SharedMemory::SharedMemoryType aType,
                        ipc::Shmem* aShmem) override;

  void DeallocShmem(ipc::Shmem& aShmem) override;

  bool IsSameProcess() const override;

  const TransactionId& GetPendingTransactionId() { return mPendingTransaction; }
  void SetPendingTransactionId(TransactionId aId,
                               const TimeStamp& aRefreshStartTime,
                               const TimeStamp& aTxnStartTime,
                               const TimeStamp& aFwdTime)
  {
    mPendingTransaction = aId;
    mRefreshStartTime = aRefreshStartTime;
    mTxnStartTime = aTxnStartTime;
    mFwdTime = aFwdTime;
  }
  TransactionId FlushTransactionId(TimeStamp& aCompositeEnd);

  // CompositableParentManager
  void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;

  void SendPendingAsyncMessages() override;

  void SetAboutToSendAsyncMessages() override;

  void NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId) override;

  base::ProcessId GetChildProcessId() override
  {
    return OtherPid();
  }

protected:
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvShutdownSync() override;

  mozilla::ipc::IPCResult RecvPaintTime(const TransactionId& aTransactionId,
                                        const TimeDuration& aPaintTime) override;

  mozilla::ipc::IPCResult RecvUpdate(const TransactionInfo& aInfo) override;

  mozilla::ipc::IPCResult RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch) override;
  mozilla::ipc::IPCResult RecvNewCompositable(const CompositableHandle& aHandle,
                                              const TextureInfo& aInfo) override;
  mozilla::ipc::IPCResult RecvReleaseLayer(const LayerHandle& aHandle) override;
  mozilla::ipc::IPCResult RecvReleaseCompositable(const CompositableHandle& aHandle) override;

  mozilla::ipc::IPCResult RecvClearCachedResources() override;
  mozilla::ipc::IPCResult RecvScheduleComposite() override;
  mozilla::ipc::IPCResult RecvSetTestSampleTime(const TimeStamp& aTime) override;
  mozilla::ipc::IPCResult RecvLeaveTestMode() override;
  mozilla::ipc::IPCResult RecvGetAnimationOpacity(const uint64_t& aCompositorAnimationsId,
                                                  float* aOpacity,
                                                  bool* aHasAnimationOpacity) override;
  mozilla::ipc::IPCResult RecvGetAnimationTransform(const uint64_t& aCompositorAnimationsId,
                                                    MaybeTransform* aTransform) override;
  mozilla::ipc::IPCResult RecvGetTransform(const LayerHandle& aHandle,
                                           MaybeTransform* aTransform) override;
  mozilla::ipc::IPCResult RecvSetAsyncScrollOffset(const FrameMetrics::ViewID& aId,
                                                   const float& aX, const float& aY) override;
  mozilla::ipc::IPCResult RecvSetAsyncZoom(const FrameMetrics::ViewID& aId,
                                           const float& aValue) override;
  mozilla::ipc::IPCResult RecvFlushApzRepaints() override;
  mozilla::ipc::IPCResult RecvGetAPZTestData(APZTestData* aOutData) override;
  mozilla::ipc::IPCResult RecvRequestProperty(const nsString& aProperty, float* aValue) override;
  mozilla::ipc::IPCResult RecvSetConfirmedTargetAPZC(const uint64_t& aBlockId,
                                                     nsTArray<ScrollableLayerGuid>&& aTargets) override;
  mozilla::ipc::IPCResult RecvRecordPaintTimes(const PaintTiming& aTiming) override;
  mozilla::ipc::IPCResult RecvGetTextureFactoryIdentifier(TextureFactoryIdentifier* aIdentifier) override;

  bool SetLayerAttributes(const OpSetLayerAttributes& aOp);

  void ActorDestroy(ActorDestroyReason why) override;

  template <typename T>
  bool BindLayer(const RefPtr<Layer>& aLayer, const T& aCreateOp) {
    return BindLayerToHandle(aLayer, aCreateOp.layer());
  }

  bool BindLayerToHandle(RefPtr<Layer> aLayer, const LayerHandle& aHandle);

  Layer* AsLayer(const LayerHandle& aLayer);

  bool Attach(Layer* aLayer, CompositableHost* aCompositable, bool aIsAsyncVideo);

  void AddIPDLReference() {
    MOZ_ASSERT(mIPCOpen == false);
    mIPCOpen = true;
    AddRef();
  }
  void ReleaseIPDLReference() {
    MOZ_ASSERT(mIPCOpen == true);
    mIPCOpen = false;
    Release();
  }
  friend class CompositorBridgeParent;
  friend class CrossProcessCompositorBridgeParent;
  friend class layout::RenderFrameParent;

private:
  // This is a function so we can log or breakpoint on why hit
  // testing tree changes are made.
  void UpdateHitTestingTree(Layer* aLayer, const char* aWhy) {
    mUpdateHitTestingTree = true;
  }

private:
  RefPtr<HostLayerManager> mLayerManager;
  CompositorBridgeParentBase* mCompositorBridge;
  RefPtr<CompositorAnimationStorage> mAnimStorage;

  // Hold the root because it might be grafted under various
  // containers in the "real" layer tree
  RefPtr<Layer> mRoot;

  // Mapping from LayerHandles to Layers.
  nsRefPtrHashtable<nsUint64HashKey, Layer> mLayerMap;

  LayersId mId;

  // These fields keep track of the latest epoch values in the child and the
  // parent. mChildEpoch is the latest epoch value received from the child.
  // mParentEpoch is the latest epoch value that we have told TabParent about
  // (via ObserveLayerUpdate).
  uint64_t mChildEpoch;
  uint64_t mParentEpoch;

  TimeDuration mVsyncRate;

  TransactionId mPendingTransaction;
  TimeStamp mRefreshStartTime;
  TimeStamp mTxnStartTime;
  TimeStamp mFwdTime;

  // When the widget/frame/browser stuff in this process begins its
  // destruction process, we need to Disconnect() all the currently
  // live shadow layers, because some of them might be orphaned from
  // the layer tree.  This happens in Destroy() above.  After we
  // Destroy() ourself, there's a window in which that information
  // hasn't yet propagated back to the child side and it might still
  // send us layer transactions.  We want to ignore those transactions
  // because they refer to "zombie layers" on this side.  So, we track
  // that state with |mDestroyed|.  This is similar to, but separate
  // from, |mLayerManager->IsDestroyed()|; we might have had Destroy()
  // called on us but the mLayerManager might not be destroyed, or
  // vice versa.  In both cases though, we want to ignore shadow-layer
  // transactions posted by the child.

  bool mDestroyed;
  bool mIPCOpen;

  // This is set during RecvUpdate to track whether we'll need to update
  // APZ's hit test regions.
  bool mUpdateHitTestingTree;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_LAYERTRANSACTIONPARENT_H
