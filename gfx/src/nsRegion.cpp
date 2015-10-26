/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsRegion.h"
#include "nsTArray.h"
#include "gfxUtils.h"
#include "mozilla/ToString.h"

bool nsRegion::Contains(const nsRegion& aRgn) const
{
  // XXX this could be made faster by iterating over
  // both regions at the same time some how
  nsRegionRectIterator iter(aRgn);
  while (const nsRect* r = iter.Next()) {
    if (!Contains (*r)) {
      return false;
    }
  }
  return true;
}

bool nsRegion::Intersects(const nsRect& aRect) const
{
  // XXX this could be made faster by using pixman_region32_contains_rect
  nsRegionRectIterator iter(*this);
  while (const nsRect* r = iter.Next()) {
    if (r->Intersects(aRect)) {
      return true;
    }
  }
  return false;
}

void nsRegion::Inflate(const nsMargin& aMargin)
{
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect.Inflate(aMargin);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t region;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&region, boxes, n);

  pixman_region32_fini(&mImpl);
  mImpl = region;
}

void nsRegion::SimplifyOutward (uint32_t aMaxRects)
{
  MOZ_ASSERT(aMaxRects >= 1, "Invalid max rect count");

  if (GetNumRects() <= aMaxRects)
    return;

  pixman_box32_t *boxes;
  int n;
  boxes = pixman_region32_rectangles(&mImpl, &n);

  // Try combining rects in horizontal bands into a single rect
  int dest = 0;
  for (int src = 1; src < n; src++)
  {
    // The goal here is to try to keep groups of rectangles that are vertically
    // discontiguous as separate rectangles in the final region. This is
    // simple and fast to implement and page contents tend to vary more
    // vertically than horizontally (which is why our rectangles are stored
    // sorted by y-coordinate, too).
    //
    // Note: if boxes share y1 because of the canonical representation they
    // will share y2
    while ((src < (n)) && boxes[dest].y1 == boxes[src].y1) {
      // merge box[i] and box[i+1]
      boxes[dest].x2 = boxes[src].x2;
      src++;
    }
    if (src < n) {
      dest++;
      boxes[dest] = boxes[src];
    }
  }

  uint32_t reducedCount = dest+1;
  // pixman has a special representation for
  // regions of 1 rectangle. So just use the
  // bounds in that case
  if (reducedCount > 1 && reducedCount <= aMaxRects) {
    // reach into pixman and lower the number
    // of rects stored in data.
    mImpl.data->numRects = reducedCount;
  } else {
    *this = GetBounds();
  }
}

// compute the covered area difference between two rows.
// by iterating over both rows simultaneously and adding up
// the additional increase in area caused by extending each
// of the rectangles to the combined height of both rows
static uint32_t ComputeMergedAreaIncrease(pixman_box32_t *topRects,
		                     pixman_box32_t *topRectsEnd,
		                     pixman_box32_t *bottomRects,
		                     pixman_box32_t *bottomRectsEnd)
{
  uint32_t totalArea = 0;
  struct pt {
    int32_t x, y;
  };


  pt *i = (pt*)topRects;
  pt *end_i = (pt*)topRectsEnd;
  pt *j = (pt*)bottomRects;
  pt *end_j = (pt*)bottomRectsEnd;
  bool top = false;
  bool bottom = false;

  int cur_x = i->x;
  bool top_next = top;
  bool bottom_next = bottom;
  //XXX: we could probably simplify this condition and perhaps move it into the loop below
  if (j->x < cur_x) {
    cur_x = j->x;
    j++;
    bottom_next = !bottom;
  } else if (j->x == cur_x) {
    i++;
    top_next = !top;
    bottom_next = !bottom;
    j++;
  } else {
    top_next = !top;
    i++;
  }

  int topRectsHeight = topRects->y2 - topRects->y1;
  int bottomRectsHeight = bottomRects->y2 - bottomRects->y1;
  int inbetweenHeight = bottomRects->y1 - topRects->y2;
  int width = cur_x;
  // top and bottom are the in-status to the left of cur_x
  do {
    if (top && !bottom) {
      totalArea += (inbetweenHeight+bottomRectsHeight)*width;
    } else if (bottom && !top) {
      totalArea += (inbetweenHeight+topRectsHeight)*width;
    } else if (bottom && top) {
      totalArea += (inbetweenHeight)*width;
    }
    top = top_next;
    bottom = bottom_next;
    // find the next edge
    if (i->x < j->x) {
      top_next = !top;
      width = i->x - cur_x;
      cur_x = i->x;
      i++;
    } else if (j->x < i->x) {
      bottom_next = !bottom;
      width = j->x - cur_x;
      cur_x = j->x;
      j++;
    } else { // i->x == j->x
      top_next = !top;
      bottom_next = !bottom;
      width = i->x - cur_x;
      cur_x = i->x;
      i++;
      j++;
    }
  } while (i < end_i && j < end_j);

  // handle any remaining rects
  while (i < end_i) {
    width = i->x - cur_x;
    cur_x = i->x;
    i++;
    if (top)
      totalArea += (inbetweenHeight+bottomRectsHeight)*width;
    top = !top;
  }

  while (j < end_j) {
    width = j->x - cur_x;
    cur_x = j->x;
    j++;
    if (bottom)
      totalArea += (inbetweenHeight+topRectsHeight)*width;
    bottom = !bottom;
  }
  return totalArea;
}

