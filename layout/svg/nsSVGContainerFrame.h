/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGCONTAINERFRAME_H
#define NS_SVGCONTAINERFRAME_H

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsFrame.h"
#include "nsIFrame.h"
#include "nsSVGDisplayableFrame.h"
#include "nsQueryFrame.h"
#include "nsRect.h"
#include "nsSVGUtils.h"

class gfxContext;
class nsFrameList;
class nsIContent;
class nsIPresShell;
class nsStyleContext;

struct nsRect;

/**
 * Base class for SVG container frames. Frame sub-classes that do not
 * display their contents directly (such as the frames for <marker> or
 * <pattern>) just inherit this class. Frame sub-classes that do or can
 * display their contents directly (such as the frames for inner-<svg> or
 * <g>) inherit our nsDisplayContainerFrame sub-class.
 *
 *                               *** WARNING ***
 *
 * Do *not* blindly cast to SVG element types in this class's methods (see the
 * warning comment for nsSVGDisplayContainerFrame below).
 */
class nsSVGContainerFrame : public nsContainerFrame
{
  friend nsIFrame* NS_NewSVGContainerFrame(nsIPresShell* aPresShell,
                                           nsStyleContext* aContext);
protected:
  nsSVGContainerFrame(nsStyleContext* aContext, ClassID aID)
    : nsContainerFrame(aContext, aID)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT);
  }

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsSVGContainerFrame)

  // Returns the transform to our gfxContext (to device pixels, not CSS px)
  virtual gfxMatrix GetCanvasTM() {
    return gfxMatrix();
  }

  /**
   * Returns true if the frame's content has a transform that applies only to
   * its children, and not to the frame itself. For example, an implicit
   * transform introduced by a 'viewBox' attribute, or an explicit transform
   * due to a root-<svg> having its currentScale/currentTransform properties
   * set. If aTransform is non-null, then it will be set to the transform.
   */
  virtual bool HasChildrenOnlyTransform(Matrix *aTransform) const {
    return false;
  }

  // nsIFrame:
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(
            aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGContainer));
  }

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {}

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override;

protected:
  /**
   * Traverses a frame tree, marking any SVGTextFrame frames as dirty
   * and calling InvalidateRenderingObservers() on it.
   */
  static void ReflowSVGNonDisplayText(nsIFrame* aContainer);
};

/**
 * Frame class or base-class for SVG containers that can or do display their
 * contents directly.
 *
 *                               *** WARNING ***
 *
 * This class's methods can *not* assume that mContent points to an instance of
 * an SVG element class since this class is inherited by
 * nsSVGGenericContainerFrame which is used for unrecognized elements in the
 * SVG namespace. Do *not* blindly cast to SVG element types.
 */
class nsSVGDisplayContainerFrame : public nsSVGContainerFrame,
                                   public nsSVGDisplayableFrame
{
protected:
  nsSVGDisplayContainerFrame(nsStyleContext* aContext, nsIFrame::ClassID aID)
    : nsSVGContainerFrame(aContext, aID)
  {
     AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
  }

public:
  NS_DECL_QUERYFRAME
  NS_DECL_QUERYFRAME_TARGET(nsSVGDisplayContainerFrame)
  NS_DECL_ABSTRACT_FRAME(nsSVGDisplayContainerFrame)

  // nsIFrame:
  virtual void InsertFrames(ChildListID     aListID,
                                nsIFrame*       aPrevFrame,
                                nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                               nsIFrame*       aOldFrame) override;
 virtual void Init(nsIContent*       aContent,
                   nsContainerFrame* aParent,
                   nsIFrame*         aPrevInFlow) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual bool IsSVGTransformed(Matrix *aOwnTransform = nullptr,
                                Matrix *aFromParentTransform = nullptr) const override;

  // nsSVGDisplayableFrame interface:
  virtual void PaintSVG(gfxContext& aContext,
                        const gfxMatrix& aTransform,
                        imgDrawingParams& aImgParams,
                        const nsIntRect* aDirtyRect = nullptr) override;
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  virtual void ReflowSVG() override;
  virtual void NotifySVGChanged(uint32_t aFlags) override;
  virtual SVGBBox GetBBoxContribution(const Matrix &aToBBoxUserspace,
                                      uint32_t aFlags) override;
  virtual bool IsDisplayContainer() override { return true; }
};

#endif
