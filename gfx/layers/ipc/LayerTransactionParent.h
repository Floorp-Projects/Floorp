/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
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
#include "nsAutoPtr.h"                  // for nsRefPtr
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
class LayerManagerComposite;
class ShadowLayerParent;
class CompositableParent;
class ShadowLayersManager;

class LayerTransactionParent final : public PLayerTransactionParent,
                                     public CompositableParentManager
{
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;
  typedef InfallibleTArray<Edit> EditArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;
  typedef InfallibleTArray<AsyncChildMessageData> AsyncChildMessageArray;
  typedef InfallibleTArray<PluginWindowData> PluginsArray;

public:
  LayerTransactionParent(LayerManagerComposite* aManager,
                         ShadowLayersManager* aLayersManager,
                         uint64_t aId);

protected:
  ~LayerTransactionParent();

public:
  void Destroy();

  LayerManagerComposite* layer_manager() const { return mLayerManager; }

  uint64_t GetId() const { return mId; }
  Layer* GetRoot() const { return mRoot; }

  // ISurfaceAllocator
  virtual bool AllocShmem(size_t aSize,
                          ipc::SharedMemory::SharedMemoryType aType,
                          ipc::Shmem* aShmem) override {
    return PLayerTransactionParent::AllocShmem(aSize, aType, aShmem);
  }

  virtual bool AllocUnsafeShmem(size_t aSize,
                                ipc::SharedMemory::SharedMemoryType aType,
                                ipc::Shmem* aShmem) override {
    return PLayerTransactionParent::AllocUnsafeShmem(aSize, aType, aShmem);
  }

  virtual void DeallocShmem(ipc::Shmem& aShmem) override
  {
    PLayerTransactionParent::DeallocShmem(aShmem);
  }

  virtual bool IsSameProcess() const override;

  const uint64_t& GetPendingTransactionId() { return mPendingTransaction; }
  void SetPendingTransactionId(uint64_t aId) { mPendingTransaction = aId; }

  // CompositableParentManager
  virtual void SendFenceHandleIfPresent(PTextureParent* aTexture,
                                        CompositableHost* aCompositableHost) override;

  virtual void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;

  virtual base::ProcessId GetChildProcessId() override
  {
    return OtherPid();
  }

  virtual void ReplyRemoveTexture(const OpReplyRemoveTexture& aReply) override;

protected:
  virtual bool RecvShutdown() override;

  virtual bool RecvUpdate(EditArray&& cset,
                          const uint64_t& aTransactionId,
                          const TargetConfig& targetConfig,
                          PluginsArray&& aPlugins,
                          const bool& isFirstPaint,
                          const bool& scheduleComposite,
                          const uint32_t& paintSequenceNumber,
                          const bool& isRepeatTransaction,
                          const mozilla::TimeStamp& aTransactionStart,
                          const int32_t& aPaintSyncId,
                          EditReplyArray* reply) override;

  virtual bool RecvUpdateNoSwap(EditArray&& cset,
                                const uint64_t& aTransactionId,
                                const TargetConfig& targetConfig,
                                PluginsArray&& aPlugins,
                                const bool& isFirstPaint,
                                const bool& scheduleComposite,
                                const uint32_t& paintSequenceNumber,
                                const bool& isRepeatTransaction,
                                const mozilla::TimeStamp& aTransactionStart,
                                const int32_t& aPaintSyncId) override;

  virtual bool RecvClearCachedResources() override;
  virtual bool RecvForceComposite() override;
  virtual bool RecvSetTestSampleTime(const TimeStamp& aTime) override;
  virtual bool RecvLeaveTestMode() override;
  virtual bool RecvGetOpacity(PLayerParent* aParent,
                              float* aOpacity) override;
  virtual bool RecvGetAnimationTransform(PLayerParent* aParent,
                                         MaybeTransform* aTransform)
                                         override;
  virtual bool RecvSetAsyncScrollOffset(const FrameMetrics::ViewID& aId,
                                        const int32_t& aX, const int32_t& aY) override;
  virtual bool RecvSetAsyncZoom(const FrameMetrics::ViewID& aId,
                                const float& aValue) override;
  virtual bool RecvFlushApzRepaints() override;
  virtual bool RecvGetAPZTestData(APZTestData* aOutData) override;
  virtual bool RecvRequestProperty(const nsString& aProperty, float* aValue) override;
  virtual bool RecvSetConfirmedTargetAPZC(const uint64_t& aBlockId,
                                          nsTArray<ScrollableLayerGuid>&& aTargets) override;

  virtual PLayerParent* AllocPLayerParent() override;
  virtual bool DeallocPLayerParent(PLayerParent* actor) override;

  virtual PCompositableParent* AllocPCompositableParent(const TextureInfo& aInfo) override;
  virtual bool DeallocPCompositableParent(PCompositableParent* actor) override;

  virtual PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                              const LayersBackend& aLayersBackend,
                                              const TextureFlags& aFlags) override;
  virtual bool DeallocPTextureParent(PTextureParent* actor) override;

  virtual bool
  RecvChildAsyncMessages(InfallibleTArray<AsyncChildMessageData>&& aMessages) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  bool Attach(ShadowLayerParent* aLayerParent,
              CompositableHost* aCompositable,
              bool aIsAsyncVideo);

  void AddIPDLReference() {
    MOZ_ASSERT(mIPCOpen == false);
    mIPCOpen = true;
    ADDREF_MANUALLY(this);
  }
  void ReleaseIPDLReference() {
    MOZ_ASSERT(mIPCOpen == true);
    mIPCOpen = false;
    RELEASE_MANUALLY(this);
  }
  friend class CompositorParent;
  friend class CrossProcessCompositorParent;
  friend class layout::RenderFrameParent;

private:
  nsRefPtr<LayerManagerComposite> mLayerManager;
  ShadowLayersManager* mShadowLayersManager;
  // Hold the root because it might be grafted under various
  // containers in the "real" layer tree
  nsRefPtr<Layer> mRoot;
  // When this is nonzero, it refers to a layer tree owned by the
  // compositor thread.  It is always true that
  //   mId != 0 => mRoot == null
  // because the "real tree" is owned by the compositor.
  uint64_t mId;

  uint64_t mPendingTransaction;
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
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_LAYERTRANSACTIONPARENT_H
