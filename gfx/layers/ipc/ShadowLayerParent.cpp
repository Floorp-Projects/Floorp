/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayerParent.h"
#include "Layers.h"                     // for Layer, ContainerLayer
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc

#include "mozilla/layers/PaintedLayerComposite.h"
#include "mozilla/layers/CanvasLayerComposite.h"
#include "mozilla/layers/ColorLayerComposite.h"
#include "mozilla/layers/ImageLayerComposite.h"
#include "mozilla/layers/ContainerLayerComposite.h"

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

ContainerLayerComposite*
ShadowLayerParent::AsContainerLayerComposite() const
{
  return mLayer && mLayer->GetType() == Layer::TYPE_CONTAINER
         ? static_cast<ContainerLayerComposite*>(mLayer.get())
         : nullptr;
}

CanvasLayerComposite*
ShadowLayerParent::AsCanvasLayerComposite() const
{
  return mLayer && mLayer->GetType() == Layer::TYPE_CANVAS
         ? static_cast<CanvasLayerComposite*>(mLayer.get())
         : nullptr;
}

ColorLayerComposite*
ShadowLayerParent::AsColorLayerComposite() const
{
  return mLayer && mLayer->GetType() == Layer::TYPE_COLOR
         ? static_cast<ColorLayerComposite*>(mLayer.get())
         : nullptr;
}

ImageLayerComposite*
ShadowLayerParent::AsImageLayerComposite() const
{
  return mLayer && mLayer->GetType() == Layer::TYPE_IMAGE
         ? static_cast<ImageLayerComposite*>(mLayer.get())
         : nullptr;
}

RefLayerComposite*
ShadowLayerParent::AsRefLayerComposite() const
{
  return mLayer && mLayer->GetType() == Layer::TYPE_REF
         ? static_cast<RefLayerComposite*>(mLayer.get())
         : nullptr;
}

PaintedLayerComposite*
ShadowLayerParent::AsPaintedLayerComposite() const
{
  return mLayer && mLayer->GetType() == Layer::TYPE_PAINTED
         ? static_cast<PaintedLayerComposite*>(mLayer.get())
         : nullptr;
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
