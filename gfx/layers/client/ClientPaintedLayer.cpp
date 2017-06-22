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

void
ClientPaintedLayer::PaintOffMainThread(DrawEventRecorderMemory* aRecorder)
{
  MOZ_ASSERT(NS_IsMainThread());
  LayerIntRegion visibleRegion = GetVisibleRegion();
  mContentClient->BeginPaint();

  uint32_t flags = GetPaintFlags();

  PaintState state =
    mContentClient->BeginPaintBuffer(this, flags);
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

    // Basic version, wait for the paint thread to finish painting.
    PaintThread::Get()->PaintContents(aRecorder, target);

    mContentClient->ReturnDrawTargetToBuffer(target);
    didUpdate = true;
  }

  // ending paint w/o any readback updates
  // TODO: Fix me
  mContentClient->EndPaint(nullptr);

  if (didUpdate) {
    UpdateContentClient(state);
   }
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
  PROFILER_LABEL("ClientPaintedLayer", "PaintThebes",
    js::ProfileEntry::Category::GRAPHICS);

  NS_ASSERTION(ClientManager()->InDrawing(),
               "Can only draw in drawing phase");

  mContentClient->BeginPaint();

  uint32_t flags = GetPaintFlags();

  PaintState state =
    mContentClient->BeginPaintBuffer(this, flags);
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

already_AddRefed<DrawEventRecorderMemory>
ClientPaintedLayer::RecordPaintedLayer()
{
  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  LayerIntSize size = bounds.Size();

  if (visibleRegion.IsEmpty()) {
    if (gfxPrefs::LayersDump()) {
      printf_stderr("PaintedLayer %p skipping\n", this);
    }
    return nullptr;
  }

  nsIntRegion regionToPaint;
  regionToPaint.Sub(mVisibleRegion.ToUnknownRegion(), GetValidRegion());

  if (regionToPaint.IsEmpty()) {
    // Do we ever have to do anything if the region to paint is empty
    // but we have a painted layer callback?
    return nullptr;
  }

  if (!ClientManager()->GetPaintedLayerCallback()) {
    ClientManager()->SetTransactionIncomplete();
    return nullptr;
  }

  // I know this is slow and we should probably use DrawTargetCapture
  // But for now, the recording draw target / replay should actually work
  // Replay for WR happens in Moz2DIMageRenderer
  IntSize imageSize(size.ToUnknownSize());

  // DrawTargetRecording also plays back the commands while
  // recording, hence the dummy DT. DummyDT will actually have
  // the drawn painted layer.
  RefPtr<DrawEventRecorderMemory> recorder =
    MakeAndAddRef<DrawEventRecorderMemory>();
  RefPtr<DrawTarget> dummyDt =
    Factory::CreateDrawTarget(gfx::BackendType::SKIA, imageSize, gfx::SurfaceFormat::B8G8R8A8);

  RefPtr<DrawTarget> dt =
    Factory::CreateRecordingDrawTarget(recorder, dummyDt, imageSize);

  dt->ClearRect(Rect(0, 0, imageSize.width, imageSize.height));
  dt->SetTransform(Matrix().PreTranslate(-bounds.x, -bounds.y));
  RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(dt);
  MOZ_ASSERT(ctx); // already checked the target above

  ClientManager()->GetPaintedLayerCallback()(this,
                                             ctx,
                                             visibleRegion.ToUnknownRegion(),
                                             visibleRegion.ToUnknownRegion(),
                                             DrawRegionClip::DRAW,
                                             nsIntRegion(),
                                             ClientManager()->GetPaintedLayerCallbackData());

  return recorder.forget();
}

void
ClientPaintedLayer::RenderLayerWithReadback(ReadbackProcessor *aReadback)
{
  RenderMaskLayers(this);

  if (CanRecordLayer(aReadback)) {
    RefPtr<DrawEventRecorderMemory> recorder = RecordPaintedLayer();
    if (recorder) {
      if (!EnsureContentClient()) {
        return;
      }

      PaintOffMainThread(recorder);
      return;
    }
  }

  if (!EnsureContentClient()) {
    return;
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
