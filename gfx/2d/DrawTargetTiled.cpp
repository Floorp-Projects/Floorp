/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define _USE_MATH_DEFINES
#include <cmath>

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
    mTiles.push_back(TileInternal(aTiles.mTiles[i]));
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
    mTiles[i].mDrawTarget->SetTransform(Matrix::Translation(mTiles[i].mTileOrigin.x,
                                                            mTiles[i].mTileOrigin.y));
  }
  mFormat = mTiles[0].mDrawTarget->GetFormat();
  return true;
}

TemporaryRef<SourceSurface>
DrawTargetTiled::Snapshot()
{
  return new SnapshotTiled(mTiles, mRect);
}

// Skip the mClippedOut check since this is only used for Flush() which
// should happen even if we're clipped.
#define TILED_COMMAND(command) \
  void \
  DrawTargetTiled::command() \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
      mTiles[i].mDrawTarget->command(); \
    } \
  }
#define TILED_COMMAND1(command, type1) \
  void \
  DrawTargetTiled::command(type1 arg1) \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
      if (!mTiles[i].mClippedOut) \
        mTiles[i].mDrawTarget->command(arg1); \
    } \
  }
#define TILED_COMMAND3(command, type1, type2, type3) \
  void \
  DrawTargetTiled::command(type1 arg1, type2 arg2, type3 arg3) \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
      if (!mTiles[i].mClippedOut) \
        mTiles[i].mDrawTarget->command(arg1, arg2, arg3); \
    } \
  }
#define TILED_COMMAND4(command, type1, type2, type3, type4) \
  void \
  DrawTargetTiled::command(type1 arg1, type2 arg2, type3 arg3, type4 arg4) \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
      if (!mTiles[i].mClippedOut) \
        mTiles[i].mDrawTarget->command(arg1, arg2, arg3, arg4); \
    } \
  }
#define TILED_COMMAND5(command, type1, type2, type3, type4, type5) \
  void \
  DrawTargetTiled::command(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5) \
  { \
    for (size_t i = 0; i < mTiles.size(); i++) { \
      if (!mTiles[i].mClippedOut) \
        mTiles[i].mDrawTarget->command(arg1, arg2, arg3, arg4, arg5); \
    } \
  }

TILED_COMMAND(Flush)
TILED_COMMAND4(DrawFilter, FilterNode*, const Rect&, const Point&, const DrawOptions&)
TILED_COMMAND1(ClearRect, const Rect&)
TILED_COMMAND4(MaskSurface, const Pattern&, SourceSurface*, Point, const DrawOptions&)
TILED_COMMAND4(StrokeRect, const Rect&, const Pattern&, const StrokeOptions&, const DrawOptions&)
TILED_COMMAND5(StrokeLine, const Point&, const Point&, const Pattern&, const StrokeOptions&, const DrawOptions&)
TILED_COMMAND5(FillGlyphs, ScaledFont*, const GlyphBuffer&, const Pattern&, const DrawOptions&, const GlyphRenderingOptions*)
TILED_COMMAND3(Mask, const Pattern&, const Pattern&, const DrawOptions&)

void
DrawTargetTiled::PushClip(const Path* aPath)
{
  mClippedOutTilesStack.push_back(std::vector<uint32_t>());
  std::vector<uint32_t>& clippedTiles = mClippedOutTilesStack.back();

  Rect deviceRect = aPath->GetBounds(mTransform);

  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut) {
      if (deviceRect.Intersects(Rect(mTiles[i].mTileOrigin.x,
                                   mTiles[i].mTileOrigin.y,
                                   mTiles[i].mDrawTarget->GetSize().width,
                                   mTiles[i].mDrawTarget->GetSize().height))) {
        mTiles[i].mDrawTarget->PushClip(aPath);
      } else {
        mTiles[i].mClippedOut = true;
        clippedTiles.push_back(i);
      }
    }
  }
}

void
DrawTargetTiled::PushClipRect(const Rect& aRect)
{
  mClippedOutTilesStack.push_back(std::vector<uint32_t>());
  std::vector<uint32_t>& clippedTiles = mClippedOutTilesStack.back();

  Rect deviceRect = mTransform.TransformBounds(aRect);

  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut) {
      if (deviceRect.Intersects(Rect(mTiles[i].mTileOrigin.x,
                                   mTiles[i].mTileOrigin.y,
                                   mTiles[i].mDrawTarget->GetSize().width,
                                   mTiles[i].mDrawTarget->GetSize().height))) {
        mTiles[i].mDrawTarget->PushClipRect(aRect);
      } else {
        mTiles[i].mClippedOut = true;
        clippedTiles.push_back(i);
      }
    }
  }
}

