/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetTiled.h"
#include "Logging.h"

using namespace std;

namespace mozilla {
namespace gfx {

DrawTargetTiled::DrawTargetTiled()
{
}

bool
DrawTargetTiled::Init(const TileSet& aTiles)
{
  if (!aTiles.mTileCount) {
    return false;
  }

  mTiles.reserve(aTiles.mTileCount);
  for (size_t i = 0; i < aTiles.mTileCount; ++i) {
    mTiles.push_back(aTiles.mTiles[i]);
    if (!aTiles.mTiles[i].mDrawTarget) {
      return false;
    }
    if (mTiles[0].mDrawTarget->GetFormat() != mTiles.back().mDrawTarget->GetFormat() ||
        mTiles[0].mDrawTarget->GetBackendType() != mTiles.back().mDrawTarget->GetBackendType()) {
      return false;
    }
    uint32_t newXMost = max(mRect.XMost(),
                            mTiles[i].mTileOrigin.x + mTiles[i].mDrawTarget->GetSize().width);
    uint32_t newYMost = max(mRect.YMost(),
                            mTiles[i].mTileOrigin.y + mTiles[i].mDrawTarget->GetSize().height);
    mRect.x = min(mRect.x, mTiles[i].mTileOrigin.x);
    mRect.y = min(mRect.y, mTiles[i].mTileOrigin.y);
    mRect.width = newXMost - mRect.x;
    mRect.height = newYMost - mRect.y;
  }
  mFormat = mTiles[0].mDrawTarget->GetFormat();
  return true;
}

class SnapshotTiled : public SourceSurface
{
public:
  SnapshotTiled(const vector<Tile>& aTiles, const IntRect& aRect)
    : mRect(aRect)
  {
    for (size_t i = 0; i < aTiles.size(); i++) {
      mSnapshots.push_back(aTiles[i].mDrawTarget->Snapshot());
      mOrigins.push_back(aTiles[i].mTileOrigin);
    }
  }

  virtual SurfaceType GetType() const { return SurfaceType::TILED; }
  virtual IntSize GetSize() const { return IntSize(mRect.XMost(), mRect.YMost()); }
  virtual SurfaceFormat GetFormat() const { return mSnapshots[0]->GetFormat(); }

  virtual TemporaryRef<DataSourceSurface> GetDataSurface()
  {
    RefPtr<DataSourceSurface> surf = Factory::CreateDataSourceSurface(GetSize(), GetFormat());
    if (MOZ2D_WARN_IF(!surf)) {
      return nullptr;
    }

    DataSourceSurface::MappedSurface mappedSurf;
    surf->Map(DataSourceSurface::MapType::WRITE, &mappedSurf);

    {
      RefPtr<DrawTarget> dt =
        Factory::CreateDrawTargetForData(BackendType::CAIRO, mappedSurf.mData,
        GetSize(), mappedSurf.mStride, GetFormat());

      for (size_t i = 0; i < mSnapshots.size(); i++) {
        RefPtr<DataSourceSurface> dataSurf = mSnapshots[i]->GetDataSurface();
        dt->CopySurface(dataSurf, IntRect(IntPoint(0, 0), mSnapshots[i]->GetSize()), mOrigins[i]);
      }
    }
    surf->Unmap();

    return surf.forget();
  }
private:
  vector<RefPtr<SourceSurface>> mSnapshots;
  vector<IntPoint> mOrigins;
  IntRect mRect;
};

TemporaryRef<SourceSurface>
DrawTargetTiled::Snapshot()
{
  return new SnapshotTiled(mTiles, mRect);
}

#define TILED_COMMAND(command) \
  void \
  DrawTargetTiled::command() \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
    \
    \
    mTiles[i].mDrawTarget->command(); \
    } \
  }
#define TILED_COMMAND1(command, type1) \
  void \
  DrawTargetTiled::command(type1 arg1) \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
    \
    \
    mTiles[i].mDrawTarget->command(arg1); \
    } \
  }
#define TILED_COMMAND3(command, type1, type2, type3) \
  void \
  DrawTargetTiled::command(type1 arg1, type2 arg2, type3 arg3) \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
    \
    \
    mTiles[i].mDrawTarget->command(arg1, arg2, arg3); \
    } \
  }
#define TILED_COMMAND4(command, type1, type2, type3, type4) \
  void \
  DrawTargetTiled::command(type1 arg1, type2 arg2, type3 arg3, type4 arg4) \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
    \
    \
    mTiles[i].mDrawTarget->command(arg1, arg2, arg3, arg4); \
    } \
  }
#define TILED_COMMAND5(command, type1, type2, type3, type4, type5) \
  void \
  DrawTargetTiled::command(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5) \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
    \
    \
    mTiles[i].mDrawTarget->command(arg1, arg2, arg3, arg4, arg5); \
    } \
  }

TILED_COMMAND(Flush)
TILED_COMMAND5(DrawSurface, SourceSurface*, const Rect&,
                            const Rect&, const DrawSurfaceOptions&,
                            const DrawOptions&)
TILED_COMMAND4(DrawFilter, FilterNode*, const Rect&, const Point&, const DrawOptions&)
TILED_COMMAND1(ClearRect, const Rect&)
TILED_COMMAND4(MaskSurface, const Pattern&, SourceSurface*, Point, const DrawOptions&)
TILED_COMMAND3(FillRect, const Rect&, const Pattern&, const DrawOptions&)
TILED_COMMAND4(StrokeRect, const Rect&, const Pattern&, const StrokeOptions&, const DrawOptions&)
TILED_COMMAND5(StrokeLine, const Point&, const Point&, const Pattern&, const StrokeOptions&, const DrawOptions&)
TILED_COMMAND4(Stroke, const Path*, const Pattern&, const StrokeOptions&, const DrawOptions&)
TILED_COMMAND3(Fill, const Path*, const Pattern&, const DrawOptions&)
TILED_COMMAND5(FillGlyphs, ScaledFont*, const GlyphBuffer&, const Pattern&, const DrawOptions&, const GlyphRenderingOptions*)
TILED_COMMAND3(Mask, const Pattern&, const Pattern&, const DrawOptions&)
TILED_COMMAND1(PushClip, const Path*)
TILED_COMMAND1(PushClipRect, const Rect&)
TILED_COMMAND(PopClip)

void
DrawTargetTiled::CopySurface(SourceSurface *aSurface,
                             const IntRect &aSourceRect,
                             const IntPoint &aDestination)
{
  // CopySurface ignores the transform, account for that here.
  for (size_t i = 0; i < mTiles.size(); i++) {
    IntRect src = aSourceRect;
    src.x += mTiles[i].mTileOrigin.x;
    src.width -= mTiles[i].mTileOrigin.x;
    src.y = mTiles[i].mTileOrigin.y;
    src.height -= mTiles[i].mTileOrigin.y;

    if (src.width <= 0 || src.height <= 0) {
      continue;
    }

    mTiles[i].mDrawTarget->CopySurface(aSurface, src, aDestination);
  }
}

void
DrawTargetTiled::SetTransform(const Matrix& aTransform)
{
  for (size_t i = 0; i < mTiles.size(); i++) {
    Matrix mat = aTransform;
    mat.PostTranslate(Float(-mTiles[i].mTileOrigin.x), Float(-mTiles[i].mTileOrigin.y));
    mTiles[i].mDrawTarget->SetTransform(mat);
  }
  DrawTarget::SetTransform(aTransform);
}

}
}
