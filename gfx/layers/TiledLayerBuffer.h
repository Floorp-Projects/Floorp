/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TILEDLAYERBUFFER_H
#define GFX_TILEDLAYERBUFFER_H

#define TILEDLAYERBUFFER_TILE_SIZE 256

// Debug defines
#ifdef MOZ_JAVA_COMPOSITOR
  // This needs to go away as we enabled tiled
  // layers everywhere.
  #define FORCE_BASICTILEDTHEBESLAYER
#endif
//#define GFX_TILEDLAYER_DEBUG_OVERLAY
//#define GFX_TILEDLAYER_PREF_WARNINGS

#include "nsRect.h"
#include "nsRegion.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

// An abstract implementation of a tile buffer. This code covers the logic of
// moving and reusing tiles and leaves the validation up to the implementor. To
// avoid the overhead of virtual dispatch, we employ the curiously recurring
// template pattern.
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
// It is required that the derived class specify the base class as a friend. It
// must also implement the following public method:
//
//   Tile GetPlaceholderTile() const;
//
//   Returns a temporary placeholder tile used as a marker. This placeholder tile
//   must never be returned by validateTile and must be == to every instance
//   of a placeholder tile.
//
// Additionally, it must implement the following protected methods:
//
//   Tile ValidateTile(Tile aTile, const nsIntPoint& aTileOrigin,
//                     const nsIntRegion& aDirtyRect);
//
//   Validates the dirtyRect. The returned Tile will replace the tile.
//
//   void ReleaseTile(Tile aTile);
//
//   Destroys the given tile.
//
//   void SwapTiles(Tile& aTileA, Tile& aTileB);
//
//   Swaps two tiles.

template<typename Derived, typename Tile>
class TiledLayerBuffer
{
public:
  TiledLayerBuffer()
    : mRetainedWidth(0)
    , mRetainedHeight(0)
  {}

  ~TiledLayerBuffer() {}

  // Given a tile origin aligned to a multiple of GetTileLength(),
  // return the tile that describes that region.
  // NOTE: To get the valid area of that tile you must intersect
  //       (aTileOrigin.x, aTileOrigin.y, GetTileLength(), GetTileLength())
  //       and GetValidRegion() to get the area of the tile that is valid.
  Tile GetTile(const nsIntPoint& aTileOrigin) const;

  // Given a tile x, y relative to the top left of the layer, this function
  // will return the tile for
  // (x*GetTileLength(), y*GetTileLength(), GetTileLength(), GetTileLength())
  Tile GetTile(int x, int y) const;

  // This operates the same as GetTile(aTileOrigin), but will also replace the
  // specified tile with the placeholder tile. This does not call ReleaseTile
  // on the removed tile.
  bool RemoveTile(const nsIntPoint& aTileOrigin, Tile& aRemovedTile);

  // This operates the same as GetTile(x, y), but will also replace the
  // specified tile with the placeholder tile. This does not call ReleaseTile
  // on the removed tile.
  bool RemoveTile(int x, int y, Tile& aRemovedTile);

  uint16_t GetTileLength() const { return TILEDLAYERBUFFER_TILE_SIZE; }

  unsigned int GetTileCount() const { return mRetainedTiles.Length(); }

  const nsIntRegion& GetValidRegion() const { return mValidRegion; }
  const nsIntRegion& GetLastPaintRegion() const { return mLastPaintRegion; }
  void SetLastPaintRegion(const nsIntRegion& aLastPaintRegion) {
    mLastPaintRegion = aLastPaintRegion;
  }

  // Given a position i, this function returns the position inside the current tile.
  int GetTileStart(int i) const {
    return (i >= 0) ? (i % GetTileLength())
                    : ((GetTileLength() - (-i % GetTileLength())) % GetTileLength());
  }

  // Rounds the given coordinate down to the nearest tile boundary.
  int RoundDownToTileEdge(int aX) const { return aX - GetTileStart(aX); }

protected:
  // The implementor should call Update() to change
  // the new valid region. This implementation will call
  // validateTile on each tile that is dirty, which is left
  // to the implementor.
  void Update(const nsIntRegion& aNewValidRegion, const nsIntRegion& aPaintRegion);

