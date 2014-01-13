/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicThebesLayer.h"
#include <stdint.h>                     // for uint32_t
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "ReadbackLayer.h"              // for ReadbackLayer, ReadbackSink
#include "ReadbackProcessor.h"          // for ReadbackProcessor::Update, etc
#include "RenderTrace.h"                // for RenderTraceInvalidateEnd, etc
#include "BasicLayersImpl.h"            // for AutoMaskData, etc
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for gfxUtils
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Matrix.h"         // for Matrix
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for Float, etc
#include "mozilla/layers/LayersTypes.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for gfxContext::Release, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl
#include "AutoMaskData.h"
#include "gfx2DGlue.h"

struct gfxMatrix;

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

static nsIntRegion
IntersectWithClip(const nsIntRegion& aRegion, gfxContext* aContext)
{
  gfxRect clip = aContext->GetClipExtents();
  clip.RoundOut();
  nsIntRect r(clip.X(), clip.Y(), clip.Width(), clip.Height());
  nsIntRegion result;
  result.And(aRegion, r);
  return result;
}

void
BasicThebesLayer::PaintThebes(gfxContext* aContext,
                              Layer* aMaskLayer,
                              LayerManager::DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              ReadbackProcessor* aReadback)
{
  PROFILER_LABEL("BasicThebesLayer", "PaintThebes");
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  if (aReadback && UsedForReadback()) {
    aReadback->GetThebesLayerUpdates(this, &readbackUpdates);
  }

  float opacity = GetEffectiveOpacity();
  gfxContext::GraphicsOperator mixBlendMode = GetEffectiveMixBlendMode();

  if (!BasicManager()->IsRetained()) {
    NS_ASSERTION(readbackUpdates.IsEmpty(), "Can't do readback for non-retained layer");

    mValidRegion.SetEmpty();
    mContentClient->Clear();

    nsIntRegion toDraw = IntersectWithClip(GetEffectiveVisibleRegion(), aContext);

    RenderTraceInvalidateStart(this, "FFFF00", toDraw.GetBounds());

    if (!toDraw.IsEmpty() && !IsHidden()) {
      if (!aCallback) {
        BasicManager()->SetTransactionIncomplete();
        return;
      }

      aContext->Save();

      bool needsClipToVisibleRegion = GetClipToVisibleRegion();
      bool needsGroup =
          opacity != 1.0 || GetOperator() != gfxContext::OPERATOR_OVER || mixBlendMode != gfxContext::OPERATOR_OVER || aMaskLayer;
      nsRefPtr<gfxContext> groupContext;
      if (needsGroup) {
        groupContext =
          BasicManager()->PushGroupForLayer(aContext, this, toDraw,
                                            &needsClipToVisibleRegion);
        if (GetOperator() != gfxContext::OPERATOR_OVER || mixBlendMode != gfxContext::OPERATOR_OVER) {
          needsClipToVisibleRegion = true;
        }
      } else {
        groupContext = aContext;
      }
      SetAntialiasingFlags(this, groupContext);
      aCallback(this, groupContext, toDraw, CLIP_NONE, nsIntRegion(), aCallbackData);
      if (needsGroup) {
        BasicManager()->PopGroupToSourceWithCachedSurface(aContext, groupContext);
        if (needsClipToVisibleRegion) {
          gfxUtils::ClipToRegion(aContext, toDraw);
        }
        AutoSetOperator setOptimizedOperator(aContext, mixBlendMode != gfxContext::OPERATOR_OVER ? mixBlendMode : GetOperator());
        PaintWithMask(aContext, opacity, aMaskLayer);
      }

      aContext->Restore();
    }

    RenderTraceInvalidateEnd(this, "FFFF00");
    return;
  }

  if (BasicManager()->IsTransactionIncomplete())
    return;

  gfxRect clipExtents;
  clipExtents = aContext->GetClipExtents();

  // Pull out the mask surface and transform here, because the mask
  // is internal to basic layers
  AutoMaskData mask;
  gfxASurface* maskSurface = nullptr;
  const gfxMatrix* maskTransform = nullptr;
  if (GetMaskData(aMaskLayer, &mask)) {
    maskSurface = mask.GetSurface();
    maskTransform = &mask.GetTransform();
  }

  if (!IsHidden() && !clipExtents.IsEmpty()) {
    mContentClient->DrawTo(this, aContext->GetDrawTarget(), opacity,
                           CompositionOpForOp(GetOperator()),
                           maskSurface, maskTransform);
  }

  for (uint32_t i = 0; i < readbackUpdates.Length(); ++i) {
    ReadbackProcessor::Update& update = readbackUpdates[i];
    nsIntPoint offset = update.mLayer->GetBackgroundLayerOffset();
    nsRefPtr<gfxContext> ctx =
      update.mLayer->GetSink()->BeginUpdate(update.mUpdateRect + offset,
                                            update.mSequenceCounter);
    if (ctx) {
      NS_ASSERTION(opacity == 1.0, "Should only read back opaque layers");
      ctx->Translate(gfxPoint(offset.x, offset.y));
      mContentClient->DrawTo(this, ctx->GetDrawTarget(), 1.0,
                             CompositionOpForOp(ctx->CurrentOperator()),
                             maskSurface, maskTransform);
      update.mLayer->GetSink()->EndUpdate(ctx, update.mUpdateRect + offset);
    }
  }
}

