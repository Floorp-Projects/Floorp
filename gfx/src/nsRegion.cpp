/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRegion.h"
#include "nsTArray.h"
#include "gfxUtils.h"
#include "mozilla/ToString.h"

using namespace std;

void nsRegion::AssertStateInternal() const {
  bool failed = false;
  // Verify consistent state inside the region.
  int32_t lastY = INT32_MIN;
  int32_t lowestX = INT32_MAX;
  int32_t highestX = INT32_MIN;
  for (auto iter = mBands.begin(); iter != mBands.end(); iter++) {
    const Band& band = *iter;
    if (band.bottom <= band.top) {
      failed = true;
      break;
    }
    if (band.top < lastY) {
      failed = true;
      break;
    }
    lastY = band.bottom;

    lowestX = std::min(lowestX, band.mStrips.begin()->left);
    highestX = std::max(highestX, band.mStrips.LastElement().right);

    int32_t lastX = INT32_MIN;
    if (iter != mBands.begin()) {
      auto prev = iter;
      prev--;

      if (prev->bottom == iter->top) {
        if (band.EqualStrips(*prev)) {
          failed = true;
          break;
        }
      }
    }
    for (const Strip& strip : band.mStrips) {
      if (strip.right <= strip.left) {
        failed = true;
        break;
      }
      if (strip.left <= lastX) {
        failed = true;
        break;
      }
      lastX = strip.right;
    }
    if (failed) {
      break;
    }
  }

  if (!(mBounds.IsEqualEdges(CalculateBounds()))) {
    failed = true;
  }

  if (failed) {
#ifdef DEBUG_REGIONS
    if (mCurrentOpGenerator) {
      mCurrentOpGenerator->OutputOp();
    }
#endif
    MOZ_ASSERT(false);
  }
}

bool nsRegion::Contains(const nsRegion& aRgn) const {
  // XXX this could be made faster by iterating over
  // both regions at the same time some how
  for (auto iter = aRgn.RectIter(); !iter.Done(); iter.Next()) {
    if (!Contains(iter.Get())) {
      return false;
    }
  }
  return true;
}

bool nsRegion::Intersects(const nsRectAbsolute& aRect) const {
  if (mBands.IsEmpty()) {
    return mBounds.Intersects(aRect);
  }

  if (!mBounds.Intersects(aRect)) {
    return false;
  }

  Strip rectStrip(aRect.X(), aRect.XMost());

  auto iter = mBands.begin();
  while (iter != mBands.end()) {
    if (iter->top >= aRect.YMost()) {
      return false;
    }

    if (iter->bottom <= aRect.Y()) {
      // This band is entirely before aRect, move on.
      iter++;
      continue;
    }

    if (!iter->Intersects(rectStrip)) {
      // This band does not intersect aRect horizontally. Move on.
      iter++;
      continue;
    }

    // This band intersects with aRect.
    return true;
  }

  return false;
}

void nsRegion::Inflate(const nsMargin& aMargin) {
  nsRegion newRegion;
  for (RectIterator iter = RectIterator(*this); !iter.Done(); iter.Next()) {
    nsRectAbsolute rect = iter.GetAbsolute();
    rect.Inflate(aMargin);
    newRegion.AddRect(rect);
  }

  *this = std::move(newRegion);
}

void nsRegion::SimplifyOutward(uint32_t aMaxRects) {
  MOZ_ASSERT(aMaxRects >= 1, "Invalid max rect count");

  if (GetNumRects() <= aMaxRects) {
    return;
  }

  // Try combining rects in horizontal bands into a single rect
  // The goal here is to try to keep groups of rectangles that are vertically
  // discontiguous as separate rectangles in the final region. This is
  // simple and fast to implement and page contents tend to vary more
  // vertically than horizontally (which is why our rectangles are stored
  // sorted by y-coordinate, too).
  //
  // Note: if boxes share y1 because of the canonical representation they
  // will share y2

  size_t idx = 0;

  while (idx < mBands.Length()) {
    size_t oldIdx = idx;
    mBands[idx].mStrips.begin()->right =
        mBands[idx].mStrips.LastElement().right;
    mBands[idx].mStrips.TruncateLength(1);
    idx++;

    // Merge any bands with the same bounds.
    while (idx < mBands.Length() &&
           mBands[idx].mStrips.begin()->left ==
               mBands[oldIdx].mStrips.begin()->left &&
           mBands[idx].mStrips.LastElement().right ==
               mBands[oldIdx].mStrips.begin()->right) {
      mBands[oldIdx].bottom = mBands[idx].bottom;
      mBands.RemoveElementAt(idx);
    }
  }

  AssertState();

  // mBands.size() is now equal to our rect count.
  if (mBands.Length() > aMaxRects) {
    *this = GetBounds();
  }
}

