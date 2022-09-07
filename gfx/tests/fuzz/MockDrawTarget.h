/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FUZZ_MOCKDRAWTARGET_H
#define FUZZ_MOCKDRAWTARGET_H

#include "mozilla/gfx/2D.h"

class MockDrawTarget : public mozilla::gfx::DrawTarget {
 public:
  using Rect = mozilla::gfx::Rect;
  using Point = mozilla::gfx::Point;
  using DrawTargetType = mozilla::gfx::DrawTargetType;
  using BackendType = mozilla::gfx::BackendType;
  using SourceSurface = mozilla::gfx::SourceSurface;
  using IntSize = mozilla::gfx::IntSize;
  using DrawSurfaceOptions = mozilla::gfx::DrawSurfaceOptions;
  using DrawOptions = mozilla::gfx::DrawOptions;
  using FilterNode = mozilla::gfx::FilterNode;
  using ShadowOptions = mozilla::gfx::ShadowOptions;
  using CompositionOp = mozilla::gfx::CompositionOp;
  using IntRect = mozilla::gfx::IntRect;
  using IntPoint = mozilla::gfx::IntPoint;
  using Pattern = mozilla::gfx::Pattern;
  using StrokeOptions = mozilla::gfx::StrokeOptions;
  using Path = mozilla::gfx::Path;
  using ScaledFont = mozilla::gfx::ScaledFont;
  using GlyphBuffer = mozilla::gfx::GlyphBuffer;
  using Float = mozilla::gfx::Float;
  using Matrix = mozilla::gfx::Matrix;
  using SurfaceFormat = mozilla::gfx::SurfaceFormat;
  using NativeSurface = mozilla::gfx::NativeSurface;
  using PathBuilder = mozilla::gfx::PathBuilder;
  using GradientStop = mozilla::gfx::GradientStop;
  using GradientStops = mozilla::gfx::GradientStops;
  using FillRule = mozilla::gfx::FillRule;
  using ExtendMode = mozilla::gfx::ExtendMode;
  using FilterType = mozilla::gfx::FilterType;

  class MockGradientStops : public GradientStops {
   public:
    MockGradientStops() {}
    virtual ~MockGradientStops() = default;
    BackendType GetBackendType() const final { return BackendType::NONE; }
  };

  MockDrawTarget() {}
  virtual ~MockDrawTarget() = default;

  DrawTargetType GetType() const final {
    return DrawTargetType::SOFTWARE_RASTER;
  }
  BackendType GetBackendType() const final { return BackendType::NONE; }
  already_AddRefed<SourceSurface> Snapshot() final { return nullptr; }
  already_AddRefed<SourceSurface> GetBackingSurface() final { return nullptr; }
  IntSize GetSize() const final { return IntSize(100, 100); }
  void Flush() final {}
  void DrawSurface(
      SourceSurface* aSurface, const Rect& aDest, const Rect& aSource,
      const DrawSurfaceOptions& aSurfOptions = DrawSurfaceOptions(),
      const DrawOptions& aOptions = DrawOptions()) final {}
  void DrawFilter(FilterNode* aNode, const Rect& aSourceRect,
                  const Point& aDestPoint,
                  const DrawOptions& aOptions = DrawOptions()) final {}
  void DrawSurfaceWithShadow(SourceSurface* aSurface, const Point& aDest,
                             const ShadowOptions& aShadow,
                             CompositionOp aOperator) final {}
  void ClearRect(const Rect& aRect) final {}
  void CopySurface(SourceSurface* aSurface, const IntRect& aSourceRect,
                   const IntPoint& aDestination) final {}
  void FillRect(const Rect& aRect, const Pattern& aPattern,
                const DrawOptions& aOptions = DrawOptions()) final {}
  void StrokeRect(const Rect& aRect, const Pattern& aPattern,
                  const StrokeOptions& aStrokeOptions = StrokeOptions(),
                  const DrawOptions& aOptions = DrawOptions()) final {}
  void StrokeLine(const Point& aStart, const Point& aEnd,
                  const Pattern& aPattern,
                  const StrokeOptions& aStrokeOptions = StrokeOptions(),
                  const DrawOptions& aOptions = DrawOptions()) final {}
  void Stroke(const Path* aPath, const Pattern& aPattern,
              const StrokeOptions& aStrokeOptions = StrokeOptions(),
              const DrawOptions& aOptions = DrawOptions()) final {}
  void Fill(const Path* aPath, const Pattern& aPattern,
            const DrawOptions& aOptions = DrawOptions()) final {}
  void FillGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                  const Pattern& aPattern,
                  const DrawOptions& aOptions = DrawOptions()) final {}
  void Mask(const Pattern& aSource, const Pattern& aMask,
            const DrawOptions& aOptions = DrawOptions()) final {}
  void MaskSurface(const Pattern& aSource, SourceSurface* aMask, Point aOffset,
                   const DrawOptions& aOptions = DrawOptions()) final {}
  void PushClip(const Path* aPath) final {}
  void PushClipRect(const Rect& aRect) final {}
  void PopClip() final {}
  void PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                 const Matrix& aMaskTransform,
                 const IntRect& aBounds = IntRect(),
                 bool aCopyBackground = false) final {}
  void PushLayerWithBlend(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                          const Matrix& aMaskTransform,
                          const IntRect& aBounds = IntRect(),
                          bool aCopyBackground = false,
                          CompositionOp = CompositionOp::OP_OVER) final {}
  void PopLayer() final {}
  already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(
      unsigned char* aData, const IntSize& aSize, int32_t aStride,
      SurfaceFormat aFormat) const final {
    return nullptr;
  }
  already_AddRefed<SourceSurface> OptimizeSourceSurface(
      SourceSurface* aSurface) const final {
    return nullptr;
  }
  already_AddRefed<SourceSurface> CreateSourceSurfaceFromNativeSurface(
      const NativeSurface& aSurface) const final {
    return nullptr;
  }
  already_AddRefed<DrawTarget> CreateSimilarDrawTarget(
      const IntSize& aSize, SurfaceFormat aFormat) const final {
    return nullptr;
  }
  bool CanCreateSimilarDrawTarget(const IntSize& aSize,
                                  SurfaceFormat aFormat) const final {
    return false;
  }
  RefPtr<DrawTarget> CreateClippedDrawTarget(const Rect& aBounds,
                                             SurfaceFormat aFormat) final {
    return nullptr;
  }
  already_AddRefed<PathBuilder> CreatePathBuilder(
      FillRule aFillRule = FillRule::FILL_WINDING) const final {
    return nullptr;
  }
  already_AddRefed<GradientStops> CreateGradientStops(
      GradientStop* aStops, uint32_t aNumStops,
      ExtendMode aExtendMode = ExtendMode::CLAMP) const final {
    RefPtr rv = new MockGradientStops();
    return rv.forget();
  }
  already_AddRefed<FilterNode> CreateFilter(FilterType aType) final {
    return nullptr;
  }
  void DetachAllSnapshots() final {}
};

#endif
