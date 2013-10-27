/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
#define MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H

#include <stdint.h>                     // for uint32_t
#include "gfxPoint.h"                   // for gfxIntSize
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/ipc/ProtocolUtils.h"
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
  AllocPGrallocBufferChild(const gfxIntSize&,
                      const uint32_t&, const uint32_t&,
                      MaybeMagicGrallocBufferHandle*) MOZ_OVERRIDE;
  virtual bool
  DeallocPGrallocBufferChild(PGrallocBufferChild* actor) MOZ_OVERRIDE;

  virtual PLayerChild* AllocPLayerChild() MOZ_OVERRIDE;
  virtual bool DeallocPLayerChild(PLayerChild* actor) MOZ_OVERRIDE;

  virtual PCompositableChild* AllocPCompositableChild(const TextureInfo& aInfo) MOZ_OVERRIDE;
  virtual bool DeallocPCompositableChild(PCompositableChild* actor) MOZ_OVERRIDE;
  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_LAYERTRANSACTIONCHILD_H
