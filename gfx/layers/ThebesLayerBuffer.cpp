/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThebesLayerBuffer.h"
#include <sys/types.h>                  // for int32_t
#include <algorithm>                    // for max
#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayersImpl.h"            // for ToData
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "Layers.h"                     // for ThebesLayer, Layer, etc
#include "gfxColor.h"                   // for gfxRGBA
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPattern.h"                 // for gfxPattern
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPoint.h"                   // for gfxPoint
#include "gfxRect.h"                    // for gfxRect
#include "gfxTeeSurface.h"              // for gfxTeeSurface
#include "gfxUtils.h"                   // for gfxUtils
#include "mozilla/Util.h"               // for ArrayLength
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Matrix.h"         // for Matrix
#include "mozilla/gfx/Point.h"          // for Point, IntPoint
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for ExtendMode::EXTEND_CLAMP, etc
#include "mozilla/layers/ShadowLayers.h"  // for ShadowableLayer
#include "mozilla/layers/TextureClient.h"  // for DeprecatedTextureClient
#include "nsSize.h"                     // for nsIntSize
#include "gfx2DGlue.h"

namespace mozilla {

using namespace gfx;

namespace layers {

nsIntRect
RotatedBuffer::GetQuadrantRectangle(XSide aXSide, YSide aYSide) const
{
  // quadrantTranslation is the amount we translate the top-left
  // of the quadrant by to get coordinates relative to the layer
  nsIntPoint quadrantTranslation = -mBufferRotation;
  quadrantTranslation.x += aXSide == LEFT ? mBufferRect.width : 0;
  quadrantTranslation.y += aYSide == TOP ? mBufferRect.height : 0;
  return mBufferRect + quadrantTranslation;
}

/**
 * @param aXSide LEFT means we draw from the left side of the buffer (which
 * is drawn on the right side of mBufferRect). RIGHT means we draw from
 * the right side of the buffer (which is drawn on the left side of
 * mBufferRect).
 * @param aYSide TOP means we draw from the top side of the buffer (which
 * is drawn on the bottom side of mBufferRect). BOTTOM means we draw from
 * the bottom side of the buffer (which is drawn on the top side of
 * mBufferRect).
 */
void
RotatedBuffer::DrawBufferQuadrant(gfxContext* aTarget,
                                  XSide aXSide, YSide aYSide,
                                  ContextSource aSource,
                                  float aOpacity,
                                  gfxASurface* aMask,
                                  const gfxMatrix* aMaskTransform) const
{
  // The rectangle that we're going to fill. Basically we're going to
  // render the buffer at mBufferRect + quadrantTranslation to get the
  // pixels in the right place, but we're only going to paint within
  // mBufferRect
  nsIntRect quadrantRect = GetQuadrantRectangle(aXSide, aYSide);
  nsIntRect fillRect;
  if (!fillRect.IntersectRect(mBufferRect, quadrantRect)) {
    return;
  }

  nsRefPtr<gfxASurface> source;

  if (aSource == BUFFER_BLACK) {
    if (mBuffer) {
      source = mBuffer;
    } else if (mDTBuffer) {
      source = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDTBuffer);
    } else {
      NS_RUNTIMEABORT("Can't draw a RotatedBuffer without any buffer!");
    }
  } else {
    MOZ_ASSERT(aSource == BUFFER_WHITE);
    if (mBufferOnWhite) {
      source = mBufferOnWhite;
    } else if (mDTBufferOnWhite) {
      source = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDTBufferOnWhite);
    } else {
      NS_RUNTIMEABORT("Can't draw a RotatedBuffer without any buffer!");
    }
  }


  aTarget->NewPath();
  aTarget->Rectangle(gfxRect(fillRect.x, fillRect.y,
                             fillRect.width, fillRect.height),
                     true);

  gfxPoint quadrantTranslation(quadrantRect.x, quadrantRect.y);
  nsRefPtr<gfxPattern> pattern = new gfxPattern(source);

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  GraphicsFilter filter = GraphicsFilter::FILTER_NEAREST;
  pattern->SetFilter(filter);
#endif

