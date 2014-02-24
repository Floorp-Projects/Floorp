/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetRecording)
  DrawTargetRecording(DrawEventRecorder *aRecorder, DrawTarget *aDT, bool aHasData = false);
  ~DrawTargetRecording();

  virtual BackendType GetType() const { return mFinalDT->GetType(); }

  virtual TemporaryRef<SourceSurface> Snapshot();

  virtual IntSize GetSize() { return mFinalDT->GetSize(); }

  /* Ensure that the DrawTarget backend has flushed all drawing operations to
   * this draw target. This must be called before using the backing surface of
   * this draw target outside of GFX 2D code.
   */
  virtual void Flush() { mFinalDT->Flush(); }

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
                           const DrawOptions &aOptions = DrawOptions());

  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions());

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
                                     CompositionOp aOperator);

  /* 
   * Clear a rectangle on the draw target to transparent black. This will
   * respect the clipping region and transform.
   *
   * aRect Rectangle to clear
   */
  virtual void ClearRect(const Rect &aRect);

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
                           const IntPoint &aDestination);

  /*
   * Fill a rectangle on the DrawTarget with a certain source pattern.
   *
   * aRect Rectangle that forms the mask of this filling operation
   * aPattern Pattern that forms the source of this filling operation
   * aOptions Options that are applied to this operation
   */
  virtual void FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions = DrawOptions());

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
                          const DrawOptions &aOptions = DrawOptions());

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
                          const DrawOptions &aOptions = DrawOptions());

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
                      const DrawOptions &aOptions = DrawOptions());
  
  /*
   * Fill a path on the draw target with a certain source pattern.
   *
   * aPath Path that is to be filled
   * aPattern Pattern that should be used for the fill
   * aOptions Draw options used for this operation
   */
  virtual void Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions = DrawOptions());

  /*
   * Fill a series of clyphs on the draw target with a certain source pattern.
   */
  virtual void FillGlyphs(ScaledFont *aFont,
                          const GlyphBuffer &aBuffer,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions = DrawOptions(),
                          const GlyphRenderingOptions *aRenderingOptions = nullptr);

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
                    const DrawOptions &aOptions = DrawOptions());

  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions());

  /*
   * Push a clip to the DrawTarget.
   *
   * aPath The path to clip to
   */
  virtual void PushClip(const Path *aPath);

  /*
   * Push an axis-aligned rectangular clip to the DrawTarget. This rectangle
   * is specified in user space.
   *
   * aRect The rect to clip to
   */
  virtual void PushClipRect(const Rect &aRect);

  /* Pop a clip from the DrawTarget. A pop without a corresponding push will
   * be ignored.
   */
  virtual void PopClip();

  /*
   * Create a SourceSurface optimized for use with this DrawTarget from
   * existing bitmap data in memory.
   *
   * The SourceSurface does not take ownership of aData, and may be freed at any time.
   */
  virtual TemporaryRef<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                                  const IntSize &aSize,
                                                                  int32_t aStride,
                                                                  SurfaceFormat aFormat) const;

  /*
   * Create a SourceSurface optimized for use with this DrawTarget from
   * an arbitrary other SourceSurface. This may return aSourceSurface or some
   * other existing surface.
   */
  virtual TemporaryRef<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const;

  /*
   * Create a SourceSurface for a type of NativeSurface. This may fail if the
   * draw target does not know how to deal with the type of NativeSurface passed
   * in.
   */
  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const;

  /*
   * Create a DrawTarget whose snapshot is optimized for use with this DrawTarget.
   */
  virtual TemporaryRef<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const;

  /*
   * Create a path builder with the specified fillmode.
   *
   * We need the fill mode up front because of Direct2D.
   * ID2D1SimplifiedGeometrySink requires the fill mode
   * to be set before calling BeginFigure().
   */
  virtual TemporaryRef<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const;

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
  virtual TemporaryRef<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const;

  virtual TemporaryRef<FilterNode> CreateFilter(FilterType aType);

  /*
   * Set a transform on the surface, this transform is applied at drawing time
   * to both the mask and source of the operation.
   */
  virtual void SetTransform(const Matrix &aTransform);

  /* Tries to get a native surface for a DrawTarget, this may fail if the
   * draw target cannot convert to this surface type.
   */
  virtual void *GetNativeSurface(NativeSurfaceType aType) { return mFinalDT->GetNativeSurface(aType); }

private:
  Path *GetPathForPathRecording(const Path *aPath) const;
  void EnsureStored(const Path *aPath);

  RefPtr<DrawEventRecorderPrivate> mRecorder;
  RefPtr<DrawTarget> mFinalDT;
};

}
}

#endif /* MOZILLA_GFX_DRAWTARGETRECORDING_H_ */
