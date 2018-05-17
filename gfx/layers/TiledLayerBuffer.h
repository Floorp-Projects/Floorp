/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TILEDLAYERBUFFER_H
#define GFX_TILEDLAYERBUFFER_H

// Debug defines
//#define GFX_TILEDLAYER_DEBUG_OVERLAY
//#define GFX_TILEDLAYER_PREF_WARNINGS
//#define GFX_TILEDLAYER_RETAINING_LOG

#include <stdint.h>                     // for uint16_t, uint32_t
#include <sys/types.h>                  // for int32_t
#include "LayersLogging.h"              // for print_stderr
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Logging.h"        // for gfxCriticalError
#include "mozilla/layers/LayersTypes.h" // for TextureDumpMode
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray

namespace mozilla {

struct TileCoordUnit {};
template<> struct IsPixel<TileCoordUnit> : mozilla::TrueType {};

namespace layers {

// You can enable all the TILING_LOG print statements by
// changing the 0 to a 1 in the following #define.
#define ENABLE_TILING_LOG 0

#if ENABLE_TILING_LOG
#  define TILING_LOG(...) printf_stderr(__VA_ARGS__);
#else
#  define TILING_LOG(...)
#endif

// Normal integer division truncates towards zero,
// we instead want to floor to hangle negative numbers.
static inline int floor_div(int a, int b)
{
  int rem = a % b;
  int div = a/b;
  if (rem == 0) {
    return div;
  } else {
    // If the signs are different substract 1.
    int sub;
    sub = a ^ b;
    // The results of this shift is either 0 or -1.
    sub >>= 8*sizeof(int)-1;
    return div+sub;
  }
}

// Tiles are aligned to a grid with one of the grid points at (0,0) and other
// grid points spaced evenly in the x- and y-directions by GetTileSize()
// multiplied by mResolution. GetScaledTileSize() provides convenience for
// accessing these values.
//
// This tile buffer stores a valid region, which defines the areas that have
// up-to-date content. The contents of tiles within this region will be reused
// from paint to paint. It also stores the region that was modified in the last
// paint operation; this is useful when one tiled layer buffer shadows another
// (as in an off-main-thread-compositing scenario), so that the shadow tiled
// layer buffer can correctly reflect the updates of the master layer buffer.
//
// The associated Tile may be of any type as long as the derived class can
// validate and return tiles of that type. Tiles will be frequently copied, so
// the tile type should be a reference or some other type with an efficient
// copy constructor.
//
// The contents of the tile buffer will be rendered at the resolution specified
// in mResolution, which can be altered with SetResolution. The resolution
// should always be a factor of the tile length, to avoid tiles covering
// non-integer amounts of pixels.

// Size and Point in number of tiles rather than in pixels
typedef gfx::IntSizeTyped<TileCoordUnit> TileCoordIntSize;
typedef gfx::IntPointTyped<TileCoordUnit> TileCoordIntPoint;

/**
 * Stores the origin and size of a tile buffer and handles switching between
 * tile indices and tile coordinates.
 *
 * Tile coordinates in TileCoordIntPoint take the first tile offset into account which
 * means that two TilesPlacement of the same layer and resolution give tile
 * coordinates in the same coordinate space (useful when changing the offset and/or
 * size of a tile buffer).
 */
struct TilesPlacement
{
  TileCoordIntPoint mFirst;
  TileCoordIntSize mSize;

  TilesPlacement(int aFirstX, int aFirstY,
                 int aRetainedWidth, int aRetainedHeight)
  : mFirst(aFirstX, aFirstY)
  , mSize(aRetainedWidth, aRetainedHeight)
  { }

  int TileIndex(TileCoordIntPoint aCoord) const
  {
    return (aCoord.x - mFirst.x) * mSize.height + aCoord.y - mFirst.y;
  }

  TileCoordIntPoint TileCoord(size_t aIndex) const
  {
    return TileCoordIntPoint(
      mFirst.x + aIndex / mSize.height,
      mFirst.y + aIndex % mSize.height
    );
  }