// compute the covered area difference between two rows.
// by iterating over both rows simultaneously and adding up
// the additional increase in area caused by extending each
// of the rectangles to the combined height of both rows
uint32_t nsRegion::ComputeMergedAreaIncrease(const Band& aTopBand,
                                             const Band& aBottomBand) {
  uint32_t totalArea = 0;

  uint32_t topHeight = aBottomBand.top - aTopBand.top;
  uint32_t bottomHeight = aBottomBand.bottom - aTopBand.bottom;
  uint32_t currentStripBottom = 0;

  // This could be done with slightly better worse case performance by merging
  // these two for-loops, but this makes the code a lot easier to understand.
  for (auto& strip : aTopBand.mStrips) {
    if (currentStripBottom == aBottomBand.mStrips.Length() ||
        strip.right < aBottomBand.mStrips[currentStripBottom].left) {
      totalArea += bottomHeight * strip.Size();
      continue;
    }

    int32_t currentX = strip.left;
    while (currentStripBottom != aBottomBand.mStrips.Length() &&
           aBottomBand.mStrips[currentStripBottom].left < strip.right) {
      if (currentX >= strip.right) {
        break;
      }
      if (currentX < aBottomBand.mStrips[currentStripBottom].left) {
        // Add the part that's not intersecting.
        totalArea += (aBottomBand.mStrips[currentStripBottom].left - currentX) *
                     bottomHeight;
      }

      currentX =
          std::max(aBottomBand.mStrips[currentStripBottom].right, currentX);
      currentStripBottom++;
    }

    // Add remainder of this strip.
    if (currentX < strip.right) {
      totalArea += (strip.right - currentX) * bottomHeight;
    }
    if (currentStripBottom) {
      currentStripBottom--;
    }
  }
  uint32_t currentStripTop = 0;
  for (auto& strip : aBottomBand.mStrips) {
    if (currentStripTop == aTopBand.mStrips.Length() ||
        strip.right < aTopBand.mStrips[currentStripTop].left) {
      totalArea += topHeight * strip.Size();
      continue;
    }

    int32_t currentX = strip.left;
    while (currentStripTop != aTopBand.mStrips.Length() &&
           aTopBand.mStrips[currentStripTop].left < strip.right) {
      if (currentX >= strip.right) {
        break;
      }
      if (currentX < aTopBand.mStrips[currentStripTop].left) {
        // Add the part that's not intersecting.
        totalArea +=
            (aTopBand.mStrips[currentStripTop].left - currentX) * topHeight;
      }

      currentX = std::max(aTopBand.mStrips[currentStripTop].right, currentX);
      currentStripTop++;
    }

    // Add remainder of this strip.
    if (currentX < strip.right) {
      totalArea += (strip.right - currentX) * topHeight;
    }
    if (currentStripTop) {
      currentStripTop--;
    }
  }
  return totalArea;
}

void nsRegion::SimplifyOutwardByArea(uint32_t aThreshold) {
  if (mBands.Length() < 2) {
    // We have only one or no row and we're done.
    return;
  }

  uint32_t currentBand = 0;
  do {
    Band& band = mBands[currentBand];

    uint32_t totalArea =
        ComputeMergedAreaIncrease(band, mBands[currentBand + 1]);

    if (totalArea <= aThreshold) {
      for (Strip& strip : mBands[currentBand + 1].mStrips) {
        // This could use an optimized function to merge two bands.
        band.InsertStrip(strip);
      }
      band.bottom = mBands[currentBand + 1].bottom;
      mBands.RemoveElementAt(currentBand + 1);
    } else {
      currentBand++;
    }
  } while (currentBand + 1 < mBands.Length());

  EnsureSimplified();
  AssertState();
}

typedef void (*visit_fn)(void* closure, VisitSide side, int x1, int y1, int x2,
                         int y2);