void
BasicThebesLayer::Validate(LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData)
{
  if (!mContentClient) {
    // This client will have a null Forwarder, which means it will not have
    // a ContentHost on the other side.
    mContentClient = new ContentClientBasic();
  }

  if (!BasicManager()->IsRetained()) {
    return;
  }

  uint32_t flags = 0;
#ifndef MOZ_WIDGET_ANDROID
  if (BasicManager()->CompositorMightResample()) {
    flags |= RotatedContentBuffer::PAINT_WILL_RESAMPLE;
  }
  if (!(flags & RotatedContentBuffer::PAINT_WILL_RESAMPLE)) {
    if (MayResample()) {
      flags |= RotatedContentBuffer::PAINT_WILL_RESAMPLE;
    }
  }
#endif
  if (mDrawAtomically) {
    flags |= RotatedContentBuffer::PAINT_NO_ROTATION;
  }
  PaintState state =
    mContentClient->BeginPaintBuffer(this, flags);
  mValidRegion.Sub(mValidRegion, state.mRegionToInvalidate);

  if (DrawTarget* target = mContentClient->BorrowDrawTargetForPainting(this, state)) {
    // The area that became invalid and is visible needs to be repainted
    // (this could be the whole visible area if our buffer switched
    // from RGB to RGBA, because we might need to repaint with
    // subpixel AA)
    state.mRegionToInvalidate.And(state.mRegionToInvalidate,
                                  GetEffectiveVisibleRegion());
    SetAntialiasingFlags(this, target);

    RenderTraceInvalidateStart(this, "FFFF00", state.mRegionToDraw.GetBounds());

    nsRefPtr<gfxContext> ctx = gfxContext::ContextForDrawTarget(target);
    PaintBuffer(ctx,
                state.mRegionToDraw, state.mRegionToDraw, state.mRegionToInvalidate,
                state.mDidSelfCopy,
                state.mClip,
                aCallback, aCallbackData);
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PaintThebes", this));
    Mutated();
    ctx = nullptr;
    mContentClient->ReturnDrawTarget(target);

    RenderTraceInvalidateEnd(this, "FFFF00");
  } else {
    // It's possible that state.mRegionToInvalidate is nonempty here,
    // if we are shrinking the valid region to nothing. So use mRegionToDraw
    // instead.
    NS_WARN_IF_FALSE(state.mRegionToDraw.IsEmpty(),
                     "No context when we have something to draw, resource exhaustion?");
  }
}

already_AddRefed<ThebesLayer>
BasicLayerManager::CreateThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ThebesLayer> layer = new BasicThebesLayer(this);
  return layer.forget();
}

}
}
