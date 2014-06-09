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
  LayerTransactionChild* c = new LayerTransactionChild();
  c->AddIPDLReference();
  // We only create PLayerTransaction objects through PRenderFrame when we don't
  // have a PCompositor. This means that the child process content will never
  // get drawn to the screen, but some tests rely on it pretending to function
  // for now.
  c->SetHasNoCompositor();
  return c;
}

bool
RenderFrameChild::DeallocPLayerTransactionChild(PLayerTransactionChild* aLayers)
{
  static_cast<LayerTransactionChild*>(aLayers)->ReleaseIPDLReference();
  return true;
}

}  // namespace layout
}  // namespace mozilla