static pixman_box32_t *
CopyRow(pixman_box32_t *dest_it, pixman_box32_t *src_start, pixman_box32_t *src_end)
{
    // XXX: std::copy
    pixman_box32_t *src_it = src_start;
    while (src_it < src_end) {
        *dest_it++ = *src_it++;
    }
    return dest_it;
}


#define WRITE_RECT(x1, x2, y1, y2) \
    do {                    \
         tmpRect->x1 = x1;  \
         tmpRect->x2 = x2;  \
         tmpRect->y1 = y1;  \
         tmpRect->y2 = y2;  \
         tmpRect++;         \
    } while (0)

/* If 'r' overlaps the current rect, then expand the current rect to include
 * it. Otherwise write the current rect out to tmpRect, and set r as the
 * updated current rect. */
#define MERGE_RECT(r)                 \
    do {                              \
      if (r->x1 <= x2) {              \
          if (x2 < r->x2)             \
              x2 = r->x2;             \
      } else {                        \
          WRITE_RECT(x1, x2, y1, y2); \
          x1 = r->x1;                 \
          x2 = r->x2;                 \
      }                               \
      r++;                            \
    } while (0)


/* Can we merge two sets of rects without extra space?
 * Yes, but not easily. We can even do it stably
 * but we don't need that property.
 *
 * This is written in the style of pixman_region_union_o */
static pixman_box32_t *
MergeRects(pixman_box32_t *r1,
           pixman_box32_t *r1_end,
           pixman_box32_t *r2,
           pixman_box32_t *r2_end,
           pixman_box32_t *tmpRect)
{
    /* This routine works by maintaining the current
     * rectangle in x1,x2,y1,y2 and either merging
     * in the left most rectangle if it overlaps or
     * outputing the current rectangle and setting
     * it to the the left most one */
    const int y1 = r1->y1;
    const int y2 = r2->y2;
    int x1;
    int x2;

    /* Find the left-most edge */
    if (r1->x1 < r2->x1) {
        x1 = r1->x1;
        x2 = r1->x2;
        r1++;
    } else {
        x1 = r2->x1;
        x2 = r2->x2;
        r2++;
    }

    while (r1 != r1_end && r2 != r2_end) {
        /* Find and merge the left-most rectangle */
        if (r1->x1 < r2->x1)
            MERGE_RECT (r1);
        else
            MERGE_RECT (r2);
    }

    /* Finish up any left overs */
    if (r1 != r1_end) {
        do {
            MERGE_RECT (r1);
        } while (r1 != r1_end);
    } else if (r2 != r2_end) {
        do {
            MERGE_RECT(r2);
        } while (r2 != r2_end);
    }

    /* Finish up the last rectangle */
    WRITE_RECT(x1, x2, y1, y2);

    return tmpRect;
}

