/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ThebesLayerBuffer.h"
#include "Layers.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "nsIDeviceContext.h"

namespace mozilla {
namespace layers {

static nsIntSize
ScaledSize(const nsIntSize& aSize, float aXScale, float aYScale)
{
  if (aXScale == 1.0 && aYScale == 1.0) {
    return aSize;
  }

  gfxRect rect(0, 0, aSize.width, aSize.height);
  rect.Scale(aXScale, aYScale);
  rect.RoundOut();
  return nsIntSize(rect.size.width, rect.size.height);
}

nsIntRect
ThebesLayerBuffer::GetQuadrantRectangle(XSide aXSide, YSide aYSide)
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
ThebesLayerBuffer::DrawBufferQuadrant(gfxContext* aTarget,
                                      XSide aXSide, YSide aYSide,
                                      float aOpacity,
                                      float aXRes, float aYRes)
{
  // The rectangle that we're going to fill. Basically we're going to
  // render the buffer at mBufferRect + quadrantTranslation to get the
  // pixels in the right place, but we're only going to paint within
  // mBufferRect
  nsIntRect quadrantRect = GetQuadrantRectangle(aXSide, aYSide);
  nsIntRect fillRect;
  if (!fillRect.IntersectRect(mBufferRect, quadrantRect))
    return;

  aTarget->NewPath();
  aTarget->Rectangle(gfxRect(fillRect.x, fillRect.y,
                             fillRect.width, fillRect.height),
                     PR_TRUE);

  gfxPoint quadrantTranslation(quadrantRect.x, quadrantRect.y);
  nsRefPtr<gfxPattern> pattern = new gfxPattern(mBuffer);

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  gfxPattern::GraphicsFilter filter = gfxPattern::FILTER_NEAREST;
  pattern->SetFilter(filter);
#endif

  gfxContextMatrixAutoSaveRestore saveMatrix(aTarget);

  // Transform from user -> buffer space.
  gfxMatrix transform;
  transform.Scale(aXRes, aYRes);
  transform.Translate(-quadrantTranslation);

  // in common cases the matrix after scaling by 1/aRes is close to 1.0,
  // so we want to make it 1.0 in both cases
  transform.Scale(1.0 / aXRes, 1.0 / aYRes);
  transform.NudgeToIntegers();

  gfxMatrix ctxMatrix = aTarget->CurrentMatrix();
  ctxMatrix.Scale(1.0 / aXRes, 1.0 / aYRes);
  ctxMatrix.NudgeToIntegers();
  aTarget->SetMatrix(ctxMatrix);

  pattern->SetMatrix(transform);
  aTarget->SetPattern(pattern);

  if (aOpacity != 1.0) {
    aTarget->Save();
    aTarget->Clip();
    aTarget->Paint(aOpacity);
    aTarget->Restore();
  } else {
    aTarget->Fill();
  }
}

void
ThebesLayerBuffer::DrawBufferWithRotation(gfxContext* aTarget, float aOpacity,
                                          float aXRes, float aYRes)
{
  // Draw four quadrants. We could use REPEAT_, but it's probably better
  // not to, to be performance-safe.
  DrawBufferQuadrant(aTarget, LEFT, TOP, aOpacity, aXRes, aYRes);
  DrawBufferQuadrant(aTarget, RIGHT, TOP, aOpacity, aXRes, aYRes);
  DrawBufferQuadrant(aTarget, LEFT, BOTTOM, aOpacity, aXRes, aYRes);
  DrawBufferQuadrant(aTarget, RIGHT, BOTTOM, aOpacity, aXRes, aYRes);
}

already_AddRefed<gfxContext>
ThebesLayerBuffer::GetContextForQuadrantUpdate(const nsIntRect& aBounds,
                                               float aXResolution,
                                               float aYResolution)
{
  nsRefPtr<gfxContext> ctx = new gfxContext(mBuffer);

  // Figure out which quadrant to draw in
  PRInt32 xBoundary = mBufferRect.XMost() - mBufferRotation.x;
  PRInt32 yBoundary = mBufferRect.YMost() - mBufferRotation.y;
  XSide sideX = aBounds.XMost() <= xBoundary ? RIGHT : LEFT;
  YSide sideY = aBounds.YMost() <= yBoundary ? BOTTOM : TOP;
  nsIntRect quadrantRect = GetQuadrantRectangle(sideX, sideY);
  NS_ASSERTION(quadrantRect.Contains(aBounds), "Messed up quadrants");
  ctx->Scale(aXResolution, aYResolution);
  ctx->Translate(-gfxPoint(quadrantRect.x, quadrantRect.y));

  return ctx.forget();
}

// Move the pixels in aBuffer specified by |aSourceRect| to |aDest|.
// |aSourceRect| and |aDest| are in the space of |aBuffer|, but
// unscaled by the resolution.  This helper does the scaling.
static void
MovePixels(gfxASurface* aBuffer,
           const nsIntRect& aSourceRect, const nsIntPoint& aDest,
           float aXResolution, float aYResolution)
{
  gfxRect src(aSourceRect.x, aSourceRect.y, aSourceRect.width, aSourceRect.height);
  gfxRect dest(aDest.x, aDest.y,  aSourceRect.width, aSourceRect.height);
  src.Scale(aXResolution, aYResolution);
  dest.Scale(aXResolution, aYResolution);

#ifdef DEBUG
  // If we're doing a self-copy, enforce that the rects we're copying
  // were computed in order to round to device pixels.  If the rects
  // we're moving *weren't* computed to round, then glitches like
  // seaming are likely.  Assume that the precision of these
  // computations is 1 app unit, and toss in a fudge factor of 2.0.
  static const gfxFloat kPrecision =
    1.0 / gfxFloat(nsIDeviceContext::AppUnitsPerCSSPixel());
  // FIXME/bug 637852: we've decided to live with transient glitches
  // during fast-panning for the time being.
  NS_WARN_IF_FALSE(
    src.WithinEpsilonOfIntegerPixels(2.0 * kPrecision * aXResolution) &&
    dest.WithinEpsilonOfIntegerPixels(2.0 * kPrecision * aXResolution),
    "Rects don't round to device pixels within precision; glitches likely to follow");
#endif

  src.Round();
  dest.Round();

  aBuffer->MovePixels(nsIntRect(src.pos.x, src.pos.y,
                                src.size.width, src.size.height),
                      nsIntPoint(dest.pos.x, dest.pos.y));
}

static void
WrapRotationAxis(PRInt32* aRotationPoint, PRInt32 aSize)
{
  if (*aRotationPoint < 0) {
    *aRotationPoint += aSize;
  } else if (*aRotationPoint >= aSize) {
    *aRotationPoint -= aSize;
  }
}

ThebesLayerBuffer::PaintState
ThebesLayerBuffer::BeginPaint(ThebesLayer* aLayer, ContentType aContentType,
                              float aXResolution, float aYResolution,
                              PRUint32 aFlags)
{
  PaintState result;
  float curXRes = aLayer->GetXResolution();
  float curYRes = aLayer->GetYResolution();

  nsIntRegion validRegion = aLayer->GetValidRegion();

  ContentType contentType;
  nsIntRegion neededRegion;
  nsIntSize destBufferDims;
  PRBool canReuseBuffer;
  nsIntRect destBufferRect;

  while (PR_TRUE) {
    contentType = aContentType;
    neededRegion = aLayer->GetVisibleRegion();
    destBufferDims = ScaledSize(neededRegion.GetBounds().Size(),
                                aXResolution, aYResolution);
    canReuseBuffer = BufferSizeOkFor(destBufferDims);

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
      destBufferRect = neededRegion.GetBounds();
    }

    if ((aFlags & PAINT_WILL_RESAMPLE) &&
        (neededRegion.GetBounds() != destBufferRect ||
         neededRegion.GetNumRects() > 1)) {
      // The area we add to neededRegion might not be painted opaquely
      contentType = gfxASurface::CONTENT_COLOR_ALPHA;

      // We need to validate the entire buffer, to make sure that only valid
      // pixels are sampled
      neededRegion = destBufferRect;
      destBufferDims = ScaledSize(neededRegion.GetBounds().Size(),
                                  aXResolution, aYResolution);
    }

    if (mBuffer &&
        (contentType != mBuffer->GetContentType() ||
         aXResolution != curXRes || aYResolution != curYRes)) {
      // We're effectively clearing the valid region, so we need to draw
      // the entire needed region now.
      //
      // XXX/cjones: a possibly worthwhile optimization to keep in mind
      // is to re-use buffers when the resolution and visible region
      // have changed in such a way that the buffer size stays the same.
      // It might make even more sense to allocate buffers from a
      // recyclable pool, so that we could keep this logic simple and
      // still get back the same buffer.
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

  // If we have non-identity resolution then mBufferRotation might not fall
  // on a buffer pixel boundary, in which case that row of pixels will contain
  // a mix of two completely different rows of the layer, which would be
  // a catastrophe. So disable rotation in that case.
  // We also need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  PRBool canHaveRotation =
    !(aFlags & PAINT_WILL_RESAMPLE) && aXResolution == 1.0 && aYResolution == 1.0;
  nsIntRect drawBounds = result.mRegionToDraw.GetBounds();
  nsRefPtr<gfxASurface> destBuffer;
  PRBool bufferDimsChanged = PR_FALSE;
  if (canReuseBuffer) {
    NS_ASSERTION(curXRes == aXResolution && curYRes == aYResolution,
                 "resolution changes must Clear()!");

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
      PRInt32 xBoundary = destBufferRect.XMost() - newRotation.x;
      PRInt32 yBoundary = destBufferRect.YMost() - newRotation.y;
      if ((drawBounds.x < xBoundary && xBoundary < drawBounds.XMost()) ||
          (drawBounds.y < yBoundary && yBoundary < drawBounds.YMost()) ||
          (newRotation != nsIntPoint(0,0) && !canHaveRotation)) {
        // The stuff we need to redraw will wrap around an edge of the
        // buffer, so move the pixels we can keep into a position that
        // lets us redraw in just one quadrant.
        if (mBufferRotation == nsIntPoint(0,0)) {
          nsIntRect srcRect(nsIntPoint(0, 0), mBufferRect.Size());
          nsIntPoint dest = mBufferRect.TopLeft() - destBufferRect.TopLeft();
          MovePixels(mBuffer, srcRect, dest, curXRes, curYRes);
          // Don't set destBuffer; we special-case self-copies, and
          // just did the necessary work above.
          mBufferRect = destBufferRect;
        } else {
          // We can't do a real self-copy because the buffer is rotated.
          // So allocate a new buffer for the destination.
          destBufferRect = neededRegion.GetBounds();
          bufferDimsChanged = PR_TRUE;
          destBuffer = CreateBuffer(contentType, destBufferDims);
          if (!destBuffer)
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
    destBufferRect = neededRegion.GetBounds();
    bufferDimsChanged = PR_TRUE;
    destBuffer = CreateBuffer(contentType, destBufferDims);
    if (!destBuffer)
      return result;
  }
  NS_ASSERTION(!(aFlags & PAINT_WILL_RESAMPLE) || destBufferRect == neededRegion.GetBounds(),
               "If we're resampling, we need to validate the entire buffer");

  // If we have no buffered data already, then destBuffer will be a fresh buffer
  // and we do not need to clear it below.
  PRBool isClear = mBuffer == nsnull;

  if (destBuffer) {
    if (mBuffer) {
      // Copy the bits
      nsRefPtr<gfxContext> tmpCtx = new gfxContext(destBuffer);
      nsIntPoint offset = -destBufferRect.TopLeft();
      tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
      tmpCtx->Scale(aXResolution, aYResolution);
      tmpCtx->Translate(gfxPoint(offset.x, offset.y));
      NS_ASSERTION(curXRes == aXResolution && curYRes == aYResolution,
                   "resolution changes must Clear()!");
      DrawBufferWithRotation(tmpCtx, 1.0, aXResolution, aYResolution);
    }

    mBuffer = destBuffer.forget();
    mBufferRect = destBufferRect;
    mBufferRotation = nsIntPoint(0,0);
  }
  if (bufferDimsChanged) {
    mBufferDims = destBufferDims;
  }
  NS_ASSERTION(canHaveRotation || mBufferRotation == nsIntPoint(0,0),
               "Rotation disabled, but we have nonzero rotation?");

  nsIntRegion invalidate;
  invalidate.Sub(aLayer->GetValidRegion(), destBufferRect);
  result.mRegionToInvalidate.Or(result.mRegionToInvalidate, invalidate);

  result.mContext = GetContextForQuadrantUpdate(drawBounds,
                                                aXResolution, aYResolution);

  gfxUtils::ClipToRegionSnapped(result.mContext, result.mRegionToDraw);
  if (contentType == gfxASurface::CONTENT_COLOR_ALPHA && !isClear) {
    result.mContext->SetOperator(gfxContext::OPERATOR_CLEAR);
    result.mContext->Paint();
    result.mContext->SetOperator(gfxContext::OPERATOR_OVER);
  }
  return result;
}

}
}

