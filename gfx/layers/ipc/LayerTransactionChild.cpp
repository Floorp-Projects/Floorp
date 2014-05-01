/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerTransactionChild.h"
#include "mozilla/layers/CompositableClient.h"  // for CompositableChild
#include "mozilla/layers/PCompositableChild.h"  // for PCompositableChild
#include "mozilla/layers/PLayerChild.h"  // for PLayerChild
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsDebug.h"                    // for NS_RUNTIMEABORT, etc
#include "nsTArray.h"                   // for nsTArray
#include "mozilla/layers/TextureClient.h"

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
  return CompositableClient::CreateIPDLActor();
}

bool
LayerTransactionChild::DeallocPCompositableChild(PCompositableChild* actor)
{
  return CompositableClient::DestroyIPDLActor(actor);
}

void
LayerTransactionChild::ActorDestroy(ActorDestroyReason why)
{
#ifdef MOZ_B2G
  // Due to poor lifetime management of gralloc (and possibly shmems) we will
  // crash at some point in the future when we get destroyed due to abnormal
  // shutdown. Its better just to crash here. On desktop though, we have a chance
  // of recovering.
  if (why == AbnormalShutdown) {
    NS_RUNTIMEABORT("ActorDestroy by IPC channel failure at LayerTransactionChild");
  }
#endif
}

PTextureChild*
LayerTransactionChild::AllocPTextureChild(const SurfaceDescriptor&,
                                          const TextureFlags&)
{
  return TextureClient::CreateIPDLActor();
}

bool
LayerTransactionChild::DeallocPTextureChild(PTextureChild* actor)
{
  return TextureClient::DestroyIPDLActor(actor);
}

}  // namespace layers
}  // namespace mozilla
