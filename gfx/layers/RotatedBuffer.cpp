/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RotatedBuffer.h"
#include <sys/types.h>                  // for int32_t
#include <algorithm>                    // for max
#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayersImpl.h"            // for ToData
#include "BufferUnrotate.h"             // for BufferUnrotate
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "Layers.h"                     // for ThebesLayer, Layer, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxUtils.h"                   // for gfxUtils
#include "mozilla/ArrayUtils.h"         // for ArrayLength
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Matrix.h"         // for Matrix
#include "mozilla/gfx/Point.h"          // for Point, IntPoint
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for ExtendMode::ExtendMode::CLAMP, etc
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

Rect
RotatedBuffer::GetSourceRectangle(XSide aXSide, YSide aYSide) const
{
  Rect result;
  if (aXSide == LEFT) {
    result.x = 0;
    result.width = mBufferRotation.x;
  } else {
    result.x = mBufferRotation.x;
    result.width = mBufferRect.width - mBufferRotation.x;
  }
  if (aYSide == TOP) {
    result.y = 0;
    result.height = mBufferRotation.y;
  } else {
    result.y = mBufferRotation.y;
    result.height = mBufferRect.height - mBufferRotation.y;
  }
  return result;
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

  MOZ_ASSERT(aOperator == CompositionOp::OP_OVER || aOperator == CompositionOp::OP_SOURCE);
  // direct2d is much slower when using OP_SOURCE so use OP_OVER and
  // (maybe) a clear instead. Normally we need to draw in a single operation
  // (to avoid flickering) but direct2d is ok since it defers rendering.
  // We should try abstract this logic in a helper when we have other use
  // cases.
  if (aTarget->GetType() == BackendType::DIRECT2D && aOperator == CompositionOp::OP_SOURCE) {
    aOperator = CompositionOp::OP_OVER;
    if (mDTBuffer->GetFormat() == SurfaceFormat::B8G8R8A8) {
      aTarget->ClearRect(ToRect(fillRect));
    }
  }

  RefPtr<gfx::SourceSurface> snapshot;
  if (aSource == BUFFER_BLACK) {
    snapshot = mDTBuffer->Snapshot();
  } else {
    MOZ_ASSERT(aSource == BUFFER_WHITE);
    snapshot = mDTBufferOnWhite->Snapshot();
  }

  if (aOperator == CompositionOp::OP_SOURCE) {
    // OP_SOURCE is unbounded in Azure, and we really don't want that behaviour here.
    // We also can't do a ClearRect+FillRect since we need the drawing to happen
    // as an atomic operation (to prevent flickering).
    aTarget->PushClipRect(gfx::Rect(fillRect.x, fillRect.y,
                                    fillRect.width, fillRect.height));
  }

  if (aMask) {
    // Transform from user -> buffer space.
    Matrix transform;
    transform.Translate(quadrantTranslation.x, quadrantTranslation.y);

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    SurfacePattern source(snapshot, ExtendMode::CLAMP, transform, Filter::POINT);
#else
    SurfacePattern source(snapshot, ExtendMode::CLAMP, transform);
#endif

    Matrix oldTransform = aTarget->GetTransform();
    aTarget->SetTransform(*aMaskTransform);
    aTarget->MaskSurface(source, aMask, Point(0, 0), DrawOptions(aOpacity, aOperator));
    aTarget->SetTransform(oldTransform);
  } else {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    DrawSurfaceOptions options(Filter::POINT);
#else
    DrawSurfaceOptions options;
#endif
    aTarget->DrawSurface(snapshot, ToRect(fillRect),
                         GetSourceRectangle(aXSide, aYSide),
                         options,
                         DrawOptions(aOpacity, aOperator));
  }

  if (aOperator == CompositionOp::OP_SOURCE) {
    aTarget->PopClip();
  }
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
RotatedContentBuffer::IsClippingCheap(DrawTarget* aTarget, const nsIntRegion& aRegion)
{
  // Assume clipping is cheap if the draw target just has an integer
  // translation, and the visible region is simple.
  return !aTarget->GetTransform().HasNonIntegerTranslation() &&
         aRegion.GetNumRects() <= 1;
}

void
RotatedContentBuffer::DrawTo(ThebesLayer* aLayer,
                             DrawTarget* aTarget,
                             float aOpacity,
                             CompositionOp aOp,
                             gfxASurface* aMask,
                             const Matrix* aMaskTransform)
{
  if (!EnsureBuffer()) {
    return;
  }

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
    gfxUtils::ClipToRegionSnapped(aTarget, aLayer->GetEffectiveVisibleRegion());
    clipped = true;
  }

  RefPtr<gfx::SourceSurface> mask;
  if (aMask) {
    mask = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(aTarget, aMask);
  }

  Matrix maskTransform;
  if (aMaskTransform) {
    maskTransform = *aMaskTransform;
  }

  DrawBufferWithRotation(aTarget, BUFFER_BLACK, aOpacity, aOp, mask, &maskTransform);
  if (clipped) {
    aTarget->PopClip();
  }
}

