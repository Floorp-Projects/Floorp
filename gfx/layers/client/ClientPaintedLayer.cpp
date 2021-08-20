/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientPaintedLayer.h"
#include "ClientTiledPaintedLayer.h"    // for ClientTiledPaintedLayer
#include <stdint.h>                     // for uint32_t
#include "client/ClientLayerManager.h"  // for ClientLayerManager, etc
#include "gfxContext.h"                 // for gfxContext
#include "gfx2DGlue.h"
#include "gfxEnv.h"   // for gfxEnv
#include "gfxRect.h"  // for gfxRect

#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/gfx/2D.h"      // for DrawTarget
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/Matrix.h"  // for Matrix
#include "mozilla/gfx/Rect.h"    // for Rect, IntRect
#include "mozilla/gfx/Types.h"   // for Float, etc
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsISupportsImpl.h"  // for Layer::AddRef, etc
#include "nsRect.h"           // for mozilla::gfx::IntRect
#include "ReadbackProcessor.h"
#include "RotatedBuffer.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

bool ClientPaintedLayer::EnsureContentClient() {
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

void ClientPaintedLayer::UpdateContentClient(PaintState& aState) {
  Mutated();

  AddToValidRegion(aState.mRegionToDraw);

  ContentClientRemoteBuffer* contentClientRemote =
      static_cast<ContentClientRemoteBuffer*>(mContentClient.get());
  MOZ_ASSERT(contentClientRemote->GetIPCHandle());

  // Hold(this) ensures this layer is kept alive through the current transaction
  // The ContentClient assumes this layer is kept alive (e.g., in CreateBuffer),
  // so deleting this Hold for whatever reason will break things.
  ClientManager()->Hold(this);
  contentClientRemote->Updated(aState.mRegionToDraw,
                               mVisibleRegion.ToUnknownRegion());
}

bool ClientPaintedLayer::UpdatePaintRegion(PaintState& aState) {
  SubtractFromValidRegion(aState.mRegionToInvalidate);

  if (!aState.mRegionToDraw.IsEmpty() &&
      !ClientManager()->GetPaintedLayerCallback()) {
    ClientManager()->SetTransactionIncomplete();
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

void ClientPaintedLayer::FinishPaintState(PaintState& aState) {
}

uint32_t ClientPaintedLayer::GetPaintFlags(ReadbackProcessor* aReadback) {
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

void ClientPaintedLayer::RenderLayerWithReadback(ReadbackProcessor* aReadback) {
  AUTO_PROFILER_LABEL("ClientPaintedLayer::RenderLayerWithReadback", GRAPHICS);
  NS_ASSERTION(ClientManager()->InDrawing(), "Can only draw in drawing phase");

  RenderMaskLayers(this);

  if (!EnsureContentClient()) {
    return;
  }

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  nsIntRegion readbackRegion;
  if (aReadback && UsedForReadback()) {
    aReadback->GetPaintedLayerUpdates(this, &readbackUpdates);
  }

  uint32_t flags = GetPaintFlags(aReadback);

  PaintState state = mContentClient->BeginPaint(this, flags);
  if (!UpdatePaintRegion(state)) {
    mContentClient->EndPaint(state, nullptr);
    FinishPaintState(state);
    return;
  }

  bool didUpdate = false;
  RotatedBuffer::DrawIterator iter;
  while (DrawTarget* target =
             mContentClient->BorrowDrawTargetForPainting(state, &iter)) {
    SetAntialiasingFlags(this, target);

    RefPtr<gfxContext> ctx =
        gfxContext::CreatePreservingTransformOrNull(target);
    MOZ_ASSERT(ctx);  // already checked the target above

    if (!gfxEnv::SkipRasterization()) {
      if (!target->IsCaptureDT()) {
        target->ClearRect(Rect());
        if (target->IsValid()) {
          ClientManager()->GetPaintedLayerCallback()(
              this, ctx, iter.mDrawRegion, iter.mDrawRegion, state.mClip,
              state.mRegionToInvalidate,
              ClientManager()->GetPaintedLayerCallbackData());
        }
      } else {
        ClientManager()->GetPaintedLayerCallback()(
            this, ctx, iter.mDrawRegion, iter.mDrawRegion, state.mClip,
            state.mRegionToInvalidate,
            ClientManager()->GetPaintedLayerCallbackData());
      }
    }

    ctx = nullptr;
    mContentClient->ReturnDrawTarget(target);
    didUpdate = true;
  }

  mContentClient->EndPaint(state, &readbackUpdates);
  FinishPaintState(state);

  if (didUpdate) {
    UpdateContentClient(state);
  }
}

already_AddRefed<PaintedLayer> ClientLayerManager::CreatePaintedLayer() {
  return CreatePaintedLayerWithHint(NONE);
}

already_AddRefed<PaintedLayer> ClientLayerManager::CreatePaintedLayerWithHint(
    PaintedLayerCreationHint aHint) {
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  if (gfxPlatform::GetPlatform()->UsesTiling()) {
    RefPtr<ClientTiledPaintedLayer> layer =
        new ClientTiledPaintedLayer(this, aHint);
    CREATE_SHADOW(Painted);
    return layer.forget();
  } else {
    RefPtr<ClientPaintedLayer> layer = new ClientPaintedLayer(this, aHint);
    CREATE_SHADOW(Painted);
    return layer.forget();
  }
}

void ClientPaintedLayer::PrintInfo(std::stringstream& aStream,
                                   const char* aPrefix) {
  PaintedLayer::PrintInfo(aStream, aPrefix);
  if (mContentClient) {
    aStream << "\n";
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mContentClient->PrintInfo(aStream, pfx.get());
  }
}

}  // namespace layers
}  // namespace mozilla