void nsRegion::VisitEdges(visit_fn visit, void* closure) const {
  if (mBands.IsEmpty()) {
    visit(closure, VisitSide::LEFT, mBounds.X(), mBounds.Y(), mBounds.X(),
          mBounds.YMost());
    visit(closure, VisitSide::RIGHT, mBounds.XMost(), mBounds.Y(),
          mBounds.XMost(), mBounds.YMost());
    visit(closure, VisitSide::TOP, mBounds.X() - 1, mBounds.Y(),
          mBounds.XMost() + 1, mBounds.Y());
    visit(closure, VisitSide::BOTTOM, mBounds.X() - 1, mBounds.YMost(),
          mBounds.XMost() + 1, mBounds.YMost());
    return;
  }

  auto band = std::begin(mBands);
  auto bandFinal = std::end(mBands);
  bandFinal--;
  for (const Strip& strip : band->mStrips) {
    visit(closure, VisitSide::LEFT, strip.left, band->top, strip.left,
          band->bottom);
    visit(closure, VisitSide::RIGHT, strip.right, band->top, strip.right,
          band->bottom);
    visit(closure, VisitSide::TOP, strip.left - 1, band->top, strip.right + 1,
          band->top);
  }

  if (band != bandFinal) {
    do {
      const Band& topBand = *band;
      band++;

      for (const Strip& strip : band->mStrips) {
        visit(closure, VisitSide::LEFT, strip.left, band->top, strip.left,
              band->bottom);
        visit(closure, VisitSide::RIGHT, strip.right, band->top, strip.right,
              band->bottom);
      }

      if (band->top == topBand.bottom) {
        // Two bands touching each other vertically.
        const Band& bottomBand = *band;
        auto topStrip = std::begin(topBand.mStrips);
        auto bottomStrip = std::begin(bottomBand.mStrips);

        int y = topBand.bottom;

        // State from this point on along the vertical edge:
        // 0 - Empty
        // 1 - Touched by top rect
        // 2 - Touched by bottom rect
        // 3 - Touched on both sides
        int state;
        const int TouchedByNothing = 0;
        const int TouchedByTop = 1;
        const int TouchedByBottom = 2;
        // We always start with nothing.
        int oldState = TouchedByNothing;
        // Last state change, adjusted by -1 if the last state change was
        // a change away from 0.
        int lastX = std::min(topStrip->left, bottomStrip->left) - 1;

        // Current edge being considered for top and bottom,
        // 0 - left, 1 - right.
        bool topEdgeIsLeft = true;
        bool bottomEdgeIsLeft = true;
        while (topStrip != std::end(topBand.mStrips) &&
               bottomStrip != std::end(bottomBand.mStrips)) {
          int topPos;
          int bottomPos;
          if (topEdgeIsLeft) {
            topPos = topStrip->left;
          } else {
            topPos = topStrip->right;
          }
          if (bottomEdgeIsLeft) {
            bottomPos = bottomStrip->left;
          } else {
            bottomPos = bottomStrip->right;
          }

          int currentX = std::min(topPos, bottomPos);
          if (topPos < bottomPos) {
            if (topEdgeIsLeft) {
              state = oldState | TouchedByTop;
            } else {
              state = oldState ^ TouchedByTop;
              topStrip++;
            }
            topEdgeIsLeft = !topEdgeIsLeft;
          } else if (bottomPos < topPos) {
            if (bottomEdgeIsLeft) {
              state = oldState | TouchedByBottom;
            } else {
              state = oldState ^ TouchedByBottom;
              bottomStrip++;
            }
            bottomEdgeIsLeft = !bottomEdgeIsLeft;
          } else {
            // bottomPos == topPos
            state = TouchedByNothing;
            if (bottomEdgeIsLeft) {
              state = TouchedByBottom;
            } else {
              bottomStrip++;
            }
            if (topEdgeIsLeft) {
              state |= TouchedByTop;
            } else {
              topStrip++;
            }
            topEdgeIsLeft = !topEdgeIsLeft;
            bottomEdgeIsLeft = !bottomEdgeIsLeft;
          }

          MOZ_ASSERT(state != oldState);
          if (oldState == TouchedByNothing) {
            // We had nothing before, make sure the left edge will be padded.
            lastX = currentX - 1;
          } else if (oldState == TouchedByTop) {
            if (state == TouchedByNothing) {
              visit(closure, VisitSide::BOTTOM, lastX, y, currentX + 1, y);
            } else {
              visit(closure, VisitSide::BOTTOM, lastX, y, currentX, y);
              lastX = currentX;
            }
          } else if (oldState == TouchedByBottom) {
            if (state == TouchedByNothing) {
              visit(closure, VisitSide::TOP, lastX, y, currentX + 1, y);
            } else {
              visit(closure, VisitSide::TOP, lastX, y, currentX, y);
              lastX = currentX;
            }
          } else {
            lastX = currentX;
          }
          oldState = state;
        }

        MOZ_ASSERT(!state || (topEdgeIsLeft || bottomEdgeIsLeft));
        if (topStrip != std::end(topBand.mStrips)) {
          if (!topEdgeIsLeft) {
            visit(closure, VisitSide::BOTTOM, lastX, y, topStrip->right + 1, y);
            topStrip++;
          }
          while (topStrip != std::end(topBand.mStrips)) {
            visit(closure, VisitSide::BOTTOM, topStrip->left - 1, y,
                  topStrip->right + 1, y);
            topStrip++;
          }
        } else if (bottomStrip != std::end(bottomBand.mStrips)) {
          if (!bottomEdgeIsLeft) {
            visit(closure, VisitSide::TOP, lastX, y, bottomStrip->right + 1, y);
            bottomStrip++;
          }
          while (bottomStrip != std::end(bottomBand.mStrips)) {
            visit(closure, VisitSide::TOP, bottomStrip->left - 1, y,
                  bottomStrip->right + 1, y);
            bottomStrip++;
          }
        }
      } else {
        for (const Strip& strip : topBand.mStrips) {
          visit(closure, VisitSide::BOTTOM, strip.left - 1, topBand.bottom,
                strip.right + 1, topBand.bottom);
        }
        for (const Strip& strip : band->mStrips) {
          visit(closure, VisitSide::TOP, strip.left - 1, band->top,
                strip.right + 1, band->top);
        }
      }
    } while (band != bandFinal);
  }

  for (const Strip& strip : band->mStrips) {
    visit(closure, VisitSide::BOTTOM, strip.left - 1, band->bottom,
          strip.right + 1, band->bottom);
  }
}