  gfxContextMatrixAutoSaveRestore saveMatrix(aTarget);

  // Transform from user -> buffer space.
  gfxMatrix transform;
  transform.Translate(-quadrantTranslation);

  pattern->SetMatrix(transform);
  aTarget->SetPattern(pattern);

  if (aMask) {
    if (aOpacity == 1.0) {
      aTarget->SetMatrix(*aMaskTransform);
      aTarget->Mask(aMask);
    } else {
      aTarget->PushGroup(GFX_CONTENT_COLOR_ALPHA);
      aTarget->Paint(aOpacity);
      aTarget->PopGroupToSource();
      aTarget->SetMatrix(*aMaskTransform);
      aTarget->Mask(aMask);
    }
  } else {
    if (aOpacity == 1.0) {
      aTarget->Fill();
    } else {
      aTarget->Save();
      aTarget->Clip();
      aTarget->Paint(aOpacity);
      aTarget->Restore();
    }
  }

  nsRefPtr<gfxASurface> surf = aTarget->CurrentSurface();
  surf->Flush();
}

/**
 * @param aXSide LEFT means we draw from the left side of the buffer (which
 * is drawn on the right side of mBufferRect). RIGHT means we draw from
 * the right side of the buffer (which is drawn on the left side of
 * mBufferRect).
 * @param aYSide TOP means we draw from the top side of the buffer (which
 * is drawn on the bottom side of mBufferRect). BOTTOM means we draw from
 * the bottom side of the buffer (which is drawn on the top side of
 * mBufferRect).
 */
void
RotatedBuffer::DrawBufferQuadrant(gfx::DrawTarget* aTarget,
                                  XSide aXSide, YSide aYSide,
                                  ContextSource aSource,
                                  float aOpacity,
                                  gfx::CompositionOp aOperator,
                                  gfx::SourceSurface* aMask,
                                  const gfx::Matrix* aMaskTransform) const
{
  // The rectangle that we're going to fill. Basically we're going to
  // render the buffer at mBufferRect + quadrantTranslation to get the
  // pixels in the right place, but we're only going to paint within
  // mBufferRect
  nsIntRect quadrantRect = GetQuadrantRectangle(aXSide, aYSide);
  nsIntRect fillRect;
  if (!fillRect.IntersectRect(mBufferRect, quadrantRect))
    return;

  gfx::Point quadrantTranslation(quadrantRect.x, quadrantRect.y);

  RefPtr<SourceSurface> snapshot;
  if (aSource == BUFFER_BLACK) {
    snapshot = mDTBuffer->Snapshot();
  } else {
    MOZ_ASSERT(aSource == BUFFER_WHITE);
    snapshot = mDTBufferOnWhite->Snapshot();
  }

  // Transform from user -> buffer space.
  Matrix transform;
  transform.Translate(quadrantTranslation.x, quadrantTranslation.y);

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  SurfacePattern source(snapshot, EXTEND_CLAMP, transform, FILTER_POINT);
#else
  SurfacePattern source(snapshot, EXTEND_CLAMP, transform);
#endif

  if (aOperator == OP_SOURCE) {
    // OP_SOURCE is unbounded in Azure, and we really don't want that behaviour here.
    // We also can't do a ClearRect+FillRect since we need the drawing to happen
    // as an atomic operation (to prevent flickering).
    aTarget->PushClipRect(gfx::Rect(fillRect.x, fillRect.y,
                                    fillRect.width, fillRect.height));
  }

  if (aMask) {
    Matrix oldTransform = aTarget->GetTransform();
    aTarget->SetTransform(*aMaskTransform);
    aTarget->MaskSurface(source, aMask, Point(0, 0), DrawOptions(aOpacity, aOperator));
    aTarget->SetTransform(oldTransform);
  } else {
    aTarget->FillRect(gfx::Rect(fillRect.x, fillRect.y,
                                fillRect.width, fillRect.height),
                      source, DrawOptions(aOpacity, aOperator));
  }

  if (aOperator == OP_SOURCE) {
    aTarget->PopClip();
  }

  aTarget->Flush();
}

