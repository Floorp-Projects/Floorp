/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RotatedBuffer.h"
#include <sys/types.h>                  // for int32_t
#include <algorithm>                    // for max
#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayersImpl.h"            // for ToData
#include "BufferUnrotate.h"             // for BufferUnrotate
#include "GeckoProfiler.h"              // for AUTO_PROFILER_LABEL
#include "Layers.h"                     // for PaintedLayer, Layer, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
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
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/Move.h"               // for Move
#include "mozilla/gfx/Point.h"          // for IntSize
#include "gfx2DGlue.h"
#include "nsLayoutUtils.h"              // for invalidation debugging
#include "PaintThread.h"

namespace mozilla {

using namespace gfx;

namespace layers {

void
BorrowDrawTarget::ReturnDrawTarget(gfx::DrawTarget*& aReturned)
{
  MOZ_ASSERT(mLoanedDrawTarget);
  MOZ_ASSERT(aReturned == mLoanedDrawTarget);
  if (mLoanedDrawTarget) {
    if (mSetTransform) {
      mLoanedDrawTarget->SetTransform(mLoanedTransform);
    }
    mLoanedDrawTarget = nullptr;
  }
  aReturned = nullptr;
}

IntRect
RotatedBuffer::GetQuadrantRectangle(XSide aXSide, YSide aYSide) const
{
  // quadrantTranslation is the amount we translate the top-left
  // of the quadrant by to get coordinates relative to the layer
  IntPoint quadrantTranslation = -mBufferRotation;
  quadrantTranslation.x += aXSide == LEFT ? mBufferRect.Width() : 0;
  quadrantTranslation.y += aYSide == TOP ? mBufferRect.Height() : 0;
  return mBufferRect + quadrantTranslation;
}

Rect
RotatedBuffer::GetSourceRectangle(XSide aXSide, YSide aYSide) const
{
  Rect result;
  if (aXSide == LEFT) {
    result.SetBoxX(0, mBufferRotation.x);
  } else {
    result.SetBoxX(mBufferRotation.x, mBufferRect.Width());
  }
  if (aYSide == TOP) {
    result.SetBoxY(0, mBufferRotation.y);
  } else {
    result.SetBoxY(mBufferRotation.y, mBufferRect.Height());
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
  IntRect quadrantRect = GetQuadrantRectangle(aXSide, aYSide);
  IntRect fillRect;
  if (!fillRect.IntersectRect(mBufferRect, quadrantRect))
    return;

  gfx::Point quadrantTranslation(quadrantRect.X(), quadrantRect.Y());

  MOZ_ASSERT(aSource != BUFFER_BOTH);
  RefPtr<SourceSurface> snapshot = GetSourceSurface(aSource);

  if (!snapshot) {
    gfxCriticalError() << "Invalid snapshot in RotatedBuffer::DrawBufferQuadrant";
    return;
  }

  // direct2d is much slower when using OP_SOURCE so use OP_OVER and
  // (maybe) a clear instead. Normally we need to draw in a single operation
  // (to avoid flickering) but direct2d is ok since it defers rendering.
  // We should try abstract this logic in a helper when we have other use
  // cases.
  if ((aTarget->GetBackendType() == BackendType::DIRECT2D ||
       aTarget->GetBackendType() == BackendType::DIRECT2D1_1) &&
      aOperator == CompositionOp::OP_SOURCE) {
    aOperator = CompositionOp::OP_OVER;
    if (snapshot->GetFormat() == SurfaceFormat::B8G8R8A8) {
      aTarget->ClearRect(IntRectToRect(fillRect));
    }
  }

  // OP_SOURCE is unbounded in Azure, and we really don't want that behaviour here.
  // We also can't do a ClearRect+FillRect since we need the drawing to happen
  // as an atomic operation (to prevent flickering).
  // We also need this clip in the case where we have a mask, since the mask surface
  // might cover more than fillRect, but we only want to touch the pixels inside
  // fillRect.
  aTarget->PushClipRect(IntRectToRect(fillRect));

  if (aMask) {
    Matrix oldTransform = aTarget->GetTransform();

    // Transform from user -> buffer space.
    Matrix transform =
      Matrix::Translation(quadrantTranslation.x, quadrantTranslation.y);

    Matrix inverseMask = *aMaskTransform;
    inverseMask.Invert();

    transform *= oldTransform;
    transform *= inverseMask;

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    SurfacePattern source(snapshot, ExtendMode::CLAMP, transform, SamplingFilter::POINT);
#else
    SurfacePattern source(snapshot, ExtendMode::CLAMP, transform);
#endif

    aTarget->SetTransform(*aMaskTransform);
    aTarget->MaskSurface(source, aMask, Point(0, 0), DrawOptions(aOpacity, aOperator));
    aTarget->SetTransform(oldTransform);
  } else {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    DrawSurfaceOptions options(SamplingFilter::POINT);
#else
    DrawSurfaceOptions options;
#endif
    aTarget->DrawSurface(snapshot, IntRectToRect(fillRect),
                         GetSourceRectangle(aXSide, aYSide),
                         options,
                         DrawOptions(aOpacity, aOperator));
  }

  aTarget->PopClip();
}

void
RotatedBuffer::DrawBufferWithRotation(gfx::DrawTarget *aTarget, ContextSource aSource,
                                      float aOpacity,
                                      gfx::CompositionOp aOperator,
                                      gfx::SourceSurface* aMask,
                                      const gfx::Matrix* aMaskTransform) const
{
  AUTO_PROFILER_LABEL("RotatedBuffer::DrawBufferWithRotation", GRAPHICS);

  // See above, in Azure Repeat should always be a safe, even faster choice
  // though! Particularly on D2D Repeat should be a lot faster, need to look
  // into that. TODO[Bas]
  DrawBufferQuadrant(aTarget, LEFT, TOP, aSource, aOpacity, aOperator, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, RIGHT, TOP, aSource, aOpacity, aOperator, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, LEFT, BOTTOM, aSource, aOpacity, aOperator, aMask, aMaskTransform);
  DrawBufferQuadrant(aTarget, RIGHT, BOTTOM, aSource, aOpacity, aOperator,aMask, aMaskTransform);
}

bool IsClippingCheap(gfx::DrawTarget* aTarget, const nsIntRegion& aRegion)
{
  // Assume clipping is cheap if the draw target just has an integer
  // translation, and the visible region is simple.
  return !aTarget->GetTransform().HasNonIntegerTranslation() &&
         aRegion.GetNumRects() <= 1;
}

void
RotatedBuffer::DrawTo(PaintedLayer* aLayer,
                      DrawTarget* aTarget,
                      float aOpacity,
                      CompositionOp aOp,
                      SourceSurface* aMask,
                      const Matrix* aMaskTransform)
{
  bool clipped = false;

  // If the entire buffer is valid, we can just draw the whole thing,
  // no need to clip. But we'll still clip if clipping is cheap ---
  // that might let us copy a smaller region of the buffer.
  // Also clip to the visible region if we're told to.
  if (!aLayer->GetValidRegion().Contains(BufferRect()) ||
      (ToData(aLayer)->GetClipToVisibleRegion() &&
       !aLayer->GetVisibleRegion().ToUnknownRegion().Contains(BufferRect())) ||
      IsClippingCheap(aTarget, aLayer->GetLocalVisibleRegion().ToUnknownRegion())) {
    // We don't want to draw invalid stuff, so we need to clip. Might as
    // well clip to the smallest area possible --- the visible region.
    // Bug 599189 if there is a non-integer-translation transform in aTarget,
    // we might sample pixels outside GetLocalVisibleRegion(), which is wrong
    // and may cause gray lines.
    gfxUtils::ClipToRegion(aTarget, aLayer->GetLocalVisibleRegion().ToUnknownRegion());
    clipped = true;
  }

  DrawBufferWithRotation(aTarget, BUFFER_BLACK, aOpacity, aOp, aMask, aMaskTransform);
  if (clipped) {
    aTarget->PopClip();
  }
}

void
RotatedBuffer::UpdateDestinationFrom(const RotatedBuffer& aSource,
                                     const gfx::IntRect& aUpdateRect)
{
  DrawIterator iter;
  while (DrawTarget* destDT =
    BorrowDrawTargetForQuadrantUpdate(aUpdateRect, BUFFER_BLACK, &iter)) {
    bool isClippingCheap = IsClippingCheap(destDT, iter.mDrawRegion);
    if (isClippingCheap) {
      gfxUtils::ClipToRegion(destDT, iter.mDrawRegion);
    }

    aSource.DrawBufferWithRotation(destDT, BUFFER_BLACK, 1.0, CompositionOp::OP_SOURCE);
    if (isClippingCheap) {
      destDT->PopClip();
    }
    ReturnDrawTarget(destDT);
  }

  if (aSource.HaveBufferOnWhite() && HaveBufferOnWhite()) {
    DrawIterator whiteIter;
    while (DrawTarget* destDT =
      BorrowDrawTargetForQuadrantUpdate(aUpdateRect, BUFFER_WHITE, &whiteIter)) {
      bool isClippingCheap = IsClippingCheap(destDT, whiteIter.mDrawRegion);
      if (isClippingCheap) {
        gfxUtils::ClipToRegion(destDT, whiteIter.mDrawRegion);
      }

      aSource.DrawBufferWithRotation(destDT, BUFFER_WHITE, 1.0, CompositionOp::OP_SOURCE);
      if (isClippingCheap) {
        destDT->PopClip();
      }
      ReturnDrawTarget(destDT);
    }
  }
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

bool
RotatedBuffer::Parameters::IsRotated() const
{
  return mBufferRotation != IntPoint(0,0);
}

bool
RotatedBuffer::Parameters::RectWrapsBuffer(const gfx::IntRect& aRect) const
{
  int32_t xBoundary = mBufferRect.XMost() - mBufferRotation.x;
  int32_t yBoundary = mBufferRect.YMost() - mBufferRotation.y;
  return (aRect.X() < xBoundary && xBoundary < aRect.XMost()) ||
         (aRect.Y() < yBoundary && yBoundary < aRect.YMost());
}

void
RotatedBuffer::Parameters::SetUnrotated()
{
  mBufferRotation = IntPoint(0,0);
  mDidSelfCopy = true;
}

RotatedBuffer::Parameters
RotatedBuffer::AdjustedParameters(const gfx::IntRect& aDestBufferRect) const
{
  IntRect keepArea;
  if (keepArea.IntersectRect(aDestBufferRect, mBufferRect)) {
    // Set mBufferRotation so that the pixels currently in mDTBuffer
    // will still be rendered in the right place when mBufferRect
    // changes to aDestBufferRect.
    IntPoint newRotation = mBufferRotation +
      (aDestBufferRect.TopLeft() - mBufferRect.TopLeft());
    WrapRotationAxis(&newRotation.x, mBufferRect.Width());
    WrapRotationAxis(&newRotation.y, mBufferRect.Height());
    NS_ASSERTION(gfx::IntRect(gfx::IntPoint(0,0), mBufferRect.Size()).Contains(newRotation),
                 "newRotation out of bounds");

    return Parameters{aDestBufferRect, newRotation};
  }

  // No pixels are going to be kept. The whole visible region
  // will be redrawn, so we don't need to copy anything, so we don't
  // set destBuffer.
  return Parameters{aDestBufferRect, IntPoint(0,0)};
}

bool
RotatedBuffer::UnrotateBufferTo(const Parameters& aParameters)
{
  RefPtr<gfx::DrawTarget> dtBuffer = GetDTBuffer();
  RefPtr<gfx::DrawTarget> dtBufferOnWhite = GetDTBufferOnWhite();

  if (mBufferRotation == IntPoint(0,0)) {
    IntRect srcRect(IntPoint(0, 0), mBufferRect.Size());
    IntPoint dest = mBufferRect.TopLeft() - aParameters.mBufferRect.TopLeft();

    MOZ_ASSERT(dtBuffer && dtBuffer->IsValid());
    dtBuffer->CopyRect(srcRect, dest);
    if (HaveBufferOnWhite()) {
      MOZ_ASSERT(dtBufferOnWhite && dtBufferOnWhite->IsValid());
      dtBufferOnWhite->CopyRect(srcRect, dest);
    }
  } else {
    // With azure and a data surface perform an buffer unrotate
    // (SelfCopy).
    unsigned char* data;
    IntSize size;
    int32_t stride;
    SurfaceFormat format;

    if (dtBuffer->LockBits(&data, &size, &stride, &format)) {
      uint8_t bytesPerPixel = BytesPerPixel(format);
      BufferUnrotate(data,
                     size.width * bytesPerPixel,
                     size.height, stride,
                     aParameters.mBufferRotation.x * bytesPerPixel,
                     aParameters.mBufferRotation.y);
      dtBuffer->ReleaseBits(data);

      if (HaveBufferOnWhite()) {
        MOZ_ASSERT(dtBufferOnWhite && dtBufferOnWhite->IsValid());
        dtBufferOnWhite->LockBits(&data, &size, &stride, &format);
        uint8_t bytesPerPixel = BytesPerPixel(format);
        BufferUnrotate(data,
                       size.width * bytesPerPixel,
                       size.height, stride,
                       aParameters.mBufferRotation.x * bytesPerPixel,
                       aParameters.mBufferRotation.y);
        dtBufferOnWhite->ReleaseBits(data);
      }
    } else {
      return false;
    }
  }
  return true;
}

void
RotatedBuffer::SetParameters(const RotatedBuffer::Parameters& aParameters)
{
  mBufferRect = aParameters.mBufferRect;
  mBufferRotation = aParameters.mBufferRotation;
  mDidSelfCopy = aParameters.mDidSelfCopy;
}

RotatedBuffer::ContentType
RotatedBuffer::GetContentType() const
{
  return ContentForFormat(GetFormat());
}

DrawTarget*
RotatedBuffer::BorrowDrawTargetForQuadrantUpdate(const IntRect& aBounds,
                                                 ContextSource aSource,
                                                 DrawIterator* aIter,
                                                 bool aSetTransform,
                                                 Matrix* aOutMatrix)
{
  IntRect bounds = aBounds;
  if (aIter) {
    // If an iterator was provided, then BeginPaint must have been run with
    // PAINT_CAN_DRAW_ROTATED, and the draw region might cover multiple quadrants.
    // Iterate over each of them, and return an appropriate buffer each time we find
    // one that intersects the draw region. The iterator mCount value tracks which
    // quadrants we have considered across multiple calls to this function.
    aIter->mDrawRegion.SetEmpty();
    while (aIter->mCount < 4) {
      IntRect quadrant = GetQuadrantRectangle((aIter->mCount & 1) ? LEFT : RIGHT,
        (aIter->mCount & 2) ? TOP : BOTTOM);
      aIter->mDrawRegion.And(aBounds, quadrant);
      aIter->mCount++;
      if (!aIter->mDrawRegion.IsEmpty()) {
        break;
      }
    }
    if (aIter->mDrawRegion.IsEmpty()) {
      return nullptr;
    }
    bounds = aIter->mDrawRegion.GetBounds();
  }

  gfx::DrawTarget* dtBuffer = GetDTBuffer();
  gfx::DrawTarget* dtBufferOnWhite = GetDTBufferOnWhite();

  MOZ_ASSERT(!mLoanedDrawTarget, "draw target has been borrowed and not returned");
  if (aSource == BUFFER_BOTH && HaveBufferOnWhite()) {
    MOZ_ASSERT(dtBuffer && dtBuffer->IsValid() && dtBufferOnWhite && dtBufferOnWhite->IsValid());
    mLoanedDrawTarget = Factory::CreateDualDrawTarget(dtBuffer, dtBufferOnWhite);
  } else if (aSource == BUFFER_WHITE) {
    mLoanedDrawTarget = dtBufferOnWhite;
  } else {
    // BUFFER_BLACK, or BUFFER_BOTH with a single buffer.
    mLoanedDrawTarget = dtBuffer;
  }

  // Figure out which quadrant to draw in
  int32_t xBoundary = mBufferRect.XMost() - mBufferRotation.x;
  int32_t yBoundary = mBufferRect.YMost() - mBufferRotation.y;
  XSide sideX = bounds.XMost() <= xBoundary ? RIGHT : LEFT;
  YSide sideY = bounds.YMost() <= yBoundary ? BOTTOM : TOP;
  IntRect quadrantRect = GetQuadrantRectangle(sideX, sideY);
  NS_ASSERTION(quadrantRect.Contains(bounds), "Messed up quadrants");

  if (aSetTransform) {
    mLoanedTransform = mLoanedDrawTarget->GetTransform();
    Matrix transform = Matrix(mLoanedTransform)
                            .PreTranslate(-quadrantRect.X(),
                                          -quadrantRect.Y());
    mLoanedDrawTarget->SetTransform(transform);
    mSetTransform = true;
  } else {
    MOZ_ASSERT(aOutMatrix);
    *aOutMatrix = Matrix::Translation(-quadrantRect.X(), -quadrantRect.Y());
    mSetTransform = false;
  }

  return mLoanedDrawTarget;
}

gfx::SurfaceFormat
RemoteRotatedBuffer::GetFormat() const
{
  return mClient->GetFormat();
}

bool
RemoteRotatedBuffer::IsLocked()
{
  return mClient->IsLocked();
}

bool
RemoteRotatedBuffer::Lock(OpenMode aMode)
{
  MOZ_ASSERT(!mTarget);
  MOZ_ASSERT(!mTargetOnWhite);

  bool locked = mClient->Lock(aMode) &&
                (!mClientOnWhite || mClientOnWhite->Lock(aMode));
  if (!locked) {
    Unlock();
    return false;
  }

  mTarget = mClient->BorrowDrawTarget();
  if (!mTarget || !mTarget->IsValid()) {
    gfxCriticalNote << "Invalid draw target " << hexa(mTarget)
                    << " in RemoteRotatedBuffer::Lock";
    Unlock();
    return false;
  }

  if (mClientOnWhite) {
    mTargetOnWhite = mClientOnWhite->BorrowDrawTarget();
    if (!mTargetOnWhite || !mTargetOnWhite->IsValid()) {
      gfxCriticalNote << "Invalid draw target(s) " << hexa(mTarget)
                      << " and " << hexa(mTargetOnWhite)
                      << " in RemoteRotatedBuffer::Lock";
      Unlock();
      return false;
    }
  }

  return true;
}

void
RemoteRotatedBuffer::Unlock()
{
  mTarget = nullptr;
  mTargetOnWhite = nullptr;

  if (mClient->IsLocked()) {
    mClient->Unlock();
  }
  if (mClientOnWhite && mClientOnWhite->IsLocked()) {
    mClientOnWhite->Unlock();
  }
}

void
RemoteRotatedBuffer::SyncWithObject(SyncObjectClient* aSyncObject)
{
  mClient->SyncWithObject(aSyncObject);
  if (mClientOnWhite) {
    mClientOnWhite->SyncWithObject(aSyncObject);
  }
}

void
RemoteRotatedBuffer::Clear()
{
  MOZ_ASSERT(!mTarget && !mTargetOnWhite);
  mClient = nullptr;
  mClientOnWhite = nullptr;
}

already_AddRefed<gfx::SourceSurface>
RemoteRotatedBuffer::GetSourceSurface(ContextSource aSource) const
{
  if (aSource == ContextSource::BUFFER_BLACK) {
    return mTarget->Snapshot();
  } else {
    MOZ_ASSERT(aSource == ContextSource::BUFFER_WHITE);
    return mTargetOnWhite->Snapshot();
  }
}

gfx::DrawTarget*
RemoteRotatedBuffer::GetDTBuffer() const
{
  return mTarget;
}

gfx::DrawTarget*
RemoteRotatedBuffer::GetDTBufferOnWhite() const
{
  return mTargetOnWhite;
}

gfx::SurfaceFormat
DrawTargetRotatedBuffer::GetFormat() const
{
  return mTarget->GetFormat();
}

already_AddRefed<gfx::SourceSurface>
DrawTargetRotatedBuffer::GetSourceSurface(ContextSource aSource) const
{
  if (aSource == ContextSource::BUFFER_BLACK) {
    return mTarget->Snapshot();
  } else {
    MOZ_ASSERT(aSource == ContextSource::BUFFER_WHITE);
    return mTargetOnWhite->Snapshot();
  }
}

gfx::DrawTarget*
DrawTargetRotatedBuffer::GetDTBuffer() const
{
  return mTarget;
}

gfx::DrawTarget*
DrawTargetRotatedBuffer::GetDTBufferOnWhite() const
{
  return mTargetOnWhite;
}

gfx::SurfaceFormat
SourceRotatedBuffer::GetFormat() const
{
  return mSource->GetFormat();
}

already_AddRefed<SourceSurface>
SourceRotatedBuffer::GetSourceSurface(ContextSource aSource) const
{
  RefPtr<SourceSurface> surf;
  if (aSource == BUFFER_BLACK) {
    surf = mSource;
  } else {
    MOZ_ASSERT(aSource == BUFFER_WHITE);
    surf = mSourceOnWhite;
  }

  MOZ_ASSERT(surf);
  return surf.forget();
}

} // namespace layers
} // namespace mozilla