DrawTarget*
RotatedContentBuffer::BorrowDrawTargetForQuadrantUpdate(const nsIntRect& aBounds,
                                                        ContextSource aSource)
{
  if (!EnsureBuffer()) {
    return nullptr;
  }

  MOZ_ASSERT(!mLoanedDrawTarget, "draw target has been borrowed and not returned");
  if (aSource == BUFFER_BOTH && HaveBufferOnWhite()) {
    if (!EnsureBufferOnWhite()) {
      return nullptr;
    }
    MOZ_ASSERT(mDTBuffer && mDTBufferOnWhite);
    mLoanedDrawTarget = Factory::CreateDualDrawTarget(mDTBuffer, mDTBufferOnWhite);
  } else if (aSource == BUFFER_WHITE) {
    if (!EnsureBufferOnWhite()) {
      return nullptr;
    }
    mLoanedDrawTarget = mDTBufferOnWhite;
  } else {
    // BUFFER_BLACK, or BUFFER_BOTH with a single buffer.
    mLoanedDrawTarget = mDTBuffer;
  }

  // Figure out which quadrant to draw in
  int32_t xBoundary = mBufferRect.XMost() - mBufferRotation.x;
  int32_t yBoundary = mBufferRect.YMost() - mBufferRotation.y;
  XSide sideX = aBounds.XMost() <= xBoundary ? RIGHT : LEFT;
  YSide sideY = aBounds.YMost() <= yBoundary ? BOTTOM : TOP;
  nsIntRect quadrantRect = GetQuadrantRectangle(sideX, sideY);
  NS_ASSERTION(quadrantRect.Contains(aBounds), "Messed up quadrants");

  mLoanedTransform = mLoanedDrawTarget->GetTransform();
  mLoanedTransform.Translate(-quadrantRect.x, -quadrantRect.y);
  mLoanedDrawTarget->SetTransform(mLoanedTransform);
  mLoanedTransform.Translate(quadrantRect.x, quadrantRect.y);

  return mLoanedDrawTarget;
}

void
BorrowDrawTarget::ReturnDrawTarget(gfx::DrawTarget*& aReturned)
{
  MOZ_ASSERT(aReturned == mLoanedDrawTarget);
  mLoanedDrawTarget->SetTransform(mLoanedTransform);
  mLoanedDrawTarget = nullptr;
  aReturned = nullptr;
}

gfxContentType
RotatedContentBuffer::BufferContentType()
{
  if (mDeprecatedBufferProvider) {
    return mDeprecatedBufferProvider->GetContentType();
  }
  if (mBufferProvider || mDTBuffer) {
    SurfaceFormat format;

    if (mBufferProvider) {
      format = mBufferProvider->AsTextureClientDrawTarget()->GetFormat();
    } else if (mDTBuffer) {
      format = mDTBuffer->GetFormat();
    }

    return ContentForFormat(format);
  }
  return gfxContentType::SENTINEL;
}

bool
RotatedContentBuffer::BufferSizeOkFor(const nsIntSize& aSize)
{
  return (aSize == mBufferRect.Size() ||
          (SizedToVisibleBounds != mBufferSizePolicy &&
           aSize < mBufferRect.Size()));
}

