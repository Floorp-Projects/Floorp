/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_DRAWTARGET_CAIRO_H_
#define _MOZILLA_GFX_DRAWTARGET_CAIRO_H_

#include "2D.h"
#include "cairo.h"
#include "PathCairo.h"

#include <vector>

namespace mozilla {
namespace gfx {

class SourceSurfaceCairo;

class GradientStopsCairo : public GradientStops
{
  public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GradientStopsCairo)
    GradientStopsCairo(GradientStop* aStops, uint32_t aNumStops,
                       ExtendMode aExtendMode)
     : mExtendMode(aExtendMode)
    {
      for (uint32_t i = 0; i < aNumStops; ++i) {
        mStops.push_back(aStops[i]);
      }
    }

    virtual ~GradientStopsCairo() {}

    const std::vector<GradientStop>& GetStops() const
    {
      return mStops;
    }

    ExtendMode GetExtendMode() const
    {
      return mExtendMode;
    }

    virtual BackendType GetBackendType() const { return BackendType::CAIRO; }

  private:
    std::vector<GradientStop> mStops;
    ExtendMode mExtendMode;
};

class DrawTargetCairo final : public DrawTarget
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetCairo, override)
  friend class BorrowedCairoContext;
  friend class BorrowedXlibDrawable;

  DrawTargetCairo();
  virtual ~DrawTargetCairo();

  virtual bool IsValid() const override;
  virtual DrawTargetType GetType() const override;
  virtual BackendType GetBackendType() const override { return BackendType::CAIRO; }
  virtual already_AddRefed<SourceSurface> Snapshot() override;
  virtual IntSize GetSize() override;

  virtual bool IsCurrentGroupOpaque() override;

  virtual void SetPermitSubpixelAA(bool aPermitSubpixelAA) override;

  virtual bool LockBits(uint8_t** aData, IntSize* aSize,
                        int32_t* aStride, SurfaceFormat* aFormat,
                        IntPoint* aOrigin = nullptr) override;
  virtual void ReleaseBits(uint8_t* aData) override;

  virtual void Flush() override;
  virtual void DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions = DrawSurfaceOptions(),
                           const DrawOptions &aOptions = DrawOptions()) override;
  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions()) override;
  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator) override;

  virtual void ClearRect(const Rect &aRect) override;

  virtual void CopySurface(SourceSurface *aSurface,
                           const IntRect &aSourceRect,
                           const IntPoint &aDestination) override;
  virtual void CopyRect(const IntRect &aSourceRect,
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
                          const DrawOptions &aOptions,
                          const GlyphRenderingOptions *aRenderingOptions = nullptr) override;
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions()) override;
  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions()) override;

  virtual void PushClip(const Path *aPath) override;
  virtual void PushClipRect(const Rect &aRect) override;
  virtual void PopClip() override;
  virtual void PushLayer(bool aOpaque, Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds = IntRect(),
                         bool aCopyBackground = false) override;
  virtual void PopLayer() override;

  virtual already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const override;

  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                            const IntSize &aSize,
                                                            int32_t aStride,
                                                            SurfaceFormat aFormat) const override;
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const override;
  virtual already_AddRefed<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override;
  virtual already_AddRefed<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override;
  virtual already_AddRefed<DrawTarget>
    CreateShadowDrawTarget(const IntSize &aSize, SurfaceFormat aFormat,
                           float aSigma) const override;

  virtual already_AddRefed<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const override;

  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override;

  virtual void *GetNativeSurface(NativeSurfaceType aType) override;

  bool Init(cairo_surface_t* aSurface, const IntSize& aSize, SurfaceFormat* aFormat = nullptr);
  bool Init(const IntSize& aSize, SurfaceFormat aFormat);
  bool Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat);

  virtual void SetTransform(const Matrix& aTransform) override;

  // Call to set up aContext for drawing (with the current transform, etc).
  // Pass the path you're going to be using if you have one.
  // Implicitly calls WillChange(aPath).
  void PrepareForDrawing(cairo_t* aContext, const Path* aPath = nullptr);

  static cairo_surface_t *GetDummySurface();

  // Cairo hardcodes this as its maximum surface size.
  static size_t GetMaxSurfaceSize() {
    return 32767;
  }

private: // methods
  // Init cairo surface without doing a cairo_surface_reference() call.
  bool InitAlreadyReferenced(cairo_surface_t* aSurface, const IntSize& aSize, SurfaceFormat* aFormat = nullptr);
  enum DrawPatternType { DRAW_FILL, DRAW_STROKE };
  void DrawPattern(const Pattern& aPattern,
                   const StrokeOptions& aStrokeOptions,
                   const DrawOptions& aOptions,
                   DrawPatternType aDrawType,
                   bool aPathBoundsClip = false);

  void CopySurfaceInternal(cairo_surface_t* aSurface,
                           const IntRect& aSource,
                           const IntPoint& aDest);

  Rect GetUserSpaceClip();

  // Call before you make any changes to the backing surface with which this
  // context is associated. Pass the path you're going to be using if you have
  // one.
  void WillChange(const Path* aPath = nullptr);

  // Call if there is any reason to disassociate the snapshot from this draw
  // target; for example, because we're going to be destroyed.
  void MarkSnapshotIndependent();

  // If the current operator is "source" then clear the destination before we
  // draw into it, to simulate the effect of an unbounded source operator.
  void ClearSurfaceForUnboundedSource(const CompositionOp &aOperator);

private: // data
  cairo_t* mContext;
  cairo_surface_t* mSurface;
  IntSize mSize;
  bool mTransformSingular;

  uint8_t* mLockedBits;

  struct PushedLayer
  {
    PushedLayer(Float aOpacity, bool aWasPermittingSubpixelAA)
      : mOpacity(aOpacity)
      , mMaskPattern(nullptr)
      , mWasPermittingSubpixelAA(aWasPermittingSubpixelAA)
    {}
    Float mOpacity;
    cairo_pattern_t* mMaskPattern;
    bool mWasPermittingSubpixelAA;
  };
  std::vector<PushedLayer> mPushedLayers;

  // The latest snapshot of this surface. This needs to be told when this
  // target is modified. We keep it alive as a cache.
  RefPtr<SourceSurfaceCairo> mSnapshot;
  static cairo_surface_t *mDummySurface;
};

} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_DRAWTARGET_CAIRO_H_