void nsRegion::SimplifyInward(uint32_t aMaxRects) {
  NS_ASSERTION(aMaxRects >= 1, "Invalid max rect count");

  if (GetNumRects() <= aMaxRects) return;

  SetEmpty();
}

uint64_t nsRegion::Area() const {
  if (mBands.IsEmpty()) {
    return mBounds.Area();
  }

  uint64_t area = 0;
  for (const Band& band : mBands) {
    uint32_t height = band.bottom - band.top;
    for (const Strip& strip : band.mStrips) {
      area += (strip.right - strip.left) * height;
    }
  }

  return area;
}

nsRegion& nsRegion::ScaleRoundOut(float aXScale, float aYScale) {
  if (mozilla::gfx::FuzzyEqual(aXScale, 1.0f) &&
      mozilla::gfx::FuzzyEqual(aYScale, 1.0f)) {
    return *this;
  }

  nsRegion newRegion;
  for (RectIterator iter = RectIterator(*this); !iter.Done(); iter.Next()) {
    nsRectAbsolute rect = iter.GetAbsolute();
    rect.ScaleRoundOut(aXScale, aYScale);
    newRegion.AddRect(rect);
  }

  *this = std::move(newRegion);
  return *this;
}

nsRegion& nsRegion::ScaleInverseRoundOut(float aXScale, float aYScale) {
  nsRegion newRegion;
  for (RectIterator iter = RectIterator(*this); !iter.Done(); iter.Next()) {
    nsRectAbsolute rect = iter.GetAbsolute();
    rect.ScaleInverseRoundOut(aXScale, aYScale);
    newRegion.AddRect(rect);
  }

  *this = std::move(newRegion);
  return *this;
}

static mozilla::gfx::IntRect TransformRect(
    const mozilla::gfx::IntRect& aRect,
    const mozilla::gfx::Matrix4x4& aTransform) {
  if (aRect.IsEmpty()) {
    return mozilla::gfx::IntRect();
  }

  mozilla::gfx::RectDouble rect(aRect.X(), aRect.Y(), aRect.Width(),
                                aRect.Height());
  rect = aTransform.TransformAndClipBounds(
      rect, mozilla::gfx::RectDouble::MaxIntRect());
  rect.RoundOut();

  mozilla::gfx::IntRect intRect;
  if (!gfxUtils::GfxRectToIntRect(ThebesRect(rect), &intRect)) {
    return mozilla::gfx::IntRect();
  }

  return intRect;
}

nsRegion& nsRegion::Transform(const mozilla::gfx::Matrix4x4& aTransform) {
  nsRegion newRegion;
  for (RectIterator iter = RectIterator(*this); !iter.Done(); iter.Next()) {
    nsRect rect = nsIntRegion::ToRect(
        TransformRect(nsIntRegion::FromRect(iter.Get()), aTransform));
    newRegion.AddRect(nsRectAbsolute::FromRect(rect));
  }

  *this = std::move(newRegion);
  return *this;
}