void nsRegion::SimplifyOutwardByArea(uint32_t aThreshold)
{

  pixman_box32_t *boxes;
  int n;
  boxes = pixman_region32_rectangles(&mImpl, &n);

  // if we have no rectangles then we're done
  if (!n)
    return;

  pixman_box32_t *end = boxes + n;
  pixman_box32_t *topRectsEnd = boxes+1;
  pixman_box32_t *topRects = boxes;

  // we need some temporary storage for merging both rows of rectangles
  nsAutoTArray<pixman_box32_t, 10> tmpStorage;
  tmpStorage.SetCapacity(n);
  pixman_box32_t *tmpRect = tmpStorage.Elements();

  pixman_box32_t *destRect = boxes;
  pixman_box32_t *rect = tmpRect;
  // find the end of the first span of rectangles
  while (topRectsEnd < end && topRectsEnd->y1 == topRects->y1) {
    topRectsEnd++;
  }

  // if we only have one row we are done
  if (topRectsEnd == end)
    return;

  pixman_box32_t *bottomRects = topRectsEnd;
  pixman_box32_t *bottomRectsEnd = bottomRects+1;
  do {
    // find the end of the bottom span of rectangles
    while (bottomRectsEnd < end && bottomRectsEnd->y1 == bottomRects->y1) {
      bottomRectsEnd++;
    }
    uint32_t totalArea = ComputeMergedAreaIncrease(topRects, topRectsEnd,
                                                   bottomRects, bottomRectsEnd);

    if (totalArea <= aThreshold) {
      // merge the rects into tmpRect
      rect = MergeRects(topRects, topRectsEnd, bottomRects, bottomRectsEnd, tmpRect);

      // set topRects to where the newly merged rects will be so that we use them
      // as our next set of topRects
      topRects = destRect;
      // copy the merged rects back into the destination
      topRectsEnd = CopyRow(destRect, tmpRect, rect);
    } else {
      // copy the unmerged rects
      destRect = CopyRow(destRect, topRects, topRectsEnd);

      topRects = bottomRects;
      topRectsEnd = bottomRectsEnd;
      if (bottomRectsEnd == end) {
        // copy the last row when we are done
        topRectsEnd = CopyRow(destRect, topRects, topRectsEnd);
      }
    }
    bottomRects = bottomRectsEnd;
  } while (bottomRectsEnd != end);


  uint32_t reducedCount = topRectsEnd - pixman_region32_rectangles(&this->mImpl, &n);
  // pixman has a special representation for
  // regions of 1 rectangle. So just use the
  // bounds in that case
  if (reducedCount > 1) {
    // reach into pixman and lower the number
    // of rects stored in data.
    this->mImpl.data->numRects = reducedCount;
  } else {
    *this = GetBounds();
  }
}


typedef void (*visit_fn)(void *closure, VisitSide side, int x1, int y1, int x2, int y2);

static bool VisitNextEdgeBetweenRect(visit_fn visit, void *closure, VisitSide side,
				     pixman_box32_t *&r1, pixman_box32_t *&r2, const int y, int &x1)
{
  // check for overlap
  if (r1->x2 >= r2->x1) {
    MOZ_ASSERT(r2->x1 >= x1);
    visit(closure, side, x1, y, r2->x1, y);

    // find the rect that ends first or always drop the one that comes first?
    if (r1->x2 < r2->x2) {
      x1 = r1->x2;
      r1++;
    } else {
      x1 = r2->x2;
      r2++;
    }
    return true;
  } else {
    MOZ_ASSERT(r1->x2 < r2->x2);
    // we handle the corners by just extending the top and bottom edges
    visit(closure, side, x1, y, r1->x2+1, y);
    r1++;
    // we assign x1 because we can assume that x1 <= r2->x1 - 1
    // However the caller may know better and if so, may update
    // x1 to r1->x1
    x1 = r2->x1 - 1;
    return false;
  }
}

//XXX: if we need to this can compute the end of the row
static void
VisitSides(visit_fn visit, void *closure, pixman_box32_t *r, pixman_box32_t *r_end)
{
  // XXX: we can drop LEFT/RIGHT and just use the orientation
  // of the line if it makes sense
  while (r != r_end) {
    visit(closure, VisitSide::LEFT, r->x1, r->y1, r->x1, r->y2);
    visit(closure, VisitSide::RIGHT, r->x2, r->y1, r->x2, r->y2);
    r++;
  }
}

static void
VisitAbove(visit_fn visit, void *closure, pixman_box32_t *r, pixman_box32_t *r_end)
{
  while (r != r_end) {
    visit(closure, VisitSide::TOP, r->x1-1, r->y1, r->x2+1, r->y1);
    r++;
  }
}

