/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWTARGETD2D1_H_
#define MOZILLA_GFX_DRAWTARGETD2D1_H_

#include "2D.h"
#include <d3d11.h>
#include <d2d1_1.h>
#include "PathD2D.h"
#include "HelpersD2D.h"

#include <vector>
#include <sstream>

#include <unordered_set>

struct IDWriteFactory;

namespace mozilla {
namespace gfx {

class SourceSurfaceD2D1;

const int32_t kLayerCacheSize1 = 5;

class DrawTargetD2D1 : public DrawTarget
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetD2D1, override)
  DrawTargetD2D1();
  virtual ~DrawTargetD2D1();

  virtual DrawTargetType GetType() const override { return DrawTargetType::HARDWARE_RASTER; }
  virtual BackendType GetBackendType() const override { return BackendType::DIRECT2D1_1; }
  virtual already_AddRefed<SourceSurface> Snapshot() override;
  virtual IntSize GetSize() override { return mSize; }

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
                                     CompositionOp aOperator) override;
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
                          const DrawOptions &aOptions = DrawOptions(),
                          const GlyphRenderingOptions *aRenderingOptions = nullptr) override;
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions()) override;
  virtual void PushClip(const Path *aPath) override;
  virtual void PushClipRect(const Rect &aRect) override;
  virtual void PushDeviceSpaceClipRects(const IntRect* aRects, uint32_t aCount) override;

  virtual void PopClip() override;
  virtual void PushLayer(bool aOpaque, Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds = IntRect(),
                         bool aCopyBackground = false) override;
  virtual void PopLayer() override;

  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                                  const IntSize &aSize,
                                                                  int32_t aStride,
                                                                  SurfaceFormat aFormat) const override;
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const override;

  virtual already_AddRefed<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override { return nullptr; }
  
  virtual already_AddRefed<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override;

  virtual already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const override;

  virtual already_AddRefed<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const override;

  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override;

  virtual bool SupportsRegionClipping() const override { return false; }
  virtual bool IsCurrentGroupOpaque() override { return CurrentLayer().mIsOpaque; }

  virtual void *GetNativeSurface(NativeSurfaceType aType) override { return nullptr; }

  virtual void DetachAllSnapshots() override { MarkChanged(); }

  virtual void GetGlyphRasterizationMetrics(ScaledFont *aScaledFont, const uint16_t* aGlyphIndices,
                                            uint32_t aNumGlyphs, GlyphMetrics* aGlyphMetrics) override;

  bool Init(const IntSize &aSize, SurfaceFormat aFormat);
  bool Init(ID3D11Texture2D* aTexture, SurfaceFormat aFormat);
  uint32_t GetByteSize() const;

  // This function will get an image for a surface, it may adjust the source
  // transform for any transformation of the resulting image relative to the
  // oritingal SourceSurface. By default, the surface and its transform are
  // interpreted in user-space, but may be specified in device-space instead.
  already_AddRefed<ID2D1Image> GetImageForSurface(SourceSurface *aSurface, Matrix &aSourceTransform,
                                              ExtendMode aExtendMode, const IntRect* aSourceRect = nullptr,
                                              bool aUserSpace = true);

  already_AddRefed<ID2D1Image> GetImageForSurface(SourceSurface *aSurface, ExtendMode aExtendMode) {
    Matrix mat;
    return GetImageForSurface(aSurface, mat, aExtendMode, nullptr);
  }

  static ID2D1Factory1 *factory();
  static void CleanupD2D();
  static IDWriteFactory *GetDWriteFactory();

  operator std::string() const {
    std::stringstream stream;
    stream << "DrawTargetD2D 1.1 (" << this << ")";
    return stream.str();
  }

  static uint32_t GetMaxSurfaceSize() {
    return D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  }

  static uint64_t mVRAMUsageDT;
  static uint64_t mVRAMUsageSS;

