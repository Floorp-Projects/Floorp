/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWTARGETTILED_H_
#define MOZILLA_GFX_DRAWTARGETTILED_H_

#include "2D.h"

#include "mozilla/Vector.h"

#include "Filters.h"
#include "Logging.h"

#include <vector>

namespace mozilla {
namespace gfx {

struct TileInternal : public Tile {
  TileInternal()
    : mClippedOut(false)
  {}

  explicit TileInternal(const Tile& aOther)
    : Tile(aOther)
    , mClippedOut(false)
  {}

  bool mClippedOut;
};


class DrawTargetTiled : public DrawTarget
{
public:
  DrawTargetTiled();

  bool Init(const TileSet& mTiles);

  virtual bool IsTiledDrawTarget() const override { return true; }

  virtual bool IsCaptureDT() const override { return mTiles[0].mDrawTarget->IsCaptureDT(); }
  virtual DrawTargetType GetType() const override { return mTiles[0].mDrawTarget->GetType(); }
  virtual BackendType GetBackendType() const override { return mTiles[0].mDrawTarget->GetBackendType(); }
  virtual already_AddRefed<SourceSurface> Snapshot() override;
  virtual void DetachAllSnapshots() override;
  virtual IntSize GetSize() const override {
    MOZ_ASSERT(mRect.Width() > 0 && mRect.Height() > 0);
    return IntSize(mRect.XMost(), mRect.YMost());
  }
  virtual IntRect GetRect() const override {
    return mRect;
  }

  virtual void Flush() override;
  virtual void DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions,
                           const DrawOptions &aOptions) override;
  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions()) override;
  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator) override { /* Not implemented */ MOZ_CRASH("GFX: DrawSurfaceWithShadow"); }

  virtual void ClearRect(const Rect &aRect) override;
  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions()) override;

  virtual void CopySurface(SourceSurface *aSurface,
                           const IntRect &aSourceRect,
                           const IntPoint &aDestination) override;

  virtual void FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions = DrawOptions()) override;
  virtual void StrokeRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions()) override;
  virtual void StrokeLine(const Point &aStart,
                          const Point &aEnd,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions()) override;
  virtual void Stroke(const Path *aPath,
                      const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions = StrokeOptions(),
                      const DrawOptions &aOptions = DrawOptions()) override;
  virtual void Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions = DrawOptions()) override;
  virtual void FillGlyphs(ScaledFont *aFont,
                          const GlyphBuffer &aBuffer,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions = DrawOptions()) override;
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions()) override;
  virtual void PushClip(const Path *aPath) override;
  virtual void PushClipRect(const Rect &aRect) override;
  virtual void PopClip() override;
  virtual void PushLayer(bool aOpaque, Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds = IntRect(),
                         bool aCopyBackground = false) override;
  virtual void PushLayerWithBlend(bool aOpaque, Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds = IntRect(),
                         bool aCopyBackground = false,
                         CompositionOp = CompositionOp::OP_OVER) override;
  virtual void PopLayer() override;


  virtual void SetTransform(const Matrix &aTransform) override;

  virtual void SetPermitSubpixelAA(bool aPermitSubpixelAA) override;

  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                                  const IntSize &aSize,
                                                                  int32_t aStride,
                                                                  SurfaceFormat aFormat) const override
  {
    return mTiles[0].mDrawTarget->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
  }
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const override
  {
    return mTiles[0].mDrawTarget->OptimizeSourceSurface(aSurface);
  }

  virtual already_AddRefed<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override
  {
    return mTiles[0].mDrawTarget->CreateSourceSurfaceFromNativeSurface(aSurface);
  }

  virtual already_AddRefed<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override
  {
    return mTiles[0].mDrawTarget->CreateSimilarDrawTarget(aSize, aFormat);
  }

  virtual already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const override
  {
    return mTiles[0].mDrawTarget->CreatePathBuilder(aFillRule);
  }

  virtual already_AddRefed<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const override
  {
    return mTiles[0].mDrawTarget->CreateGradientStops(aStops, aNumStops, aExtendMode);
  }
  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override
  {
    return mTiles[0].mDrawTarget->CreateFilter(aType);
  }

private:
  std::vector<TileInternal> mTiles;

  // mClippedOutTilesStack[clipIndex][tileIndex] is true if the tile at
  // tileIndex has become completely clipped out at the clip stack depth
  // clipIndex.
  Vector<std::vector<bool>,8> mClippedOutTilesStack;

  IntRect mRect;

  struct PushedLayer
  {
    explicit PushedLayer(bool aOldPermitSubpixelAA)
      : mOldPermitSubpixelAA(aOldPermitSubpixelAA)
    {}
    bool mOldPermitSubpixelAA;
  };
  std::vector<PushedLayer> mPushedLayers;
};

class SnapshotTiled : public SourceSurface
{
public:
  SnapshotTiled(const std::vector<TileInternal>& aTiles, const IntRect& aRect)
    : mRect(aRect)
  {
    for (size_t i = 0; i < aTiles.size(); i++) {
      mSnapshots.push_back(aTiles[i].mDrawTarget->Snapshot());
      mOrigins.push_back(aTiles[i].mTileOrigin);
    }
  }

  virtual SurfaceType GetType() const override { return SurfaceType::TILED; }
  virtual IntSize GetSize() const override {
    MOZ_ASSERT(mRect.Width() > 0 && mRect.Height() > 0);
    return IntSize(mRect.XMost(), mRect.YMost());
  }
  virtual IntRect GetRect() const override {
    return mRect;
  }
  virtual SurfaceFormat GetFormat() const override { return mSnapshots[0]->GetFormat(); }

  virtual already_AddRefed<DataSourceSurface> GetDataSurface() override
  {
    RefPtr<DataSourceSurface> surf = Factory::CreateDataSourceSurface(mRect.Size(), GetFormat());
    if (!surf) {
      gfxCriticalError() << "DrawTargetTiled::GetDataSurface failed to allocate surface";
      return nullptr;
    }

    DataSourceSurface::MappedSurface mappedSurf;
    if (!surf->Map(DataSourceSurface::MapType::WRITE, &mappedSurf)) {
      gfxCriticalError() << "DrawTargetTiled::GetDataSurface failed to map surface";
      return nullptr;
    }

    {
      RefPtr<DrawTarget> dt =
        Factory::CreateDrawTargetForData(BackendType::CAIRO, mappedSurf.mData,
        mRect.Size(), mappedSurf.mStride, GetFormat());

      if (!dt) {
        gfxWarning() << "DrawTargetTiled::GetDataSurface failed in CreateDrawTargetForData";
        surf->Unmap();
        return nullptr;
      }
      for (size_t i = 0; i < mSnapshots.size(); i++) {
        RefPtr<DataSourceSurface> dataSurf = mSnapshots[i]->GetDataSurface();
        dt->CopySurface(dataSurf, IntRect(IntPoint(0, 0), mSnapshots[i]->GetSize()), mOrigins[i] - mRect.TopLeft());
      }
    }
    surf->Unmap();

    return surf.forget();
  }

  std::vector<RefPtr<SourceSurface>> mSnapshots;
  std::vector<IntPoint> mOrigins;
  IntRect mRect;
};

} // namespace gfx
} // namespace mozilla

#endif