void
RotatedBuffer::DrawBufferWithRotation(gfxContext* aTarget, ContextSource aSource,
                                      float aOpacity,
                                      gfxASurface* aMask,
                                      const gfxMatrix* aMaskTransform) const
{
  PROFILER_LABEL("RotatedBuffer", "DrawBufferWithRotation");
  // Draw four quadrants. We could use REPEAT_, but it's probably better
  // not to, to be performance-safe.
  DrawBufferQuadrant(aTarget, LEFT, TOP, aSource, aOpacity, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, RIGHT, TOP, aSource, aOpacity, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, LEFT, BOTTOM, aSource, aOpacity, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, RIGHT, BOTTOM, aSource, aOpacity, aMask, aMaskTransform);
}

void
RotatedBuffer::DrawBufferWithRotation(gfx::DrawTarget *aTarget, ContextSource aSource,
                                      float aOpacity,
                                      gfx::CompositionOp aOperator,
                                      gfx::SourceSurface* aMask,
                                      const gfx::Matrix* aMaskTransform) const
{
  PROFILER_LABEL("RotatedBuffer", "DrawBufferWithRotation");
  // See above, in Azure Repeat should always be a safe, even faster choice
  // though! Particularly on D2D Repeat should be a lot faster, need to look
  // into that. TODO[Bas]
  DrawBufferQuadrant(aTarget, LEFT, TOP, aSource, aOpacity, aOperator, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, RIGHT, TOP, aSource, aOpacity, aOperator, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, LEFT, BOTTOM, aSource, aOpacity, aOperator, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, RIGHT, BOTTOM, aSource, aOpacity, aOperator,aMask, aMaskTransform);
}

/* static */ bool
ThebesLayerBuffer::IsClippingCheap(gfxContext* aTarget, const nsIntRegion& aRegion)
{
  // Assume clipping is cheap if the context just has an integer
  // translation, and the visible region is simple.
  return !aTarget->CurrentMatrix().HasNonIntegerTranslation() &&
         aRegion.GetNumRects() <= 1;
}

void
ThebesLayerBuffer::DrawTo(ThebesLayer* aLayer,
                          gfxContext* aTarget,
                          float aOpacity,
                          gfxASurface* aMask,
                          const gfxMatrix* aMaskTransform)
{
  EnsureBuffer();

  if (aTarget->IsCairo()) {
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

    DrawBufferWithRotation(aTarget, BUFFER_BLACK, aOpacity, aMask, aMaskTransform);
    aTarget->Restore();
  } else {
    RefPtr<DrawTarget> dt = aTarget->GetDrawTarget();
    bool clipped = false;

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
      gfxUtils::ClipToRegionSnapped(dt, aLayer->GetEffectiveVisibleRegion());
      clipped = true;
    }

    RefPtr<SourceSurface> mask;
    if (aMask) {
      mask = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(dt, aMask);
    }

    Matrix maskTransform;
    if (aMaskTransform) {
      maskTransform = ToMatrix(*aMaskTransform);
    }

    CompositionOp op = CompositionOpForOp(aTarget->CurrentOperator());
    DrawBufferWithRotation(dt, BUFFER_BLACK, aOpacity, op, mask, &maskTransform);
    if (clipped) {
      dt->PopClip();
    }
  }
}

static void
FillSurface(gfxASurface* aSurface, const nsIntRegion& aRegion,
            const nsIntPoint& aOffset, const gfxRGBA& aColor)
{
  nsRefPtr<gfxContext> ctx = new gfxContext(aSurface);
  ctx->Translate(-gfxPoint(aOffset.x, aOffset.y));
  gfxUtils::ClipToRegion(ctx, aRegion);
  ctx->SetColor(aColor);
  ctx->Paint();
}