bool
RotatedContentBuffer::EnsureBuffer()
{
  NS_ASSERTION(!mLoanedDrawTarget, "Loaned draw target must be returned");
  if (!mDTBuffer) {
    if (mDeprecatedBufferProvider) {
      mDTBuffer = mDeprecatedBufferProvider->LockDrawTarget();
    } else if (mBufferProvider) {
      mDTBuffer = mBufferProvider->AsTextureClientDrawTarget()->GetAsDrawTarget();
    }
  }

  NS_WARN_IF_FALSE(mDTBuffer, "no buffer");
  return !!mDTBuffer;
}

bool
RotatedContentBuffer::EnsureBufferOnWhite()
{
  NS_ASSERTION(!mLoanedDrawTarget, "Loaned draw target must be returned");
  if (!mDTBufferOnWhite) {
    if (mDeprecatedBufferProviderOnWhite) {
      mDTBufferOnWhite = mDeprecatedBufferProviderOnWhite->LockDrawTarget();
    } else if (mBufferProviderOnWhite) {
      mDTBufferOnWhite =
        mBufferProviderOnWhite->AsTextureClientDrawTarget()->GetAsDrawTarget();
    }
  }

  NS_WARN_IF_FALSE(mDTBufferOnWhite, "no buffer");
  return mDTBufferOnWhite;
}

bool
RotatedContentBuffer::HaveBuffer() const
{
  return mDTBuffer || mDeprecatedBufferProvider || mBufferProvider;
}

