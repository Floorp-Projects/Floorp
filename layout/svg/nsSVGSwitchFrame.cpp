/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "gfxRect.h"
#include "SVGObserverUtils.h"
#include "nsSVGGFrame.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/SVGSwitchElement.h"
#include "nsSVGUtils.h"
#include "SVGTextFrame.h"
#include "nsSVGContainerFrame.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

class nsSVGSwitchFrame final : public nsSVGGFrame {
  friend nsIFrame* NS_NewSVGSwitchFrame(mozilla::PresShell* aPresShell,
                                        ComputedStyle* aStyle);

 protected:
  explicit nsSVGSwitchFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsSVGGFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSVGSwitchFrame)

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGSwitch"), aResult);
  }
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  // nsSVGDisplayableFrame interface:
  virtual void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                        imgDrawingParams& aImgParams,
                        const nsIntRect* aDirtyRect = nullptr) override;
  nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  virtual void ReflowSVG() override;
  virtual SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                                      uint32_t aFlags) override;

 private:
  nsIFrame* GetActiveChildFrame();
  void ReflowAllSVGTextFramesInsideNonActiveChildren(nsIFrame* aActiveChild);
  static void AlwaysReflowSVGTextFrameDoForOneKid(nsIFrame* aKid);
};

//----------------------------------------------------------------------
// Implementation

nsIFrame* NS_NewSVGSwitchFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsSVGSwitchFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGSwitchFrame)

#ifdef DEBUG
void nsSVGSwitchFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::svgSwitch),
               "Content is not an SVG switch");

  nsSVGGFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

void nsSVGSwitchFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                        const nsDisplayListSet& aLists) {
  nsIFrame* kid = GetActiveChildFrame();
  if (kid) {
    BuildDisplayListForChild(aBuilder, kid, aLists);
  }
}

void nsSVGSwitchFrame::PaintSVG(gfxContext& aContext,
                                const gfxMatrix& aTransform,
                                imgDrawingParams& aImgParams,
                                const nsIntRect* aDirtyRect) {
  NS_ASSERTION(
      !NS_SVGDisplayListPaintingEnabled() || (mState & NS_FRAME_IS_NONDISPLAY),
      "If display lists are enabled, only painting of non-display "
      "SVG should take this code path");

  if (StyleEffects()->mOpacity == 0.0) {
    return;
  }

  nsIFrame* kid = GetActiveChildFrame();
  if (kid) {
    gfxMatrix tm = aTransform;
    if (kid->GetContent()->IsSVGElement()) {
      tm = nsSVGUtils::GetTransformMatrixInUserSpace(kid) * tm;
    }
    nsSVGUtils::PaintFrameWithEffects(kid, aContext, tm, aImgParams,
                                      aDirtyRect);
  }
}

nsIFrame* nsSVGSwitchFrame::GetFrameForPoint(const gfxPoint& aPoint) {
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
                   (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only hit-testing of non-display "
               "SVG should take this code path");

  nsIFrame* kid = GetActiveChildFrame();
  nsSVGDisplayableFrame* svgFrame = do_QueryFrame(kid);
  if (svgFrame) {
    // Transform the point from our SVG user space to our child's.
    gfxPoint point = aPoint;
    gfxMatrix m =
        static_cast<const SVGElement*>(GetContent())
            ->PrependLocalTransformsTo(gfxMatrix(), eChildToUserSpace);
    m = static_cast<const SVGElement*>(kid->GetContent())
            ->PrependLocalTransformsTo(m, eUserSpaceToParent);
    if (!m.IsIdentity()) {
      if (!m.Invert()) {
        return nullptr;
      }
      point = m.TransformPoint(point);
    }
    return svgFrame->GetFrameForPoint(point);
  }

  return nullptr;
}

static bool shouldReflowSVGTextFrameInside(nsIFrame* aFrame) {
  return aFrame->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer) ||
         aFrame->IsSVGForeignObjectFrame() ||
         !aFrame->IsFrameOfType(nsIFrame::eSVG);
}

