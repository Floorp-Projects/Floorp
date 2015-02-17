/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TILEDLAYERBUFFER_H
#define GFX_TILEDLAYERBUFFER_H

// Debug defines
//#define GFX_TILEDLAYER_DEBUG_OVERLAY
//#define GFX_TILEDLAYER_PREF_WARNINGS

#include <stdint.h>                     // for uint16_t, uint32_t
#include <sys/types.h>                  // for int32_t
#include "gfxPlatform.h"                // for GetTileWidth/GetTileHeight
#include "mozilla/gfx/Logging.h"        // for gfxCriticalError
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
#include <ui/Fence.h>
#endif

namespace mozilla {
namespace layers {

// You can enable all the TILING_LOG print statements by
// changing the 0 to a 1 in the following #define.
#define ENABLE_TILING_LOG 0

#if ENABLE_TILING_LOG
#  define TILING_LOG(...) printf_stderr(__VA_ARGS__);
#else
#  define TILING_LOG(...)
#endif

// An abstract implementation of a tile buffer. This code covers the logic of
// moving and reusing tiles and leaves the validation up to the implementor. To
// avoid the overhead of virtual dispatch, we employ the curiously recurring
// template pattern.
//
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
//
// The contents of the tile buffer will be rendered at the resolution specified
// in mResolution, which can be altered with SetResolution. The resolution
// should always be a factor of the tile length, to avoid tiles covering
// non-integer amounts of pixels.

template<typename Derived, typename Tile>
class TiledLayerBuffer
{
public:
  TiledLayerBuffer()
    : mRetainedWidth(0)
    , mRetainedHeight(0)
    , mResolution(1)
    , mTileSize(gfxPlatform::GetPlatform()->GetTileWidth(), gfxPlatform::GetPlatform()->GetTileHeight())
  {}

  ~TiledLayerBuffer() {}

  // Given a tile origin aligned to a multiple of GetScaledTileSize,
  // return the tile that describes that region.
  // NOTE: To get the valid area of that tile you must intersect
  //       (aTileOrigin.x, aTileOrigin.y,
  //        GetScaledTileSize().width, GetScaledTileSize().height)
  //       and GetValidRegion() to get the area of the tile that is valid.
  Tile GetTile(const nsIntPoint& aTileOrigin) const;

  // Given a tile x, y relative to the top left of the layer, this function
  // will return the tile for
  // (x*GetScaledTileSize().width, y*GetScaledTileSize().height,
  //  GetScaledTileSize().width, GetScaledTileSize().height)
  Tile GetTile(int x, int y) const;

  // This operates the same as GetTile(aTileOrigin), but will also replace the
  // specified tile with the placeholder tile. This does not call ReleaseTile
  // on the removed tile.
  bool RemoveTile(const nsIntPoint& aTileOrigin, Tile& aRemovedTile);

  // This operates the same as GetTile(x, y), but will also replace the
  // specified tile with the placeholder tile. This does not call ReleaseTile
  // on the removed tile.
  bool RemoveTile(int x, int y, Tile& aRemovedTile);

  const gfx::IntSize& GetTileSize() const { return mTileSize; }

  gfx::IntSize GetScaledTileSize() const { return RoundedToInt(gfx::Size(mTileSize) / mResolution); }

  unsigned int GetTileCount() const { return mRetainedTiles.Length(); }

  const nsIntRegion& GetValidRegion() const { return mValidRegion; }
  const nsIntRegion& GetPaintedRegion() const { return mPaintedRegion; }
  void ClearPaintedRegion() { mPaintedRegion.SetEmpty(); }

  void ResetPaintedAndValidState() {
    mPaintedRegion.SetEmpty();
    mValidRegion.SetEmpty();
    mRetainedWidth = 0;
    mRetainedHeight = 0;
    for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
      if (!mRetainedTiles[i].IsPlaceholderTile()) {
        AsDerived().ReleaseTile(mRetainedTiles[i]);
      }
    }
    mRetainedTiles.Clear();
  }

