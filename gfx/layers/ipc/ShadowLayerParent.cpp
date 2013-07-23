/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerTransactionParent.h"
#include "ShadowLayerParent.h"
#include "ShadowLayers.h"

#include "BasicLayers.h"

namespace mozilla {
namespace layers {

ShadowLayerParent::ShadowLayerParent() : mLayer(nullptr)
{
}

ShadowLayerParent::~ShadowLayerParent()
{
}

void
ShadowLayerParent::Bind(Layer* layer)
{
  mLayer = layer;
}

void
ShadowLayerParent::Destroy()
{
  // It's possible for Destroy() to come in just after this has been
  // created, but just before the transaction in which Bind() would
  // have been called.  In that case, we'll ignore shadow-layers
  // transactions from there on and never get a layer here.
  if (mLayer) {
    mLayer->Disconnect();
  }
}

ContainerLayer*
ShadowLayerParent::AsContainer() const
{
  return static_cast<ContainerLayer*>(AsLayer());
}

void
ShadowLayerParent::ActorDestroy(ActorDestroyReason why)
{
  switch (why) {
  case AncestorDeletion:
    NS_RUNTIMEABORT("shadow layer deleted out of order!");
    return;                     // unreached

  case Deletion:
    // See comment near Destroy() above.
    if (mLayer) {
      mLayer->Disconnect();
    }
    break;

  case AbnormalShutdown:
    if (mLayer) {
      mLayer->Disconnect();
    }
    break;

  case NormalShutdown:
    // let IPDL-generated code automatically clean up Shmems and so
    // forth; our channel is disconnected anyway
    break;

  case FailedConstructor:
    NS_RUNTIMEABORT("FailedConstructor isn't possible in PLayerTransaction");
    return;                     // unreached
  }

  mLayer = nullptr;
}

} // namespace layers
} // namespace mozilla
