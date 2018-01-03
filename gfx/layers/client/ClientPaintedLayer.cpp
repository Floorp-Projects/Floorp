/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
#include "RotatedBuffer.h"

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

  return true;
}

void
ClientPaintedLayer::UpdateContentClient(PaintState& aState)
{
  Mutated();

  AddToValidRegion(aState.mRegionToDraw);

  ContentClientRemoteBuffer *contentClientRemote =
      static_cast<ContentClientRemoteBuffer *>(mContentClient.get());
  MOZ_ASSERT(contentClientRemote->GetIPCHandle());

  // Hold(this) ensures this layer is kept alive through the current transaction
  // The ContentClient assumes this layer is kept alive (e.g., in CreateBuffer),
  // so deleting this Hold for whatever reason will break things.
  ClientManager()->Hold(this);
  contentClientRemote->Updated(aState.mRegionToDraw,
                               mVisibleRegion.ToUnknownRegion());
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
  uint32_t flags = ContentClient::PAINT_CAN_DRAW_ROTATED;
  #ifndef MOZ_IGNORE_PAINT_WILL_RESAMPLE
   if (ClientManager()->CompositorMightResample()) {
     flags |= ContentClient::PAINT_WILL_RESAMPLE;
   }
   if (!(flags & ContentClient::PAINT_WILL_RESAMPLE)) {
     if (MayResample()) {
       flags |= ContentClient::PAINT_WILL_RESAMPLE;
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

  uint32_t flags = GetPaintFlags();

  PaintState state = mContentClient->BeginPaint(this, flags);
  if (!UpdatePaintRegion(state)) {
    return;
  }

  bool didUpdate = false;
  RotatedBuffer::DrawIterator iter;
  while (DrawTarget* target = mContentClient->BorrowDrawTargetForPainting(state, &iter)) {
    if (!target || !target->IsValid()) {
      if (target) {
        mContentClient->ReturnDrawTarget(target);
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
    mContentClient->ReturnDrawTarget(target);
    didUpdate = true;
  }

  mContentClient->EndPaint(aReadbackUpdates);

  if (didUpdate) {
    UpdateContentClient(state);
  }
}

class MOZ_RAII AutoQueuedAsyncPaint
{
public:
  explicit AutoQueuedAsyncPaint(ClientLayerManager* aLayerManager)
    : mLayerManager(aLayerManager)
    , mQueuedAsyncPaints(false)
  { }

  void Queue() { mQueuedAsyncPaints = true; }

  ~AutoQueuedAsyncPaint()
  {
    if (mQueuedAsyncPaints) {
      mLayerManager->SetQueuedAsyncPaints();
    }
  }

private:
  ClientLayerManager* mLayerManager;
  bool mQueuedAsyncPaints;
};

/***
 * If we can, let's paint this ClientPaintedLayer's contents off the main thread.
 * The essential idea is that we ask the ContentClient for a DrawTarget and record
 * the moz2d commands. On the Paint Thread, we replay those commands to the
 * destination draw target. There are a couple of lifetime issues here though:
 *
 * 1) TextureClient owns the underlying buffer and DrawTarget. Because of this
 *    we have to keep the TextureClient and DrawTarget alive but trick the
 *    TextureClient into thinking it's already returned the DrawTarget
 *    since we iterate through different Rects to get DrawTargets*. If
 *    the TextureClient goes away, the DrawTarget and thus buffer can too.
 * 2) When ContentClient::EndPaint happens, it flushes the DrawTarget. We have
 *    to Reflush on the Paint Thread
 * 3) DrawTarget API is NOT thread safe. We get around this by recording
 *    on the main thread and painting on the paint thread. Logically,
 *    ClientLayerManager will force a flushed paint and block the main thread
 *    if we have another transaction. Thus we have a gap between when the main
 *    thread records, the paint thread paints, and we block the main thread
 *    from trying to paint again. The underlying API however is NOT thread safe.
 *  4) We have both "sync" and "async" OMTP. Sync OMTP means we paint on the main thread
 *     but block the main thread while the paint thread paints. Async OMTP doesn't block
 *     the main thread. Sync OMTP is only meant to be used as a debugging tool.
 */
void
ClientPaintedLayer::PaintOffMainThread()
{
  AutoQueuedAsyncPaint asyncPaints(ClientManager());

  uint32_t flags = GetPaintFlags();
  PaintState state = mContentClient->BeginPaint(this, flags | ContentClient::PAINT_ASYNC);

  if (state.mBufferState && state.mBufferState->HasOperations()) {
    PaintThread::Get()->PrepareBuffer(state.mBufferState);
    asyncPaints.Queue();
  }

  if (!UpdatePaintRegion(state)) {
    return;
  }

  bool didUpdate = false;
  RotatedBuffer::DrawIterator iter;

  // Debug Protip: Change to BorrowDrawTargetForPainting if using sync OMTP.
  while (RefPtr<CapturedPaintState> captureState =
          mContentClient->BorrowDrawTargetForRecording(state, &iter))
  {
    DrawTarget* target = captureState->mTargetDual;
    if (!target || !target->IsValid()) {
      if (target) {
        mContentClient->ReturnDrawTarget(target);
      }
      continue;
    }

    RefPtr<DrawTargetCapture> captureDT =
      Factory::CreateCaptureDrawTarget(target->GetBackendType(),
                                       target->GetSize(),
                                       target->GetFormat());

    captureDT->SetTransform(captureState->mTargetTransform);
    SetAntialiasingFlags(this, captureDT);

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

    captureState->mCapture = captureDT.forget();
    PaintThread::Get()->PaintContents(captureState,
                                      ContentClient::PrepareDrawTargetForPainting);

    mContentClient->ReturnDrawTarget(target);

    asyncPaints.Queue();
    didUpdate = true;
  }

  PaintThread::Get()->EndLayer();
  mContentClient->EndPaint(nullptr);

  if (didUpdate) {
    UpdateContentClient(state);
  }
}

void
ClientPaintedLayer::RenderLayerWithReadback(ReadbackProcessor *aReadback)
{
  RenderMaskLayers(this);

  if (!EnsureContentClient()) {
    return;
  }

  if (CanRecordLayer(aReadback)) {
    PaintOffMainThread();
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
