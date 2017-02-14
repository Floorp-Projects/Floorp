/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderDisplayItemLayer.h"

#include "WebRenderLayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "nsDisplayList.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {
namespace layers {

void
WebRenderDisplayItemLayer::RenderLayer()
{
  if (mItem) {
    // We might have recycled this layer. Throw away the old commands.
    mCommands.Clear();
    mItem->CreateWebRenderCommands(mCommands, this);
  }
  // else we have an empty transaction and just use the
  // old commands.

  WrBridge()->AddWebRenderCommands(mCommands);
}

} // namespace layers
} // namespace mozilla
