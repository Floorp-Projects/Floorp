/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
#define MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H

#include <stdint.h>              // for uint32_t
#include "mozilla/Attributes.h"  // for override
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

namespace layers {

class ShadowLayerForwarder;

class LayerTransactionChild : public PLayerTransactionChild {
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

  void SetForwarder(ShadowLayerForwarder* aForwarder) {
    mForwarder = aForwarder;
  }

  LayersId GetId() const { return mId; }

  void MarkDestroyed() { mDestroyed = true; }

 protected:
  explicit LayerTransactionChild(const LayersId& aId)
      : mForwarder(nullptr), mIPCOpen(false), mDestroyed(false), mId(aId) {}
  virtual ~LayerTransactionChild() = default;

  void ActorDestroy(ActorDestroyReason why) override;

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

  ShadowLayerForwarder* mForwarder;
  bool mIPCOpen;
  bool mDestroyed;
  LayersId mId;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