bool
RotatedContentBuffer::HaveBufferOnWhite() const
{
  return mDTBufferOnWhite || mDeprecatedBufferProviderOnWhite || mBufferProviderOnWhite;
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

void
RotatedContentBuffer::FlushBuffers()
{
  if (mDTBuffer) {
    mDTBuffer->Flush();
  }
  if (mDTBufferOnWhite) {
    mDTBufferOnWhite->Flush();
  }
}

RotatedContentBuffer::PaintState
RotatedContentBuffer::BeginPaint(ThebesLayer* aLayer,
                                 uint32_t aFlags)
{
  PaintState result;
  // We need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  bool canHaveRotation = gfxPlatform::BufferRotationEnabled() &&
                         !(aFlags & (PAINT_WILL_RESAMPLE | PAINT_NO_ROTATION));

  nsIntRegion validRegion = aLayer->GetValidRegion();

  bool canUseOpaqueSurface = aLayer->CanUseOpaqueSurface();
  ContentType layerContentType =
    canUseOpaqueSurface ? gfxContentType::COLOR :
                          gfxContentType::COLOR_ALPHA;

  SurfaceMode mode;
  nsIntRegion neededRegion;
  bool canReuseBuffer;
  nsIntRect destBufferRect;

  while (true) {
    mode = aLayer->GetSurfaceMode();
    neededRegion = aLayer->GetVisibleRegion();
    canReuseBuffer = HaveBuffer() && BufferSizeOkFor(neededRegion.GetBounds().Size());
    result.mContentType = layerContentType;

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

    if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
#if defined(MOZ_GFX_OPTIMIZE_MOBILE) || defined(MOZ_WIDGET_GONK)
      mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
#else
      if (!aLayer->GetParent() ||
          !aLayer->GetParent()->SupportsComponentAlphaChildren() ||
          !aLayer->Manager()->IsCompositingCheap() ||
          !aLayer->AsShadowableLayer() ||
          !aLayer->AsShadowableLayer()->HasShadow() ||
          !gfxPlatform::ComponentAlphaEnabled()) {
        mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      } else {
        result.mContentType = gfxContentType::COLOR;
      }
#endif
    }

    if ((aFlags & PAINT_WILL_RESAMPLE) &&
        (!neededRegion.GetBounds().IsEqualInterior(destBufferRect) ||
         neededRegion.GetNumRects() > 1)) {
      // The area we add to neededRegion might not be painted opaquely
      if (mode == SurfaceMode::SURFACE_OPAQUE) {
        result.mContentType = gfxContentType::COLOR_ALPHA;
        mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      }

      // We need to validate the entire buffer, to make sure that only valid
      // pixels are sampled
      neededRegion = destBufferRect;
    }

    // If we have an existing buffer, but the content type has changed or we
    // have transitioned into/out of component alpha, then we need to recreate it.
    if (HaveBuffer() &&
        (result.mContentType != BufferContentType() ||
        (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) != HaveBufferOnWhite())) {

      // We're effectively clearing the valid region, so we need to draw
      // the entire needed region now.
      result.mRegionToInvalidate = aLayer->GetValidRegion();
      validRegion.SetEmpty();
      Clear();
      // Restart decision process with the cleared buffer. We can only go
      // around the loop one more iteration, since mDTBuffer is null now.
      continue;
    }

    break;
  }

  NS_ASSERTION(destBufferRect.Contains(neededRegion.GetBounds()),
               "Destination rect doesn't contain what we need to paint");

  result.mRegionToDraw.Sub(neededRegion, validRegion);

  // Do not modify result.mRegionToDraw or result.mContentType after this call.
  // Do not modify mBufferRect, mBufferRotation, or mDidSelfCopy,
  // or call CreateBuffer before this call.
  FinalizeFrame(result.mRegionToDraw);

  if (result.mRegionToDraw.IsEmpty())
    return result;

  nsIntRect drawBounds = result.mRegionToDraw.GetBounds();
  RefPtr<DrawTarget> destDTBuffer;
  RefPtr<DrawTarget> destDTBufferOnWhite;
  uint32_t bufferFlags = canHaveRotation ? ALLOW_REPEAT : 0;
  if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
    bufferFlags |= BUFFER_COMPONENT_ALPHA;
  }
  if (canReuseBuffer) {
    if (!EnsureBuffer()) {
      return result;
    }
    nsIntRect keepArea;
    if (keepArea.IntersectRect(destBufferRect, mBufferRect)) {
      // Set mBufferRotation so that the pixels currently in mDTBuffer
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
          MOZ_ASSERT(mDTBuffer);
          mDTBuffer->CopyRect(IntRect(srcRect.x, srcRect.y, srcRect.width, srcRect.height),
                              IntPoint(dest.x, dest.y));
          if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
            if (!EnsureBufferOnWhite()) {
              return result;
            }
            MOZ_ASSERT(mDTBufferOnWhite);
            mDTBufferOnWhite->CopyRect(IntRect(srcRect.x, srcRect.y, srcRect.width, srcRect.height),
                                       IntPoint(dest.x, dest.y));
          }
          result.mDidSelfCopy = true;
          mDidSelfCopy = true;
          // Don't set destBuffer; we special-case self-copies, and
          // just did the necessary work above.
          mBufferRect = destBufferRect;
        } else {
          // With azure and a data surface perform an buffer unrotate
          // (SelfCopy).
          unsigned char* data;
          IntSize size;
          int32_t stride;
          SurfaceFormat format;

          if (mDTBuffer->LockBits(&data, &size, &stride, &format)) {
            uint8_t bytesPerPixel = BytesPerPixel(format);
            BufferUnrotate(data,
                           size.width * bytesPerPixel,
                           size.height, stride,
                           newRotation.x * bytesPerPixel, newRotation.y);
            mDTBuffer->ReleaseBits(data);

            if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
              if (!EnsureBufferOnWhite()) {
                return result;
              }
              MOZ_ASSERT(mDTBufferOnWhite);
              mDTBufferOnWhite->LockBits(&data, &size, &stride, &format);
              uint8_t bytesPerPixel = BytesPerPixel(format);
              BufferUnrotate(data,
                             size.width * bytesPerPixel,
                             size.height, stride,
                             newRotation.x * bytesPerPixel, newRotation.y);
              mDTBufferOnWhite->ReleaseBits(data);
            }

            // Buffer unrotate moves all the pixels, note that
            // we self copied for SyncBackToFrontBuffer
            result.mDidSelfCopy = true;
            mDidSelfCopy = true;
            mBufferRect = destBufferRect;
            mBufferRotation = nsIntPoint(0, 0);
          }

          if (!result.mDidSelfCopy) {
            destBufferRect = ComputeBufferRect(neededRegion.GetBounds());
            CreateBuffer(result.mContentType, destBufferRect, bufferFlags,
                         &destDTBuffer, &destDTBufferOnWhite);
            if (!destDTBuffer) {
              return result;
            }
          }
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
    CreateBuffer(result.mContentType, destBufferRect, bufferFlags,
                 &destDTBuffer, &destDTBufferOnWhite);
    if (!destDTBuffer) {
      return result;
    }
  }

  NS_ASSERTION(!(aFlags & PAINT_WILL_RESAMPLE) || destBufferRect == neededRegion.GetBounds(),
               "If we're resampling, we need to validate the entire buffer");

  // If we have no buffered data already, then destBuffer will be a fresh buffer
  // and we do not need to clear it below.
  bool isClear = !HaveBuffer();

  if (destDTBuffer) {
    if (!isClear && (mode != SurfaceMode::SURFACE_COMPONENT_ALPHA || HaveBufferOnWhite())) {
      // Copy the bits
      nsIntPoint offset = -destBufferRect.TopLeft();
      Matrix mat;
      mat.Translate(offset.x, offset.y);
      destDTBuffer->SetTransform(mat);
      if (!EnsureBuffer()) {
        return result;
      }
       MOZ_ASSERT(mDTBuffer, "Have we got a Thebes buffer for some reason?");
      DrawBufferWithRotation(destDTBuffer, BUFFER_BLACK, 1.0, CompositionOp::OP_SOURCE);
      destDTBuffer->SetTransform(Matrix());

      if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
        NS_ASSERTION(destDTBufferOnWhite, "Must have a white buffer!");
        destDTBufferOnWhite->SetTransform(mat);
        if (!EnsureBufferOnWhite()) {
          return result;
        }
        MOZ_ASSERT(mDTBufferOnWhite, "Have we got a Thebes buffer for some reason?");
        DrawBufferWithRotation(destDTBufferOnWhite, BUFFER_WHITE, 1.0, CompositionOp::OP_SOURCE);
        destDTBufferOnWhite->SetTransform(Matrix());
      }
    }

    mDTBuffer = destDTBuffer.forget();
    mDTBufferOnWhite = destDTBufferOnWhite.forget();
    mBufferRect = destBufferRect;
    mBufferRotation = nsIntPoint(0,0);
  }
  NS_ASSERTION(canHaveRotation || mBufferRotation == nsIntPoint(0,0),
               "Rotation disabled, but we have nonzero rotation?");

  nsIntRegion invalidate;
  invalidate.Sub(aLayer->GetValidRegion(), destBufferRect);
  result.mRegionToInvalidate.Or(result.mRegionToInvalidate, invalidate);
  result.mClip = DrawRegionClip::DRAW_SNAPPED;
  result.mMode = mode;

  return result;
}

