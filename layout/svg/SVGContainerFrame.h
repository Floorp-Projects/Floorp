/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGCONTAINERFRAME_H_
#define LAYOUT_SVG_SVGCONTAINERFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/UniquePtr.h"
#include "nsContainerFrame.h"
#include "nsIFrame.h"
#include "nsQueryFrame.h"
#include "nsRect.h"

class gfxContext;
class nsFrameList;
class nsIContent;

struct nsRect;

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGContainerFrame(mozilla::PresShell* aPresShell,
                                  mozilla::ComputedStyle* aStyle);

namespace mozilla {

/**
 * Base class for SVG container frames. Frame sub-classes that do not
 * display their contents directly (such as the frames for <marker> or
 * <pattern>) just inherit this class. Frame sub-classes that do or can
 * display their contents directly (such as the frames for inner-<svg> or
 * <g>) inherit our SVGDisplayContainerFrame sub-class.
 *
 *                               *** WARNING ***
 *
 * Do *not* blindly cast to SVG element types in this class's methods (see the
 * warning comment for SVGDisplayContainerFrame below).
 */
class SVGContainerFrame : public nsContainerFrame {
  friend nsIFrame* ::NS_NewSVGContainerFrame(mozilla::PresShell* aPresShell,
                                             ComputedStyle* aStyle);

 protected:
  SVGContainerFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                    ClassID aID)
      : nsContainerFrame(aStyle, aPresContext, aID) {
    AddStateBits(NS_FRAME_SVG_LAYOUT);
  }

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGContainerFrame)

  // Returns the transform to our gfxContext (to device pixels, not CSS px)
  virtual gfxMatrix GetCanvasTM() { return gfxMatrix(); }

  /**
   * Returns true if the frame's content has a transform that applies only to
   * its children, and not to the frame itself. For example, an implicit
   * transform introduced by a 'viewBox' attribute, or an explicit transform
   * due to a root-<svg> having its currentScale/currentTransform properties
   * set. If aTransform is non-null, then it will be set to the transform.
   */
  virtual bool HasChildrenOnlyTransform(Matrix* aTransform) const {
    return false;
  }

  // nsIFrame:
  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override;
  void RemoveFrame(DestroyContext&, ChildListID, nsIFrame*) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    if (aFlags & eSupportsContainLayoutAndPaint) {
      return false;
    }

    return nsContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGContainer));
  }

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override {}

  bool ComputeCustomOverflow(mozilla::OverflowAreas& aOverflowAreas) override;

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
 * SVGGenericContainerFrame which is used for unrecognized elements in the
 * SVG namespace. Do *not* blindly cast to SVG element types.
 */
class SVGDisplayContainerFrame : public SVGContainerFrame,
                                 public ISVGDisplayableFrame {
 protected:
  SVGDisplayContainerFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                           nsIFrame::ClassID aID)
      : SVGContainerFrame(aStyle, aPresContext, aID) {
    AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
  }

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_QUERYFRAME_TARGET(SVGDisplayContainerFrame)
  NS_DECL_ABSTRACT_FRAME(SVGDisplayContainerFrame)

  // nsIFrame:
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override;
  void RemoveFrame(DestroyContext&, ChildListID, nsIFrame*) override;
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  bool IsSVGTransformed(Matrix* aOwnTransform = nullptr,
                        Matrix* aFromParentTransform = nullptr) const override;

  // ISVGDisplayableFrame interface:
  void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                imgDrawingParams& aImgParams) override;
  nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;
  SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                              uint32_t aFlags) override;
  bool IsDisplayContainer() override { return true; }
  gfxMatrix GetCanvasTM() override;

 protected:
  /**
   * Cached canvasTM value.
   */
  UniquePtr<gfxMatrix> mCanvasTM;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGCONTAINERFRAME_H_
