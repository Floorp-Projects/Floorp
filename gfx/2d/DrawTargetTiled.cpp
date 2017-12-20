/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetTiled.h"
#include "Logging.h"
#include "PathHelpers.h"

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
    mRect.MoveTo(min(mRect.X(), mTiles[i].mTileOrigin.x),
                 min(mRect.Y(), mTiles[i].mTileOrigin.y));
    mRect.SetRightEdge(newXMost);
    mRect.SetBottomEdge(newYMost);
    mTiles[i].mDrawTarget->SetTransform(Matrix::Translation(-mTiles[i].mTileOrigin.x,
                                                            -mTiles[i].mTileOrigin.y));
  }
  mFormat = mTiles[0].mDrawTarget->GetFormat();
  SetPermitSubpixelAA(IsOpaque(mFormat));
  return true;
}

already_AddRefed<SourceSurface>
DrawTargetTiled::Snapshot()
{
  return MakeAndAddRef<SnapshotTiled>(mTiles, mRect);
}

void
DrawTargetTiled::DetachAllSnapshots()
{}

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
TILED_COMMAND4(FillGlyphs, ScaledFont*, const GlyphBuffer&, const Pattern&, const DrawOptions&)
TILED_COMMAND3(Mask, const Pattern&, const Pattern&, const DrawOptions&)

void
DrawTargetTiled::PushClip(const Path* aPath)
{
  if (!mClippedOutTilesStack.append(std::vector<bool>(mTiles.size()))) {
    MOZ_CRASH("out of memory");
  }
  std::vector<bool>& clippedTiles = mClippedOutTilesStack.back();

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
        clippedTiles[i] = true;
      }
    }
  }
}

void
DrawTargetTiled::PushClipRect(const Rect& aRect)
{
  if (!mClippedOutTilesStack.append(std::vector<bool>(mTiles.size()))) {
    MOZ_CRASH("out of memory");
  }
  std::vector<bool>& clippedTiles = mClippedOutTilesStack.back();

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
        clippedTiles[i] = true;
      }
    }
  }
}

void
DrawTargetTiled::PopClip()
{
  std::vector<bool>& clippedTiles = mClippedOutTilesStack.back();
  MOZ_ASSERT(clippedTiles.size() == mTiles.size());
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut) {
      mTiles[i].mDrawTarget->PopClip();
    } else if (clippedTiles[i]) {
      mTiles[i].mClippedOut = false;
    }
  }

  mClippedOutTilesStack.popBack();
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
DrawTargetTiled::SetPermitSubpixelAA(bool aPermitSubpixelAA)
{
  DrawTarget::SetPermitSubpixelAA(aPermitSubpixelAA);
  for (size_t i = 0; i < mTiles.size(); i++) {
    mTiles[i].mDrawTarget->SetPermitSubpixelAA(aPermitSubpixelAA);
  }
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

void
DrawTargetTiled::Stroke(const Path* aPath, const Pattern& aPattern, const StrokeOptions& aStrokeOptions, const DrawOptions& aDrawOptions)
{
  // Approximate the stroke extents, since Path::GetStrokeExtents can be slow
  Rect deviceRect = aPath->GetBounds(mTransform);
  deviceRect.Inflate(MaxStrokeExtents(aStrokeOptions, mTransform));
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
DrawTargetTiled::StrokeRect(const Rect& aRect, const Pattern& aPattern, const StrokeOptions &aStrokeOptions, const DrawOptions& aDrawOptions)
{
  Rect deviceRect = mTransform.TransformBounds(aRect);
  Margin strokeMargin = MaxStrokeExtents(aStrokeOptions, mTransform);
  Rect outerRect = deviceRect;
  outerRect.Inflate(strokeMargin);
  Rect innerRect;
  if (mTransform.IsRectilinear()) {
    // If rects are mapped to rects, we can compute the inner rect
    // of the stroked rect.
    innerRect = deviceRect;
    innerRect.Deflate(strokeMargin);
  }
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (mTiles[i].mClippedOut) {
      continue;
    }
    Rect tileRect(mTiles[i].mTileOrigin.x,
                  mTiles[i].mTileOrigin.y,
                  mTiles[i].mDrawTarget->GetSize().width,
                  mTiles[i].mDrawTarget->GetSize().height);
    if (outerRect.Intersects(tileRect) && !innerRect.Contains(tileRect)) {
      mTiles[i].mDrawTarget->StrokeRect(aRect, aPattern, aStrokeOptions, aDrawOptions);
    }
  }
}

void
DrawTargetTiled::StrokeLine(const Point& aStart, const Point& aEnd, const Pattern& aPattern, const StrokeOptions &aStrokeOptions, const DrawOptions& aDrawOptions)
{
  Rect lineBounds = Rect(aStart, Size()).UnionEdges(Rect(aEnd, Size()));
  Rect deviceRect = mTransform.TransformBounds(lineBounds);
  deviceRect.Inflate(MaxStrokeExtents(aStrokeOptions, mTransform));
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut &&
        deviceRect.Intersects(Rect(mTiles[i].mTileOrigin.x,
                                   mTiles[i].mTileOrigin.y,
                                   mTiles[i].mDrawTarget->GetSize().width,
                                   mTiles[i].mDrawTarget->GetSize().height))) {
      mTiles[i].mDrawTarget->StrokeLine(aStart, aEnd, aPattern, aStrokeOptions, aDrawOptions);
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

void
DrawTargetTiled::PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                           const Matrix& aMaskTransform, const IntRect& aBounds,
                           bool aCopyBackground)
{
  // XXX - not sure this is what we want or whether we want to continue drawing to a larger
  // intermediate surface, that would require tweaking the code in here a little though.
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut) {
      IntRect bounds = aBounds;
      bounds.MoveBy(-mTiles[i].mTileOrigin);
      mTiles[i].mDrawTarget->PushLayer(aOpaque, aOpacity, aMask, aMaskTransform, bounds, aCopyBackground);
    }
  }

  PushedLayer layer(GetPermitSubpixelAA());
  mPushedLayers.push_back(layer);
  SetPermitSubpixelAA(aOpaque);
}

void
DrawTargetTiled::PopLayer()
{
  // XXX - not sure this is what we want or whether we want to continue drawing to a larger
  // intermediate surface, that would require tweaking the code in here a little though.
  for (size_t i = 0; i < mTiles.size(); i++) {
    if (!mTiles[i].mClippedOut) {
      mTiles[i].mDrawTarget->PopLayer();
    }
  }

  MOZ_ASSERT(mPushedLayers.size());
  const PushedLayer& layer = mPushedLayers.back();
  SetPermitSubpixelAA(layer.mOldPermitSubpixelAA);
}

} // namespace gfx
} // namespace mozilla
