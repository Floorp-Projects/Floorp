/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWTARGETRECORDING_H_
#define MOZILLA_GFX_DRAWTARGETRECORDING_H_

#include "2D.h"
#include "DrawEventRecorder.h"

namespace mozilla {
namespace gfx {

class DrawTargetRecording : public DrawTarget
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetRecording, override)
  DrawTargetRecording(DrawEventRecorder *aRecorder, DrawTarget *aDT, bool aHasData = false);

  /**
   * Used for creating a DrawTargetRecording for a CreateSimilarDrawTarget call.
   *
   * @param aDT DrawTargetRecording on which CreateSimilarDrawTarget  was called
   * @param aSize size for the similar DrawTarget
   * @param aFormat format for the similar DrawTarget
   */
  DrawTargetRecording(const DrawTargetRecording *aDT, const IntSize &aSize,
                      SurfaceFormat aFormat);

  ~DrawTargetRecording();

  virtual DrawTargetType GetType() const override { return mFinalDT->GetType(); }
  virtual BackendType GetBackendType() const override { return mFinalDT->GetBackendType(); }
  virtual bool IsRecording() const override { return true; }

  virtual already_AddRefed<SourceSurface> Snapshot() override;

  virtual void DetachAllSnapshots() override;

  virtual IntSize GetSize() override { return mFinalDT->GetSize(); }

  /* Ensure that the DrawTarget backend has flushed all drawing operations to
   * this draw target. This must be called before using the backing surface of
   * this draw target outside of GFX 2D code.
   */
  virtual void Flush() override { mFinalDT->Flush(); }

