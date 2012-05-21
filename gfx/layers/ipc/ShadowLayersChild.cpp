/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayerChild.h"
#include "ShadowLayersChild.h"

namespace mozilla {
namespace layers {

void
ShadowLayersChild::Destroy()
{
  NS_ABORT_IF_FALSE(0 == ManagedPLayerChild().Length(),
                    "layers should have been cleaned up by now");
  PLayersChild::Send__delete__(this);
  // WARNING: |this| has gone to the great heap in the sky
}

PLayerChild*
ShadowLayersChild::AllocPLayer()
{
  // we always use the "power-user" ctor
  NS_RUNTIMEABORT("not reached");
  return NULL;
}

bool
ShadowLayersChild::DeallocPLayer(PLayerChild* actor)
{
  delete actor;
  return true;
}

}  // namespace layers
}  // namespace mozilla
