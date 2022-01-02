/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGCLIPPATHFRAME_H_
#define LAYOUT_SVG_SVGCLIPPATHFRAME_H_

#include "gfxMatrix.h"
#include "mozilla/Attributes.h"
#include "mozilla/SVGContainerFrame.h"

class gfxContext;

namespace mozilla {
class ISVGDisplayableFrame;
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGClipPathFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGClipPathFrame final : public SVGContainerFrame {
  friend nsIFrame* ::NS_NewSVGClipPathFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

  using Matrix = gfx::Matrix;
  using SourceSurface = gfx::SourceSurface;
  using imgDrawingParams = image::imgDrawingParams;

 protected:
  explicit SVGClipPathFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGContainerFrame(aStyle, aPresContext, kClassID),
        mIsBeingProcessed(false) {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGClipPathFrame)

  // nsIFrame methods:
  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override {}

  virtual bool IsSVGTransformed(Matrix* aOwnTransforms,
                                Matrix* aFromParentTransforms) const override;

  // SVGClipPathFrame methods:

  /**
   * Applies the clipPath by pushing a clip path onto the DrawTarget.
   *
   * This method must only be used if IsTrivial() returns true, otherwise use
   * GetClipMask.
   *
   * @param aContext The context that the clip path is to be applied to.
   * @param aClippedFrame The/an nsIFrame of the element that references this
   *   clipPath that is currently being processed.
   * @param aMatrix The transform from aClippedFrame's user space to aContext's
   *   current transform.
   */
  void ApplyClipPath(gfxContext& aContext, nsIFrame* aClippedFrame,
                     const gfxMatrix& aMatrix);

  /**
   * Returns an alpha mask surface containing the clipping geometry.
   *
   * This method must only be used if IsTrivial() returns false, otherwise use
   * ApplyClipPath.
   *
   * @param aReferenceContext Used to determine the backend for and size of the
   *   returned SourceSurface, the size being limited to the device space clip
   *   extents on the context.
   * @param aClippedFrame The/an nsIFrame of the element that references this
   *   clipPath that is currently being processed.
   * @param aMatrix The transform from aClippedFrame's user space to aContext's
   *   current transform.
   * @param [in, optional] aExtraMask An extra surface that the returned
   *   surface should be masked with.
   * @param [in, optional] aExtraMasksTransform The transform to use with
   *   aExtraMask. Should be passed when aExtraMask is passed.
   */
  already_AddRefed<SourceSurface> GetClipMask(
      gfxContext& aReferenceContext, nsIFrame* aClippedFrame,
      const gfxMatrix& aMatrix, SourceSurface* aExtraMask = nullptr,
      const Matrix& aExtraMasksTransform = Matrix());

  /**
   * Paint mask directly onto a given context(aMaskContext).
   *
   * @param aMaskContext The target of mask been painting on.
   * @param aClippedFrame The/an nsIFrame of the element that references this
   *   clipPath that is currently being processed.
   * @param aMatrix The transform from aClippedFrame's user space to
   *   current transform.
   * @param [in, optional] aExtraMask An extra surface that the returned
   *   surface should be masked with.
   * @param [in, optional] aExtraMasksTransform The transform to use with
   *   aExtraMask. Should be passed when aExtraMask is passed.
   */
  void PaintClipMask(gfxContext& aMaskContext, nsIFrame* aClippedFrame,
                     const gfxMatrix& aMatrix, SourceSurface* aExtraMask,
                     const Matrix& aExtraMasksTransform);

  /**
   * aPoint is expected to be in aClippedFrame's SVG user space.
   */
  bool PointIsInsideClipPath(nsIFrame* aClippedFrame, const gfxPoint& aPoint);

  // Check if this clipPath is made up of more than one geometry object.
  // If so, the clipping API in cairo isn't enough and we need to use
  // mask based clipping.
  bool IsTrivial(ISVGDisplayableFrame** aSingleChild = nullptr);

  bool IsValid();

  // nsIFrame interface:
  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGClipPath"_ns, aResult);
  }
#endif

  SVGBBox GetBBoxForClipPathFrame(const SVGBBox& aBBox,
                                  const gfxMatrix& aMatrix, uint32_t aFlags);

  /**
   * If the clipPath element transforms its children due to
   * clipPathUnits="objectBoundingBox" being set on it and/or due to the
   * 'transform' attribute being set on it, this function returns the resulting
   * transform.
   */
  gfxMatrix GetClipPathTransform(nsIFrame* aClippedFrame);

 private:
  // SVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  already_AddRefed<DrawTarget> CreateClipMask(gfxContext& aReferenceContext,
                                              gfx::IntPoint& aOffset);

  void PaintFrameIntoMask(nsIFrame* aFrame, nsIFrame* aClippedFrame,
                          gfxContext& aTarget);

  // Set, during a GetClipMask() call, to the transform that still needs to be
  // concatenated to the transform of the DrawTarget that was passed to
  // GetClipMask in order to establish the coordinate space that the clipPath
  // establishes for its contents (i.e. including applying 'clipPathUnits' and
  // any 'transform' attribute set on the clipPath) specifically for clipping
  // the frame that was passed to GetClipMask at that moment in time.  This is
  // set so that if our GetCanvasTM method is called while GetClipMask is
  // painting its children, the returned matrix will include the transforms
  // that should be used when creating the mask for the frame passed to
  // GetClipMask.
  //
  // Note: The removal of GetCanvasTM is nearly complete, so our GetCanvasTM
  // may not even be called soon/any more.
  gfxMatrix mMatrixForChildren;

  // Flag used to indicate whether a methods that may reenter due to
  // following a reference to another instance is currently executing.
  bool mIsBeingProcessed;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGCLIPPATHFRAME_H_
