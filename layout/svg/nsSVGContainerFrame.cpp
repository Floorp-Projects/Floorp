/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGContainerFrame.h"

// Keep others in (case-insensitive) order:
#include "DrawResult.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/RestyleManagerInlines.h"
#include "nsCSSFrameConstructor.h"
#include "SVGObserverUtils.h"
#include "nsSVGElement.h"
#include "nsSVGUtils.h"
#include "nsSVGAnimatedTransformList.h"
#include "SVGTextFrame.h"

using namespace mozilla;
using namespace mozilla::image;

NS_QUERYFRAME_HEAD(nsSVGContainerFrame)
  NS_QUERYFRAME_ENTRY(nsSVGContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_QUERYFRAME_HEAD(nsSVGDisplayContainerFrame)
  NS_QUERYFRAME_ENTRY(nsSVGDisplayContainerFrame)
  NS_QUERYFRAME_ENTRY(nsSVGDisplayableFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGContainerFrame)

nsIFrame*
NS_NewSVGContainerFrame(nsIPresShell* aPresShell,
                        nsStyleContext* aContext)
{
  nsIFrame* frame =
    new (aPresShell) nsSVGContainerFrame(aContext, nsSVGContainerFrame::kClassID);
  // If we were called directly, then the frame is for a <defs> or
  // an unknown element type. In both cases we prevent the content
  // from displaying directly.
  frame->AddStateBits(NS_FRAME_IS_NONDISPLAY);
  return frame;
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGContainerFrame)

void
nsSVGContainerFrame::AppendFrames(ChildListID  aListID,
                                  nsFrameList& aFrameList)
{
  InsertFrames(aListID, mFrames.LastChild(), aFrameList);
}

void
nsSVGContainerFrame::InsertFrames(ChildListID aListID,
                                  nsIFrame* aPrevFrame,
                                  nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  mFrames.InsertFrames(this, aPrevFrame, aFrameList);
}

void
nsSVGContainerFrame::RemoveFrame(ChildListID aListID,
                                 nsIFrame* aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  mFrames.DestroyFrame(aOldFrame);
}

bool
nsSVGContainerFrame::ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas)
{
  if (mState & NS_FRAME_IS_NONDISPLAY) {
    // We don't maintain overflow rects.
    // XXX It would have be better if the restyle request hadn't even happened.
    return false;
  }
  return nsContainerFrame::ComputeCustomOverflow(aOverflowAreas);
}

/**
 * Traverses a frame tree, marking any SVGTextFrame frames as dirty
 * and calling InvalidateRenderingObservers() on it.
 *
 * The reason that this helper exists is because SVGTextFrame is special.
 * None of the other SVG frames ever need to be reflowed when they have the
 * NS_FRAME_IS_NONDISPLAY bit set on them because their PaintSVG methods
 * (and those of any containers that they can validly be contained within) do
 * not make use of mRect or overflow rects. "em" lengths, etc., are resolved
 * as those elements are painted.
 *
 * SVGTextFrame is different because its anonymous block and inline frames
 * need to be reflowed in order to get the correct metrics when things like
 * inherited font-size of an ancestor changes, or a delayed webfont loads and
 * applies.
 *
 * However, we only need to do this work if we were reflowed with
 * NS_FRAME_IS_DIRTY, which implies that all descendants are dirty.  When
 * that reflow reaches an NS_FRAME_IS_NONDISPLAY frame it would normally
 * stop, but this helper looks for any SVGTextFrame descendants of such
 * frames and marks them NS_FRAME_IS_DIRTY so that the next time that they
 * are painted their anonymous kid will first get the necessary reflow.
 */
/* static */ void
nsSVGContainerFrame::ReflowSVGNonDisplayText(nsIFrame* aContainer)
{
  if (!(aContainer->GetStateBits() & NS_FRAME_IS_DIRTY)) {
    return;
  }
  NS_ASSERTION((aContainer->GetStateBits() & NS_FRAME_IS_NONDISPLAY) ||
               !aContainer->IsFrameOfType(nsIFrame::eSVG),
               "it is wasteful to call ReflowSVGNonDisplayText on a container "
               "frame that is not NS_FRAME_IS_NONDISPLAY");
  for (nsIFrame* kid : aContainer->PrincipalChildList()) {
    LayoutFrameType type = kid->Type();
    if (type == LayoutFrameType::SVGText) {
      static_cast<SVGTextFrame*>(kid)->ReflowSVGNonDisplayText();
    } else {
      if (kid->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer) ||
          type == LayoutFrameType::SVGForeignObject ||
          !kid->IsFrameOfType(nsIFrame::eSVG)) {
        ReflowSVGNonDisplayText(kid);
      }
    }
  }
}