static void
VisitBelow(visit_fn visit, void *closure, pixman_box32_t *r, pixman_box32_t *r_end)
{
  while (r != r_end) {
    visit(closure, VisitSide::BOTTOM, r->x1-1, r->y2, r->x2+1, r->y2);
    r++;
  }
}

static pixman_box32_t *
VisitInbetween(visit_fn visit, void *closure, pixman_box32_t *r1,
               pixman_box32_t *r1_end,
               pixman_box32_t *r2,
               pixman_box32_t *r2_end)
{
  const int y = r1->y2;
  int x1;

  bool overlap = false;
  while (r1 != r1_end && r2 != r2_end) {
    if (!overlap) {
      /* Find the left-most edge */
      if (r1->x1 < r2->x1) {
	x1 = r1->x1 - 1;
      } else {
	x1 = r2->x1 - 1;
      }
    }

    MOZ_ASSERT((x1 >= (r1->x1 - 1)) || (x1 >= (r2->x1 - 1)));
    if (r1->x1 < r2->x1) {
      overlap = VisitNextEdgeBetweenRect(visit, closure, VisitSide::BOTTOM, r1, r2, y, x1);
    } else {
      overlap = VisitNextEdgeBetweenRect(visit, closure, VisitSide::TOP, r2, r1, y, x1);
    }
  }

  /* Finish up which ever row has remaining rects*/
  if (r1 != r1_end) {
    // top row
    do {
      visit(closure, VisitSide::BOTTOM, x1, y, r1->x2 + 1, y);
      r1++;
      if (r1 == r1_end)
	break;
      x1 = r1->x1 - 1;
    } while (true);
  } else if (r2 != r2_end) {
    // bottom row
    do {
      visit(closure, VisitSide::TOP, x1, y, r2->x2 + 1, y);
      r2++;
      if (r2 == r2_end)
	break;
      x1 = r2->x1 - 1;
    } while (true);
  }

  return 0;
}

void nsRegion::VisitEdges (visit_fn visit, void *closure)
{
  pixman_box32_t *boxes;
  int n;
  boxes = pixman_region32_rectangles(&mImpl, &n);

  // if we have no rectangles then we're done
  if (!n)
    return;

  pixman_box32_t *end = boxes + n;
  pixman_box32_t *topRectsEnd = boxes + 1;
  pixman_box32_t *topRects = boxes;

  // find the end of the first span of rectangles
  while (topRectsEnd < end && topRectsEnd->y1 == topRects->y1) {
    topRectsEnd++;
  }

  // In order to properly handle convex corners we always visit the sides first
  // that way when we visit the corners we can pad using the value from the sides
  VisitSides(visit, closure, topRects, topRectsEnd);

  VisitAbove(visit, closure, topRects, topRectsEnd);

  pixman_box32_t *bottomRects = topRects;
  pixman_box32_t *bottomRectsEnd = topRectsEnd;
  if (topRectsEnd != end) {
    do {
      // find the next row of rects
      bottomRects = topRectsEnd;
      bottomRectsEnd = topRectsEnd + 1;
      while (bottomRectsEnd < end && bottomRectsEnd->y1 == bottomRects->y1) {
        bottomRectsEnd++;
      }

      VisitSides(visit, closure, bottomRects, bottomRectsEnd);

      if (topRects->y2 == bottomRects->y1) {
        VisitInbetween(visit, closure, topRects, topRectsEnd,
                                       bottomRects, bottomRectsEnd);
      } else {
        VisitBelow(visit, closure, topRects, topRectsEnd);
        VisitAbove(visit, closure, bottomRects, bottomRectsEnd);
      }

      topRects = bottomRects;
      topRectsEnd = bottomRectsEnd;
    } while (bottomRectsEnd != end);
  }

  // the bottom of the region doesn't touch anything else so we
  // can always visit it at the end
  VisitBelow(visit, closure, bottomRects, bottomRectsEnd);
}


void nsRegion::SimplifyInward (uint32_t aMaxRects)
{
  NS_ASSERTION(aMaxRects >= 1, "Invalid max rect count");

  if (GetNumRects() <= aMaxRects)
    return;

  SetEmpty();
}

uint64_t nsRegion::Area () const
{
  uint64_t area = 0;
  nsRegionRectIterator iter(*this);
  const nsRect* r;
  while ((r = iter.Next()) != nullptr) {
    area += uint64_t(r->width)*r->height;
  }
  return area;
}