already_AddRefed<gfxContext>
ThebesLayerBuffer::GetContextForQuadrantUpdate(const nsIntRect& aBounds, ContextSource aSource, nsIntPoint *aTopLeft)
{
  EnsureBuffer();

  nsRefPtr<gfxContext> ctx;
  if (aSource == BUFFER_BOTH && HaveBufferOnWhite()) {
    EnsureBufferOnWhite();
    if (mBuffer) {
      MOZ_ASSERT(mBufferOnWhite);
      gfxASurface* surfaces[2] = { mBuffer, mBufferOnWhite };
      nsRefPtr<gfxTeeSurface> surf = new gfxTeeSurface(surfaces, ArrayLength(surfaces));

      // XXX If the device offset is set on the individual surfaces instead of on
      // the tee surface, we render in the wrong place. Why?
      gfxPoint deviceOffset = mBuffer->GetDeviceOffset();
      surfaces[0]->SetDeviceOffset(gfxPoint(0, 0));
      surfaces[1]->SetDeviceOffset(gfxPoint(0, 0));
      surf->SetDeviceOffset(deviceOffset);

      surf->SetAllowUseAsSource(false);
      ctx = new gfxContext(surf);
    } else {
      MOZ_ASSERT(mDTBuffer && mDTBufferOnWhite);
      RefPtr<DrawTarget> dualDT = Factory::CreateDualDrawTarget(mDTBuffer, mDTBufferOnWhite);
      ctx = new gfxContext(dualDT);
    }
  } else if (aSource == BUFFER_WHITE) {
    EnsureBufferOnWhite();
    if (mBufferOnWhite) {
      ctx = new gfxContext(mBufferOnWhite);
    } else {
      ctx = new gfxContext(mDTBufferOnWhite);
    }
  } else {
    // BUFFER_BLACK, or BUFFER_BOTH with a single buffer.
    if (mBuffer) {
      ctx = new gfxContext(mBuffer);
    } else {
      ctx = new gfxContext(mDTBuffer);
    }
  }

  // Figure out which quadrant to draw in
  int32_t xBoundary = mBufferRect.XMost() - mBufferRotation.x;
  int32_t yBoundary = mBufferRect.YMost() - mBufferRotation.y;
  XSide sideX = aBounds.XMost() <= xBoundary ? RIGHT : LEFT;
  YSide sideY = aBounds.YMost() <= yBoundary ? BOTTOM : TOP;
  nsIntRect quadrantRect = GetQuadrantRectangle(sideX, sideY);
  NS_ASSERTION(quadrantRect.Contains(aBounds), "Messed up quadrants");
  ctx->Translate(-gfxPoint(quadrantRect.x, quadrantRect.y));

  if (aTopLeft) {
    *aTopLeft = nsIntPoint(quadrantRect.x, quadrantRect.y);
  }

  return ctx.forget();
}

gfxContentType
ThebesLayerBuffer::BufferContentType()
{
  if (mBuffer) {
    return mBuffer->GetContentType();
  }
  if (mBufferProvider) {
    return mBufferProvider->GetContentType();
  }
  if (mDTBuffer) {
    switch (mDTBuffer->GetFormat()) {
    case FORMAT_A8:
      return GFX_CONTENT_ALPHA;
    case FORMAT_B8G8R8A8:
    case FORMAT_R8G8B8A8:
      return GFX_CONTENT_COLOR_ALPHA;
    default:
      return GFX_CONTENT_COLOR;
    }
  }
  return GFX_CONTENT_SENTINEL;
}

bool
ThebesLayerBuffer::BufferSizeOkFor(const nsIntSize& aSize)
{
  return (aSize == mBufferRect.Size() ||
          (SizedToVisibleBounds != mBufferSizePolicy &&
           aSize < mBufferRect.Size()));
}

bool
ThebesLayerBuffer::IsAzureBuffer()
{
  MOZ_ASSERT(!(mDTBuffer && mBuffer), "Trying to use Azure and Thebes in the same buffer?");
  if (mDTBuffer) {
    return true;
  }
  if (mBuffer) {
    return false;
  }
  if (mBufferProvider) {
    return gfxPlatform::GetPlatform()->SupportsAzureContentForType(
      mBufferProvider->BackendType());
  }
  return SupportsAzureContent();
}

void
ThebesLayerBuffer::EnsureBuffer()
{
  if ((!mBuffer && !mDTBuffer) && mBufferProvider) {
    if (IsAzureBuffer()) {
      mDTBuffer = mBufferProvider->LockDrawTarget();
      mBuffer = nullptr;
    } else {
      mBuffer = mBufferProvider->LockSurface();
      mDTBuffer = nullptr;
    }
  }
}

