/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientThebesLayer.h"
#include "ClientTiledThebesLayer.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

static void
SetAntialiasingFlags(Layer* aLayer, gfxContext* aTarget)
{
  if (!aTarget->IsCairo()) {
    RefPtr<DrawTarget> dt = aTarget->GetDrawTarget();

    if (dt->GetFormat() != FORMAT_B8G8R8A8) {
      return;
    }

    const nsIntRect& bounds = aLayer->GetVisibleRegion().GetBounds();
    gfx::Rect transformedBounds = dt->GetTransform().TransformBounds(gfx::Rect(Float(bounds.x), Float(bounds.y),
                                                                     Float(bounds.width), Float(bounds.height)));
    transformedBounds.RoundOut();
    IntRect intTransformedBounds;
    transformedBounds.ToIntRect(&intTransformedBounds);
    dt->SetPermitSubpixelAA(!(aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) ||
                            dt->GetOpaqueRect().Contains(intTransformedBounds));
  } else {
    nsRefPtr<gfxASurface> surface = aTarget->CurrentSurface();
    if (surface->GetContentType() != gfxASurface::CONTENT_COLOR_ALPHA) {
      // Destination doesn't have alpha channel; no need to set any special flags
      return;
    }

    const nsIntRect& bounds = aLayer->GetVisibleRegion().GetBounds();
    surface->SetSubpixelAntialiasingEnabled(
        !(aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) ||
        surface->GetOpaqueRect().Contains(
          aTarget->UserToDevice(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height))));
  }
}

void
ClientThebesLayer::PaintThebes()
{
  PROFILER_LABEL("ClientThebesLayer", "PaintThebes");
  NS_ASSERTION(ClientManager()->InDrawing(),
               "Can only draw in drawing phase");
  
  //TODO: This is going to copy back pixels that we might end up
  // drawing over anyway. It would be nice if we could avoid
  // this duplication.
  mContentClient->SyncFrontBufferToBackBuffer();

  bool canUseOpaqueSurface = CanUseOpaqueSurface();
  ContentType contentType =
    canUseOpaqueSurface ? gfxASurface::CONTENT_COLOR :
                          gfxASurface::CONTENT_COLOR_ALPHA;

  {
    uint32_t flags = 0;
#ifndef MOZ_WIDGET_ANDROID
    if (ClientManager()->CompositorMightResample()) {
      flags |= ThebesLayerBuffer::PAINT_WILL_RESAMPLE;
    }
    if (!(flags & ThebesLayerBuffer::PAINT_WILL_RESAMPLE)) {
      if (MayResample()) {
        flags |= ThebesLayerBuffer::PAINT_WILL_RESAMPLE;
      }
    }
#endif
    PaintState state =
      mContentClient->BeginPaintBuffer(this, contentType, flags);
    mValidRegion.Sub(mValidRegion, state.mRegionToInvalidate);

    if (state.mContext) {
      // The area that became invalid and is visible needs to be repainted
      // (this could be the whole visible area if our buffer switched
      // from RGB to RGBA, because we might need to repaint with
      // subpixel AA)
      state.mRegionToInvalidate.And(state.mRegionToInvalidate,
                                    GetEffectiveVisibleRegion());
      nsIntRegion extendedDrawRegion = state.mRegionToDraw;
      SetAntialiasingFlags(this, state.mContext);

      PaintBuffer(state.mContext,
                  state.mRegionToDraw, extendedDrawRegion, state.mRegionToInvalidate,
                  state.mDidSelfCopy);
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PaintThebes", this));
      Mutated();
    } else {
      // It's possible that state.mRegionToInvalidate is nonempty here,
      // if we are shrinking the valid region to nothing. So use mRegionToDraw
      // instead.
      NS_WARN_IF_FALSE(state.mRegionToDraw.IsEmpty(),
                       "No context when we have something to draw, resource exhaustion?");
    }
  }
}

void
ClientThebesLayer::RenderLayer()
{
  if (GetMaskLayer()) {
    ToClientLayer(GetMaskLayer())->RenderLayer();
  }
  
  if (!mContentClient) {
    mContentClient = ContentClient::CreateContentClient(ClientManager());
    if (!mContentClient) {
      return;
    }
    mContentClient->Connect();
    ClientManager()->Attach(mContentClient, this);
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  mContentClient->BeginPaint();
  PaintThebes();
  mContentClient->EndPaint();
}

void
ClientThebesLayer::PaintBuffer(gfxContext* aContext,
                               const nsIntRegion& aRegionToDraw,
                               const nsIntRegion& aExtendedRegionToDraw,
                               const nsIntRegion& aRegionToInvalidate,
                               bool aDidSelfCopy)
{
  ContentClientRemote* contentClientRemote = static_cast<ContentClientRemote*>(mContentClient.get());
  MOZ_ASSERT(contentClientRemote->GetIPDLActor());

  // NB: this just throws away the entire valid region if there are
  // too many rects.
  mValidRegion.SimplifyInward(8);

  if (!ClientManager()->GetThebesLayerCallback()) {
    ClientManager()->SetTransactionIncomplete();
    return;
  }
  ClientManager()->GetThebesLayerCallback()(this, 
                                            aContext, 
                                            aExtendedRegionToDraw, 
                                            aRegionToInvalidate,
                                            ClientManager()->GetThebesLayerCallbackData());

  // Everything that's visible has been validated. Do this instead of just
  // OR-ing with aRegionToDraw, since that can lead to a very complex region
  // here (OR doesn't automatically simplify to the simplest possible
  // representation of a region.)
  nsIntRegion tmp;
  tmp.Or(mVisibleRegion, aExtendedRegionToDraw);
  mValidRegion.Or(mValidRegion, tmp);

  // Hold(this) ensures this layer is kept alive through the current transaction
  // The ContentClient assumes this layer is kept alive (e.g., in CreateBuffer,
  // DestroyThebesBuffer), so deleting this Hold for whatever reason will break things.
  ClientManager()->Hold(this);
  contentClientRemote->Updated(aRegionToDraw,
                               mVisibleRegion,
                               aDidSelfCopy);
}

already_AddRefed<ThebesLayer>
ClientLayerManager::CreateThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
#ifdef FORCE_BASICTILEDTHEBESLAYER
  if (GetCompositorBackendType() == LAYERS_OPENGL) {
    nsRefPtr<ClientTiledThebesLayer> layer =
      new ClientTiledThebesLayer(this);
    CREATE_SHADOW(Thebes);
    return layer.forget();
  } else
#endif
  {
    nsRefPtr<ClientThebesLayer> layer =
      new ClientThebesLayer(this);
    CREATE_SHADOW(Thebes);
    return layer.forget();
  }
}


}
}
