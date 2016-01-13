/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_SOURCESURFACESKIA_H
#define _MOZILLA_GFX_SOURCESURFACESKIA_H

#ifdef USE_SKIA_GPU
#include "skia/include/gpu/GrContext.h"
#include "skia/include/gpu/gl/GrGLInterface.h"
#endif

#include "skia/include/core/SkCanvas.h"

#include "2D.h"
#include "HelpersSkia.h"
#include "Rect.h"
#include "PathSkia.h"
#include <sstream>
#include <vector>

namespace mozilla {
namespace gfx {

class SourceSurfaceSkia;

class DrawTargetSkia : public DrawTarget
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetSkia, override)
  DrawTargetSkia();
  virtual ~DrawTargetSkia();

  virtual DrawTargetType GetType() const override;
  virtual BackendType GetBackendType() const override { return BackendType::SKIA; }
  virtual already_AddRefed<SourceSurface> Snapshot() override;
  virtual IntSize GetSize() override { return mSize; }
  virtual bool LockBits(uint8_t** aData, IntSize* aSize,
                        int32_t* aStride, SurfaceFormat* aFormat) override;
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
  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions()) override;
  virtual void PushClip(const Path *aPath) override;
  virtual void PushClipRect(const Rect& aRect) override;
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
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override;
  virtual already_AddRefed<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override;
  virtual already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const override;
  virtual already_AddRefed<GradientStops> CreateGradientStops(GradientStop *aStops, uint32_t aNumStops, ExtendMode aExtendMode = ExtendMode::CLAMP) const override;
  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override;
  virtual void SetTransform(const Matrix &aTransform) override;
  virtual void *GetNativeSurface(NativeSurfaceType aType) override;

  bool Init(const IntSize &aSize, SurfaceFormat aFormat);
  void Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat);

#ifdef USE_SKIA_GPU
  bool InitWithGrContext(GrContext* aGrContext,
                         const IntSize &aSize,
                         SurfaceFormat aFormat) override;
#endif

  // Skia assumes that texture sizes fit in 16-bit signed integers.
  static size_t GetMaxSurfaceSize() {
    return 32767;
  }

  operator std::string() const {
    std::stringstream stream;
    stream << "DrawTargetSkia(" << this << ")";
    return stream.str();
  }

private:
  friend class SourceSurfaceSkia;
  void SnapshotDestroyed();

  void MarkChanged();

  bool ShouldLCDRenderText(FontType aFontType, AntialiasMode aAntialiasMode);

  bool UsingSkiaGPU() const;

  struct PushedLayer
  {
    PushedLayer(bool aOldPermitSubpixelAA,
                bool aOpaque,
                Float aOpacity,
                SourceSurface* aMask,
                const Matrix& aMaskTransform)
      : mOldPermitSubpixelAA(aOldPermitSubpixelAA),
        mOpaque(aOpaque),
        mOpacity(aOpacity),
        mMask(aMask),
        mMaskTransform(aMaskTransform)
    {}
    bool mOldPermitSubpixelAA;
    bool mOpaque;
    Float mOpacity;
    RefPtr<SourceSurface> mMask;
    Matrix mMaskTransform;
  };
  std::vector<PushedLayer> mPushedLayers;

#ifdef USE_SKIA_GPU
  RefPtrSkia<GrContext> mGrContext;
  GrGLuint mTexture;
#endif

  IntSize mSize;
  RefPtrSkia<SkCanvas> mCanvas;
  SourceSurfaceSkia* mSnapshot;
};

} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_SOURCESURFACESKIA_H
