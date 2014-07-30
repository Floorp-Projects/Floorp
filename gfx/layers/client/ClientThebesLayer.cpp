/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientThebesLayer.h"
#include "ClientTiledThebesLayer.h"     // for ClientTiledThebesLayer
#include "SimpleTiledContentClient.h"
#include <stdint.h>                     // for uint32_t
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "client/ClientLayerManager.h"  // for ClientLayerManager, etc
#include "gfxContext.h"                 // for gfxContext
#include "gfxRect.h"                    // for gfxRect
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/Matrix.h"         // for Matrix
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for Float, etc
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Preferences.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "gfx2DGlue.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
ClientThebesLayer::PaintThebes()
{
  PROFILER_LABEL("ClientThebesLayer", "PaintThebes",
    js::ProfileEntry::Category::GRAPHICS);

  NS_ASSERTION(ClientManager()->InDrawing(),
               "Can only draw in drawing phase");
  
  uint32_t flags = RotatedContentBuffer::PAINT_CAN_DRAW_ROTATED;
#ifndef MOZ_WIDGET_ANDROID
  if (ClientManager()->CompositorMightResample()) {
    flags |= RotatedContentBuffer::PAINT_WILL_RESAMPLE;
  }
  if (!(flags & RotatedContentBuffer::PAINT_WILL_RESAMPLE)) {
    if (MayResample()) {
      flags |= RotatedContentBuffer::PAINT_WILL_RESAMPLE;
    }
  }
#endif
  PaintState state =
    mContentClient->BeginPaintBuffer(this, flags);
  mValidRegion.Sub(mValidRegion, state.mRegionToInvalidate);

  if (!state.mRegionToDraw.IsEmpty() && !ClientManager()->GetThebesLayerCallback()) {
    ClientManager()->SetTransactionIncomplete();
    return;
  }

  // The area that became invalid and is visible needs to be repainted
  // (this could be the whole visible area if our buffer switched
  // from RGB to RGBA, because we might need to repaint with
  // subpixel AA)
  state.mRegionToInvalidate.And(state.mRegionToInvalidate,
                                GetEffectiveVisibleRegion());

  bool didUpdate = false;
  RotatedContentBuffer::DrawIterator iter;
  while (DrawTarget* target = mContentClient->BorrowDrawTargetForPainting(state, &iter)) {
    SetAntialiasingFlags(this, target);

    nsRefPtr<gfxContext> ctx = gfxContext::ContextForDrawTarget(target);

    ClientManager()->GetThebesLayerCallback()(this,
                                              ctx,
                                              iter.mDrawRegion,
                                              state.mClip,
                                              state.mRegionToInvalidate,
                                              ClientManager()->GetThebesLayerCallbackData());

    ctx = nullptr;
    mContentClient->ReturnDrawTargetToBuffer(target);
    didUpdate = true;
  }

  if (didUpdate) {
    Mutated();

    mValidRegion.Or(mValidRegion, state.mRegionToDraw);

    ContentClientRemote* contentClientRemote = static_cast<ContentClientRemote*>(mContentClient.get());
    MOZ_ASSERT(contentClientRemote->GetIPDLActor());

    // Hold(this) ensures this layer is kept alive through the current transaction
    // The ContentClient assumes this layer is kept alive (e.g., in CreateBuffer),
    // so deleting this Hold for whatever reason will break things.
    ClientManager()->Hold(this);
    contentClientRemote->Updated(state.mRegionToDraw,
                                 mVisibleRegion,
                                 state.mDidSelfCopy);
  }
}

void
ClientThebesLayer::RenderLayer()
{
  if (GetMaskLayer()) {
    ToClientLayer(GetMaskLayer())->RenderLayer();
  }
  
  if (!mContentClient) {
    mContentClient = ContentClient::CreateContentClient(ClientManager()->AsShadowForwarder());
    if (!mContentClient) {
      return;
    }
    mContentClient->Connect();
    ClientManager()->AsShadowForwarder()->Attach(mContentClient, this);
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  mContentClient->BeginPaint();
  PaintThebes();
  mContentClient->EndPaint();
}

bool
ClientLayerManager::IsOptimizedFor(ThebesLayer* aLayer, ThebesLayerCreationHint aHint)
{
#ifdef MOZ_B2G
  // The only creation hint is whether the layer is scrollable or not, and this
  // is only respected on B2G, where it's used to determine whether to use
  // tiled layers or not.
  // There are pretty nasty performance consequences for not using tiles on
  // large, scrollable layers, so we want the layer to be recreated in this
  // situation.
  return aHint == aLayer->GetCreationHint();
#else
  return LayerManager::IsOptimizedFor(aLayer, aHint);
#endif
}

already_AddRefed<ThebesLayer>
ClientLayerManager::CreateThebesLayer()
{
  return CreateThebesLayerWithHint(NONE);
}

already_AddRefed<ThebesLayer>
ClientLayerManager::CreateThebesLayerWithHint(ThebesLayerCreationHint aHint)
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  if (
#ifdef MOZ_B2G
      aHint == SCROLLABLE &&
#endif
      gfxPrefs::LayersTilesEnabled() &&
      (AsShadowForwarder()->GetCompositorBackendType() == LayersBackend::LAYERS_OPENGL ||
       AsShadowForwarder()->GetCompositorBackendType() == LayersBackend::LAYERS_D3D9 ||
       AsShadowForwarder()->GetCompositorBackendType() == LayersBackend::LAYERS_D3D11)) {
    if (gfxPrefs::LayersUseSimpleTiles()) {
      nsRefPtr<SimpleClientTiledThebesLayer> layer =
        new SimpleClientTiledThebesLayer(this, aHint);
      CREATE_SHADOW(Thebes);
      return layer.forget();
    } else {
      nsRefPtr<ClientTiledThebesLayer> layer =
        new ClientTiledThebesLayer(this, aHint);
      CREATE_SHADOW(Thebes);
      return layer.forget();
    }
  } else
  {
    nsRefPtr<ClientThebesLayer> layer =
      new ClientThebesLayer(this, aHint);
    CREATE_SHADOW(Thebes);
    return layer.forget();
  }
}


}
}
