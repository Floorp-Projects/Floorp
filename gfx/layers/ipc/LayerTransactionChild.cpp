/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerTransactionChild.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/CompositableChild.h"
#include "mozilla/layers/PCompositableChild.h"  // for PCompositableChild
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsDebug.h"                    // for NS_RUNTIMEABORT, etc
#include "nsTArray.h"                   // for nsTArray
#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace layers {


void
LayerTransactionChild::Destroy()
{
  if (!IPCOpen()) {
    return;
  }
  // mDestroyed is used to prevent calling Send__delete__() twice.
  // When this function is called from CompositorBridgeChild::Destroy(),
  // under Send__delete__() call, this function is called from
  // ShadowLayerForwarder's destructor.
  // When it happens, IPCOpen() is still true.
  // See bug 1004191.
  mDestroyed = true;

  SendShutdown();
}


PCompositableChild*
LayerTransactionChild::AllocPCompositableChild(const TextureInfo& aInfo)
{
  MOZ_ASSERT(!mDestroyed);
  return CompositableChild::CreateActor();
}

bool
LayerTransactionChild::DeallocPCompositableChild(PCompositableChild* actor)
{
  CompositableChild::DestroyActor(actor);
  return true;
}

void
LayerTransactionChild::ActorDestroy(ActorDestroyReason why)
{
  mDestroyed = true;
}

} // namespace layers
} // namespace mozilla
