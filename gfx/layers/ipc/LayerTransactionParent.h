/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_LAYERTRANSACTIONPARENT_H
#define MOZILLA_LAYERS_LAYERTRANSACTIONPARENT_H

#include "mozilla/layers/PLayerTransactionParent.h"
#include "ShadowLayers.h"
#include "ShadowLayersManager.h"
#include "CompositableTransactionParent.h"

namespace mozilla {

namespace layout {
class RenderFrameParent;
}

namespace layers {

class Layer;
class LayerManagerComposite;
class ShadowLayerParent;
class CompositableParent;

class LayerTransactionParent : public PLayerTransactionParent,
                               public CompositableParentManager
{
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;
  typedef InfallibleTArray<Edit> EditArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;

public:
  LayerTransactionParent(LayerManagerComposite* aManager,
                         ShadowLayersManager* aLayersManager,
                         uint64_t aId);
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


protected:
  virtual bool RecvUpdate(const EditArray& cset,
                          const TargetConfig& targetConfig,
                          const bool& isFirstPaint,
                          EditReplyArray* reply) MOZ_OVERRIDE;

  virtual bool RecvUpdateNoSwap(const EditArray& cset,
                                const TargetConfig& targetConfig,
                                const bool& isFirstPaint) MOZ_OVERRIDE;

  virtual bool RecvClearCachedResources() MOZ_OVERRIDE;
  virtual bool RecvGetOpacity(PLayerParent* aParent,
                              float* aOpacity) MOZ_OVERRIDE;
  virtual bool RecvGetTransform(PLayerParent* aParent,
                                gfx3DMatrix* aTransform) MOZ_OVERRIDE;

  virtual PGrallocBufferParent*
  AllocPGrallocBufferParent(const gfxIntSize& aSize,
                      const uint32_t& aFormat, const uint32_t& aUsage,
                      MaybeMagicGrallocBufferHandle* aOutHandle) MOZ_OVERRIDE;
  virtual bool
  DeallocPGrallocBufferParent(PGrallocBufferParent* actor) MOZ_OVERRIDE;

  virtual PLayerParent* AllocPLayerParent() MOZ_OVERRIDE;
  virtual bool DeallocPLayerParent(PLayerParent* actor) MOZ_OVERRIDE;

  virtual PCompositableParent* AllocPCompositableParent(const TextureInfo& aInfo) MOZ_OVERRIDE;
  virtual bool DeallocPCompositableParent(PCompositableParent* actor) MOZ_OVERRIDE;

  void Attach(ShadowLayerParent* aLayerParent, CompositableParent* aCompositable);

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
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_LAYERTRANSACTIONPARENT_H
