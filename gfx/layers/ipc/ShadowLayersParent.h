/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayersParent_h
#define mozilla_layers_ShadowLayersParent_h

#include "mozilla/layers/PLayersParent.h"
#include "ShadowLayers.h"
#include "ShadowLayersManager.h"
#include "CompositableTransactionParent.h"

namespace mozilla {

namespace layout {
class RenderFrameParent;
}

namespace layers {

class Layer;
class ShadowLayerManager;
class ShadowLayerParent;
class CompositableParent;

class ShadowLayersParent : public PLayersParent,
                           public CompositableParentManager
{
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;
  typedef InfallibleTArray<Edit> EditArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;

public:
  ShadowLayersParent(ShadowLayerManager* aManager,
                     ShadowLayersManager* aLayersManager,
                     uint64_t aId);
  ~ShadowLayersParent();

  void Destroy();

  ShadowLayerManager* layer_manager() const { return mLayerManager; }

  uint64_t GetId() const { return mId; }
  ContainerLayer* GetRoot() const { return mRoot; }

  // ISurfaceAllocator
  virtual bool AllocShmem(size_t aSize,
                          ipc::SharedMemory::SharedMemoryType aType,
                          ipc::Shmem* aShmem) {
    return PLayersParent::AllocShmem(aSize, aType, aShmem);
  }

  virtual bool AllocUnsafeShmem(size_t aSize,
                                ipc::SharedMemory::SharedMemoryType aType,
                                ipc::Shmem* aShmem) {
    return PLayersParent::AllocUnsafeShmem(aSize, aType, aShmem);
  }

  virtual void DeallocShmem(ipc::Shmem& aShmem) MOZ_OVERRIDE
  {
    PLayersParent::DeallocShmem(aShmem);
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

  virtual PGrallocBufferParent*
  AllocPGrallocBuffer(const gfxIntSize& aSize, const gfxContentType& aContent,
                      MaybeMagicGrallocBufferHandle* aOutHandle) MOZ_OVERRIDE;
  virtual bool
  DeallocPGrallocBuffer(PGrallocBufferParent* actor) MOZ_OVERRIDE;

  virtual PLayerParent* AllocPLayer() MOZ_OVERRIDE;
  virtual bool DeallocPLayer(PLayerParent* actor) MOZ_OVERRIDE;

  virtual PCompositableParent* AllocPCompositable(const TextureInfo& aInfo) MOZ_OVERRIDE;
  virtual bool DeallocPCompositable(PCompositableParent* actor) MOZ_OVERRIDE;
  
  void Attach(ShadowLayerParent* aLayerParent, CompositableParent* aCompositable);

private:
  nsRefPtr<ShadowLayerManager> mLayerManager;
  ShadowLayersManager* mShadowLayersManager;
  // Hold the root because it might be grafted under various
  // containers in the "real" layer tree
  nsRefPtr<ContainerLayer> mRoot;
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

#endif // ifndef mozilla_layers_ShadowLayersParent_h
