/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGCLIPPATHFRAME_H__
#define __NS_SVGCLIPPATHFRAME_H__

#include "gfxMatrix.h"
#include "mozilla/Attributes.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class gfxContext;
class nsSVGDisplayableFrame;

class nsSVGClipPathFrame final : public nsSVGContainerFrame
{
  friend nsIFrame*
  NS_NewSVGClipPathFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::image::DrawResult DrawResult;

protected:
  explicit nsSVGClipPathFrame(nsStyleContext* aContext)
    : nsSVGContainerFrame(aContext, mozilla::FrameType::SVGClipPath)
    , mIsBeingProcessed(false)
  {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame methods:
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {}

  // nsSVGClipPathFrame methods:

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
  void ApplyClipPath(gfxContext& aContext,
                     nsIFrame* aClippedFrame,
                     const gfxMatrix &aMatrix);

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
   * @param [out] aMaskTransform The transform to use with the returned
   *   surface.
   * @param [in, optional] aExtraMask An extra surface that the returned
   *   surface should be masked with.
   * @param [in, optional] aExtraMasksTransform The transform to use with
   *   aExtraMask. Should be passed when aExtraMask is passed.
   */
  mozilla::Pair<DrawResult, RefPtr<SourceSurface>>
  GetClipMask(gfxContext& aReferenceContext, nsIFrame* aClippedFrame,
              const gfxMatrix& aMatrix, Matrix* aMaskTransform,
              SourceSurface* aExtraMask = nullptr,
              const Matrix& aExtraMasksTransform = Matrix());

  /**
   * Paint mask directly onto a given context(aMaskContext).
   *
   * @param aMaskContext The target of mask been painting on.
   * @param aClippedFrame The/an nsIFrame of the element that references this
   *   clipPath that is currently being processed.
   * @param aMatrix The transform from aClippedFrame's user space to
   *   current transform.
   * @param [out] aMaskTransform The transform to use with the returned
   *   surface.
   * @param [in, optional] aExtraMask An extra surface that the returned
   *   surface should be masked with.
   * @param [in, optional] aExtraMasksTransform The transform to use with
   *   aExtraMask. Should be passed when aExtraMask is passed.
   */
  DrawResult
  PaintClipMask(gfxContext& aMaskContext, nsIFrame* aClippedFrame,
                const gfxMatrix& aMatrix, Matrix* aMaskTransform,
                SourceSurface* aExtraMask, const Matrix& aExtraMasksTransform);

  /**
   * aPoint is expected to be in aClippedFrame's SVG user space.
   */
  bool PointIsInsideClipPath(nsIFrame* aClippedFrame, const gfxPoint &aPoint);

  // Check if this clipPath is made up of more than one geometry object.
  // If so, the clipping API in cairo isn't enough and we need to use
  // mask based clipping.
  bool IsTrivial(nsSVGDisplayableFrame **aSingleChild = nullptr);

  bool IsValid();

  // nsIFrame interface:
  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGClipPath"), aResult);
  }
#endif

  SVGBBox GetBBoxForClipPathFrame(const SVGBBox& aBBox,
                                  const gfxMatrix& aMatrix);

  /**
   * If the clipPath element transforms its children due to
   * clipPathUnits="objectBoundingBox" being set on it and/or due to the
   * 'transform' attribute being set on it, this function returns the resulting
   * transform.
   */
  gfxMatrix GetClipPathTransform(nsIFrame* aClippedFrame);

private:

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  already_AddRefed<DrawTarget> CreateClipMask(gfxContext& aReferenceContext,
                                              mozilla::gfx::IntPoint& aOffset);

  DrawResult PaintFrameIntoMask(nsIFrame *aFrame, nsIFrame* aClippedFrame,
                                gfxContext& aTarget, const gfxMatrix& aMatrix);

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

#endif