DrawTarget*
RotatedContentBuffer::BorrowDrawTargetForPainting(ThebesLayer* aLayer,
                                                  const PaintState& aPaintState)
{
  if (aPaintState.mMode == SurfaceMode::SURFACE_NONE) {
    return nullptr;
  }

  DrawTarget* result = BorrowDrawTargetForQuadrantUpdate(aPaintState.mRegionToDraw.GetBounds(),
                                                         BUFFER_BOTH);

  if (aPaintState.mMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
    MOZ_ASSERT(mDTBuffer && mDTBufferOnWhite);
    nsIntRegionRectIterator iter(aPaintState.mRegionToDraw);
    const nsIntRect *iterRect;
    while ((iterRect = iter.Next())) {
      mDTBuffer->FillRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height),
                          ColorPattern(Color(0.0, 0.0, 0.0, 1.0)));
      mDTBufferOnWhite->FillRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height),
                                 ColorPattern(Color(1.0, 1.0, 1.0, 1.0)));
    }
  } else if (aPaintState.mContentType == gfxContentType::COLOR_ALPHA && HaveBuffer()) {
    // HaveBuffer() => we have an existing buffer that we must clear
    nsIntRegionRectIterator iter(aPaintState.mRegionToDraw);
    const nsIntRect *iterRect;
    while ((iterRect = iter.Next())) {
      result->ClearRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height));
    }
  }

  return result;
}

}
}