void
ThebesLayerBuffer::EnsureBufferOnWhite()
{
  if ((!mBufferOnWhite && !mDTBufferOnWhite) && mBufferProviderOnWhite) {
    if (IsAzureBuffer()) {
      mDTBufferOnWhite = mBufferProviderOnWhite->LockDrawTarget();
      mBufferOnWhite = nullptr;
    } else {
      mBufferOnWhite = mBufferProviderOnWhite->LockSurface();
      mDTBufferOnWhite = nullptr;
    }
  }
}

bool
ThebesLayerBuffer::HaveBuffer() const
{
  return mDTBuffer || mBuffer || mBufferProvider;
}

bool
ThebesLayerBuffer::HaveBufferOnWhite() const
{
  return mDTBufferOnWhite || mBufferOnWhite || mBufferProviderOnWhite;
}

static void
WrapRotationAxis(int32_t* aRotationPoint, int32_t aSize)
{
  if (*aRotationPoint < 0) {
    *aRotationPoint += aSize;
  } else if (*aRotationPoint >= aSize) {
    *aRotationPoint -= aSize;
  }
}

static nsIntRect
ComputeBufferRect(const nsIntRect& aRequestedRect)
{
  nsIntRect rect(aRequestedRect);
  // Set a minimum width to guarantee a minimum size of buffers we
  // allocate (and work around problems on some platforms with smaller
  // dimensions).  64 is the magic number needed to work around the
  // rendering glitch, and guarantees image rows can be SIMD'd for
  // even r5g6b5 surfaces pretty much everywhere.
  rect.width = std::max(aRequestedRect.width, 64);
#ifdef MOZ_WIDGET_GONK
  // Set a minumum height to guarantee a minumum height of buffers we
  // allocate. Some GL implementations fail to render gralloc textures
  // with a height 9px-16px. It happens on Adreno 200. Adreno 320 does not
  // have this problem. 32 is choosed as alignment of gralloc buffers.
  // See Bug 873937.
  // Increase the height only when the requested height is more than 0.
  // See Bug 895976.
  // XXX it might be better to disable it on the gpu that does not have
  // the height problem.
  if (rect.height > 0) {
    rect.height = std::max(aRequestedRect.height, 32);
  }
#endif
  return rect;
}