  nsIntRegion     mValidRegion;
  nsIntRegion     mLastPaintRegion;

  /**
   * mRetainedTiles is a rectangular buffer of mRetainedWidth x mRetainedHeight
   * stored as column major with the same origin as mValidRegion.GetBounds().
   * Any tile that does not intersect mValidRegion is a PlaceholderTile.
   * Only the region intersecting with mValidRegion should be read from a tile,
   * another other region is assumed to be uninitialized.
   */
  nsTArray<Tile>  mRetainedTiles;
  int             mRetainedWidth;  // in tiles
  int             mRetainedHeight; // in tiles

private:
  const Derived& AsDerived() const { return *static_cast<const Derived*>(this); }
  Derived& AsDerived() { return *static_cast<Derived*>(this); }

  bool IsPlaceholder(Tile aTile) const { return aTile == AsDerived().GetPlaceholderTile(); }
};

class BasicTiledLayerBuffer;

// Shadow layers may implement this interface in order to be notified when a
// tiled layer buffer is updated.
class TiledLayerComposer
{
public:
  /**
   * Update the current retained layer with the updated layer data.
   * The BasicTiledLayerBuffer is expected to be in the ReadLock state
   * prior to this being called. aTiledBuffer is copy constructed and
   * is retained until it has been uploaded/copyed and unlocked.
   */
  virtual void PaintedTiledLayerBuffer(const BasicTiledLayerBuffer* aTiledBuffer) = 0;
};

// Normal integer division truncates towards zero,
// we instead want to floor to hangle negative numbers.
static int floor_div(int a, int b)
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

template<typename Derived, typename Tile> Tile
TiledLayerBuffer<Derived, Tile>::GetTile(const nsIntPoint& aTileOrigin) const
{
  // TODO Cache firstTileOriginX/firstTileOriginY
  // Find the tile x/y of the first tile and the target tile relative to the (0, 0)
  // origin, the difference is the tile x/y relative to the start of the tile buffer.
  int firstTileX = floor_div(mValidRegion.GetBounds().x, GetTileLength());
  int firstTileY = floor_div(mValidRegion.GetBounds().y, GetTileLength());
  return GetTile(floor_div(aTileOrigin.x, GetTileLength()) - firstTileX,
                 floor_div(aTileOrigin.y, GetTileLength()) - firstTileY);
}

template<typename Derived, typename Tile> Tile
TiledLayerBuffer<Derived, Tile>::GetTile(int x, int y) const
{
  int index = x * mRetainedHeight + y;
  return mRetainedTiles.SafeElementAt(index, AsDerived().GetPlaceholderTile());
}

template<typename Derived, typename Tile> bool
TiledLayerBuffer<Derived, Tile>::RemoveTile(const nsIntPoint& aTileOrigin,
                                            Tile& aRemovedTile)
{
  int firstTileX = floor_div(mValidRegion.GetBounds().x, GetTileLength());
  int firstTileY = floor_div(mValidRegion.GetBounds().y, GetTileLength());
  return RemoveTile(floor_div(aTileOrigin.x, GetTileLength()) - firstTileX,
                    floor_div(aTileOrigin.y, GetTileLength()) - firstTileY,
                    aRemovedTile);
}

template<typename Derived, typename Tile> bool
TiledLayerBuffer<Derived, Tile>::RemoveTile(int x, int y, Tile& aRemovedTile)
{
  int index = x * mRetainedHeight + y;
  const Tile& tileToRemove = mRetainedTiles.SafeElementAt(index, AsDerived().GetPlaceholderTile());
  if (!IsPlaceholder(tileToRemove)) {
    aRemovedTile = tileToRemove;
    mRetainedTiles[index] = AsDerived().GetPlaceholderTile();
    return true;
  }
  return false;
}

