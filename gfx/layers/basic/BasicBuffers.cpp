/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicThebesLayer.h"
#include "BasicBuffers.h"
#include "gfxUtils.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {


static bool
IsClippingCheap(gfxContext* aTarget, const nsIntRegion& aRegion)
{
  // Assume clipping is cheap if the context just has an integer
  // translation, and the visible region is simple.
  return !aTarget->CurrentMatrix().HasNonIntegerTranslation() &&
         aRegion.GetNumRects() <= 1; 
}

void
BasicThebesLayerBuffer::DrawTo(ThebesLayer* aLayer,
                               gfxContext* aTarget,
                               float aOpacity,
                               Layer* aMaskLayer)
{
  aTarget->Save();
  // If the entire buffer is valid, we can just draw the whole thing,
  // no need to clip. But we'll still clip if clipping is cheap ---
  // that might let us copy a smaller region of the buffer.
  // Also clip to the visible region if we're told to.
  if (!aLayer->GetValidRegion().Contains(BufferRect()) ||
      (ToData(aLayer)->GetClipToVisibleRegion() &&
       !aLayer->GetVisibleRegion().Contains(BufferRect())) ||
      IsClippingCheap(aTarget, aLayer->GetEffectiveVisibleRegion())) {
    // We don't want to draw invalid stuff, so we need to clip. Might as
    // well clip to the smallest area possible --- the visible region.
    // Bug 599189 if there is a non-integer-translation transform in aTarget,
    // we might sample pixels outside GetEffectiveVisibleRegion(), which is wrong
    // and may cause gray lines.
    gfxUtils::ClipToRegionSnapped(aTarget, aLayer->GetEffectiveVisibleRegion());
  }

  // Pull out the mask surface and transform here, because the mask
  // is internal to basic layers
  gfxMatrix maskTransform;
  if (nsRefPtr<gfxASurface> maskSurface =
        GetMaskSurfaceAndTransform(aMaskLayer, &maskTransform)) {
    DrawBufferWithRotation(aTarget, aOpacity, maskSurface, &maskTransform);
  } else {
    DrawBufferWithRotation(aTarget, aOpacity);
  }
  aTarget->Restore();
}

already_AddRefed<gfxASurface>
BasicThebesLayerBuffer::CreateBuffer(ContentType aType, 
                                     const nsIntSize& aSize, PRUint32 aFlags)
{
  return mLayer->CreateBuffer(aType, aSize);
}

void
BasicThebesLayerBuffer::SetBackingBufferAndUpdateFrom(
  gfxASurface* aBuffer,
  gfxASurface* aSource, const nsIntRect& aRect, const nsIntPoint& aRotation,
  const nsIntRegion& aUpdateRegion)
{
  SetBackingBuffer(aBuffer, aRect, aRotation);
  nsRefPtr<gfxContext> destCtx =
    GetContextForQuadrantUpdate(aUpdateRegion.GetBounds());
  destCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
  if (IsClippingCheap(destCtx, aUpdateRegion)) {
    gfxUtils::ClipToRegion(destCtx, aUpdateRegion);
  }

  BasicThebesLayerBuffer srcBuffer(aSource, aRect, aRotation);
  srcBuffer.DrawBufferWithRotation(destCtx, 1.0);
}

}
}