ThebesLayerBuffer::PaintState
ThebesLayerBuffer::BeginPaint(ThebesLayer* aLayer, ContentType aContentType,
                              uint32_t aFlags)
{
  PaintState result;
  // We need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  bool canHaveRotation = gfxPlatform::BufferRotationEnabled() &&
                         !(aFlags & (PAINT_WILL_RESAMPLE | PAINT_NO_ROTATION));

  nsIntRegion validRegion = aLayer->GetValidRegion();

  Layer::SurfaceMode mode;
  ContentType contentType;
  nsIntRegion neededRegion;
  bool canReuseBuffer;
  nsIntRect destBufferRect;

  while (true) {
    mode = aLayer->GetSurfaceMode();
    contentType = aContentType;
    neededRegion = aLayer->GetVisibleRegion();
    canReuseBuffer = HaveBuffer() && BufferSizeOkFor(neededRegion.GetBounds().Size());

    if (canReuseBuffer) {
      if (mBufferRect.Contains(neededRegion.GetBounds())) {
        // We don't need to adjust mBufferRect.
        destBufferRect = mBufferRect;
      } else if (neededRegion.GetBounds().Size() <= mBufferRect.Size()) {
        // The buffer's big enough but doesn't contain everything that's
        // going to be visible. We'll move it.
        destBufferRect = nsIntRect(neededRegion.GetBounds().TopLeft(), mBufferRect.Size());
      } else {
        destBufferRect = neededRegion.GetBounds();
      }
    } else {
      // We won't be reusing the buffer.  Compute a new rect.
      destBufferRect = ComputeBufferRect(neededRegion.GetBounds());
    }

    if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
#if defined(MOZ_GFX_OPTIMIZE_MOBILE) || defined(MOZ_WIDGET_GONK)
      mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
#else
      if (!aLayer->GetParent() ||
          !aLayer->GetParent()->SupportsComponentAlphaChildren() ||
          !aLayer->Manager()->IsCompositingCheap() ||
          !aLayer->AsShadowableLayer() ||
          !aLayer->AsShadowableLayer()->HasShadow() ||
          !gfxPlatform::ComponentAlphaEnabled()) {
        mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
      } else {
        contentType = GFX_CONTENT_COLOR;
      }
#endif
    }

    if ((aFlags & PAINT_WILL_RESAMPLE) &&
        (!neededRegion.GetBounds().IsEqualInterior(destBufferRect) ||
         neededRegion.GetNumRects() > 1)) {
      // The area we add to neededRegion might not be painted opaquely
      if (mode == Layer::SURFACE_OPAQUE) {
        contentType = GFX_CONTENT_COLOR_ALPHA;
        mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
      }

      // We need to validate the entire buffer, to make sure that only valid
      // pixels are sampled
      neededRegion = destBufferRect;
    }

    // If we have an existing buffer, but the content type has changed or we
    // have transitioned into/out of component alpha, then we need to recreate it.
    if (HaveBuffer() &&
        (contentType != BufferContentType() ||
        (mode == Layer::SURFACE_COMPONENT_ALPHA) != HaveBufferOnWhite())) {

      // We're effectively clearing the valid region, so we need to draw
      // the entire needed region now.
      result.mRegionToInvalidate = aLayer->GetValidRegion();
      validRegion.SetEmpty();
      Clear();
      // Restart decision process with the cleared buffer. We can only go
      // around the loop one more iteration, since mBuffer is null now.
      continue;
    }

    break;
  }

  NS_ASSERTION(destBufferRect.Contains(neededRegion.GetBounds()),
               "Destination rect doesn't contain what we need to paint");

  result.mRegionToDraw.Sub(neededRegion, validRegion);
  if (result.mRegionToDraw.IsEmpty())
    return result;

  nsIntRect drawBounds = result.mRegionToDraw.GetBounds();
  nsRefPtr<gfxASurface> destBuffer;
  nsRefPtr<gfxASurface> destBufferOnWhite;
  RefPtr<DrawTarget> destDTBuffer;
  RefPtr<DrawTarget> destDTBufferOnWhite;
  uint32_t bufferFlags = canHaveRotation ? ALLOW_REPEAT : 0;
  if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
    bufferFlags |= BUFFER_COMPONENT_ALPHA;
  }
  if (canReuseBuffer) {
    EnsureBuffer();
    nsIntRect keepArea;
    if (keepArea.IntersectRect(destBufferRect, mBufferRect)) {
      // Set mBufferRotation so that the pixels currently in mBuffer
      // will still be rendered in the right place when mBufferRect
      // changes to destBufferRect.
      nsIntPoint newRotation = mBufferRotation +
        (destBufferRect.TopLeft() - mBufferRect.TopLeft());
      WrapRotationAxis(&newRotation.x, mBufferRect.width);
      WrapRotationAxis(&newRotation.y, mBufferRect.height);
      NS_ASSERTION(nsIntRect(nsIntPoint(0,0), mBufferRect.Size()).Contains(newRotation),
                   "newRotation out of bounds");
      int32_t xBoundary = destBufferRect.XMost() - newRotation.x;
      int32_t yBoundary = destBufferRect.YMost() - newRotation.y;
      if ((drawBounds.x < xBoundary && xBoundary < drawBounds.XMost()) ||
          (drawBounds.y < yBoundary && yBoundary < drawBounds.YMost()) ||
          (newRotation != nsIntPoint(0,0) && !canHaveRotation)) {
        // The stuff we need to redraw will wrap around an edge of the
        // buffer, so move the pixels we can keep into a position that
        // lets us redraw in just one quadrant.
        if (mBufferRotation == nsIntPoint(0,0)) {
          nsIntRect srcRect(nsIntPoint(0, 0), mBufferRect.Size());
          nsIntPoint dest = mBufferRect.TopLeft() - destBufferRect.TopLeft();
          if (IsAzureBuffer()) {
            MOZ_ASSERT(mDTBuffer);
            RefPtr<SourceSurface> source = mDTBuffer->Snapshot();
            mDTBuffer->CopySurface(source,
                                   IntRect(srcRect.x, srcRect.y, srcRect.width, srcRect.height),
                                   IntPoint(dest.x, dest.y));
            if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
              EnsureBufferOnWhite();
              MOZ_ASSERT(mDTBufferOnWhite);
              RefPtr<SourceSurface> sourceOnWhite = mDTBufferOnWhite->Snapshot();
              mDTBufferOnWhite->CopySurface(sourceOnWhite,
                                            IntRect(srcRect.x, srcRect.y, srcRect.width, srcRect.height),
                                            IntPoint(dest.x, dest.y));
            }
          } else {
            MOZ_ASSERT(mBuffer);
            mBuffer->MovePixels(srcRect, dest);
            if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
              EnsureBufferOnWhite();
              MOZ_ASSERT(mBufferOnWhite);
              mBufferOnWhite->MovePixels(srcRect, dest);
            }
          }
          result.mDidSelfCopy = true;
          // Don't set destBuffer; we special-case self-copies, and
          // just did the necessary work above.
          mBufferRect = destBufferRect;
        } else {
          // We can't do a real self-copy because the buffer is rotated.
          // So allocate a new buffer for the destination.
          destBufferRect = ComputeBufferRect(neededRegion.GetBounds());
          CreateBuffer(contentType, destBufferRect, bufferFlags,
                       getter_AddRefs(destBuffer), getter_AddRefs(destBufferOnWhite),
                       &destDTBuffer, &destDTBufferOnWhite);
          if (!destBuffer && !destDTBuffer)
            return result;
        }
      } else {
        mBufferRect = destBufferRect;
        mBufferRotation = newRotation;
      }
    } else {
      // No pixels are going to be kept. The whole visible region
      // will be redrawn, so we don't need to copy anything, so we don't
      // set destBuffer.
      mBufferRect = destBufferRect;
      mBufferRotation = nsIntPoint(0,0);
    }
  } else {
    // The buffer's not big enough, so allocate a new one
    CreateBuffer(contentType, destBufferRect, bufferFlags,
                 getter_AddRefs(destBuffer), getter_AddRefs(destBufferOnWhite),
                 &destDTBuffer, &destDTBufferOnWhite);
    if (!destBuffer && !destDTBuffer)
      return result;
  }

  NS_ASSERTION(!(aFlags & PAINT_WILL_RESAMPLE) || destBufferRect == neededRegion.GetBounds(),
               "If we're resampling, we need to validate the entire buffer");

  // If we have no buffered data already, then destBuffer will be a fresh buffer
  // and we do not need to clear it below.
  bool isClear = !HaveBuffer();

  if (destBuffer) {
    if (!isClear && (mode != Layer::SURFACE_COMPONENT_ALPHA || HaveBufferOnWhite())) {
      // Copy the bits
      nsRefPtr<gfxContext> tmpCtx = new gfxContext(destBuffer);
      nsIntPoint offset = -destBufferRect.TopLeft();
      tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
      tmpCtx->Translate(gfxPoint(offset.x, offset.y));
      EnsureBuffer();
      DrawBufferWithRotation(tmpCtx, BUFFER_BLACK);

      if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
        EnsureBufferOnWhite();
        NS_ASSERTION(destBufferOnWhite, "Must have a white buffer!");
        nsRefPtr<gfxContext> tmpCtx = new gfxContext(destBufferOnWhite);
        tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
        tmpCtx->Translate(gfxPoint(offset.x, offset.y));
        DrawBufferWithRotation(tmpCtx, BUFFER_WHITE);
      }
    }

    mBuffer = destBuffer.forget();
    mDTBuffer = nullptr;
    mBufferRect = destBufferRect;
    mBufferOnWhite = destBufferOnWhite.forget();
    mDTBufferOnWhite = nullptr;
    mBufferRotation = nsIntPoint(0,0);
  } else if (destDTBuffer) {
    if (!isClear && (mode != Layer::SURFACE_COMPONENT_ALPHA || HaveBufferOnWhite())) {
      // Copy the bits
      nsIntPoint offset = -destBufferRect.TopLeft();
      Matrix mat;
      mat.Translate(offset.x, offset.y);
      destDTBuffer->SetTransform(mat);
      EnsureBuffer();
      MOZ_ASSERT(mDTBuffer, "Have we got a Thebes buffer for some reason?");
      DrawBufferWithRotation(destDTBuffer, BUFFER_BLACK, 1.0, OP_SOURCE);
      destDTBuffer->SetTransform(Matrix());

      if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
        NS_ASSERTION(destDTBufferOnWhite, "Must have a white buffer!");
        destDTBufferOnWhite->SetTransform(mat);
        EnsureBufferOnWhite();
        MOZ_ASSERT(destDTBufferOnWhite, "Have we got a Thebes buffer for some reason?");
        DrawBufferWithRotation(destDTBufferOnWhite, BUFFER_WHITE, 1.0, OP_SOURCE);
        destDTBufferOnWhite->SetTransform(Matrix());
      }
    }

    mDTBuffer = destDTBuffer.forget();
    mBuffer = nullptr;
    mDTBufferOnWhite = destDTBufferOnWhite.forget();
    mBufferOnWhite = nullptr;
    mBufferRect = destBufferRect;
    mBufferRotation = nsIntPoint(0,0);
  }
  NS_ASSERTION(canHaveRotation || mBufferRotation == nsIntPoint(0,0),
               "Rotation disabled, but we have nonzero rotation?");

  nsIntRegion invalidate;
  invalidate.Sub(aLayer->GetValidRegion(), destBufferRect);
  result.mRegionToInvalidate.Or(result.mRegionToInvalidate, invalidate);

  nsIntPoint topLeft;
  result.mContext = GetContextForQuadrantUpdate(drawBounds, BUFFER_BOTH, &topLeft);

  if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
    if (IsAzureBuffer()) {
      MOZ_ASSERT(mDTBuffer && mDTBufferOnWhite);
      nsIntRegionRectIterator iter(result.mRegionToDraw);
      const nsIntRect *iterRect;
      while ((iterRect = iter.Next())) {
        mDTBuffer->FillRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height),
                            ColorPattern(Color(0.0, 0.0, 0.0, 1.0)));
        mDTBufferOnWhite->FillRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height),
                                   ColorPattern(Color(1.0, 1.0, 1.0, 1.0)));
      }
    } else {
      MOZ_ASSERT(mBuffer && mBufferOnWhite);
      FillSurface(mBuffer, result.mRegionToDraw, topLeft, gfxRGBA(0.0, 0.0, 0.0, 1.0));
      FillSurface(mBufferOnWhite, result.mRegionToDraw, topLeft, gfxRGBA(1.0, 1.0, 1.0, 1.0));
    }
    gfxUtils::ClipToRegionSnapped(result.mContext, result.mRegionToDraw);
  } else if (contentType == GFX_CONTENT_COLOR_ALPHA && !isClear) {
    if (IsAzureBuffer()) {
      nsIntRegionRectIterator iter(result.mRegionToDraw);
      const nsIntRect *iterRect;
      while ((iterRect = iter.Next())) {
        result.mContext->GetDrawTarget()->ClearRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height));
      }
      // Clear will do something expensive with a complex clip pushed, so clip
      // here.
      gfxUtils::ClipToRegionSnapped(result.mContext, result.mRegionToDraw);
    } else {
      MOZ_ASSERT(result.mContext->IsCairo());
      gfxUtils::ClipToRegionSnapped(result.mContext, result.mRegionToDraw);
      result.mContext->SetOperator(gfxContext::OPERATOR_CLEAR);
      result.mContext->Paint();
      result.mContext->SetOperator(gfxContext::OPERATOR_OVER);
    }
  } else {
    gfxUtils::ClipToRegionSnapped(result.mContext, result.mRegionToDraw);
  }

  return result;
}

}
}