nsRegion& nsRegion::ScaleRoundOut (float aXScale, float aYScale)
{
  if (mozilla::gfx::FuzzyEqual(aXScale, 1.0f) &&
      mozilla::gfx::FuzzyEqual(aYScale, 1.0f)) {
    return *this;
  }

  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect.ScaleRoundOut(aXScale, aYScale);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t region;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&region, boxes, n);

  pixman_region32_fini(&mImpl);
  mImpl = region;
  return *this;
}

nsRegion& nsRegion::ScaleInverseRoundOut (float aXScale, float aYScale)
{
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect.ScaleInverseRoundOut(aXScale, aYScale);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t region;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&region, boxes, n);

  pixman_region32_fini(&mImpl);
  mImpl = region;
  return *this;
}

static mozilla::gfx::IntRect
TransformRect(const mozilla::gfx::IntRect& aRect, const mozilla::gfx::Matrix4x4& aTransform)
{
    if (aRect.IsEmpty()) {
        return mozilla::gfx::IntRect();
    }

    mozilla::gfx::RectDouble rect(aRect.x, aRect.y, aRect.width, aRect.height);
    rect = aTransform.TransformAndClipBounds(rect, mozilla::gfx::RectDouble::MaxIntRect());
    rect.RoundOut();

    mozilla::gfx::IntRect intRect;
    if (!gfxUtils::GfxRectToIntRect(ThebesRect(rect), &intRect)) {
        return mozilla::gfx::IntRect();
    }

    return intRect;
}

nsRegion& nsRegion::Transform (const mozilla::gfx::Matrix4x4 &aTransform)
{
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    boxes[i] = RectToBox(nsIntRegion::ToRect(TransformRect(nsIntRegion::FromRect(rect), aTransform)));
  }

  pixman_region32_t region;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&region, boxes, n);

  pixman_region32_fini(&mImpl);
  mImpl = region;
  return *this;
}


nsRegion nsRegion::ScaleToOtherAppUnitsRoundOut (int32_t aFromAPP, int32_t aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRegion region = *this;
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&region.mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect = rect.ScaleToOtherAppUnitsRoundOut(aFromAPP, aToAPP);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t pixmanRegion;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&pixmanRegion, boxes, n);

  pixman_region32_fini(&region.mImpl);
  region.mImpl = pixmanRegion;
  return region;
}

nsRegion nsRegion::ScaleToOtherAppUnitsRoundIn (int32_t aFromAPP, int32_t aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRegion region = *this;
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&region.mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect = rect.ScaleToOtherAppUnitsRoundIn(aFromAPP, aToAPP);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t pixmanRegion;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&pixmanRegion, boxes, n);

  pixman_region32_fini(&region.mImpl);
  region.mImpl = pixmanRegion;
  return region;
}

nsIntRegion nsRegion::ToPixels (nscoord aAppUnitsPerPixel, bool aOutsidePixels) const
{
  nsRegion region = *this;
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&region.mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    mozilla::gfx::IntRect deviceRect;
    if (aOutsidePixels)
      deviceRect = rect.ToOutsidePixels(aAppUnitsPerPixel);
    else
      deviceRect = rect.ToNearestPixels(aAppUnitsPerPixel);

    boxes[i] = RectToBox(deviceRect);
  }

  nsIntRegion intRegion;
  pixman_region32_fini(&intRegion.mImpl.mImpl);
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&intRegion.mImpl.mImpl, boxes, n);

  return intRegion;
}

nsIntRegion nsRegion::ToOutsidePixels (nscoord aAppUnitsPerPixel) const
{
  return ToPixels(aAppUnitsPerPixel, true);
}

nsIntRegion nsRegion::ToNearestPixels (nscoord aAppUnitsPerPixel) const
{
  return ToPixels(aAppUnitsPerPixel, false);
}

nsIntRegion nsRegion::ScaleToNearestPixels (float aScaleX, float aScaleY,
                                            nscoord aAppUnitsPerPixel) const
{
  nsIntRegion result;
  nsRegionRectIterator rgnIter(*this);
  const nsRect* currentRect;
  while ((currentRect = rgnIter.Next())) {
    mozilla::gfx::IntRect deviceRect =
      currentRect->ScaleToNearestPixels(aScaleX, aScaleY, aAppUnitsPerPixel);
    result.Or(result, deviceRect);
  }
  return result;
}

