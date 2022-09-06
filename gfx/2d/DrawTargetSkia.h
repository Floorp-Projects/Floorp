/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_DRAWTARGETSKIA_H
#define _MOZILLA_GFX_DRAWTARGETSKIA_H

#include "2D.h"
#include <sstream>
#include <vector>

#ifdef MOZ_WIDGET_COCOA
#  include <ApplicationServices/ApplicationServices.h>
#endif

class SkCanvas;
class SkSurface;

namespace mozilla {

template <>
class RefPtrTraits<SkSurface> {
 public:
  static void Release(SkSurface* aSurface);
  static void AddRef(SkSurface* aSurface);
};

namespace gfx {

class DataSourceSurface;
class SourceSurfaceSkia;
class BorrowedCGContext;

class DrawTargetSkia : public DrawTarget {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetSkia, override)
  DrawTargetSkia();
  virtual ~DrawTargetSkia();

  virtual DrawTargetType GetType() const override;
  virtual BackendType GetBackendType() const override {
    return BackendType::SKIA;
  }
  already_AddRefed<SourceSurface> Snapshot(SurfaceFormat aFormat);
  virtual already_AddRefed<SourceSurface> Snapshot() override {
    return Snapshot(mFormat);
  }
  already_AddRefed<SourceSurface> GetBackingSurface() override;
  virtual IntSize GetSize() const override { return mSize; };
  virtual bool LockBits(uint8_t** aData, IntSize* aSize, int32_t* aStride,
                        SurfaceFormat* aFormat,
                        IntPoint* aOrigin = nullptr) override;
  virtual void ReleaseBits(uint8_t* aData) override;
  virtual void Flush() override;
  virtual void DrawSurface(
      SourceSurface* aSurface, const Rect& aDest, const Rect& aSource,
      const DrawSurfaceOptions& aSurfOptions = DrawSurfaceOptions(),
      const DrawOptions& aOptions = DrawOptions()) override;
  virtual void DrawFilter(FilterNode* aNode, const Rect& aSourceRect,
                          const Point& aDestPoint,
                          const DrawOptions& aOptions = DrawOptions()) override;
  virtual void DrawSurfaceWithShadow(SourceSurface* aSurface,
                                     const Point& aDest,
                                     const ShadowOptions& aShadow,
                                     CompositionOp aOperator) override;
  void Clear(const Rect& aRect, bool aClipped);
  void Clear() { Clear(Rect(GetRect()), false); }
  virtual void ClearRect(const Rect& aRect) override { Clear(aRect, true); }
  void BlendSurface(SourceSurface* aSurface, const IntRect& aSourceRect,
                    const IntPoint& aDestination, CompositionOp aOperator);
  virtual void CopySurface(SourceSurface* aSurface, const IntRect& aSourceRect,
                           const IntPoint& aDestination) override {
    BlendSurface(aSurface, aSourceRect, aDestination, CompositionOp::OP_SOURCE);
  }
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
  virtual void StrokeGlyphs(
      ScaledFont* aFont, const GlyphBuffer& aBuffer, const Pattern& aPattern,
      const StrokeOptions& aStrokeOptions = StrokeOptions(),
      const DrawOptions& aOptions = DrawOptions()) override;
  virtual void Mask(const Pattern& aSource, const Pattern& aMask,
                    const DrawOptions& aOptions = DrawOptions()) override;
  virtual void MaskSurface(
      const Pattern& aSource, SourceSurface* aMask, Point aOffset,
      const DrawOptions& aOptions = DrawOptions()) override;
  virtual bool Draw3DTransformedSurface(SourceSurface* aSurface,
                                        const Matrix4x4& aMatrix) override;
  virtual void PushClip(const Path* aPath) override;
  virtual void PushClipRect(const Rect& aRect) override;
  virtual void PushDeviceSpaceClipRects(const IntRect* aRects,
                                        uint32_t aCount) override;
  virtual void PopClip() override;
  virtual void PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds = IntRect(),
                         bool aCopyBackground = false) override;
  virtual void PushLayerWithBlend(
      bool aOpaque, Float aOpacity, SourceSurface* aMask,
      const Matrix& aMaskTransform, const IntRect& aBounds = IntRect(),
      bool aCopyBackground = false,
      CompositionOp aCompositionOp = CompositionOp::OP_OVER) override;
  virtual void PopLayer() override;
  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(
      unsigned char* aData, const IntSize& aSize, int32_t aStride,
      SurfaceFormat aFormat) const override;
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurface(
      SourceSurface* aSurface) const override;
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurfaceForUnknownAlpha(
      SourceSurface* aSurface) const override;
  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromNativeSurface(
      const NativeSurface& aSurface) const override;
  virtual already_AddRefed<DrawTarget> CreateSimilarDrawTarget(
      const IntSize& aSize, SurfaceFormat aFormat) const override;
  virtual bool CanCreateSimilarDrawTarget(const IntSize& aSize,
                                          SurfaceFormat aFormat) const override;
  virtual RefPtr<DrawTarget> CreateClippedDrawTarget(
      const Rect& aBounds, SurfaceFormat aFormat) override;

