/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderFrameChild.h"
#include "mozilla/layers/LayerTransactionChild.h"

using mozilla::layers::PLayerTransactionChild;
using mozilla::layers::LayerTransactionChild;

namespace mozilla {
namespace layout {

void
RenderFrameChild::Destroy()
{
  size_t numChildren = ManagedPLayerTransactionChild().Length();
  NS_ABORT_IF_FALSE(0 == numChildren || 1 == numChildren,
                    "render frame must only have 0 or 1 layer forwarder");

  if (numChildren) {
    LayerTransactionChild* layers =
      static_cast<LayerTransactionChild*>(ManagedPLayerTransactionChild()[0]);
    layers->Destroy();
    // |layers| was just deleted, take care
  }

  Send__delete__(this);
  // WARNING: |this| is dead, hands off
}

PLayerTransactionChild*
RenderFrameChild::AllocPLayerTransactionChild()
{
  return new LayerTransactionChild();
}

bool
RenderFrameChild::DeallocPLayerTransactionChild(PLayerTransactionChild* aLayers)
{
  delete aLayers;
  return true;
}

}  // namespace layout
}  // namespace mozilla
