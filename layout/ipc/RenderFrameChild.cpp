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
RenderFrameChild::ActorDestroy(ActorDestroyReason why)
{
  mWasDestroyed = true;
}

void
RenderFrameChild::Destroy()
{
  if (mWasDestroyed) {
    return;
  }

  Send__delete__(this);
  // WARNING: |this| is dead, hands off
}

} // namespace layout
} // namespace mozilla