nsRegion nsRegion::ScaleToOtherAppUnitsRoundOut(int32_t aFromAPP,
                                                int32_t aToAPP) const {
  if (aFromAPP == aToAPP) {
    return *this;
  }
  nsRegion newRegion;
  for (RectIterator iter = RectIterator(*this); !iter.Done(); iter.Next()) {
    nsRect rect = iter.Get();
    rect = rect.ScaleToOtherAppUnitsRoundOut(aFromAPP, aToAPP);
    newRegion.AddRect(nsRectAbsolute::FromRect(rect));
  }

  return newRegion;
}

nsRegion nsRegion::ScaleToOtherAppUnitsRoundIn(int32_t aFromAPP,
                                               int32_t aToAPP) const {
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRegion newRegion;
  for (RectIterator iter = RectIterator(*this); !iter.Done(); iter.Next()) {
    nsRect rect = iter.Get();
    rect = rect.ScaleToOtherAppUnitsRoundIn(aFromAPP, aToAPP);
    newRegion.AddRect(nsRectAbsolute::FromRect(rect));
  }

  return newRegion;
}

nsIntRegion nsRegion::ToPixels(nscoord aAppUnitsPerPixel,
                               bool aOutsidePixels) const {
  nsIntRegion intRegion;
  for (RectIterator iter = RectIterator(*this); !iter.Done(); iter.Next()) {
    mozilla::gfx::IntRect deviceRect;
    nsRect rect = iter.Get();
    if (aOutsidePixels)
      deviceRect = rect.ToOutsidePixels(aAppUnitsPerPixel);
    else
      deviceRect = rect.ToNearestPixels(aAppUnitsPerPixel);
    intRegion.OrWith(deviceRect);
  }

  return intRegion;
}

nsIntRegion nsRegion::ToOutsidePixels(nscoord aAppUnitsPerPixel) const {
  return ToPixels(aAppUnitsPerPixel, true);
}

nsIntRegion nsRegion::ToNearestPixels(nscoord aAppUnitsPerPixel) const {
  return ToPixels(aAppUnitsPerPixel, false);
}

nsIntRegion nsRegion::ScaleToNearestPixels(float aScaleX, float aScaleY,
                                           nscoord aAppUnitsPerPixel) const {
  nsIntRegion result;
  for (auto iter = RectIter(); !iter.Done(); iter.Next()) {
    mozilla::gfx::IntRect deviceRect =
        iter.Get().ScaleToNearestPixels(aScaleX, aScaleY, aAppUnitsPerPixel);
    result.Or(result, deviceRect);
  }
  return result;
}

nsIntRegion nsRegion::ScaleToOutsidePixels(float aScaleX, float aScaleY,
                                           nscoord aAppUnitsPerPixel) const {
  // make a copy of the region so that we can mutate it inplace
  nsIntRegion intRegion;
  for (RectIterator iter = RectIterator(*this); !iter.Done(); iter.Next()) {
    nsRect rect = iter.Get();
    intRegion.OrWith(
        rect.ScaleToOutsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel));
  }
  return intRegion;
}

