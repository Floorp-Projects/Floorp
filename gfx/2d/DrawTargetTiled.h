/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWTARGETTILED_H_
#define MOZILLA_GFX_DRAWTARGETTILED_H_

#include "2D.h"
#include "Filters.h"

#include <vector>

namespace mozilla {
namespace gfx {

class DrawTargetTiled : public DrawTarget
{
public:
  DrawTargetTiled();

  bool Init(const TileSet& mTiles);

  virtual bool IsTiledDrawTarget() const { return true; }

  virtual DrawTargetType GetType() const MOZ_OVERRIDE { return mTiles[0].mDrawTarget->GetType(); }
  virtual BackendType GetBackendType() const { return mTiles[0].mDrawTarget->GetBackendType(); }
  virtual TemporaryRef<SourceSurface> Snapshot();
  virtual IntSize GetSize() { return IntSize(mRect.XMost(), mRect.YMost()); }

  virtual void Flush();
  virtual void DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions,
                           const DrawOptions &aOptions);
  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions());
  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator) { /* Not implemented */ MOZ_CRASH(); }

  virtual void ClearRect(const Rect &aRect);
  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions());

  virtual void CopySurface(SourceSurface *aSurface,
                           const IntRect &aSourceRect,
                           const IntPoint &aDestination);

  virtual void FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions = DrawOptions());
  virtual void StrokeRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions());
  virtual void StrokeLine(const Point &aStart,
                          const Point &aEnd,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions());
  virtual void Stroke(const Path *aPath,
                      const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions = StrokeOptions(),
                      const DrawOptions &aOptions = DrawOptions());
  virtual void Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions = DrawOptions());
  virtual void FillGlyphs(ScaledFont *aFont,
                          const GlyphBuffer &aBuffer,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions = DrawOptions(),
                          const GlyphRenderingOptions *aRenderingOptions = nullptr);
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions());
  virtual void PushClip(const Path *aPath);
  virtual void PushClipRect(const Rect &aRect);
  virtual void PopClip();

  virtual void SetTransform(const Matrix &aTransform);

  virtual TemporaryRef<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                                  const IntSize &aSize,
                                                                  int32_t aStride,
                                                                  SurfaceFormat aFormat) const
  {
    return mTiles[0].mDrawTarget->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
  }
  virtual TemporaryRef<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const
  {
    return mTiles[0].mDrawTarget->OptimizeSourceSurface(aSurface);
  }

  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
  {
    return mTiles[0].mDrawTarget->CreateSourceSurfaceFromNativeSurface(aSurface);
  }

  virtual TemporaryRef<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
  {
    return mTiles[0].mDrawTarget->CreateSimilarDrawTarget(aSize, aFormat);
  }

  virtual TemporaryRef<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const
  {
    return mTiles[0].mDrawTarget->CreatePathBuilder(aFillRule);
  }

  virtual TemporaryRef<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const
  {
    return mTiles[0].mDrawTarget->CreateGradientStops(aStops, aNumStops, aExtendMode);
  }
  virtual TemporaryRef<FilterNode> CreateFilter(FilterType aType)
  {
    return mTiles[0].mDrawTarget->CreateFilter(aType);
  }

private:
  std::vector<Tile> mTiles;
  IntRect mRect;
};

}
}

#endif
