/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayerChild.h"
#include "Layers.h"
#include "ShadowLayers.h"

namespace mozilla {
namespace layers {

ShadowLayerChild::ShadowLayerChild(ShadowableLayer* aLayer)
  : mLayer(aLayer)
{ }

ShadowLayerChild::~ShadowLayerChild()
{ }

void
ShadowLayerChild::ActorDestroy(ActorDestroyReason why)
{
  NS_ABORT_IF_FALSE(AncestorDeletion != why,
                    "shadowable layer should have been cleaned up by now");

  if (AbnormalShutdown == why) {
    // This is last-ditch emergency shutdown.  Just have the layer
    // forget its IPDL resources; IPDL-generated code will clean up
    // automatically in this case.
    mLayer->AsLayer()->Disconnect();
    mLayer = nullptr;
  }
}

}  // namespace layers
}  // namespace mozilla