nsIntRegion nsRegion::ScaleToInsidePixels(float aScaleX, float aScaleY,
                                          nscoord aAppUnitsPerPixel) const {
  /* When scaling a rect, walk forward through the rect list up until the y
   * value is greater than the current rect's YMost() value.
   *
   * For each rect found, check if the rects have a touching edge (in unscaled
   * coordinates), and if one edge is entirely contained within the other.
   *
   * If it is, then the contained edge can be moved (in scaled pixels) to ensure
   * that no gap exists.
   *
   * Since this could be potentially expensive - O(n^2), we only attempt this
   * algorithm for the first rect.
   */

  if (mBands.IsEmpty()) {
    nsIntRect rect = mBounds.ToNSRect().ScaleToInsidePixels(aScaleX, aScaleY,
                                                            aAppUnitsPerPixel);
    return nsIntRegion(rect);
  }

  nsIntRegion intRegion;
  RectIterator iter = RectIterator(*this);

  nsRect first = iter.Get();

  mozilla::gfx::IntRect firstDeviceRect =
      first.ScaleToInsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel);

  for (iter.Next(); !iter.Done(); iter.Next()) {
    nsRect rect = iter.Get();
    mozilla::gfx::IntRect deviceRect =
        rect.ScaleToInsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel);

    if (rect.Y() <= first.YMost()) {
      if (rect.XMost() == first.X() && rect.YMost() <= first.YMost()) {
        // rect is touching on the left edge of the first rect and contained
        // within the length of its left edge
        deviceRect.SetRightEdge(firstDeviceRect.X());
      } else if (rect.X() == first.XMost() && rect.YMost() <= first.YMost()) {
        // rect is touching on the right edge of the first rect and contained
        // within the length of its right edge
        deviceRect.SetLeftEdge(firstDeviceRect.XMost());
      } else if (rect.Y() == first.YMost()) {
        // The bottom of the first rect is on the same line as the top of rect,
        // but they aren't necessarily contained.
        if (rect.X() <= first.X() && rect.XMost() >= first.XMost()) {
          // The top of rect contains the bottom of the first rect
          firstDeviceRect.SetBottomEdge(deviceRect.Y());
        } else if (rect.X() >= first.X() && rect.XMost() <= first.XMost()) {
          // The bottom of the first contains the top of rect
          deviceRect.SetTopEdge(firstDeviceRect.YMost());
        }
      }
    }

    intRegion.OrWith(deviceRect);
  }

  intRegion.OrWith(firstDeviceRect);
  return intRegion;
}

// A cell's "value" is a pair consisting of
// a) the area of the subrectangle it corresponds to, if it's in
// aContainingRect and in the region, 0 otherwise
// b) the area of the subrectangle it corresponds to, if it's in the region,
// 0 otherwise
// Addition, subtraction and identity are defined on these values in the
// obvious way. Partial order is lexicographic.
// A "large negative value" is defined with large negative numbers for both
// fields of the pair. This negative value has the property that adding any
// number of non-negative values to it always results in a negative value.
//
// The GetLargestRectangle algorithm works in three phases:
//  1) Convert the region into a grid by adding vertical/horizontal lines for
//     each edge of each rectangle in the region.
//  2) For each rectangle in the region, for each cell it contains, set that
//     cells's value as described above.
//  3) Calculate the submatrix with the largest sum such that none of its cells
//     contain any 0s (empty regions). The rectangle represented by the
//     submatrix is the largest rectangle in the region.
//
// Let k be the number of rectangles in the region.
// Let m be the height of the grid generated in step 1.
// Let n be the width of the grid generated in step 1.
//
// Step 1 is O(k) in time and O(m+n) in space for the sparse grid.
// Step 2 is O(mn) in time and O(mn) in additional space for the full grid.
// Step 3 is O(m^2 n) in time and O(mn) in additional space
//
// The implementation of steps 1 and 2 are rather straightforward. However our
// implementation of step 3 uses dynamic programming to achieve its efficiency.
//
// Psuedo code for step 3 is as follows where G is the grid from step 1 and A
// is the array from step 2:
// Phase3 = function (G, A, m, n) {
//   let (t,b,l,r,_) = MaxSum2D(A,m,n)
//   return rect(G[t],G[l],G[r],G[b]);
// }
// MaxSum2D = function (A, m, n) {
//   S = array(m+1,n+1)
//   S[0][i] = 0 for i in [0,n]
//   S[j][0] = 0 for j in [0,m]
//   S[j][i] = (if A[j-1][i-1] = 0 then some large negative value
//                                 else A[j-1][i-1])
//           + S[j-1][n] + S[j][i-1] - S[j-1][i-1]
//
//   // top, bottom, left, right, area
//   var maxRect = (-1, -1, -1, -1, 0);
//
//   for all (m',m'') in [0, m]^2 {
//     let B = { S[m'][i] - S[m''][i] | 0 <= i <= n }
//     let ((l,r),area) = MaxSum1D(B,n+1)
//     if (area > maxRect.area) {
//       maxRect := (m', m'', l, r, area)
//     }
//   }
//
//   return maxRect;
// }
//
// Originally taken from Improved algorithms for the k-maximum subarray problem
// for small k - SE Bae, T Takaoka but modified to show the explicit tracking
// of indices and we already have the prefix sums from our one call site so
// there's no need to construct them.
// MaxSum1D = function (A,n) {
//   var minIdx = 0;
//   var min = 0;
//   var maxIndices = (0,0);
//   var max = 0;
//   for i in range(n) {
//     let cand = A[i] - min;
//     if (cand > max) {
//       max := cand;
//       maxIndices := (minIdx, i)
//     }
//     if (min > A[i]) {
//       min := A[i];
//       minIdx := i;
//     }
//   }
//   return (minIdx, maxIdx, max);
// }