template<typename Derived, typename Tile> void
TiledLayerBuffer<Derived, Tile>::Update(const nsIntRegion& aNewValidRegion,
                                        const nsIntRegion& aPaintRegion)
{
  nsTArray<Tile>  newRetainedTiles;
  nsTArray<Tile>& oldRetainedTiles = mRetainedTiles;
  const nsIntRect oldBound = mValidRegion.GetBounds();
  const nsIntRect newBound = aNewValidRegion.GetBounds();
  const nsIntPoint oldBufferOrigin(RoundDownToTileEdge(oldBound.x),
                                   RoundDownToTileEdge(oldBound.y));
  const nsIntPoint newBufferOrigin(RoundDownToTileEdge(newBound.x),
                                   RoundDownToTileEdge(newBound.y));
  const nsIntRegion& oldValidRegion = mValidRegion;
  const nsIntRegion& newValidRegion = aNewValidRegion;
  const int oldRetainedHeight = mRetainedHeight;

  // Pass 1: Recycle valid content from the old buffer
  // Recycle tiles from the old buffer that contain valid regions.
  // Insert placeholders tiles if we have no valid area for that tile
  // which we will allocate in pass 2.
  // TODO: Add a tile pool to reduce new allocation
  int tileX = 0;
  int tileY;
  // Iterate over the new drawing bounds in steps of tiles.
  for (int32_t x = newBound.x; x < newBound.XMost(); tileX++) {
    // Compute tileRect(x,y,width,height) in layer space coordinate
    // giving us the rect of the tile that hits the newBounds.
    int width = GetTileLength() - GetTileStart(x);
    if (x + width > newBound.XMost()) {
      width = newBound.x + newBound.width - x;
    }

    tileY = 0;
    for (int32_t y = newBound.y; y < newBound.YMost(); tileY++) {
      int height = GetTileLength() - GetTileStart(y);
      if (y + height > newBound.y + newBound.height) {
        height = newBound.y + newBound.height - y;
      }

      const nsIntRect tileRect(x,y,width,height);
      if (oldValidRegion.Intersects(tileRect) && newValidRegion.Intersects(tileRect)) {
        // This old tiles contains some valid area so move it to the new tile
        // buffer. Replace the tile in the old buffer with a placeholder
        // to leave the old buffer index unaffected.
        int tileX = floor_div(x - oldBufferOrigin.x, GetTileLength());
        int tileY = floor_div(y - oldBufferOrigin.y, GetTileLength());
        int index = tileX * oldRetainedHeight + tileY;

        // The tile may have been removed, skip over it in this case.
        if (IsPlaceholder(oldRetainedTiles.
                          SafeElementAt(index, AsDerived().GetPlaceholderTile()))) {
          newRetainedTiles.AppendElement(AsDerived().GetPlaceholderTile());
        } else {
          Tile tileWithPartialValidContent = oldRetainedTiles[index];
          newRetainedTiles.AppendElement(tileWithPartialValidContent);
          oldRetainedTiles[index] = AsDerived().GetPlaceholderTile();
        }

      } else {
        // This tile is either:
        // 1) Outside the new valid region and will simply be an empty
        // placeholder forever.
        // 2) The old buffer didn't have any data for this tile. We postpone
        // the allocation of this tile after we've reused any tile with
        // valid content because then we know we can safely recycle
        // with taking from a tile that has recyclable content.
        newRetainedTiles.AppendElement(AsDerived().GetPlaceholderTile());
      }

      y += height;
    }

    x += width;
  }

  // Keep track of the number of horizontal/vertical tiles
  // in the buffer so that we can easily look up a tile.
  mRetainedWidth = tileX;
  mRetainedHeight = tileY;

  NS_ABORT_IF_FALSE(aNewValidRegion.Contains(aPaintRegion), "Painting a region outside the visible region");
#ifdef DEBUG
  nsIntRegion oldAndPainted(oldValidRegion);
  oldAndPainted.Or(oldAndPainted, aPaintRegion);
#endif
  NS_ABORT_IF_FALSE(oldAndPainted.Contains(newValidRegion), "newValidRegion has not been fully painted");

  nsIntRegion regionToPaint(aPaintRegion);

  // Pass 2: Validate
  // We know at this point that any tile in the new buffer that had valid content
  // from the previous buffer is placed correctly in the new buffer.
  // We know that any tile in the old buffer that isn't a place holder is
  // of no use and can be recycled.
  // We also know that any place holder tile in the new buffer must be
  // allocated.
  tileX = 0;
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Update %i, %i, %i, %i\n", newBound.x, newBound.y, newBound.width, newBound.height);
#endif
  for (int x = newBound.x; x < newBound.x + newBound.width; tileX++) {
    // Compute tileRect(x,y,width,height) in layer space coordinate
    // giving us the rect of the tile that hits the newBounds.
    int tileStartX = RoundDownToTileEdge(x);
    int width = GetTileLength() - GetTileStart(x);
    if (x + width > newBound.XMost())
      width = newBound.XMost() - x;

    tileY = 0;
    for (int y = newBound.y; y < newBound.y + newBound.height; tileY++) {
      int tileStartY = RoundDownToTileEdge(y);
      int height = GetTileLength() - GetTileStart(y);
      if (y + height > newBound.YMost()) {
        height = newBound.YMost() - y;
      }

      const nsIntRect tileRect(x, y, width, height);

      nsIntRegion tileDrawRegion;
      tileDrawRegion.And(tileRect, regionToPaint);

      if (tileDrawRegion.IsEmpty()) {
        // We have a tile but it doesn't hit the draw region
        // because we can reuse all of the content from the
        // previous buffer.
#ifdef DEBUG
        int currTileX = floor_div(x - newBufferOrigin.x, GetTileLength());
        int currTileY = floor_div(y - newBufferOrigin.y, GetTileLength());
        int index = currTileX * mRetainedHeight + currTileY;
        NS_ABORT_IF_FALSE(!newValidRegion.Intersects(tileRect) ||
                          !IsPlaceholder(newRetainedTiles.
                                         SafeElementAt(index, AsDerived().GetPlaceholderTile())),
                          "If we don't draw a tile we shouldn't have a placeholder there.");
#endif
        y += height;
        continue;
      }

      int tileX = floor_div(x - newBufferOrigin.x, GetTileLength());
      int tileY = floor_div(y - newBufferOrigin.y, GetTileLength());
      int index = tileX * mRetainedHeight + tileY;
      NS_ABORT_IF_FALSE(index >= 0 && index < newRetainedTiles.Length(), "index out of range");
      Tile newTile = newRetainedTiles[index];
      while (IsPlaceholder(newTile) && oldRetainedTiles.Length() > 0) {
        AsDerived().SwapTiles(newTile, oldRetainedTiles[oldRetainedTiles.Length()-1]);
        oldRetainedTiles.RemoveElementAt(oldRetainedTiles.Length()-1);
      }

      // We've done our best effort to recycle a tile but it can be null
      // in which case it's up to the derived class's ValidateTile()
      // implementation to allocate a new tile before drawing
      nsIntPoint tileOrigin(tileStartX, tileStartY);
      newTile = AsDerived().ValidateTile(newTile, nsIntPoint(tileStartX, tileStartY),
                                         tileDrawRegion);
      NS_ABORT_IF_FALSE(!IsPlaceholder(newTile), "index out of range");
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
      printf_stderr("Store Validate tile %i, %i -> %i\n", tileStartX, tileStartY, index);
#endif
      newRetainedTiles[index] = newTile;

      y += height;
    }

    x += width;
  }

  // Throw away any tiles we didn't recycle
  // TODO: Add a tile pool
  while (oldRetainedTiles.Length() > 0) {
    Tile oldTile = oldRetainedTiles[oldRetainedTiles.Length()-1];
    oldRetainedTiles.RemoveElementAt(oldRetainedTiles.Length()-1);
    AsDerived().ReleaseTile(oldTile);
  }

  mRetainedTiles = newRetainedTiles;
  mValidRegion = aNewValidRegion;
  mLastPaintRegion = aPaintRegion;
}

} // layers
} // mozilla

#endif // GFX_TILEDLAYERBUFFER_H