void
nsSVGDisplayContainerFrame::Init(nsIContent*       aContent,
                                 nsContainerFrame* aParent,
                                 nsIFrame*         aPrevInFlow)
{
  if (!(GetStateBits() & NS_STATE_IS_OUTER_SVG)) {
    AddStateBits(aParent->GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD);
  }
  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

void
nsSVGDisplayContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                             const nsDisplayListSet& aLists)
{
  // mContent could be a XUL element so check for an SVG element before casting
  if (mContent->IsSVGElement() &&
      !static_cast<const nsSVGElement*>(GetContent())->HasValidDimensions()) {
    return;
  }
  DisplayOutline(aBuilder, aLists);
  return BuildDisplayListForNonBlockChildren(aBuilder, aLists);
}

void
nsSVGDisplayContainerFrame::InsertFrames(ChildListID aListID,
                                         nsIFrame* aPrevFrame,
                                         nsFrameList& aFrameList)
{
  // memorize first old frame after insertion point
  // XXXbz once again, this would work a lot better if the nsIFrame
  // methods returned framelist iterators....
  nsIFrame* nextFrame = aPrevFrame ?
    aPrevFrame->GetNextSibling() : GetChildList(aListID).FirstChild();
  nsIFrame* firstNewFrame = aFrameList.FirstChild();

  // Insert the new frames
  nsSVGContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);

  // If we are not a non-display SVG frame and we do not have a bounds update
  // pending, then we need to schedule one for our new children:
  if (!(GetStateBits() &
        (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN |
         NS_FRAME_IS_NONDISPLAY))) {
    for (nsIFrame* kid = firstNewFrame; kid != nextFrame;
         kid = kid->GetNextSibling()) {
      nsSVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
      if (SVGFrame) {
        MOZ_ASSERT(!(kid->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                   "Check for this explicitly in the |if|, then");
        bool isFirstReflow = (kid->GetStateBits() & NS_FRAME_FIRST_REFLOW);
        // Remove bits so that ScheduleBoundsUpdate will work:
        kid->RemoveStateBits(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                             NS_FRAME_HAS_DIRTY_CHILDREN);
        // No need to invalidate the new kid's old bounds, so we just use
        // nsSVGUtils::ScheduleBoundsUpdate.
        nsSVGUtils::ScheduleReflowSVG(kid);
        if (isFirstReflow) {
          // Add back the NS_FRAME_FIRST_REFLOW bit:
          kid->AddStateBits(NS_FRAME_FIRST_REFLOW);
        }
      }
    }
  }
}

void
nsSVGDisplayContainerFrame::RemoveFrame(ChildListID aListID,
                                        nsIFrame* aOldFrame)
{
  SVGObserverUtils::InvalidateRenderingObservers(aOldFrame);

  // nsSVGContainerFrame::RemoveFrame doesn't call down into
  // nsContainerFrame::RemoveFrame, so it doesn't call FrameNeedsReflow. We
  // need to schedule a repaint and schedule an update to our overflow rects.
  SchedulePaint();
  PresContext()->RestyleManager()->PostRestyleEvent(
    mContent->AsElement(), nsRestyleHint(0), nsChangeHint_UpdateOverflow);

  nsSVGContainerFrame::RemoveFrame(aListID, aOldFrame);

  if (!(GetStateBits() & (NS_FRAME_IS_NONDISPLAY | NS_STATE_IS_OUTER_SVG))) {
    nsSVGUtils::NotifyAncestorsOfFilterRegionChange(this);
  }
}

bool
nsSVGDisplayContainerFrame::IsSVGTransformed(gfx::Matrix *aOwnTransform,
                                             gfx::Matrix *aFromParentTransform) const
{
  bool foundTransform = false;

  // Check if our parent has children-only transforms:
  nsIFrame *parent = GetParent();
  if (parent &&
      parent->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer)) {
    foundTransform = static_cast<nsSVGContainerFrame*>(parent)->
                       HasChildrenOnlyTransform(aFromParentTransform);
  }

  // mContent could be a XUL element so check for an SVG element before casting
  if (mContent->IsSVGElement()) {
    nsSVGElement *content = static_cast<nsSVGElement*>(GetContent());
    nsSVGAnimatedTransformList* transformList =
      content->GetAnimatedTransformList();
    if ((transformList && transformList->HasTransform()) ||
        content->GetAnimateMotionTransform()) {
      if (aOwnTransform) {
        *aOwnTransform = gfx::ToMatrix(
                           content->PrependLocalTransformsTo(
                             gfxMatrix(), eUserSpaceToParent));
      }
      foundTransform = true;
    }
  }
  return foundTransform;
}

//----------------------------------------------------------------------
// nsSVGDisplayableFrame methods

void
nsSVGDisplayContainerFrame::PaintSVG(gfxContext& aContext,
                                     const gfxMatrix& aTransform,
                                     imgDrawingParams& aImgParams,
                                     const nsIntRect *aDirtyRect)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY) ||
               PresContext()->IsGlyph(),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  if (StyleEffects()->mOpacity == 0.0) {
    return;
  }

  gfxMatrix matrix = aTransform;
  if (GetContent()->IsSVGElement()) { // must check before cast
    matrix = static_cast<const nsSVGElement*>(GetContent())->
               PrependLocalTransformsTo(matrix, eChildToUserSpace);
    if (matrix.IsSingular()) {
      return;
    }
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    gfxMatrix m = matrix;
    // PaintFrameWithEffects() expects the transform that is passed to it to
    // include the transform to the passed frame's user space, so add it:
    const nsIContent* content = kid->GetContent();
    if (content->IsSVGElement()) { // must check before cast
      const nsSVGElement* element = static_cast<const nsSVGElement*>(content);
      if (!element->HasValidDimensions()) {
        continue; // nothing to paint for kid
      }
      m = element->PrependLocalTransformsTo(m, eUserSpaceToParent);
      if (m.IsSingular()) {
        continue;
      }
    }
    nsSVGUtils::PaintFrameWithEffects(kid, aContext, m, aImgParams, aDirtyRect);
  }
}

