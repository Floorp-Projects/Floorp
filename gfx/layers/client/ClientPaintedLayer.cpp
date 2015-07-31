/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientPaintedLayer.h"
#include "ClientTiledPaintedLayer.h"     // for ClientTiledPaintedLayer
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
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "gfx2DGlue.h"
#include "ReadbackProcessor.h"

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
ClientPaintedLayer::PaintThebes()
{
#ifdef XP_WIN
  if (gfxWindowsPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    // If our rendering device has reset simply avoid rendering completely.
    return;
  }
#endif

  PROFILER_LABEL("ClientPaintedLayer", "PaintThebes",
    js::ProfileEntry::Category::GRAPHICS);

  NS_ASSERTION(ClientManager()->InDrawing(),
               "Can only draw in drawing phase");
  
  uint32_t flags = RotatedContentBuffer::PAINT_CAN_DRAW_ROTATED;
#ifndef MOZ_IGNORE_PAINT_WILL_RESAMPLE
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

  if (!state.mRegionToDraw.IsEmpty() && !ClientManager()->GetPaintedLayerCallback()) {
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

    ClientManager()->GetPaintedLayerCallback()(this,
                                              ctx,
                                              iter.mDrawRegion,
                                              state.mRegionToDraw,
                                              state.mClip,
                                              state.mRegionToInvalidate,
                                              ClientManager()->GetPaintedLayerCallbackData());

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
ClientPaintedLayer::RenderLayerWithReadback(ReadbackProcessor *aReadback)
{
  RenderMaskLayers(this);
  
  if (!mContentClient) {
    mContentClient = ContentClient::CreateContentClient(ClientManager()->AsShadowForwarder());
    if (!mContentClient) {
      return;
    }
    mContentClient->Connect();
    ClientManager()->AsShadowForwarder()->Attach(mContentClient, this);
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  nsIntRegion readbackRegion;
  if (aReadback && UsedForReadback()) {
    aReadback->GetPaintedLayerUpdates(this, &readbackUpdates);
  }

  IntPoint origin(mVisibleRegion.GetBounds().x, mVisibleRegion.GetBounds().y);
  mContentClient->BeginPaint();
  PaintThebes();
  mContentClient->EndPaint(&readbackUpdates);
}

already_AddRefed<PaintedLayer>
ClientLayerManager::CreatePaintedLayer()
{
  return CreatePaintedLayerWithHint(NONE);
}

already_AddRefed<PaintedLayer>
ClientLayerManager::CreatePaintedLayerWithHint(PaintedLayerCreationHint aHint)
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  if (gfxPrefs::LayersTilesEnabled()
#ifndef MOZ_X11
      && (AsShadowForwarder()->GetCompositorBackendType() == LayersBackend::LAYERS_OPENGL ||
          AsShadowForwarder()->GetCompositorBackendType() == LayersBackend::LAYERS_D3D9 ||
          AsShadowForwarder()->GetCompositorBackendType() == LayersBackend::LAYERS_D3D11)
#endif
  ) {
    nsRefPtr<ClientTiledPaintedLayer> layer = new ClientTiledPaintedLayer(this, aHint);
    CREATE_SHADOW(Painted);
    return layer.forget();
  } else {
    nsRefPtr<ClientPaintedLayer> layer = new ClientPaintedLayer(this, aHint);
    CREATE_SHADOW(Painted);
    return layer.forget();
  }
}

void
ClientPaintedLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  PaintedLayer::PrintInfo(aStream, aPrefix);
  if (mContentClient) {
    aStream << "\n";
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mContentClient->PrintInfo(aStream, pfx.get());
  }
}

} // namespace layers
} // namespace mozilla