void nsSVGSwitchFrame::AlwaysReflowSVGTextFrameDoForOneKid(nsIFrame* aKid) {
  if (!NS_SUBTREE_DIRTY(aKid)) {
    return;
  }

  if (aKid->IsSVGTextFrame()) {
    MOZ_ASSERT(!aKid->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY),
               "A non-display SVGTextFrame directly contained in a display "
               "container?");
    static_cast<SVGTextFrame*>(aKid)->ReflowSVG();
  } else if (shouldReflowSVGTextFrameInside(aKid)) {
    if (!aKid->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
      for (nsIFrame* kid : aKid->PrincipalChildList()) {
        AlwaysReflowSVGTextFrameDoForOneKid(kid);
      }
    } else {
      // This child is in a nondisplay context, something like:
      // <switch>
      //   ...
      //   <g><mask><text></text></mask></g>
      // </switch>
      // We should not call ReflowSVG on it.
      nsSVGContainerFrame::ReflowSVGNonDisplayText(aKid);
    }
  }
}

void nsSVGSwitchFrame::ReflowAllSVGTextFramesInsideNonActiveChildren(
    nsIFrame* aActiveChild) {
  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    if (aActiveChild == kid) {
      continue;
    }

    AlwaysReflowSVGTextFrameDoForOneKid(kid);
  }
}

void nsSVGSwitchFrame::ReflowSVG() {
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  MOZ_ASSERT(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
             "ReflowSVG mechanism not designed for this");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  // If the NS_FRAME_FIRST_REFLOW bit has been removed from our parent frame,
  // then our outer-<svg> has previously had its initial reflow. In that case
  // we need to make sure that that bit has been removed from ourself _before_
  // recursing over our children to ensure that they know too. Otherwise, we
  // need to remove it _after_ recursing over our children so that they know
  // the initial reflow is currently underway.

  bool isFirstReflow = (mState & NS_FRAME_FIRST_REFLOW);

  bool outerSVGHasHadFirstReflow =
      (GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW) == 0;

  if (outerSVGHasHadFirstReflow) {
    RemoveStateBits(NS_FRAME_FIRST_REFLOW);  // tell our children
  }

  nsOverflowAreas overflowRects;

  nsIFrame* child = GetActiveChildFrame();
  ReflowAllSVGTextFramesInsideNonActiveChildren(child);

  nsSVGDisplayableFrame* svgChild = do_QueryFrame(child);
  if (svgChild) {
    MOZ_ASSERT(!(child->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
               "Check for this explicitly in the |if|, then");
    svgChild->ReflowSVG();

    // We build up our child frame overflows here instead of using
    // nsLayoutUtils::UnionChildOverflow since SVG frame's all use the same
    // frame list, and we're iterating over that list now anyway.
    ConsiderChildOverflow(overflowRects, child);
  } else if (child && shouldReflowSVGTextFrameInside(child)) {
    MOZ_ASSERT(child->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY),
               "Check for this explicitly in the |if|, then");
    ReflowSVGNonDisplayText(child);
  }

  if (isFirstReflow) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    SVGObserverUtils::UpdateEffects(this);
  }

  FinishAndStoreOverflow(overflowRects, mRect.Size());

  // Remove state bits after FinishAndStoreOverflow so that it doesn't
  // invalidate on first reflow:
  RemoveStateBits(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                  NS_FRAME_HAS_DIRTY_CHILDREN);
}

SVGBBox nsSVGSwitchFrame::GetBBoxContribution(const Matrix& aToBBoxUserspace,
                                              uint32_t aFlags) {
  nsIFrame* kid = GetActiveChildFrame();
  nsSVGDisplayableFrame* svgKid = do_QueryFrame(kid);
  if (svgKid) {
    nsIContent* content = kid->GetContent();
    gfxMatrix transform = ThebesMatrix(aToBBoxUserspace);
    if (content->IsSVGElement()) {
      transform = static_cast<SVGElement*>(content)->PrependLocalTransformsTo(
                      {}, eChildToUserSpace) *
                  nsSVGUtils::GetTransformMatrixInUserSpace(kid) * transform;
    }
    return svgKid->GetBBoxContribution(ToMatrix(transform), aFlags);
  }
  return SVGBBox();
}

nsIFrame* nsSVGSwitchFrame::GetActiveChildFrame() {
  nsIContent* activeChild =
      static_cast<mozilla::dom::SVGSwitchElement*>(GetContent())
          ->GetActiveChild();

  if (activeChild) {
    for (nsIFrame* kid = mFrames.FirstChild(); kid;
         kid = kid->GetNextSibling()) {
      if (activeChild == kid->GetContent()) {
        return kid;
      }
    }
  }
  return nullptr;
}