  bool HasTile(TileCoordIntPoint aCoord) const
  {
    return aCoord.x >= mFirst.x && aCoord.x < mFirst.x + mSize.width &&
           aCoord.y >= mFirst.y && aCoord.y < mFirst.y + mSize.height;
  }
};


// Given a position i, this function returns the position inside the current tile.
inline int GetTileStart(int i, int aTileLength)
{
  return (i >= 0) ? (i % aTileLength)
                  : ((aTileLength - (-i % aTileLength)) %
                     aTileLength);
}

// Rounds the given coordinate down to the nearest tile boundary.
inline int RoundDownToTileEdge(int aX, int aTileLength) { return aX - GetTileStart(aX, aTileLength); }

template<typename Derived, typename Tile>
class TiledLayerBuffer
{
public:
  TiledLayerBuffer()
    : mTiles(0, 0, 0, 0)
    , mResolution(1)
    , mTileSize(mozilla::gfx::gfxVars::TileSize())
  {}

  ~TiledLayerBuffer() {}

  gfx::IntPoint GetTileOffset(TileCoordIntPoint aPosition) const
  {
    gfx::IntSize scaledTileSize = GetScaledTileSize();
    return gfx::IntPoint(aPosition.x * scaledTileSize.width,
                         aPosition.y * scaledTileSize.height) + mTileOrigin;
  }

  const TilesPlacement& GetPlacement() const { return mTiles; }

  const gfx::IntSize& GetTileSize() const { return mTileSize; }

  gfx::IntSize GetScaledTileSize() const { return gfx::IntSize::Round(gfx::Size(mTileSize) / mResolution); }

  unsigned int GetTileCount() const { return mRetainedTiles.Length(); }

  Tile& GetTile(size_t i) { return mRetainedTiles[i]; }

  const nsIntRegion& GetValidRegion() const { return mValidRegion; }

  // Get and set draw scaling. mResolution affects the resolution at which the
  // contents of the buffer are drawn. mResolution has no effect on the
  // coordinate space of the valid region, but does affect the size of an
  // individual tile's rect in relation to the valid region.
  // Setting the resolution will invalidate the buffer.
  float GetResolution() const { return mResolution; }
  bool IsLowPrecision() const { return mResolution < 1; }

  void Dump(std::stringstream& aStream, const char* aPrefix, bool aDumpHtml,
            TextureDumpMode aCompress);

protected:

  nsIntRegion     mValidRegion;

  /**
   * mRetainedTiles is a rectangular buffer of mTiles.mSize.width x mTiles.mSize.height
   * stored as column major with the same origin as mValidRegion.GetBounds().
   * Any tile that does not intersect mValidRegion is a PlaceholderTile.
   * Only the region intersecting with mValidRegion should be read from a tile,
   * another other region is assumed to be uninitialized. The contents of the
   * tiles is scaled by mResolution.
   */
  nsTArray<Tile>  mRetainedTiles;
  TilesPlacement  mTiles;
  float           mResolution;
  gfx::IntSize    mTileSize;
  gfx::IntPoint   mTileOrigin;
};

template<typename Derived, typename Tile> void
TiledLayerBuffer<Derived, Tile>::Dump(std::stringstream& aStream,
                                      const char* aPrefix,
                                      bool aDumpHtml,
                                      TextureDumpMode aCompress)
{
  for (size_t i = 0; i < mRetainedTiles.Length(); ++i) {
    const TileCoordIntPoint tileCoord = mTiles.TileCoord(i);
    gfx::IntPoint tileOffset = GetTileOffset(tileCoord);

    aStream << "\n" << aPrefix << "Tile (x=" <<
      tileOffset.x << ", y=" << tileOffset.y << "): ";
    if (!mRetainedTiles[i].IsPlaceholderTile()) {
      mRetainedTiles[i].DumpTexture(aStream, aCompress);
    } else {
      aStream << "empty tile";
    }
  }
}

} // namespace layers
} // namespace mozilla

#endif // GFX_TILEDLAYERBUFFER_H
