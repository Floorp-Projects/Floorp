/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
#define MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H

#include <stdint.h>                     // for uint32_t
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTracker
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

namespace layout {
class RenderFrameChild;
class ShadowLayerForwarder;
}

namespace layers {

class LayerTransactionChild : public PLayerTransactionChild
                            , public AsyncTransactionTrackersHolder
{
  typedef InfallibleTArray<AsyncParentMessageData> AsyncParentMessageArray;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LayerTransactionChild)
  /**
   * Clean this up, finishing with Send__delete__().
   *
   * It is expected (checked with an assert) that all shadow layers
   * created by this have already been destroyed and
   * Send__delete__()d by the time this method is called.
   */
  void Destroy();

  bool IPCOpen() const { return mIPCOpen; }

  void SetForwarder(ShadowLayerForwarder* aForwarder)
  {
    mForwarder = aForwarder;
  }

  virtual void SendFenceHandle(AsyncTransactionTracker* aTracker,
                               PTextureChild* aTexture,
                               const FenceHandle& aFence);

protected:
  LayerTransactionChild()
    : mForwarder(nullptr)
    , mIPCOpen(false)
    , mDestroyed(false)
  {}
  ~LayerTransactionChild() { }

  virtual PLayerChild* AllocPLayerChild() MOZ_OVERRIDE;
  virtual bool DeallocPLayerChild(PLayerChild* actor) MOZ_OVERRIDE;

  virtual PCompositableChild* AllocPCompositableChild(const TextureInfo& aInfo) MOZ_OVERRIDE;
  virtual bool DeallocPCompositableChild(PCompositableChild* actor) MOZ_OVERRIDE;

  virtual PTextureChild* AllocPTextureChild(const SurfaceDescriptor& aSharedData,
                                            const TextureFlags& aFlags) MOZ_OVERRIDE;
  virtual bool DeallocPTextureChild(PTextureChild* actor) MOZ_OVERRIDE;

  virtual bool
  RecvParentAsyncMessages(const InfallibleTArray<AsyncParentMessageData>& aMessages) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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
  friend class CompositorChild;
  friend class layout::RenderFrameChild;

  ShadowLayerForwarder* mForwarder;
  bool mIPCOpen;
  bool mDestroyed;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