nsIntRegion nsRegion::ScaleToOutsidePixels (float aScaleX, float aScaleY,
                                            nscoord aAppUnitsPerPixel) const
{
  nsIntRegion result;
  nsRegionRectIterator rgnIter(*this);
  const nsRect* currentRect;
  while ((currentRect = rgnIter.Next())) {
    mozilla::gfx::IntRect deviceRect =
      currentRect->ScaleToOutsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel);
    result.Or(result, deviceRect);
  }
  return result;
}

nsIntRegion nsRegion::ScaleToInsidePixels (float aScaleX, float aScaleY,
                                           nscoord aAppUnitsPerPixel) const
{
  /* When scaling a rect, walk forward through the rect list up until the y value is greater
   * than the current rect's YMost() value.
   *
   * For each rect found, check if the rects have a touching edge (in unscaled coordinates),
   * and if one edge is entirely contained within the other.
   *
   * If it is, then the contained edge can be moved (in scaled pixels) to ensure that no
   * gap exists.
   *
   * Since this could be potentially expensive - O(n^2), we only attempt this algorithm
   * for the first rect.
   */

  // make a copy of this region so that we can mutate it in place
  nsRegion region = *this;
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&region.mImpl, &n);

  nsIntRegion intRegion;
  if (n) {
    nsRect first = BoxToRect(boxes[0]);
    mozilla::gfx::IntRect firstDeviceRect =
      first.ScaleToInsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel);

    for (int i=1; i<n; i++) {
      nsRect rect = nsRect(boxes[i].x1, boxes[i].y1,
	  boxes[i].x2 - boxes[i].x1,
	  boxes[i].y2 - boxes[i].y1);
      mozilla::gfx::IntRect deviceRect =
	rect.ScaleToInsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel);

      if (rect.y <= first.YMost()) {
	if (rect.XMost() == first.x && rect.YMost() <= first.YMost()) {
	  // rect is touching on the left edge of the first rect and contained within
	  // the length of its left edge
	  deviceRect.SetRightEdge(firstDeviceRect.x);
	} else if (rect.x == first.XMost() && rect.YMost() <= first.YMost()) {
	  // rect is touching on the right edge of the first rect and contained within
	  // the length of its right edge
	  deviceRect.SetLeftEdge(firstDeviceRect.XMost());
	} else if (rect.y == first.YMost()) {
	  // The bottom of the first rect is on the same line as the top of rect, but
	  // they aren't necessarily contained.
	  if (rect.x <= first.x && rect.XMost() >= first.XMost()) {
	    // The top of rect contains the bottom of the first rect
	    firstDeviceRect.SetBottomEdge(deviceRect.y);
	  } else if (rect.x >= first.x && rect.XMost() <= first.XMost()) {
	    // The bottom of the first contains the top of rect
	    deviceRect.SetTopEdge(firstDeviceRect.YMost());
	  }
	}
      }

      boxes[i] = RectToBox(deviceRect);
    }

    boxes[0] = RectToBox(firstDeviceRect);

    pixman_region32_fini(&intRegion.mImpl.mImpl);
    // This will union all of the rectangles and runs in about O(n lg(n))
    pixman_region32_init_rects(&intRegion.mImpl.mImpl, boxes, n);
  }
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
//   S[j][i] = (if A[j-1][i-1] = 0 then some large negative value else A[j-1][i-1])
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
      if (i == 0 || mStops[i-1] != c) {
        mStops.InsertElementAt(i, c);
      }
    }

    // Returns the array index of the given partition point. The partition
    // point must already be present in the partitioning.
    int32_t IndexOf(nscoord p) const {
      return mStops.BinaryIndexOf(p);
    }

    // Returns the partition at the given index which must be non-zero and
    // less than the number of partitions in this partitioning.
    nscoord StopAt(int32_t index) const {
      return mStops[index];
    }

    // Returns the size of the gap between the partition at the given index and
    // the next partition in this partitioning. If the index is the last index
    // in the partitioning, the result is undefined.
    nscoord StopSize(int32_t index) const {
      return mStops[index+1] - mStops[index];
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
    SizePair& operator=(const SizePair& aOther) {
      mSizeContainingRect = aOther.mSizeContainingRect;
      mSize = aOther.mSize;
      return *this;
    }
    bool operator<(const SizePair& aOther) const {
      if (mSizeContainingRect < aOther.mSizeContainingRect)
        return true;
      if (mSizeContainingRect > aOther.mSizeContainingRect)
        return false;
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
  SizePair MaxSum1D(const nsTArray<SizePair> &A, int32_t n,
                    int32_t *minIdx, int32_t *maxIdx) {
    // The min/max indicies of the largest subarray found so far
    SizePair min, max;
    int32_t currentMinIdx = 0;

    *minIdx = 0;
    *maxIdx = 0;

    // Because we're given the array in prefix sum form, we know the first
    // element is 0
    for(int32_t i = 1; i < n; i++) {
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
} // namespace

nsRect nsRegion::GetLargestRectangle (const nsRect& aContainingRect) const {
  nsRect bestRect;

  if (GetNumRects() <= 1) {
    bestRect = GetBounds();
    return bestRect;
  }

  AxisPartition xaxis, yaxis;

  // Step 1: Calculate the grid lines
  nsRegionRectIterator iter(*this);
  const nsRect *currentRect;
  while ((currentRect = iter.Next())) {
    xaxis.InsertCoord(currentRect->x);
    xaxis.InsertCoord(currentRect->XMost());
    yaxis.InsertCoord(currentRect->y);
    yaxis.InsertCoord(currentRect->YMost());
  }
  if (!aContainingRect.IsEmpty()) {
    xaxis.InsertCoord(aContainingRect.x);
    xaxis.InsertCoord(aContainingRect.XMost());
    yaxis.InsertCoord(aContainingRect.y);
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

  iter.Reset();
  while ((currentRect = iter.Next())) {
    int32_t xstart = xaxis.IndexOf(currentRect->x);
    int32_t xend = xaxis.IndexOf(currentRect->XMost());
    int32_t y = yaxis.IndexOf(currentRect->y);
    int32_t yend = yaxis.IndexOf(currentRect->YMost());

    for (; y < yend; y++) {
      nscoord height = yaxis.StopSize(y);
      for (int32_t x = xstart; x < xend; x++) {
        nscoord width = xaxis.StopSize(x);
        int64_t size = width*int64_t(height);
        if (currentRect->Intersects(aContainingRect)) {
          areas[y*matrixWidth+x].mSizeContainingRect = size;
        }
        areas[y*matrixWidth+x].mSize = size;
      }
    }
  }

  // Step 3: Find the maximum submatrix sum that does not contain a rectangle
  {
    // First get the prefix sum array
    int32_t m = matrixHeight + 1;
    int32_t n = matrixWidth + 1;
    nsTArray<SizePair> pareas(m*n);
    pareas.SetLength(m*n);
    for (int32_t y = 1; y < m; y++) {
      for (int32_t x = 1; x < n; x++) {
        SizePair area = areas[(y-1)*matrixWidth+x-1];
        if (!area.mSize) {
          area = SizePair::VeryLargeNegative();
        }
        area = area + pareas[    y*n+x-1]
                    + pareas[(y-1)*n+x  ]
                    - pareas[(y-1)*n+x-1];
        pareas[y*n+x] = area;
      }
    }

    // No longer need the grid
    areas.SetLength(0);

    SizePair bestArea;
    struct {
      int32_t left, top, right, bottom;
    } bestRectIndices = { 0, 0, 0, 0 };
    for (int32_t m1 = 0; m1 < m; m1++) {
      for (int32_t m2 = m1+1; m2 < m; m2++) {
        nsTArray<SizePair> B;
        B.SetLength(n);
        for (int32_t i = 0; i < n; i++) {
          B[i] = pareas[m2*n+i] - pareas[m1*n+i];
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
    bestRect.SizeTo(xaxis.StopAt(bestRectIndices.right) - bestRect.x,
                    yaxis.StopAt(bestRectIndices.bottom) - bestRect.y);
  }

  return bestRect;
}

std::ostream& operator<<(std::ostream& stream, const nsRegion& m) {
  stream << "[";

  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(const_cast<pixman_region32_t*>(&m.mImpl), &n);
  for (int i=0; i<n; i++) {
    if (i != 0) {
      stream << "; ";
    }
    stream << boxes[i].x1 << "," << boxes[i].y1 << "," << boxes[i].x2 << "," << boxes[i].y2;
  }

  stream << "]";
  return stream;
}

nsCString
nsRegion::ToString() const {
  return nsCString(mozilla::ToString(*this).c_str());
}
