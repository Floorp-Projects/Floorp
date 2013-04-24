/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
#define MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H

#include "mozilla/layers/PLayerTransactionChild.h"

namespace mozilla {
namespace layers {

class LayerTransactionChild : public PLayerTransactionChild
{
public:
  LayerTransactionChild() { }
  ~LayerTransactionChild() { }

  /**
   * Clean this up, finishing with Send__delete__().
   *
   * It is expected (checked with an assert) that all shadow layers
   * created by this have already been destroyed and
   * Send__delete__()d by the time this method is called.
   */
  void Destroy();

protected:
  virtual PGrallocBufferChild*
  AllocPGrallocBuffer(const gfxIntSize&, const gfxContentType&,
                      MaybeMagicGrallocBufferHandle*) MOZ_OVERRIDE;
  virtual bool
  DeallocPGrallocBuffer(PGrallocBufferChild* actor) MOZ_OVERRIDE;

  virtual PLayerChild* AllocPLayer() MOZ_OVERRIDE;
  virtual bool DeallocPLayer(PLayerChild* actor) MOZ_OVERRIDE;

  virtual PCompositableChild* AllocPCompositable(const TextureInfo& aInfo) MOZ_OVERRIDE;
  virtual bool DeallocPCompositable(PCompositableChild* actor) MOZ_OVERRIDE;
  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
