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
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/PLayerTransactionParent.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray

class gfx3DMatrix;

namespace mozilla {

namespace ipc {
class Shmem;
}

namespace layout {
class RenderFrameParent;
}

namespace layers {

class Layer;
class LayerManagerComposite;
class ShadowLayerParent;
class CompositableParent;
class ShadowLayersManager;

class LayerTransactionParent : public PLayerTransactionParent,
                               public CompositableParentManager
{
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;
  typedef InfallibleTArray<Edit> EditArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;
  typedef InfallibleTArray<AsyncChildMessageData> AsyncChildMessageArray;

public:
  LayerTransactionParent(LayerManagerComposite* aManager,
                         ShadowLayersManager* aLayersManager,
                         uint64_t aId,
                         ProcessId aOtherProcess);
  ~LayerTransactionParent();

  void Destroy();

  LayerManagerComposite* layer_manager() const { return mLayerManager; }

  uint64_t GetId() const { return mId; }
  Layer* GetRoot() const { return mRoot; }

  // ISurfaceAllocator
  virtual bool AllocShmem(size_t aSize,
                          ipc::SharedMemory::SharedMemoryType aType,
                          ipc::Shmem* aShmem) {
    return PLayerTransactionParent::AllocShmem(aSize, aType, aShmem);
  }

  virtual bool AllocUnsafeShmem(size_t aSize,
                                ipc::SharedMemory::SharedMemoryType aType,
                                ipc::Shmem* aShmem) {
    return PLayerTransactionParent::AllocUnsafeShmem(aSize, aType, aShmem);
  }

  virtual void DeallocShmem(ipc::Shmem& aShmem) MOZ_OVERRIDE
  {
    PLayerTransactionParent::DeallocShmem(aShmem);
  }

  virtual LayersBackend GetCompositorBackendType() const MOZ_OVERRIDE;

  virtual bool IsSameProcess() const MOZ_OVERRIDE;

  const uint64_t& GetPendingTransactionId() { return mPendingTransaction; }
  void SetPendingTransactionId(uint64_t aId) { mPendingTransaction = aId; }

  // CompositableParentManager
  virtual void SendFenceHandle(AsyncTransactionTracker* aTracker,
                               PTextureParent* aTexture,
                               const FenceHandle& aFence) MOZ_OVERRIDE;

  virtual void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) MOZ_OVERRIDE;

  virtual base::ProcessId GetChildProcessId() MOZ_OVERRIDE
  {
    return mChildProcessId;
  }

protected:
  virtual bool RecvUpdate(const EditArray& cset,
                          const uint64_t& aTransactionId,
                          const TargetConfig& targetConfig,
                          const bool& isFirstPaint,
                          const bool& scheduleComposite,
                          const uint32_t& paintSequenceNumber,
                          EditReplyArray* reply) MOZ_OVERRIDE;

  virtual bool RecvUpdateNoSwap(const EditArray& cset,
                                const uint64_t& aTransactionId,
                                const TargetConfig& targetConfig,
                                const bool& isFirstPaint,
                                const bool& scheduleComposite,
                                const uint32_t& paintSequenceNumber) MOZ_OVERRIDE;

  virtual bool RecvClearCachedResources() MOZ_OVERRIDE;
  virtual bool RecvForceComposite() MOZ_OVERRIDE;
  virtual bool RecvSetTestSampleTime(const TimeStamp& aTime) MOZ_OVERRIDE;
  virtual bool RecvLeaveTestMode() MOZ_OVERRIDE;
  virtual bool RecvGetOpacity(PLayerParent* aParent,
                              float* aOpacity) MOZ_OVERRIDE;
  virtual bool RecvGetAnimationTransform(PLayerParent* aParent,
                                         MaybeTransform* aTransform)
                                         MOZ_OVERRIDE;
  virtual bool RecvSetAsyncScrollOffset(PLayerParent* aLayer,
                                        const int32_t& aX, const int32_t& aY) MOZ_OVERRIDE;
  virtual bool RecvGetAPZTestData(APZTestData* aOutData);

  virtual PLayerParent* AllocPLayerParent() MOZ_OVERRIDE;
  virtual bool DeallocPLayerParent(PLayerParent* actor) MOZ_OVERRIDE;

  virtual PCompositableParent* AllocPCompositableParent(const TextureInfo& aInfo) MOZ_OVERRIDE;
  virtual bool DeallocPCompositableParent(PCompositableParent* actor) MOZ_OVERRIDE;

  virtual PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                              const TextureFlags& aFlags) MOZ_OVERRIDE;
  virtual bool DeallocPTextureParent(PTextureParent* actor) MOZ_OVERRIDE;

  virtual bool
  RecvChildAsyncMessages(const InfallibleTArray<AsyncChildMessageData>& aMessages) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  bool Attach(ShadowLayerParent* aLayerParent,
              CompositableHost* aCompositable,
              bool aIsAsyncVideo);

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

  // Child side's process id.
  base::ProcessId mChildProcessId;

  bool mDestroyed;

  bool mIPCOpen;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_LAYERTRANSACTIONPARENT_H
