/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientThebesLayer.h"
#include "ClientTiledThebesLayer.h"     // for ClientTiledThebesLayer
#include <stdint.h>                     // for uint32_t
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "client/ClientLayerManager.h"  // for ClientLayerManager, etc
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxContext.h"                 // for gfxContext
#include "gfxRect.h"                    // for gfxRect
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

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

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
    canUseOpaqueSurface ? GFX_CONTENT_COLOR :
                          GFX_CONTENT_COLOR_ALPHA;

  {
    uint32_t flags = 0;
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
                  state.mDidSelfCopy, state.mClip);
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
  // It is very important that this is called after EndPaint, because destroying
  // textures is a three stage process:
  // 1. We are done with the buffer and move it to ContentClient::mOldTextures,
  // that happens in DestroyBuffers which is may be called indirectly from
  // PaintThebes.
  // 2. The content client calls RemoveTextureClient on the texture clients in
  // mOldTextures and forgets them. They then become invalid. The compositable
  // client keeps a record of IDs. This happens in EndPaint.
  // 3. An IPC message is sent to destroy the corresponding texture host. That
  // happens from OnTransaction.
  // It is important that these steps happen in order.
  mContentClient->OnTransaction();
}

void
ClientThebesLayer::PaintBuffer(gfxContext* aContext,
                               const nsIntRegion& aRegionToDraw,
                               const nsIntRegion& aExtendedRegionToDraw,
                               const nsIntRegion& aRegionToInvalidate,
                               bool aDidSelfCopy, DrawRegionClip aClip)
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
                                            aClip,
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
      gfxPlatform::GetPrefLayersEnableTiles() && AsShadowForwarder()->GetCompositorBackendType() == LAYERS_OPENGL) {
    nsRefPtr<ClientTiledThebesLayer> layer =
      new ClientTiledThebesLayer(this);
    CREATE_SHADOW(Thebes);
    return layer.forget();
  } else
  {
    nsRefPtr<ClientThebesLayer> layer =
      new ClientThebesLayer(this);
    CREATE_SHADOW(Thebes);
    return layer.forget();
  }
}


}
}
