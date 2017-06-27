/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderDisplayItemLayer.h"

#include "LayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "nsDisplayList.h"
#include "mozilla/gfx/Matrix.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

WebRenderDisplayItemLayer::~WebRenderDisplayItemLayer()
{
  MOZ_COUNT_DTOR(WebRenderDisplayItemLayer);
}

void
WebRenderDisplayItemLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                       const StackingContextHelper& aSc)
{
  if (mVisibleRegion.IsEmpty()) {
    return;
  }

  ScrollingLayersHelper scroller(this, aBuilder, aSc);

  if (mItem) {
    wr::WrSize contentSize; // this won't actually be used by anything
    wr::DisplayListBuilder builder(WrBridge()->GetPipeline(), contentSize);
    // We might have recycled this layer. Throw away the old commands.
    mParentCommands.Clear();

    mItem->CreateWebRenderCommands(builder, aSc, mParentCommands, WrManager(),
                                   GetDisplayListBuilder());
    builder.Finalize(contentSize, mBuiltDisplayList);
  } else {
    // else we have an empty transaction and just use the
    // old commands.
    WebRenderLayerManager* manager = WrManager();
    MOZ_ASSERT(manager);

    // Since our recording relies on our parent layer's transform and stacking context
    // If this layer or our parent changed, this empty transaction won't work.
    if (manager->IsMutatedLayer(this) || manager->IsMutatedLayer(GetParent())) {
      manager->SetTransactionIncomplete();
      return;
    }
  }

  aBuilder.PushBuiltDisplayList(mBuiltDisplayList);
  WrBridge()->AddWebRenderParentCommands(mParentCommands);
}

} // namespace layers
} // namespace mozilla
