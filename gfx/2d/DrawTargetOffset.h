/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWTARGETOFFSET_H_
#define MOZILLA_GFX_DRAWTARGETOFFSET_H_

#include "2D.h"

#include "mozilla/Vector.h"

#include "Filters.h"
#include "Logging.h"

#include <vector>

namespace mozilla {
namespace gfx {

class SourceSurfaceOffset : public SourceSurface {
 public:
  SourceSurfaceOffset(RefPtr<SourceSurface> aSurface, IntPoint aOffset)
      : mSurface(aSurface), mOffset(aOffset) {}
  virtual SurfaceType GetType() const override { return SurfaceType::OFFSET; }
  virtual IntSize GetSize() const override { return mSurface->GetSize(); }
  virtual IntRect GetRect() const override {
    return mSurface->GetRect() + mOffset;
  }
  virtual SurfaceFormat GetFormat() const override {
    return mSurface->GetFormat();
  }
  virtual already_AddRefed<DataSourceSurface> GetDataSurface() override {
    return mSurface->GetDataSurface();
  }

 private:
  RefPtr<SourceSurface> mSurface;
  IntPoint mOffset;
};

class DrawTargetOffset : public DrawTarget {
 public:
  DrawTargetOffset();

  bool Init(DrawTarget* aDrawTarget, IntPoint aOrigin);

  // We'll pestimistically return true here
  virtual bool IsTiledDrawTarget() const override { return true; }

  virtual bool IsCaptureDT() const override {
    return mDrawTarget->IsCaptureDT();
  }
  virtual DrawTargetType GetType() const override {
    return mDrawTarget->GetType();
  }
  virtual BackendType GetBackendType() const override {
    return mDrawTarget->GetBackendType();
  }
  virtual already_AddRefed<SourceSurface> Snapshot() override;
  virtual already_AddRefed<SourceSurface> IntoLuminanceSource(
      LuminanceType aLuminanceType, float aOpacity) override;
  virtual void DetachAllSnapshots() override;
  virtual IntSize GetSize() const override { return mDrawTarget->GetSize(); }
  virtual IntRect GetRect() const override {
    return IntRect(mOrigin, GetSize());
  }

  virtual void Flush() override;
  virtual void DrawSurface(SourceSurface* aSurface, const Rect& aDest,
                           const Rect& aSource,
                           const DrawSurfaceOptions& aSurfOptions,
                           const DrawOptions& aOptions) override;
  virtual void DrawFilter(FilterNode* aNode, const Rect& aSourceRect,
                          const Point& aDestPoint,
                          const DrawOptions& aOptions = DrawOptions()) override;
  virtual void DrawSurfaceWithShadow(
      SourceSurface* aSurface, const Point& aDest, const Color& aColor,
      const Point& aOffset, Float aSigma,
      CompositionOp aOperator) override { /* Not implemented */
    MOZ_CRASH("GFX: DrawSurfaceWithShadow");
  }

  virtual void ClearRect(const Rect& aRect) override;
  virtual void MaskSurface(
      const Pattern& aSource, SourceSurface* aMask, Point aOffset,
      const DrawOptions& aOptions = DrawOptions()) override;

  virtual void CopySurface(SourceSurface* aSurface, const IntRect& aSourceRect,
                           const IntPoint& aDestination) override;

  virtual void FillRect(const Rect& aRect, const Pattern& aPattern,
                        const DrawOptions& aOptions = DrawOptions()) override;
  virtual void StrokeRect(const Rect& aRect, const Pattern& aPattern,
                          const StrokeOptions& aStrokeOptions = StrokeOptions(),
                          const DrawOptions& aOptions = DrawOptions()) override;
  virtual void StrokeLine(const Point& aStart, const Point& aEnd,
                          const Pattern& aPattern,
                          const StrokeOptions& aStrokeOptions = StrokeOptions(),
                          const DrawOptions& aOptions = DrawOptions()) override;
  virtual void Stroke(const Path* aPath, const Pattern& aPattern,
                      const StrokeOptions& aStrokeOptions = StrokeOptions(),
                      const DrawOptions& aOptions = DrawOptions()) override;
  virtual void Fill(const Path* aPath, const Pattern& aPattern,
                    const DrawOptions& aOptions = DrawOptions()) override;
  virtual void FillGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                          const Pattern& aPattern,
                          const DrawOptions& aOptions = DrawOptions()) override;
  virtual void Mask(const Pattern& aSource, const Pattern& aMask,
                    const DrawOptions& aOptions = DrawOptions()) override;
  virtual void PushClip(const Path* aPath) override;
  virtual void PushClipRect(const Rect& aRect) override;
  virtual void PopClip() override;
  virtual void PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds = IntRect(),
                         bool aCopyBackground = false) override;
  virtual void PushLayerWithBlend(
      bool aOpaque, Float aOpacity, SourceSurface* aMask,
      const Matrix& aMaskTransform, const IntRect& aBounds = IntRect(),
      bool aCopyBackground = false,
      CompositionOp = CompositionOp::OP_OVER) override;
  virtual void PopLayer() override;

  virtual void SetTransform(const Matrix& aTransform) override;

  virtual void SetPermitSubpixelAA(bool aPermitSubpixelAA) override;

  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(
      unsigned char* aData, const IntSize& aSize, int32_t aStride,
      SurfaceFormat aFormat) const override {
    return mDrawTarget->CreateSourceSurfaceFromData(aData, aSize, aStride,
                                                    aFormat);
  }
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurface(
      SourceSurface* aSurface) const override {
    return mDrawTarget->OptimizeSourceSurface(aSurface);
  }

  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromNativeSurface(
      const NativeSurface& aSurface) const override {
    return mDrawTarget->CreateSourceSurfaceFromNativeSurface(aSurface);
  }

  virtual already_AddRefed<DrawTarget> CreateSimilarDrawTarget(
      const IntSize& aSize, SurfaceFormat aFormat) const override {
    return mDrawTarget->CreateSimilarDrawTarget(aSize, aFormat);
  }

  virtual bool CanCreateSimilarDrawTarget(
      const IntSize& aSize, SurfaceFormat aFormat) const override {
    return mDrawTarget->CanCreateSimilarDrawTarget(aSize, aFormat);
  }

  virtual already_AddRefed<PathBuilder> CreatePathBuilder(
      FillRule aFillRule = FillRule::FILL_WINDING) const override {
    return mDrawTarget->CreatePathBuilder(aFillRule);
  }

  virtual already_AddRefed<GradientStops> CreateGradientStops(
      GradientStop* aStops, uint32_t aNumStops,
      ExtendMode aExtendMode = ExtendMode::CLAMP) const override {
    return mDrawTarget->CreateGradientStops(aStops, aNumStops, aExtendMode);
  }
  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override {
    return mDrawTarget->CreateFilter(aType);
  }

 private:
  RefPtr<DrawTarget> mDrawTarget;
  IntPoint mOrigin;
};

}  // namespace gfx
}  // namespace mozilla

#endif
