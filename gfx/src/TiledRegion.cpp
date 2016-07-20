/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TiledRegion.h"

#include <algorithm>

#include "mozilla/fallible.h"

namespace mozilla {
namespace gfx {

static const int32_t kTileSize = 256;
static const size_t kMaxTiles = 1000;

/**
 * TiledRegionImpl stores an array of non-empty rectangles (pixman_box32_ts) to
 * represent the region. Each rectangle is contained in a single tile;
 * rectangles never cross tile boundaries. The rectangles are sorted by their
 * tile's origin in top-to-bottom, left-to-right order.
 * (Note that this can mean that a rectangle r1 can come before another
 * rectangle r2 even if r2.y1 < r1.y1, as long as the two rects are in the same
 * row of tiles and r1.x1 < r2.x1.)
 * Empty tiles take up no space in the array - there is no rectangle stored for
 * them. As a result, any algorithm that needs to deal with empty tiles will
 * iterate through the mRects array and compare the positions of two
 * consecutive rects to figure out whether there are any empty tiles between
 * them.
 */

static pixman_box32_t
IntersectionOfNonEmptyBoxes(const pixman_box32_t& aBox1,
                            const pixman_box32_t& aBox2)
{
  return pixman_box32_t {
    std::max(aBox1.x1, aBox2.x1),
    std::max(aBox1.y1, aBox2.y1),
    std::min(aBox1.x2, aBox2.x2),
    std::min(aBox1.y2, aBox2.y2)
  };
}

// A TileIterator points to a specific tile inside a certain tile range, or to
// the end of the tile range. Advancing a TileIterator will move to the next
// tile inside the range (or to the range end). The next tile is either the
// tile to the right of the current one, or the first tile of the next tile
// row if the current tile is already the last tile in the row.
class TileIterator {
public:
  TileIterator(const pixman_box32_t& aTileBounds, const IntPoint& aPosition)
    : mTileBounds(aTileBounds)
    , mPos(aPosition)
  {}

  bool operator!=(const TileIterator& aOther) { return mPos != aOther.mPos; }
  bool operator==(const TileIterator& aOther) { return mPos == aOther.mPos; }

  IntPoint operator*() const { return mPos; }

  const TileIterator& operator++() {
    mPos.x += kTileSize;
    if (mPos.x >= mTileBounds.x2) {
      mPos.x = mTileBounds.x1;
      mPos.y += kTileSize;
    }
    return *this;
  }

  TileIterator& operator=(const IntPoint& aPosition)
  {
    mPos = aPosition;
    return *this;
  }

  bool IsBeforeTileContainingPoint(const IntPoint& aPoint) const
  {
    return (mPos.y + kTileSize) <= aPoint.y  ||
      (mPos.y <= aPoint.y && (mPos.x + kTileSize) <= aPoint.x);
  }

  bool IsAtTileContainingPoint(const IntPoint& aPoint) const
  {
    return mPos.y <= aPoint.y && aPoint.y < (mPos.y + kTileSize) &&
           mPos.x <= aPoint.x && aPoint.x < (mPos.x + kTileSize);

  }

  pixman_box32_t IntersectionWith(const pixman_box32_t& aRect) const
  {
    pixman_box32_t tile = { mPos.x, mPos.y,
                            mPos.x + kTileSize, mPos.y + kTileSize };
    return IntersectionOfNonEmptyBoxes(tile, aRect);
  }

private:
  const pixman_box32_t& mTileBounds;
  IntPoint mPos;
};

// A TileRange describes a range of tiles contained inside a certain tile
// bounds (which is a rectangle that includes all tiles that you're
// interested in). The tile range can start and end at any point inside a
// tile row.
// The tile range end is described by the tile that starts at the bottom
// left corner of the tile bounds, i.e. the first tile under the tile
// bounds.
class TileRange {
public:
  // aTileBounds, aStart and aEnd need to be aligned with the tile grid.
  TileRange(const pixman_box32_t& aTileBounds,
            const IntPoint& aStart, const IntPoint& aEnd)
    : mTileBounds(aTileBounds)
    , mStart(aStart)
    , mEnd(aEnd)
  {}
  // aTileBounds needs to be aligned with the tile grid.
  explicit TileRange(const pixman_box32_t& aTileBounds)
    : mTileBounds(aTileBounds)
    , mStart(mTileBounds.x1, mTileBounds.y1)
    , mEnd(mTileBounds.x1, mTileBounds.y2)
  {}

