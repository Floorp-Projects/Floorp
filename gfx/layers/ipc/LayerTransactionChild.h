/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
#define MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H

#include <stdint.h>                     // for uint32_t
#include "mozilla/Attributes.h"         // for override
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTracker
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

namespace layout {
class RenderFrameChild;
} // namespace layout

namespace layers {

class ShadowLayerForwarder;

class LayerTransactionChild : public PLayerTransactionChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LayerTransactionChild)
  /**
   * Clean this up, finishing with SendShutDown() which will cause __delete__
   * to be sent from the parent side.
   *
   * It is expected (checked with an assert) that all shadow layers
   * created by this have already been destroyed and
   * Send__delete__()d by the time this method is called.
   */
  void Destroy();

  bool IPCOpen() const { return mIPCOpen && !mDestroyed; }
  bool IsDestroyed() const { return mDestroyed; }

  void SetForwarder(ShadowLayerForwarder* aForwarder)
  {
    mForwarder = aForwarder;
  }

  uint64_t GetId() const { return mId; }

protected:
  explicit LayerTransactionChild(const uint64_t& aId)
    : mForwarder(nullptr)
    , mIPCOpen(false)
    , mDestroyed(false)
    , mId(aId)
  {}
  ~LayerTransactionChild() { }

  virtual PLayerChild* AllocPLayerChild() override;
  virtual bool DeallocPLayerChild(PLayerChild* actor) override;

  virtual PCompositableChild* AllocPCompositableChild(const TextureInfo& aInfo) override;
  virtual bool DeallocPCompositableChild(PCompositableChild* actor) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

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
  friend class CompositorBridgeChild;
  friend class layout::RenderFrameChild;

  ShadowLayerForwarder* mForwarder;
  bool mIPCOpen;
  bool mDestroyed;
  uint64_t mId;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