namespace {
// This class represents a partitioning of an axis delineated by coordinates.
// It internally maintains a sorted array of coordinates.
class AxisPartition {
 public:
  // Adds a new partition at the given coordinate to this partitioning. If
  // the coordinate is already present in the partitioning, this does nothing.
  void InsertCoord(nscoord c) {
    uint32_t i = mStops.IndexOfFirstElementGt(c);
    if (i == 0 || mStops[i - 1] != c) {
      mStops.InsertElementAt(i, c);
    }
  }

  // Returns the array index of the given partition point. The partition
  // point must already be present in the partitioning.
  int32_t IndexOf(nscoord p) const { return mStops.BinaryIndexOf(p); }

  // Returns the partition at the given index which must be non-zero and
  // less than the number of partitions in this partitioning.
  nscoord StopAt(int32_t index) const { return mStops[index]; }

  // Returns the size of the gap between the partition at the given index and
  // the next partition in this partitioning. If the index is the last index
  // in the partitioning, the result is undefined.
  nscoord StopSize(int32_t index) const {
    return mStops[index + 1] - mStops[index];
  }

  // Returns the number of partitions in this partitioning.
  int32_t GetNumStops() const { return mStops.Length(); }

 private:
  nsTArray<nscoord> mStops;
};

const int64_t kVeryLargeNegativeNumber = 0xffff000000000000ll;

struct SizePair {
  int64_t mSizeContainingRect;
  int64_t mSize;

  SizePair() : mSizeContainingRect(0), mSize(0) {}

  static SizePair VeryLargeNegative() {
    SizePair result;
    result.mSize = result.mSizeContainingRect = kVeryLargeNegativeNumber;
    return result;
  }
  bool operator<(const SizePair& aOther) const {
    if (mSizeContainingRect < aOther.mSizeContainingRect) return true;
    if (mSizeContainingRect > aOther.mSizeContainingRect) return false;
    return mSize < aOther.mSize;
  }
  bool operator>(const SizePair& aOther) const {
    return aOther.operator<(*this);
  }
  SizePair operator+(const SizePair& aOther) const {
    SizePair result = *this;
    result.mSizeContainingRect += aOther.mSizeContainingRect;
    result.mSize += aOther.mSize;
    return result;
  }
  SizePair operator-(const SizePair& aOther) const {
    SizePair result = *this;
    result.mSizeContainingRect -= aOther.mSizeContainingRect;
    result.mSize -= aOther.mSize;
    return result;
  }
};

// Returns the sum and indices of the subarray with the maximum sum of the
// given array (A,n), assuming the array is already in prefix sum form.
SizePair MaxSum1D(const nsTArray<SizePair>& A, int32_t n, int32_t* minIdx,
                  int32_t* maxIdx) {
  // The min/max indicies of the largest subarray found so far
  SizePair min, max;
  int32_t currentMinIdx = 0;

  *minIdx = 0;
  *maxIdx = 0;

  // Because we're given the array in prefix sum form, we know the first
  // element is 0
  for (int32_t i = 1; i < n; i++) {
    SizePair cand = A[i] - min;
    if (cand > max) {
      max = cand;
      *minIdx = currentMinIdx;
      *maxIdx = i;
    }
    if (min > A[i]) {
      min = A[i];
      currentMinIdx = i;
    }
  }

  return max;
}
}  // namespace