  TileIterator Begin() const { return TileIterator(mTileBounds, mStart); }
  TileIterator End() const { return TileIterator(mTileBounds, mEnd); }

  // The number of tiles in this tile range.
  size_t Length() const
  {
    if (mEnd.y == mStart.y) {
      return (mEnd.x - mStart.x) / kTileSize;
    }
    int64_t numberOfFullRows = (((int64_t)mEnd.y - (int64_t)mStart.y) / kTileSize) - 1;
    int64_t tilesInFirstRow = ((int64_t)mTileBounds.x2 - (int64_t)mStart.x) / kTileSize;
    int64_t tilesInLastRow = ((int64_t)mEnd.x - (int64_t)mTileBounds.x1) / kTileSize;
    int64_t tilesInFullRow = ((int64_t)mTileBounds.x2 - (int64_t)mTileBounds.x1) / kTileSize;
    int64_t total = tilesInFirstRow + (tilesInFullRow * numberOfFullRows) + tilesInLastRow;
    MOZ_ASSERT(total > 0);
    // The total may be larger than what fits in a size_t, so clamp it to
    // SIZE_MAX in that case.
    return ((uint64_t)total > (uint64_t)SIZE_MAX) ? SIZE_MAX : (size_t)total;
  }

  // If aTileOrigin does not describe a tile inside our tile bounds, move it
  // to the next tile that you'd encounter by "advancing" a tile iterator
  // inside these tile bounds. If aTileOrigin is after the last tile inside
  // our tile bounds, move it to the range end tile.
  // The result of this method is a valid end tile for a tile range with our
  // tile bounds.
  IntPoint MoveIntoBounds(const IntPoint& aTileOrigin) const
  {
    IntPoint p = aTileOrigin;
    if (p.x < mTileBounds.x1) {
      p.x = mTileBounds.x1;
    } else if (p.x >= mTileBounds.x2) {
      p.x = mTileBounds.x1;
      p.y += kTileSize;
    }
    if (p.y < mTileBounds.y1) {
      p.y = mTileBounds.y1;
      p.x = mTileBounds.x1;
    } else if (p.y >= mTileBounds.y2) {
      // There's only one valid state after the end of the tile range, and that's
      // the bottom left point of the tile bounds.
      p.x = mTileBounds.x1;
      p.y = mTileBounds.y2;
    }
    return p;
  }

private:
  const pixman_box32_t& mTileBounds;
  const IntPoint mStart;
  const IntPoint mEnd;
};

static IntPoint
TileContainingPoint(const IntPoint& aPoint)
{
  return IntPoint(RoundDownToMultiple(aPoint.x, kTileSize),
                  RoundDownToMultiple(aPoint.y, kTileSize));
}

enum class IterationAction : uint8_t {
  CONTINUE,
  STOP
};

enum class IterationEndReason : uint8_t {
  NOT_STOPPED,
  STOPPED
};

template<
  typename HandleEmptyTilesFunction,
  typename HandleNonEmptyTileFunction,
  typename RectArrayT>
IterationEndReason ProcessIntersectedTiles(const pixman_box32_t& aRect,
                                           RectArrayT& aRectArray,
                                           HandleEmptyTilesFunction aHandleEmptyTiles,
                                           HandleNonEmptyTileFunction aHandleNonEmptyTile)
{
  pixman_box32_t tileBounds = {
    RoundDownToMultiple(aRect.x1, kTileSize),
    RoundDownToMultiple(aRect.y1, kTileSize),
    RoundUpToMultiple(aRect.x2, kTileSize),
    RoundUpToMultiple(aRect.y2, kTileSize)
  };
  if (tileBounds.x2 < tileBounds.x1 || tileBounds.y2 < tileBounds.y1) {
    // RoundUpToMultiple probably overflowed. Bail out.
    return IterationEndReason::STOPPED;
  }

  TileRange tileRange(tileBounds);
  TileIterator rangeEnd = tileRange.End();

  // tileIterator points to the next tile in tileRange, or to rangeEnd if we're
  // done.
  TileIterator tileIterator = tileRange.Begin();

  // We iterate over the rectangle array. Depending on the position of the
  // rectangle we encounter, we may need to advance tileIterator by zero, one,
  // or more tiles:
  //  - Zero if the rectangle we encountered is outside the tiles that
  //    intersect aRect.
  //  - One if the rectangle is in the exact tile that we're interested in next
  //    (i.e. the tile that tileIterator points at).
  //  - More than one if the encountered rectangle is in a tile that's further
  //    to the right or to the bottom than tileIterator. In that case there is
  //    at least one empty tile between the last rectangle we encountered and
  //    the current one.
  for (size_t i = 0; i < aRectArray.Length() && tileIterator != rangeEnd; i++) {
    MOZ_ASSERT(aRectArray[i].x1 < aRectArray[i].x2 && aRectArray[i].y1 < aRectArray[i].y2, "empty rect");
    IntPoint rectOrigin(aRectArray[i].x1, aRectArray[i].y1);
    if (tileIterator.IsBeforeTileContainingPoint(rectOrigin)) {
      IntPoint tileOrigin = TileContainingPoint(rectOrigin);
      IntPoint afterEmptyTiles = tileRange.MoveIntoBounds(tileOrigin);
      TileRange emptyTiles(tileBounds, *tileIterator, afterEmptyTiles);
      if (aHandleEmptyTiles(aRectArray, i, emptyTiles) == IterationAction::STOP) {
        return IterationEndReason::STOPPED;
      }
      tileIterator = afterEmptyTiles;
      if (tileIterator == rangeEnd) {
        return IterationEndReason::NOT_STOPPED;
      }
    }
    if (tileIterator.IsAtTileContainingPoint(rectOrigin)) {
      pixman_box32_t rectIntersection = tileIterator.IntersectionWith(aRect);
      if (aHandleNonEmptyTile(aRectArray, i, rectIntersection) == IterationAction::STOP) {
        return IterationEndReason::STOPPED;
      }
      ++tileIterator;
    }
  }

  if (tileIterator != rangeEnd) {
    // We've looked at all of our existing rectangles but haven't covered all
    // of the tiles that we're interested in yet. So we need to deal with the
    // remaining tiles now.
    size_t endIndex = aRectArray.Length();
    TileRange emptyTiles(tileBounds, *tileIterator, *rangeEnd);
    if (aHandleEmptyTiles(aRectArray, endIndex, emptyTiles) == IterationAction::STOP) {
      return IterationEndReason::STOPPED;
    }
  }
  return IterationEndReason::NOT_STOPPED;
}

static pixman_box32_t
UnionBoundsOfNonEmptyBoxes(const pixman_box32_t& aBox1,
                           const pixman_box32_t& aBox2)
{
  return { std::min(aBox1.x1, aBox2.x1),
           std::min(aBox1.y1, aBox2.y1),
           std::max(aBox1.x2, aBox2.x2),
           std::max(aBox1.y2, aBox2.y2) };
}

// Returns true when adding the rectangle was successful, and false if
// allocation failed.
// When this returns false, our internal state might not be consistent and we
// need to be cleared.
bool
TiledRegionImpl::AddRect(const pixman_box32_t& aRect)
{
  // We are adding a rectangle that can span multiple tiles.
  // For each empty tile that aRect intersects, we need to add the intersection
  // of aRect with that tile to mRects, respecting the order of mRects.
  // For each tile that already has a rectangle, we need to enlarge that
  // existing rectangle to include the intersection of aRect with the tile.
  return ProcessIntersectedTiles(aRect, mRects,
    [&aRect](nsTArray<pixman_box32_t>& rects, size_t& rectIndex, TileRange emptyTiles) {
      CheckedInt<size_t> newLength(rects.Length());
      newLength += emptyTiles.Length();
      if (!newLength.isValid() || newLength.value() >= kMaxTiles ||
          !rects.InsertElementsAt(rectIndex, emptyTiles.Length(), fallible)) {
        return IterationAction::STOP;
      }
      for (TileIterator tileIt = emptyTiles.Begin();
           tileIt != emptyTiles.End();
           ++tileIt, ++rectIndex) {
        rects[rectIndex] = tileIt.IntersectionWith(aRect);
      }
      return IterationAction::CONTINUE;
    },
    [](nsTArray<pixman_box32_t>& rects, size_t rectIndex, const pixman_box32_t& rectIntersectionWithTile) {
      rects[rectIndex] =
        UnionBoundsOfNonEmptyBoxes(rects[rectIndex], rectIntersectionWithTile);
      return IterationAction::CONTINUE;
    }) == IterationEndReason::NOT_STOPPED;
}

static bool
NonEmptyBoxesIntersect(const pixman_box32_t& aBox1, const pixman_box32_t& aBox2)
{
  return aBox1.x1 < aBox2.x2 && aBox2.x1 < aBox1.x2 &&
         aBox1.y1 < aBox2.y2 && aBox2.y1 < aBox1.y2;
}

bool
TiledRegionImpl::Intersects(const pixman_box32_t& aRect) const
{
  // aRect intersects this region if it intersects any of our rectangles.
  return ProcessIntersectedTiles(aRect, mRects,
    [](const nsTArray<pixman_box32_t>& rects, size_t& rectIndex, TileRange emptyTiles) {
      // Ignore empty tiles and keep on iterating.
      return IterationAction::CONTINUE;
    },
    [](const nsTArray<pixman_box32_t>& rects, size_t rectIndex, const pixman_box32_t& rectIntersectionWithTile) {
      if (NonEmptyBoxesIntersect(rects[rectIndex], rectIntersectionWithTile)) {
        // Found an intersecting rectangle, so aRect intersects this region.
        return IterationAction::STOP;
      }
      return IterationAction::CONTINUE;
    }) == IterationEndReason::STOPPED;
}

static bool
NonEmptyBoxContainsNonEmptyBox(const pixman_box32_t& aBox1, const pixman_box32_t& aBox2)
{
  return aBox1.x1 <= aBox2.x1 && aBox2.x2 <= aBox1.x2 &&
         aBox1.y1 <= aBox2.y1 && aBox2.y2 <= aBox1.y2;
}

bool
TiledRegionImpl::Contains(const pixman_box32_t& aRect) const
{
  // aRect is contained in this region if aRect does not intersect any empty
  // tiles and, for each non-empty tile, if the intersection of aRect with that
  // tile is contained in the existing rectangle we have in that tile.
  return ProcessIntersectedTiles(aRect, mRects,
    [](const nsTArray<pixman_box32_t>& rects, size_t& rectIndex, TileRange emptyTiles) {
      // Found an empty tile that intersects aRect, so aRect is not contained
      // in this region.
      return IterationAction::STOP;
    },
    [](const nsTArray<pixman_box32_t>& rects, size_t rectIndex, const pixman_box32_t& rectIntersectionWithTile) {
      if (!NonEmptyBoxContainsNonEmptyBox(rects[rectIndex], rectIntersectionWithTile)) {
        // Our existing rectangle in this tile does not cover the part of aRect that
        // intersects this tile, so aRect is not contained in this region.
        return IterationAction::STOP;
      }
      return IterationAction::CONTINUE;
    }) == IterationEndReason::NOT_STOPPED;
}

} // namespace gfx
} // namespace mozilla