nsIFrame*
nsSVGDisplayContainerFrame::GetFrameForPoint(const gfxPoint& aPoint)
{
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only hit-testing of a "
               "clipPath's contents should take this code path");
  return nsSVGUtils::HitTestChildren(this, aPoint);
}

void
nsSVGDisplayContainerFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  MOZ_ASSERT(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
             "ReflowSVG mechanism not designed for this");

  MOZ_ASSERT(!IsSVGOuterSVGFrame(), "Do not call on outer-<svg>");

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
    RemoveStateBits(NS_FRAME_FIRST_REFLOW); // tell our children
  }

  nsOverflowAreas overflowRects;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      MOZ_ASSERT(!(kid->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                 "Check for this explicitly in the |if|, then");
      kid->AddStateBits(mState & NS_FRAME_IS_DIRTY);
      SVGFrame->ReflowSVG();

      // We build up our child frame overflows here instead of using
      // nsLayoutUtils::UnionChildOverflow since SVG frame's all use the same
      // frame list, and we're iterating over that list now anyway.
      ConsiderChildOverflow(overflowRects, kid);
    } else {
      // Inside a non-display container frame, we might have some
      // SVGTextFrames.  We need to cause those to get reflowed in
      // case they are the target of a rendering observer.
      NS_ASSERTION(kid->GetStateBits() & NS_FRAME_IS_NONDISPLAY,
                   "expected kid to be a NS_FRAME_IS_NONDISPLAY frame");
      if (kid->GetStateBits() & NS_FRAME_IS_DIRTY) {
        nsSVGContainerFrame* container = do_QueryFrame(kid);
        if (container && container->GetContent()->IsSVGElement()) {
          ReflowSVGNonDisplayText(container);
        }
      }
    }
  }

  // <svg> can create an SVG viewport with an offset due to its
  // x/y/width/height attributes, and <use> can introduce an offset with an
  // empty mRect (any width/height is copied to an anonymous <svg> child).
  // Other than that containers should not set mRect since all other offsets
  // come from transforms, which are accounted for by nsDisplayTransform.
  // Note that we rely on |overflow:visible| to allow display list items to be
  // created for our children.
  MOZ_ASSERT(mContent->IsAnyOfSVGElements(nsGkAtoms::svg, nsGkAtoms::symbol) ||
             (mContent->IsSVGElement(nsGkAtoms::use) &&
              mRect.Size() == nsSize(0,0)) ||
             mRect.IsEqualEdges(nsRect()),
             "Only inner-<svg>/<use> is expected to have mRect set");

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

void
nsSVGDisplayContainerFrame::NotifySVGChanged(uint32_t aFlags)
{
  MOZ_ASSERT(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
             "Invalidation logic may need adjusting");

  if (aFlags & TRANSFORM_CHANGED) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nullptr;
  }

  nsSVGUtils::NotifyChildrenOfSVGChange(this, aFlags);
}

SVGBBox
nsSVGDisplayContainerFrame::GetBBoxContribution(
  const Matrix &aToBBoxUserspace,
  uint32_t aFlags)
{
  SVGBBox bboxUnion;

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsIContent *content = kid->GetContent();
    nsSVGDisplayableFrame* svgKid = do_QueryFrame(kid);
    // content could be a XUL element so check for an SVG element before casting
    if (svgKid && (!content->IsSVGElement() ||
                   static_cast<const nsSVGElement*>(content)->HasValidDimensions())) {

      gfxMatrix transform = gfx::ThebesMatrix(aToBBoxUserspace);
      if (content->IsSVGElement()) {
        transform = static_cast<nsSVGElement*>(content)->
                      PrependLocalTransformsTo(transform);
      }
      // We need to include zero width/height vertical/horizontal lines, so we have
      // to use UnionEdges.
      bboxUnion.UnionEdges(svgKid->GetBBoxContribution(gfx::ToMatrix(transform), aFlags));
    }
    kid = kid->GetNextSibling();
  }

  return bboxUnion;
}

gfxMatrix
nsSVGDisplayContainerFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    NS_ASSERTION(GetParent(), "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(GetParent());
    nsSVGElement *content = static_cast<nsSVGElement*>(GetContent());

    gfxMatrix tm = content->PrependLocalTransformsTo(parent->GetCanvasTM());

    mCanvasTM = new gfxMatrix(tm);
  }

  return *mCanvasTM;
}