  /*
   * Draw a surface to the draw target. Possibly doing partial drawing or
   * applying scaling. No sampling happens outside the source.
   *
   * aSurface Source surface to draw
   * aDest Destination rectangle that this drawing operation should draw to
   * aSource Source rectangle in aSurface coordinates, this area of aSurface
   *         will be stretched to the size of aDest.
   * aOptions General draw options that are applied to the operation
   * aSurfOptions DrawSurface options that are applied
   */
  virtual void DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions = DrawSurfaceOptions(),
                           const DrawOptions &aOptions = DrawOptions()) override;

  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions()) override;

  /*
   * Blend a surface to the draw target with a shadow. The shadow is drawn as a
   * gaussian blur using a specified sigma. The shadow is clipped to the size
   * of the input surface, so the input surface should contain a transparent
   * border the size of the approximate coverage of the blur (3 * aSigma).
   * NOTE: This function works in device space!
   *
   * aSurface Source surface to draw.
   * aDest Destination point that this drawing operation should draw to.
   * aColor Color of the drawn shadow
   * aOffset Offset of the shadow
   * aSigma Sigma used for the guassian filter kernel
   * aOperator Composition operator used
   */
  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator) override;

  /* 
   * Clear a rectangle on the draw target to transparent black. This will
   * respect the clipping region and transform.
   *
   * aRect Rectangle to clear
   */
  virtual void ClearRect(const Rect &aRect) override;

  /*
   * This is essentially a 'memcpy' between two surfaces. It moves a pixel
   * aligned area from the source surface unscaled directly onto the
   * drawtarget. This ignores both transform and clip.
   *
   * aSurface Surface to copy from
   * aSourceRect Source rectangle to be copied
   * aDest Destination point to copy the surface to
   */
  virtual void CopySurface(SourceSurface *aSurface,
                           const IntRect &aSourceRect,
                           const IntPoint &aDestination) override;

  /*
   * Fill a rectangle on the DrawTarget with a certain source pattern.
   *
   * aRect Rectangle that forms the mask of this filling operation
   * aPattern Pattern that forms the source of this filling operation
   * aOptions Options that are applied to this operation
   */
  virtual void FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions = DrawOptions()) override;

  /*
   * Stroke a rectangle on the DrawTarget with a certain source pattern.
   *
   * aRect Rectangle that forms the mask of this stroking operation
   * aPattern Pattern that forms the source of this stroking operation
   * aOptions Options that are applied to this operation
   */
  virtual void StrokeRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions()) override;

  /*
   * Stroke a line on the DrawTarget with a certain source pattern.
   *
   * aStart Starting point of the line
   * aEnd End point of the line
   * aPattern Pattern that forms the source of this stroking operation
   * aOptions Options that are applied to this operation
   */
  virtual void StrokeLine(const Point &aStart,
                          const Point &aEnd,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions()) override;

  /*
   * Stroke a path on the draw target with a certain source pattern.
   *
   * aPath Path that is to be stroked
   * aPattern Pattern that should be used for the stroke
   * aStrokeOptions Stroke options used for this operation
   * aOptions Draw options used for this operation
   */
  virtual void Stroke(const Path *aPath,
                      const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions = StrokeOptions(),
                      const DrawOptions &aOptions = DrawOptions()) override;
  
  /*
   * Fill a path on the draw target with a certain source pattern.
   *
   * aPath Path that is to be filled
   * aPattern Pattern that should be used for the fill
   * aOptions Draw options used for this operation
   */
  virtual void Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions = DrawOptions()) override;

  /*
   * Fill a series of clyphs on the draw target with a certain source pattern.
   */
  virtual void FillGlyphs(ScaledFont *aFont,
                          const GlyphBuffer &aBuffer,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions = DrawOptions(),
                          const GlyphRenderingOptions *aRenderingOptions = nullptr) override;

  /*
   * This takes a source pattern and a mask, and composites the source pattern
   * onto the destination surface using the alpha channel of the mask pattern
   * as a mask for the operation.
   *
   * aSource Source pattern
   * aMask Mask pattern
   * aOptions Drawing options
   */
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions()) override;

  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions()) override;

  /*
   * Push a clip to the DrawTarget.
   *
   * aPath The path to clip to
   */
  virtual void PushClip(const Path *aPath) override;

  /*
   * Push an axis-aligned rectangular clip to the DrawTarget. This rectangle
   * is specified in user space.
   *
   * aRect The rect to clip to
   */
  virtual void PushClipRect(const Rect &aRect) override;

  /* Pop a clip from the DrawTarget. A pop without a corresponding push will
   * be ignored.
   */
  virtual void PopClip() override;

  /**
   * Push a 'layer' to the DrawTarget, a layer is a temporary surface that all
   * drawing will be redirected to, this is used for example to support group
   * opacity or the masking of groups. Clips must be balanced within a layer,
   * i.e. between a matching PushLayer/PopLayer pair there must be as many
   * PushClip(Rect) calls as there are PopClip calls.
   *
   * @param aOpaque Whether the layer will be opaque
   * @param aOpacity Opacity of the layer
   * @param aMask Mask applied to the layer
   * @param aMaskTransform Transform applied to the layer mask
   * @param aBounds Optional bounds in device space to which the layer is
   *                limited in size.
   * @param aCopyBackground Whether to copy the background into the layer, this
   *                        is only supported when aOpaque is true.
   */
  virtual void PushLayer(bool aOpaque, Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds = IntRect(),
                         bool aCopyBackground = false) override;

  /**
   * This balances a call to PushLayer and proceeds to blend the layer back
   * onto the background. This blend will blend the temporary surface back
   * onto the target in device space using POINT sampling and operator over.
   */
  virtual void PopLayer() override;

  /*
   * Create a SourceSurface optimized for use with this DrawTarget from
   * existing bitmap data in memory.
   *
   * The SourceSurface does not take ownership of aData, and may be freed at any time.
   */
  virtual already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                                  const IntSize &aSize,
                                                                  int32_t aStride,
                                                                  SurfaceFormat aFormat) const override;

  /*
   * Create a SourceSurface optimized for use with this DrawTarget from
   * an arbitrary other SourceSurface. This may return aSourceSurface or some
   * other existing surface.
   */
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const override;

  /*
   * Create a SourceSurface for a type of NativeSurface. This may fail if the
   * draw target does not know how to deal with the type of NativeSurface passed
   * in.
   */
  virtual already_AddRefed<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override;

  /*
   * Create a DrawTarget whose snapshot is optimized for use with this DrawTarget.
   */
  virtual already_AddRefed<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override;

  /*
   * Create a path builder with the specified fillmode.
   *
   * We need the fill mode up front because of Direct2D.
   * ID2D1SimplifiedGeometrySink requires the fill mode
   * to be set before calling BeginFigure().
   */
  virtual already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const override;

  /*
   * Create a GradientStops object that holds information about a set of
   * gradient stops, this object is required for linear or radial gradient
   * patterns to represent the color stops in the gradient.
   *
   * aStops An array of gradient stops
   * aNumStops Number of stops in the array aStops
   * aExtendNone This describes how to extend the stop color outside of the
   *             gradient area.
   */
  virtual already_AddRefed<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const override;

  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override;

  /*
   * Set a transform on the surface, this transform is applied at drawing time
   * to both the mask and source of the operation.
   */
  virtual void SetTransform(const Matrix &aTransform) override;

  /* Tries to get a native surface for a DrawTarget, this may fail if the
   * draw target cannot convert to this surface type.
   */
  virtual void *GetNativeSurface(NativeSurfaceType aType) override { return mFinalDT->GetNativeSurface(aType); }

  virtual bool IsCurrentGroupOpaque() override {
    return mFinalDT->IsCurrentGroupOpaque();
  }

private:
  Path *GetPathForPathRecording(const Path *aPath) const;
  already_AddRefed<PathRecording> EnsurePathStored(const Path *aPath);
  void EnsurePatternDependenciesStored(const Pattern &aPattern);

  RefPtr<DrawEventRecorderPrivate> mRecorder;
  RefPtr<DrawTarget> mFinalDT;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_DRAWTARGETRECORDING_H_ */
