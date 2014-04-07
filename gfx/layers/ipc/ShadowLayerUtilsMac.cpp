/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/gfx/Point.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/CompositorTypes.h"

#include "gfx2DGlue.h"
#include "gfxPlatform.h"

#include "gfxSharedQuartzSurface.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
}

/*static*/ void
LayerManagerComposite::PlatformSyncBeforeReplyUpdate()
{
}

/*static*/ bool
LayerManagerComposite::SupportsDirectTexturing()
{
  return false;
}


} // namespace layers
} // namespace mozilla