  // Given a position i, this function returns the position inside the current tile.
  int GetTileStart(int i, int aTileLength) const {
    return (i >= 0) ? (i % aTileLength)
                    : ((aTileLength - (-i % aTileLength)) %
                       aTileLength);
  }

  // Rounds the given coordinate down to the nearest tile boundary.
  int RoundDownToTileEdge(int aX, int aTileLength) const { return aX - GetTileStart(aX, aTileLength); }

  // Get and set draw scaling. mResolution affects the resolution at which the
  // contents of the buffer are drawn. mResolution has no effect on the
  // coordinate space of the valid region, but does affect the size of an
  // individual tile's rect in relation to the valid region.
  // Setting the resolution will invalidate the buffer.
  float GetResolution() const { return mResolution; }
  void SetResolution(float aResolution) {
    if (mResolution == aResolution) {
      return;
    }

    Update(nsIntRegion(), nsIntRegion());
    mResolution = aResolution;
  }
  bool IsLowPrecision() const { return mResolution < 1; }

  typedef Tile* Iterator;
  Iterator TilesBegin() { return mRetainedTiles.Elements(); }
  Iterator TilesEnd() { return mRetainedTiles.Elements() + mRetainedTiles.Length(); }

  void Dump(std::stringstream& aStream, const char* aPrefix, bool aDumpHtml);

protected:
  // The implementor should call Update() to change
  // the new valid region. This implementation will call
  // validateTile on each tile that is dirty, which is left
  // to the implementor.
  void Update(const nsIntRegion& aNewValidRegion, const nsIntRegion& aPaintRegion);

  nsIntRegion     mValidRegion;
  nsIntRegion     mPaintedRegion;

  /**
   * mRetainedTiles is a rectangular buffer of mRetainedWidth x mRetainedHeight
   * stored as column major with the same origin as mValidRegion.GetBounds().
   * Any tile that does not intersect mValidRegion is a PlaceholderTile.
   * Only the region intersecting with mValidRegion should be read from a tile,
   * another other region is assumed to be uninitialized. The contents of the
   * tiles is scaled by mResolution.
   */
  nsTArray<Tile>  mRetainedTiles;
  int             mRetainedWidth;  // in tiles
  int             mRetainedHeight; // in tiles
  float           mResolution;
  gfx::IntSize    mTileSize;

private:
  const Derived& AsDerived() const { return *static_cast<const Derived*>(this); }
  Derived& AsDerived() { return *static_cast<Derived*>(this); }
};

class ClientTiledLayerBuffer;
class SurfaceDescriptorTiles;
class ISurfaceAllocator;

// Shadow layers may implement this interface in order to be notified when a
// tiled layer buffer is updated.
class TiledLayerComposer
{
public:
  /**
   * Update the current retained layer with the updated layer data.
   * It is expected that the tiles described by aTiledDescriptor are all in the
   * ReadLock state, so that the locks can be adopted when recreating a
   * ClientTiledLayerBuffer locally. This lock will be retained until the buffer
   * has completed uploading.
   *
   * Returns false if a deserialization error happened, in which case we will
   * have to kill the child process.
   */
  virtual bool UseTiledLayerBuffer(ISurfaceAllocator* aAllocator,
                                   const SurfaceDescriptorTiles& aTiledDescriptor) = 0;

  /**
   * If some part of the buffer is being rendered at a lower precision, this
   * returns that region. If it is not, an empty region will be returned.
   */
  virtual const nsIntRegion& GetValidLowPrecisionRegion() const = 0;

