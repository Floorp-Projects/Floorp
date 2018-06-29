/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWTARGETCAPTURE_H_
#define MOZILLA_GFX_DRAWTARGETCAPTURE_H_

#include "2D.h"
#include "CaptureCommandList.h"

#include "Filters.h"

namespace mozilla {
namespace gfx {

class DrawingCommand;
class SourceSurfaceCapture;
class AlphaBoxBlur;

class DrawTargetCaptureImpl : public DrawTargetCapture
{
  friend class SourceSurfaceCapture;

public:
  DrawTargetCaptureImpl(BackendType aBackend, const IntSize& aSize, SurfaceFormat aFormat);

  bool Init(const IntSize& aSize, DrawTarget* aRefDT);
  void InitForData(int32_t aStride, size_t aSurfaceAllocationSize);

  virtual BackendType GetBackendType() const override { return mRefDT->GetBackendType(); }
  virtual DrawTargetType GetType() const override { return mRefDT->GetType(); }
  virtual bool IsCaptureDT() const override { return true; }
  virtual already_AddRefed<SourceSurface> Snapshot() override;
  virtual already_AddRefed<SourceSurface> IntoLuminanceSource(LuminanceType aLuminanceType,
                                                              float aOpacity) override;
  virtual void SetPermitSubpixelAA(bool aPermitSubpixelAA) override;
  virtual void DetachAllSnapshots() override;
  virtual IntSize GetSize() const override { return mSize; }
  virtual void Flush() override {}
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
                          const DrawOptions &aOptions = DrawOptions()) override;
  virtual void StrokeGlyphs(ScaledFont* aFont,
                            const GlyphBuffer& aBuffer,
                            const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions = StrokeOptions(),
                            const DrawOptions& aOptions = DrawOptions()) override;
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions()) override;
  virtual void PushClip(const Path *aPath) override;
  virtual void PushClipRect(const Rect &aRect) override;
  virtual void PopClip() override;
  virtual void PushLayer(bool aOpaque,
                         Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds,
                         bool aCopyBackground) override;
  virtual void PopLayer() override;
  virtual void Blur(const AlphaBoxBlur& aBlur) override;

  virtual void SetTransform(const Matrix &aTransform) override;

  virtual bool SupportsRegionClipping() const override { return mRefDT->SupportsRegionClipping(); }

  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                                  const IntSize &aSize,
                                                                  int32_t aStride,
                                                                  SurfaceFormat aFormat) const override
  {
    return mRefDT->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
  }
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const override;

  virtual already_AddRefed<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override
  {
    return mRefDT->CreateSourceSurfaceFromNativeSurface(aSurface);
  }

  virtual already_AddRefed<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override;
  virtual RefPtr<DrawTarget>
    CreateSimilarRasterTarget(const IntSize& aSize, SurfaceFormat aFormat) const override;

  virtual already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const override
  {
    return mRefDT->CreatePathBuilder(aFillRule);
  }

  virtual already_AddRefed<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const override
  {
    return mRefDT->CreateGradientStops(aStops, aNumStops, aExtendMode);
  }
  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override;

  void ReplayToDrawTarget(DrawTarget* aDT, const Matrix& aTransform);

  void Dump() override;

protected:
  virtual ~DrawTargetCaptureImpl();

  void MarkChanged();

private:
  // This storage system was used to minimize the amount of heap allocations
  // that are required while recording. It should be noted there's no
  // guarantees on the alignments of DrawingCommands allocated in this array.
  template<typename T>
  T* AppendToCommandList() {
    if (T::AffectsSnapshot) {
      MarkChanged();
    }
    return mCommands.Append<T>();
  }
  template<typename T>
  T* ReuseOrAppendToCommandList() {
    if (T::AffectsSnapshot) {
      MarkChanged();
    }
    return mCommands.ReuseOrAppend<T>();
  }

  RefPtr<DrawTarget> mRefDT;
  IntSize mSize;
  RefPtr<SourceSurfaceCapture> mSnapshot;

  // These are set if the draw target must be explicitly backed by data.
  int32_t mStride;
  size_t mSurfaceAllocationSize;

  struct PushedLayer
  {
    explicit PushedLayer(bool aOldPermitSubpixelAA)
      : mOldPermitSubpixelAA(aOldPermitSubpixelAA)
    {}
    bool mOldPermitSubpixelAA;
  };
  std::vector<PushedLayer> mPushedLayers;

  CaptureCommandList mCommands;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_DRAWTARGETCAPTURE_H_ */