  virtual already_AddRefed<PathBuilder> CreatePathBuilder(
      FillRule aFillRule = FillRule::FILL_WINDING) const override;
  virtual already_AddRefed<GradientStops> CreateGradientStops(
      GradientStop* aStops, uint32_t aNumStops,
      ExtendMode aExtendMode = ExtendMode::CLAMP) const override;
  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override;
  virtual void SetTransform(const Matrix& aTransform) override;
  virtual void* GetNativeSurface(NativeSurfaceType aType) override;
  virtual void DetachAllSnapshots() override { MarkChanged(); }

  bool Init(const IntSize& aSize, SurfaceFormat aFormat);
  bool Init(unsigned char* aData, const IntSize& aSize, int32_t aStride,
            SurfaceFormat aFormat, bool aUninitialized = false);
  bool Init(SkCanvas* aCanvas);
  bool Init(RefPtr<DataSourceSurface>&& aSurface);

  // Skia assumes that texture sizes fit in 16-bit signed integers.
  static size_t GetMaxSurfaceSize() { return 32767; }

  operator std::string() const {
    std::stringstream stream;
    stream << "DrawTargetSkia(" << this << ")";
    return stream.str();
  }

  Maybe<IntRect> GetDeviceClipRect(bool aAllowComplex = false) const;

  Maybe<Rect> GetGlyphLocalBounds(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                                  const Pattern& aPattern,
                                  const StrokeOptions* aStrokeOptions,
                                  const DrawOptions& aOptions);

 private:
  friend class SourceSurfaceSkia;

  static void ReleaseMappedSkSurface(void* aPixels, void* aContext);

  void MarkChanged();

  void DrawGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                  const Pattern& aPattern,
                  const StrokeOptions* aStrokeOptions = nullptr,
                  const DrawOptions& aOptions = DrawOptions());

  struct PushedLayer {
    PushedLayer(bool aOldPermitSubpixelAA, SourceSurface* aMask)
        : mOldPermitSubpixelAA(aOldPermitSubpixelAA), mMask(aMask) {}
    bool mOldPermitSubpixelAA;
    RefPtr<SourceSurface> mMask;
  };
  std::vector<PushedLayer> mPushedLayers;

  IntSize mSize;
  RefPtr<SkSurface> mSurface;
  SkCanvas* mCanvas = nullptr;
  RefPtr<DataSourceSurface> mBackingSurface;
  RefPtr<SourceSurfaceSkia> mSnapshot;
  Mutex mSnapshotLock MOZ_UNANNOTATED;

#ifdef MOZ_WIDGET_COCOA
  friend class BorrowedCGContext;

  CGContextRef BorrowCGContext(const DrawOptions& aOptions);
  void ReturnCGContext(CGContextRef);
  bool FillGlyphsWithCG(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                        const Pattern& aPattern,
                        const DrawOptions& aOptions = DrawOptions());

  CGContextRef mCG;
  CGColorSpaceRef mColorSpace;
  uint8_t* mCanvasData;
  IntSize mCGSize;
  bool mNeedLayer;
#endif
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _MOZILLA_GFX_SOURCESURFACESKIA_H