  virtual const nsIntRegion& GetValidRegion() const = 0;

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  /**
   * Store a fence that will signal when the current buffer is no longer being read.
   * Similar to android's GLConsumer::setReleaseFence()
   */
  virtual void SetReleaseFence(const android::sp<android::Fence>& aReleaseFence) = 0;
#endif
};

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

template<typename Derived, typename Tile> Tile
TiledLayerBuffer<Derived, Tile>::GetTile(const nsIntPoint& aTileOrigin) const
{
  // TODO Cache firstTileOriginX/firstTileOriginY
  // Find the tile x/y of the first tile and the target tile relative to the (0, 0)
  // origin, the difference is the tile x/y relative to the start of the tile buffer.
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  int firstTileX = floor_div(mValidRegion.GetBounds().x, scaledTileSize.width);
  int firstTileY = floor_div(mValidRegion.GetBounds().y, scaledTileSize.height);
  return GetTile(floor_div(aTileOrigin.x, scaledTileSize.width) - firstTileX,
                 floor_div(aTileOrigin.y, scaledTileSize.height) - firstTileY);
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
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  int firstTileX = floor_div(mValidRegion.GetBounds().x, scaledTileSize.width);
  int firstTileY = floor_div(mValidRegion.GetBounds().y, scaledTileSize.height);
  return RemoveTile(floor_div(aTileOrigin.x, scaledTileSize.width) - firstTileX,
                    floor_div(aTileOrigin.y, scaledTileSize.height) - firstTileY,
                    aRemovedTile);
}

template<typename Derived, typename Tile> bool
TiledLayerBuffer<Derived, Tile>::RemoveTile(int x, int y, Tile& aRemovedTile)
{
  int index = x * mRetainedHeight + y;
  const Tile& tileToRemove = mRetainedTiles.SafeElementAt(index, AsDerived().GetPlaceholderTile());
  if (!tileToRemove.IsPlaceholderTile()) {
    aRemovedTile = tileToRemove;
    mRetainedTiles[index] = AsDerived().GetPlaceholderTile();
    return true;
  }
  return false;
}

template<typename Derived, typename Tile> void
TiledLayerBuffer<Derived, Tile>::Dump(std::stringstream& aStream,
                                      const char* aPrefix,
                                      bool aDumpHtml)
{
  nsIntRect visibleRect = GetValidRegion().GetBounds();
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  for (int32_t x = visibleRect.x; x < visibleRect.x + visibleRect.width;) {
    int32_t tileStartX = GetTileStart(x, scaledTileSize.width);
    int32_t w = scaledTileSize.width - tileStartX;

    for (int32_t y = visibleRect.y; y < visibleRect.y + visibleRect.height;) {
      int32_t tileStartY = GetTileStart(y, scaledTileSize.height);
      Tile tileTexture =
        GetTile(nsIntPoint(RoundDownToTileEdge(x, scaledTileSize.width),
                           RoundDownToTileEdge(y, scaledTileSize.height)));
      int32_t h = scaledTileSize.height - tileStartY;

      aStream << "\n" << aPrefix << "Tile (x=" <<
        RoundDownToTileEdge(x, scaledTileSize.width) << ", y=" <<
        RoundDownToTileEdge(y, scaledTileSize.height) << "): ";
      if (tileTexture != AsDerived().GetPlaceholderTile()) {
        tileTexture.DumpTexture(aStream);
      } else {
        aStream << "empty tile";
      }
      y += h;
    }
    x += w;
  }
}

template<typename Derived, typename Tile> void
TiledLayerBuffer<Derived, Tile>::Update(const nsIntRegion& newValidRegion,
                                        const nsIntRegion& aPaintRegion)
{
  gfx::IntSize scaledTileSize = GetScaledTileSize();

  nsTArray<Tile>  newRetainedTiles;
  nsTArray<Tile>& oldRetainedTiles = mRetainedTiles;
  const nsIntRect oldBound = mValidRegion.GetBounds();
  const nsIntRect newBound = newValidRegion.GetBounds();
  const nsIntPoint oldBufferOrigin(RoundDownToTileEdge(oldBound.x, scaledTileSize.width),
                                   RoundDownToTileEdge(oldBound.y, scaledTileSize.height));
  const nsIntPoint newBufferOrigin(RoundDownToTileEdge(newBound.x, scaledTileSize.width),
                                   RoundDownToTileEdge(newBound.y, scaledTileSize.height));

  // This is the reason we break the style guide with newValidRegion instead
  // of aNewValidRegion - so that the names match better and code easier to read
  const nsIntRegion& oldValidRegion = mValidRegion;
  const int oldRetainedHeight = mRetainedHeight;

  // Pass 1: Recycle valid content from the old buffer
  // Recycle tiles from the old buffer that contain valid regions.
  // Insert placeholders tiles if we have no valid area for that tile
  // which we will allocate in pass 2.
  // TODO: Add a tile pool to reduce new allocation
  int tileX = 0;
  int tileY = 0;
  int tilesMissing = 0;
  // Iterate over the new drawing bounds in steps of tiles.
  for (int32_t x = newBound.x; x < newBound.XMost(); tileX++) {
    // Compute tileRect(x,y,width,height) in layer space coordinate
    // giving us the rect of the tile that hits the newBounds.
    int width = scaledTileSize.width - GetTileStart(x, scaledTileSize.width);
    if (x + width > newBound.XMost()) {
      width = newBound.x + newBound.width - x;
    }

    tileY = 0;
    for (int32_t y = newBound.y; y < newBound.YMost(); tileY++) {
      int height = scaledTileSize.height - GetTileStart(y, scaledTileSize.height);
      if (y + height > newBound.y + newBound.height) {
        height = newBound.y + newBound.height - y;
      }

      const nsIntRect tileRect(x,y,width,height);
      if (oldValidRegion.Intersects(tileRect) && newValidRegion.Intersects(tileRect)) {
        // This old tiles contains some valid area so move it to the new tile
        // buffer. Replace the tile in the old buffer with a placeholder
        // to leave the old buffer index unaffected.
        int tileX = floor_div(x - oldBufferOrigin.x, scaledTileSize.width);
        int tileY = floor_div(y - oldBufferOrigin.y, scaledTileSize.height);
        int index = tileX * oldRetainedHeight + tileY;

        // The tile may have been removed, skip over it in this case.
        if (oldRetainedTiles.
                          SafeElementAt(index, AsDerived().GetPlaceholderTile()).IsPlaceholderTile()) {
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

        if (aPaintRegion.Intersects(tileRect)) {
          tilesMissing++;
        }
      }

      y += height;
    }

    x += width;
  }

  // Keep track of the number of horizontal/vertical tiles
  // in the buffer so that we can easily look up a tile.
  mRetainedWidth = tileX;
  mRetainedHeight = tileY;

  // Pass 1.5: Release excess tiles in oldRetainedTiles
  // Tiles in oldRetainedTiles that aren't in newRetainedTiles will be recycled
  // before creating new ones, but there could still be excess unnecessary
  // tiles. As tiles may not have a fixed memory cost (for example, due to
  // double-buffering), we should release these excess tiles first.
  int oldTileCount = 0;
  for (size_t i = 0; i < oldRetainedTiles.Length(); i++) {
    Tile oldTile = oldRetainedTiles[i];
    if (oldTile.IsPlaceholderTile()) {
      continue;
    }

    if (oldTileCount >= tilesMissing) {
      oldRetainedTiles[i] = AsDerived().GetPlaceholderTile();
      AsDerived().ReleaseTile(oldTile);
    } else {
      oldTileCount ++;
    }
  }

  if (!newValidRegion.Contains(aPaintRegion)) {
    gfxCriticalError() << "Painting outside visible:"
		       << " paint " << aPaintRegion.ToString().get()
                       << " old valid " << oldValidRegion.ToString().get()
                       << " new valid " << newValidRegion.ToString().get();
  }
#ifdef DEBUG
  nsIntRegion oldAndPainted(oldValidRegion);
  oldAndPainted.Or(oldAndPainted, aPaintRegion);
  if (!oldAndPainted.Contains(newValidRegion)) {
    gfxCriticalError() << "Not fully painted:"
		       << " paint " << aPaintRegion.ToString().get()
                       << " old valid " << oldValidRegion.ToString().get()
                       << " old painted " << oldAndPainted.ToString().get()
                       << " new valid " << newValidRegion.ToString().get();
  }
#endif

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
    int tileStartX = RoundDownToTileEdge(x, scaledTileSize.width);
    int width = scaledTileSize.width - GetTileStart(x, scaledTileSize.width);
    if (x + width > newBound.XMost())
      width = newBound.XMost() - x;

    tileY = 0;
    for (int y = newBound.y; y < newBound.y + newBound.height; tileY++) {
      int tileStartY = RoundDownToTileEdge(y, scaledTileSize.height);
      int height = scaledTileSize.height - GetTileStart(y, scaledTileSize.height);
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
        int currTileX = floor_div(x - newBufferOrigin.x, scaledTileSize.width);
        int currTileY = floor_div(y - newBufferOrigin.y, scaledTileSize.height);
        int index = currTileX * mRetainedHeight + currTileY;
        // If allocating a tile failed we can run into this assertion.
        // Rendering is going to be glitchy but we don't want to crash.
        NS_ASSERTION(!newValidRegion.Intersects(tileRect) ||
                     !newRetainedTiles.
                                    SafeElementAt(index, AsDerived().GetPlaceholderTile()).IsPlaceholderTile(),
                     "Unexpected placeholder tile");

#endif
        y += height;
        continue;
      }

      int tileX = floor_div(x - newBufferOrigin.x, scaledTileSize.width);
      int tileY = floor_div(y - newBufferOrigin.y, scaledTileSize.height);
      int index = tileX * mRetainedHeight + tileY;
      MOZ_ASSERT(index >= 0 &&
                 static_cast<unsigned>(index) < newRetainedTiles.Length(),
                 "index out of range");

      Tile newTile = newRetainedTiles[index];

      // Try to reuse a tile from the old retained tiles that had no partially
      // valid content.
      while (newTile.IsPlaceholderTile() && oldRetainedTiles.Length() > 0) {
        AsDerived().SwapTiles(newTile, oldRetainedTiles[oldRetainedTiles.Length()-1]);
        oldRetainedTiles.RemoveElementAt(oldRetainedTiles.Length()-1);
        if (!newTile.IsPlaceholderTile()) {
          oldTileCount--;
        }
      }

      // We've done our best effort to recycle a tile but it can be null
      // in which case it's up to the derived class's ValidateTile()
      // implementation to allocate a new tile before drawing
      nsIntPoint tileOrigin(tileStartX, tileStartY);
      newTile = AsDerived().ValidateTile(newTile, nsIntPoint(tileStartX, tileStartY),
                                         tileDrawRegion);
      NS_ASSERTION(!newTile.IsPlaceholderTile(), "Unexpected placeholder tile - failed to allocate?");
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
      printf_stderr("Store Validate tile %i, %i -> %i\n", tileStartX, tileStartY, index);
#endif
      newRetainedTiles[index] = newTile;

      y += height;
    }

    x += width;
  }

  AsDerived().PostValidate(aPaintRegion);
  for (unsigned int i = 0; i < newRetainedTiles.Length(); ++i) {
    AsDerived().UnlockTile(newRetainedTiles[i]);
  }

  // At this point, oldTileCount should be zero
  MOZ_ASSERT(oldTileCount == 0, "Failed to release old tiles");

  mRetainedTiles = newRetainedTiles;
  mValidRegion = newValidRegion;
  mPaintedRegion.Or(mPaintedRegion, aPaintRegion);
}

} // layers
} // mozilla

#endif // GFX_TILEDLAYERBUFFER_H