private:
  friend class SourceSurfaceD2D1;

  typedef std::unordered_set<DrawTargetD2D1*> TargetSet;

  // This function will mark the surface as changing, and make sure any
  // copy-on-write snapshots are notified.
  void MarkChanged();
  bool ShouldClipTemporarySurfaceDrawing(CompositionOp aOp, const Pattern& aPattern, bool aClipIsComplex);
  void PrepareForDrawing(CompositionOp aOp, const Pattern &aPattern);
  void FinalizeDrawing(CompositionOp aOp, const Pattern &aPattern);
  void FlushTransformToDC() {
    if (mTransformDirty) {
      mDC->SetTransform(D2DMatrix(mTransform));
      mTransformDirty = false;
    }
  }
  void AddDependencyOnSource(SourceSurfaceD2D1* aSource);

  // Must be called with all clips popped and an identity matrix set.
  already_AddRefed<ID2D1Image> GetImageForLayerContent(bool aShouldPreserveContent = true);

  ID2D1Image* CurrentTarget()
  {
    if (CurrentLayer().mCurrentList) {
      return CurrentLayer().mCurrentList;
    }
    return mBitmap;
  }

  // This returns the clipped geometry, in addition it returns aClipBounds which
  // represents the intersection of all pixel-aligned rectangular clips that
  // are currently set. The returned clipped geometry must be clipped by these
  // bounds to correctly reflect the total clip. This is in device space and
  // only for clips applied to the -current layer-.
  already_AddRefed<ID2D1Geometry> GetClippedGeometry(IntRect *aClipBounds);

  already_AddRefed<ID2D1Geometry> GetInverseClippedGeometry();

  // This gives the device space clip rect applied to the -current layer-.
  bool GetDeviceSpaceClipRect(D2D1_RECT_F& aClipRect, bool& aIsPixelAligned);

  void PopAllClips();
  void PushAllClips();
  void PushClipsToDC(ID2D1DeviceContext *aDC, bool aForceIgnoreAlpha = false, const D2D1_RECT_F& aMaxRect = D2D1::InfiniteRect());
  void PopClipsFromDC(ID2D1DeviceContext *aDC);

  already_AddRefed<ID2D1Brush> CreateTransparentBlackBrush();
  already_AddRefed<ID2D1SolidColorBrush> GetSolidColorBrush(const D2D_COLOR_F& aColor);
  already_AddRefed<ID2D1Brush> CreateBrushForPattern(const Pattern &aPattern, Float aAlpha = 1.0f);

  void PushClipGeometry(ID2D1Geometry* aGeometry, const D2D1_MATRIX_3X2_F& aTransform, bool aPixelAligned = false);

  void PushD2DLayer(ID2D1DeviceContext *aDC, ID2D1Geometry *aGeometry, const D2D1_MATRIX_3X2_F &aTransform,
                    bool aPixelAligned = false, bool aForceIgnoreAlpha = false,
                    const D2D1_RECT_F& aLayerRect = D2D1::InfiniteRect());

  // This function is used to determine if the mDC is still valid; if it is
  // stale, we should avoid using it to execute any draw commands.
  bool IsDeviceContextValid();

  IntSize mSize;

  RefPtr<ID2D1Geometry> mCurrentClippedGeometry;
  // This is only valid if mCurrentClippedGeometry is non-null. And will
  // only be the intersection of all pixel-aligned retangular clips. This is in
  // device space.
  IntRect mCurrentClipBounds;
  mutable RefPtr<ID2D1DeviceContext> mDC;
  RefPtr<ID2D1Bitmap1> mBitmap;
  RefPtr<ID2D1CommandList> mCommandList;

  RefPtr<ID2D1SolidColorBrush> mSolidColorBrush;

  // We store this to prevent excessive SetTextRenderingParams calls.
  RefPtr<IDWriteRenderingParams> mTextRenderingParams;

  // List of pushed clips.
  struct PushedClip
  {
    D2D1_RECT_F mBounds;
    // If mGeometry is non-null, the mTransform member will be used.
    D2D1_MATRIX_3X2_F mTransform;
    RefPtr<ID2D1Geometry> mGeometry;
    // Indicates if mBounds, and when non-null, mGeometry with mTransform
    // applied, are pixel-aligned.
    bool mIsPixelAligned;
  };

  // List of pushed layers.
  struct PushedLayer
  {
    PushedLayer() : mClipsArePushed(false), mIsOpaque(false), mOldPermitSubpixelAA(false) {}

    std::vector<PushedClip> mPushedClips;
    RefPtr<ID2D1CommandList> mCurrentList;
    // True if the current clip stack is pushed to the CurrentTarget().
    bool mClipsArePushed;
    bool mIsOpaque;
    bool mOldPermitSubpixelAA;
  };
  std::vector<PushedLayer> mPushedLayers;
  PushedLayer& CurrentLayer()
  {
    return mPushedLayers.back();
  }

  // The latest snapshot of this surface. This needs to be told when this
  // target is modified. We keep it alive as a cache.
  RefPtr<SourceSurfaceD2D1> mSnapshot;
  // A list of targets we need to flush when we're modified.
  TargetSet mDependentTargets;
  // A list of targets which have this object in their mDependentTargets set
  TargetSet mDependingOnTargets;

  uint32_t mUsedCommandListsSincePurge;
  // When a BlendEffect has been drawn to a command list, and that command list is
  // subsequently used -again- as an input to a blend effect for a command list,
  // this causes an infinite recursion inside D2D as it tries to resolve the bounds.
  // If we resolve the current command list before this happens
  // we can avoid the subsequent hang. (See bug 1293586)
  bool mDidComplexBlendWithListInList;

  static ID2D1Factory1 *mFactory;
  static IDWriteFactory *mDWriteFactory;
  // This value is uesed to verify if the DrawTarget is created by a stale device.
  uint32_t mDeviceSeq;
};

}
}

#endif /* MOZILLA_GFX_DRAWTARGETD2D_H_ */