void
DrawTargetTiled::PopClip()
{
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut) {
      mTiles[i].mDrawTarget->PopClip();
    }
  }

  std::vector<uint32_t>& clippedTiles = mClippedOutTilesStack.back();
  for (size_t i = 0; i < clippedTiles.size(); i++) {
    mTiles[clippedTiles[i]].mClippedOut = false;
  }

  mClippedOutTilesStack.pop_back();
}

void
DrawTargetTiled::CopySurface(SourceSurface *aSurface,
                             const IntRect &aSourceRect,
                             const IntPoint &aDestination)
{
  for (size_t i = 0; i < mTiles.size(); i++) {
    IntPoint tileOrigin = mTiles[i].mTileOrigin;
    IntSize tileSize = mTiles[i].mDrawTarget->GetSize();
    if (!IntRect(aDestination, aSourceRect.Size()).Intersects(IntRect(tileOrigin, tileSize))) {
      continue;
    }
    // CopySurface ignores the transform, account for that here.
    mTiles[i].mDrawTarget->CopySurface(aSurface, aSourceRect, aDestination - tileOrigin);
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

void
DrawTargetTiled::DrawSurface(SourceSurface* aSurface, const Rect& aDest, const Rect& aSource, const DrawSurfaceOptions& aSurfaceOptions, const DrawOptions& aDrawOptions)
{
  Rect deviceRect = mTransform.TransformBounds(aDest);
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut &&
        deviceRect.Intersects(Rect(mTiles[i].mTileOrigin.x,
                                   mTiles[i].mTileOrigin.y,
                                   mTiles[i].mDrawTarget->GetSize().width,
                                   mTiles[i].mDrawTarget->GetSize().height))) {
      mTiles[i].mDrawTarget->DrawSurface(aSurface, aDest, aSource, aSurfaceOptions, aDrawOptions);
    }
  }
}

void
DrawTargetTiled::FillRect(const Rect& aRect, const Pattern& aPattern, const DrawOptions& aDrawOptions)
{
  Rect deviceRect = mTransform.TransformBounds(aRect);
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut &&
        deviceRect.Intersects(Rect(mTiles[i].mTileOrigin.x,
                                   mTiles[i].mTileOrigin.y,
                                   mTiles[i].mDrawTarget->GetSize().width,
                                   mTiles[i].mDrawTarget->GetSize().height))) {
      mTiles[i].mDrawTarget->FillRect(aRect, aPattern, aDrawOptions);
    }
  }
}

// The logic for this comes from _cairo_stroke_style_max_distance_from_path
static Rect
PathExtentsToMaxStrokeExtents(const StrokeOptions &aStrokeOptions,
                              const Rect &aRect,
                              const Matrix &aTransform)
{
  double styleExpansionFactor = 0.5f;

  if (aStrokeOptions.mLineCap == CapStyle::SQUARE) {
    styleExpansionFactor = M_SQRT1_2;
  }

  if (aStrokeOptions.mLineJoin == JoinStyle::MITER &&
      styleExpansionFactor < M_SQRT2 * aStrokeOptions.mMiterLimit) {
    styleExpansionFactor = M_SQRT2 * aStrokeOptions.mMiterLimit;
  }

  styleExpansionFactor *= aStrokeOptions.mLineWidth;

  double dx = styleExpansionFactor * hypot(aTransform._11, aTransform._21);
  double dy = styleExpansionFactor * hypot(aTransform._22, aTransform._12);

  Rect result = aRect;
  result.Inflate(dx, dy);
  return result;
}

void
DrawTargetTiled::Stroke(const Path* aPath, const Pattern& aPattern, const StrokeOptions& aStrokeOptions, const DrawOptions& aDrawOptions)
{
  // Approximate the stroke extents, since Path::GetStrokeExtents can be slow
  Rect deviceRect = PathExtentsToMaxStrokeExtents(aStrokeOptions,
                                                 aPath->GetBounds(mTransform),
                                                 mTransform);
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut &&
        deviceRect.Intersects(Rect(mTiles[i].mTileOrigin.x,
                                   mTiles[i].mTileOrigin.y,
                                   mTiles[i].mDrawTarget->GetSize().width,
                                   mTiles[i].mDrawTarget->GetSize().height))) {
      mTiles[i].mDrawTarget->Stroke(aPath, aPattern, aStrokeOptions, aDrawOptions);
    }
  }
}

void
DrawTargetTiled::Fill(const Path* aPath, const Pattern& aPattern, const DrawOptions& aDrawOptions)
{
  Rect deviceRect = aPath->GetBounds(mTransform);
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut &&
        deviceRect.Intersects(Rect(mTiles[i].mTileOrigin.x,
                                   mTiles[i].mTileOrigin.y,
                                   mTiles[i].mDrawTarget->GetSize().width,
                                   mTiles[i].mDrawTarget->GetSize().height))) {
      mTiles[i].mDrawTarget->Fill(aPath, aPattern, aDrawOptions);
    }
  }
}

}
}
