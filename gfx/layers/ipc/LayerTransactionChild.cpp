/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayerChild.h"
#include "LayerTransactionChild.h"
#include "ShadowLayerUtils.h"
#include "mozilla/layers/CompositableClient.h"

namespace mozilla {
namespace layers {

void
LayerTransactionChild::Destroy()
{
  NS_ABORT_IF_FALSE(0 == ManagedPLayerChild().Length(),
                    "layers should have been cleaned up by now");
  PLayerTransactionChild::Send__delete__(this);
  // WARNING: |this| has gone to the great heap in the sky
}

PGrallocBufferChild*
LayerTransactionChild::AllocPGrallocBufferChild(const gfxIntSize&,
                                           const uint32_t&,
                                           const uint32_t&,
                                           MaybeMagicGrallocBufferHandle*)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  return GrallocBufferActor::Create();
#else
  NS_RUNTIMEABORT("No gralloc buffers for you");
  return nullptr;
#endif
}

bool
LayerTransactionChild::DeallocPGrallocBufferChild(PGrallocBufferChild* actor)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  delete actor;
  return true;
#else
  NS_RUNTIMEABORT("Um, how did we get here?");
  return false;
#endif
}

PLayerChild*
LayerTransactionChild::AllocPLayerChild()
{
  // we always use the "power-user" ctor
  NS_RUNTIMEABORT("not reached");
  return nullptr;
}

bool
LayerTransactionChild::DeallocPLayerChild(PLayerChild* actor)
{
  delete actor;
  return true;
}

PCompositableChild*
LayerTransactionChild::AllocPCompositableChild(const TextureInfo& aInfo)
{
  return new CompositableChild();
}

bool
LayerTransactionChild::DeallocPCompositableChild(PCompositableChild* actor)
{
  delete actor;
  return true;
}

void
LayerTransactionChild::ActorDestroy(ActorDestroyReason why)
{
  if (why == AbnormalShutdown) {
    NS_RUNTIMEABORT("ActorDestroy by IPC channel failure at LayerTransactionChild");
  }
}

}  // namespace layers
}  // namespace mozilla