nsRect nsRegion::GetLargestRectangle(const nsRect& aContainingRect) const {
  nsRect bestRect;

  if (GetNumRects() <= 1) {
    bestRect = GetBounds();
    return bestRect;
  }

  AxisPartition xaxis, yaxis;

  // Step 1: Calculate the grid lines
  for (auto iter = RectIter(); !iter.Done(); iter.Next()) {
    const nsRect& rect = iter.Get();
    xaxis.InsertCoord(rect.X());
    xaxis.InsertCoord(rect.XMost());
    yaxis.InsertCoord(rect.Y());
    yaxis.InsertCoord(rect.YMost());
  }
  if (!aContainingRect.IsEmpty()) {
    xaxis.InsertCoord(aContainingRect.X());
    xaxis.InsertCoord(aContainingRect.XMost());
    yaxis.InsertCoord(aContainingRect.Y());
    yaxis.InsertCoord(aContainingRect.YMost());
  }

  // Step 2: Fill out the grid with the areas
  // Note: due to the ordering of rectangles in the region, it is not always
  // possible to combine steps 2 and 3 so we don't try to be clever.
  int32_t matrixHeight = yaxis.GetNumStops() - 1;
  int32_t matrixWidth = xaxis.GetNumStops() - 1;
  int32_t matrixSize = matrixHeight * matrixWidth;
  nsTArray<SizePair> areas(matrixSize);
  areas.SetLength(matrixSize);

  for (auto iter = RectIter(); !iter.Done(); iter.Next()) {
    const nsRect& rect = iter.Get();
    int32_t xstart = xaxis.IndexOf(rect.X());
    int32_t xend = xaxis.IndexOf(rect.XMost());
    int32_t y = yaxis.IndexOf(rect.Y());
    int32_t yend = yaxis.IndexOf(rect.YMost());

    for (; y < yend; y++) {
      nscoord height = yaxis.StopSize(y);
      for (int32_t x = xstart; x < xend; x++) {
        nscoord width = xaxis.StopSize(x);
        int64_t size = width * int64_t(height);
        if (rect.Intersects(aContainingRect)) {
          areas[y * matrixWidth + x].mSizeContainingRect = size;
        }
        areas[y * matrixWidth + x].mSize = size;
      }
    }
  }

  // Step 3: Find the maximum submatrix sum that does not contain a rectangle
  {
    // First get the prefix sum array
    int32_t m = matrixHeight + 1;
    int32_t n = matrixWidth + 1;
    nsTArray<SizePair> pareas(m * n);
    pareas.SetLength(m * n);
    for (int32_t y = 1; y < m; y++) {
      for (int32_t x = 1; x < n; x++) {
        SizePair area = areas[(y - 1) * matrixWidth + x - 1];
        if (!area.mSize) {
          area = SizePair::VeryLargeNegative();
        }
        area = area + pareas[y * n + x - 1] + pareas[(y - 1) * n + x] -
               pareas[(y - 1) * n + x - 1];
        pareas[y * n + x] = area;
      }
    }

    // No longer need the grid
    areas.SetLength(0);

    SizePair bestArea;
    struct {
      int32_t left, top, right, bottom;
    } bestRectIndices = {0, 0, 0, 0};
    for (int32_t m1 = 0; m1 < m; m1++) {
      for (int32_t m2 = m1 + 1; m2 < m; m2++) {
        nsTArray<SizePair> B;
        B.SetLength(n);
        for (int32_t i = 0; i < n; i++) {
          B[i] = pareas[m2 * n + i] - pareas[m1 * n + i];
        }
        int32_t minIdx, maxIdx;
        SizePair area = MaxSum1D(B, n, &minIdx, &maxIdx);
        if (area > bestArea) {
          bestRectIndices.left = minIdx;
          bestRectIndices.top = m1;
          bestRectIndices.right = maxIdx;
          bestRectIndices.bottom = m2;
          bestArea = area;
        }
      }
    }

    bestRect.MoveTo(xaxis.StopAt(bestRectIndices.left),
                    yaxis.StopAt(bestRectIndices.top));
    bestRect.SizeTo(xaxis.StopAt(bestRectIndices.right) - bestRect.X(),
                    yaxis.StopAt(bestRectIndices.bottom) - bestRect.Y());
  }

  return bestRect;
}

std::ostream& operator<<(std::ostream& stream, const nsRegion& m) {
  stream << "[";

  bool first = true;
  for (auto iter = m.RectIter(); !iter.Done(); iter.Next()) {
    if (!first) {
      stream << "; ";
    } else {
      first = false;
    }
    const nsRect& rect = iter.Get();
    stream << rect.X() << "," << rect.Y() << "," << rect.XMost() << ","
           << rect.YMost();
  }

  stream << "]";
  return stream;
}

void nsRegion::OutputToStream(std::string aObjName,
                              std::ostream& stream) const {
  auto iter = RectIter();
  nsRect r = iter.Get();
  stream << "nsRegion " << aObjName << "(nsRect(" << r.X() << ", " << r.Y()
         << ", " << r.Width() << ", " << r.Height() << "));\n";
  iter.Next();

  for (; !iter.Done(); iter.Next()) {
    nsRect r = iter.Get();
    stream << aObjName << ".OrWith(nsRect(" << r.X() << ", " << r.Y() << ", "
           << r.Width() << ", " << r.Height() << "));\n";
  }
}

nsCString nsRegion::ToString() const {
  return nsCString(mozilla::ToString(*this).c_str());
}
