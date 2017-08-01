/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientPaintedLayer.h"
#include "ClientTiledPaintedLayer.h"     // for ClientTiledPaintedLayer
#include <stdint.h>                     // for uint32_t
#include "GeckoProfiler.h"              // for AUTO_PROFILER_LABEL
#include "client/ClientLayerManager.h"  // for ClientLayerManager, etc
#include "gfxContext.h"                 // for gfxContext
#include "gfx2DGlue.h"
#include "gfxRect.h"                    // for gfxRect
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/Matrix.h"         // for Matrix
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for Float, etc
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Preferences.h"
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "PaintThread.h"
#include "ReadbackProcessor.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

bool
ClientPaintedLayer::EnsureContentClient()
{
  if (!mContentClient) {
    mContentClient = ContentClient::CreateContentClient(
      ClientManager()->AsShadowForwarder());

    if (!mContentClient) {
      return false;
    }

    mContentClient->Connect();
    ClientManager()->AsShadowForwarder()->Attach(mContentClient, this);
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  return true;
}

bool
ClientPaintedLayer::CanRecordLayer(ReadbackProcessor* aReadback)
{
  // If we don't have a paint thread, this is either not the content
  // process or the pref is disabled.
  if (!PaintThread::Get()) {
    return false;
  }

  // Not supported yet
  if (aReadback && UsedForReadback()) {
    return false;
  }

  // If we have mask layers, we have to render those first
  // In this case, don't record for now.
  if (GetMaskLayer()) {
    return false;
  }

  // Component alpha layers aren't supported yet since we have to
  // hold onto both the front/back buffer of a texture client.
  if (GetSurfaceMode() == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
    return false;
  }

  return GetAncestorMaskLayerCount() == 0;
}

void
ClientPaintedLayer::UpdateContentClient(PaintState& aState)
{
  Mutated();

  AddToValidRegion(aState.mRegionToDraw);

  ContentClientRemote *contentClientRemote =
      static_cast<ContentClientRemote *>(mContentClient.get());
  MOZ_ASSERT(contentClientRemote->GetIPCHandle());

  // Hold(this) ensures this layer is kept alive through the current transaction
  // The ContentClient assumes this layer is kept alive (e.g., in CreateBuffer),
  // so deleting this Hold for whatever reason will break things.
  ClientManager()->Hold(this);
  contentClientRemote->Updated(aState.mRegionToDraw,
                               mVisibleRegion.ToUnknownRegion(),
                               aState.mDidSelfCopy);
}

bool
ClientPaintedLayer::UpdatePaintRegion(PaintState& aState)
{
  SubtractFromValidRegion(aState.mRegionToInvalidate);

  if (!aState.mRegionToDraw.IsEmpty() && !ClientManager()->GetPaintedLayerCallback()) {
    ClientManager()->SetTransactionIncomplete();
    mContentClient->EndPaint(nullptr);
    return false;
   }

   // The area that became invalid and is visible needs to be repainted
   // (this could be the whole visible area if our buffer switched
   // from RGB to RGBA, because we might need to repaint with
   // subpixel AA)
  aState.mRegionToInvalidate.And(aState.mRegionToInvalidate,
                                 GetLocalVisibleRegion().ToUnknownRegion());
  return true;
}

uint32_t
ClientPaintedLayer::GetPaintFlags()
{
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
  return flags;
}

void
ClientPaintedLayer::PaintThebes(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates)
{
  AUTO_PROFILER_LABEL("ClientPaintedLayer::PaintThebes", GRAPHICS);

  NS_ASSERTION(ClientManager()->InDrawing(),
               "Can only draw in drawing phase");

  mContentClient->BeginPaint();

  uint32_t flags = GetPaintFlags();

  PaintState state = mContentClient->BeginPaintBuffer(this, flags);
  if (!UpdatePaintRegion(state)) {
    return;
  }

  bool didUpdate = false;
  RotatedContentBuffer::DrawIterator iter;
  while (DrawTarget* target = mContentClient->BorrowDrawTargetForPainting(state, &iter)) {
    if (!target || !target->IsValid()) {
      if (target) {
        mContentClient->ReturnDrawTargetToBuffer(target);
      }
      continue;
    }

    SetAntialiasingFlags(this, target);

    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(target);
    MOZ_ASSERT(ctx); // already checked the target above

    ClientManager()->GetPaintedLayerCallback()(this,
                                              ctx,
                                              iter.mDrawRegion,
                                              iter.mDrawRegion,
                                              state.mClip,
                                              state.mRegionToInvalidate,
                                              ClientManager()->GetPaintedLayerCallbackData());

    ctx = nullptr;
    mContentClient->ReturnDrawTargetToBuffer(target);
    didUpdate = true;
  }

  mContentClient->EndPaint(aReadbackUpdates);

  if (didUpdate) {
    UpdateContentClient(state);
  }
}

bool
ClientPaintedLayer::PaintOffMainThread()
{
  mContentClient->BeginAsyncPaint();

  uint32_t flags = GetPaintFlags();

  PaintState state = mContentClient->BeginPaintBuffer(this, flags);
  if (!UpdatePaintRegion(state)) {
    return false;
  }

  bool didUpdate = false;
  RotatedContentBuffer::DrawIterator iter;
  while (DrawTarget* target = mContentClient->BorrowDrawTargetForPainting(state, &iter)) {
    if (!target || !target->IsValid()) {
      if (target) {
        mContentClient->ReturnDrawTargetToBuffer(target);
      }
      continue;
    }

    // We don't clear the rect here like WRPaintedBlobLayers do
    // because ContentClient already clears the surface for us during BeginPaint.
    RefPtr<DrawTargetCapture> captureDT =
      Factory::CreateCaptureDrawTarget(target->GetBackendType(),
                                       target->GetSize(),
                                       target->GetFormat());
    captureDT->SetTransform(target->GetTransform());

    SetAntialiasingFlags(this, captureDT);
    SetAntialiasingFlags(this, target);

    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(captureDT);
    MOZ_ASSERT(ctx); // already checked the target above

    ClientManager()->GetPaintedLayerCallback()(this,
                                              ctx,
                                              iter.mDrawRegion,
                                              iter.mDrawRegion,
                                              state.mClip,
                                              state.mRegionToInvalidate,
                                              ClientManager()->GetPaintedLayerCallbackData());

    ctx = nullptr;

    PaintThread::Get()->PaintContents(captureDT, target);

    mContentClient->ReturnDrawTargetToBuffer(target);
    didUpdate = true;
  }

  mContentClient->EndPaint(nullptr);

  if (didUpdate) {
    UpdateContentClient(state);
  }
  return true;
}

void
ClientPaintedLayer::RenderLayerWithReadback(ReadbackProcessor *aReadback)
{
  RenderMaskLayers(this);

  if (!EnsureContentClient()) {
    return;
  }

  if (CanRecordLayer(aReadback)) {
    if (PaintOffMainThread()) {
      return;
    }
  }

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  nsIntRegion readbackRegion;
  if (aReadback && UsedForReadback()) {
    aReadback->GetPaintedLayerUpdates(this, &readbackUpdates);
  }

  PaintThebes(&readbackUpdates);
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
  // The non-tiling ContentClient requires CrossProcessSemaphore which
  // isn't implemented for OSX.
#ifdef XP_MACOSX
  if (true) {
#else
  if (gfxPrefs::LayersTilesEnabled()) {
#endif
    RefPtr<ClientTiledPaintedLayer> layer = new ClientTiledPaintedLayer(this, aHint);
    CREATE_SHADOW(Painted);
    return layer.forget();
  } else {
    RefPtr<ClientPaintedLayer> layer = new ClientPaintedLayer(this, aHint);
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
