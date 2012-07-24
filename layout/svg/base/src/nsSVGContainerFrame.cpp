/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGContainerFrame.h"

// Keep others in (case-insensitive) order:
#include "nsSVGEffects.h"
#include "nsSVGElement.h"
#include "nsSVGUtils.h"
#include "SVGAnimatedTransformList.h"

using namespace mozilla;

NS_QUERYFRAME_HEAD(nsSVGContainerFrame)
  NS_QUERYFRAME_ENTRY(nsSVGContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGContainerFrameBase)

NS_QUERYFRAME_HEAD(nsSVGDisplayContainerFrame)
  NS_QUERYFRAME_ENTRY(nsSVGDisplayContainerFrame)
  NS_QUERYFRAME_ENTRY(nsISVGChildFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGContainerFrame)

nsIFrame*
NS_NewSVGContainerFrame(nsIPresShell* aPresShell,
                        nsStyleContext* aContext)
{
  nsIFrame *frame = new (aPresShell) nsSVGContainerFrame(aContext);
  // If we were called directly, then the frame is for a <defs> or
  // an unknown element type. In both cases we prevent the content
  // from displaying directly.
  frame->AddStateBits(NS_STATE_SVG_NONDISPLAY_CHILD);
  return frame;
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGContainerFrame)
NS_IMPL_FRAMEARENA_HELPERS(nsSVGDisplayContainerFrame)

NS_IMETHODIMP
nsSVGContainerFrame::AppendFrames(ChildListID  aListID,
                                  nsFrameList& aFrameList)
{
  return InsertFrames(aListID, mFrames.LastChild(), aFrameList);  
}

NS_IMETHODIMP
nsSVGContainerFrame::InsertFrames(ChildListID aListID,
                                  nsIFrame* aPrevFrame,
                                  nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  mFrames.InsertFrames(this, aPrevFrame, aFrameList);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGContainerFrame::RemoveFrame(ChildListID aListID,
                                 nsIFrame* aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  mFrames.DestroyFrame(aOldFrame);
  return NS_OK;
}

bool
nsSVGContainerFrame::UpdateOverflow()
{
  if (mState & NS_STATE_SVG_NONDISPLAY_CHILD) {
    // We don't maintain overflow rects.
    // XXX It would have be better if the restyle request hadn't even happened.
    return false;
  }
  return nsSVGContainerFrameBase::UpdateOverflow();
}

NS_IMETHODIMP
nsSVGDisplayContainerFrame::Init(nsIContent* aContent,
                                 nsIFrame* aParent,
                                 nsIFrame* aPrevInFlow)
{
  if (!(GetStateBits() & NS_STATE_IS_OUTER_SVG)) {
    AddStateBits(aParent->GetStateBits() &
      (NS_STATE_SVG_NONDISPLAY_CHILD | NS_STATE_SVG_CLIPPATH_CHILD));
  }
  nsresult rv = nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
  return rv;
}

NS_IMETHODIMP
nsSVGDisplayContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                             const nsRect&           aDirtyRect,
                                             const nsDisplayListSet& aLists)
{
  if (!static_cast<const nsSVGElement*>(mContent)->HasValidDimensions()) {
    return NS_OK;
  }
  return BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsSVGDisplayContainerFrame::InsertFrames(ChildListID aListID,
                                         nsIFrame* aPrevFrame,
                                         nsFrameList& aFrameList)
{
  // memorize first old frame after insertion point
  // XXXbz once again, this would work a lot better if the nsIFrame
  // methods returned framelist iterators....
  nsIFrame* firstOldFrame = aPrevFrame ?
    aPrevFrame->GetNextSibling() : GetChildList(aListID).FirstChild();
  nsIFrame* firstNewFrame = aFrameList.FirstChild();
  
  // Insert the new frames
  nsSVGContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);

  // If we are not a non-display SVG frame and we do not have a bounds update
  // pending, then we need to schedule one for our new children:
  if (!(GetStateBits() &
        (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN |
         NS_STATE_SVG_NONDISPLAY_CHILD))) {
    for (nsIFrame* kid = firstNewFrame; kid != firstOldFrame;
         kid = kid->GetNextSibling()) {
      nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
      if (SVGFrame) {
        NS_ABORT_IF_FALSE(!(kid->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
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

  return NS_OK;
}

NS_IMETHODIMP
nsSVGDisplayContainerFrame::RemoveFrame(ChildListID aListID,
                                        nsIFrame* aOldFrame)
{
  nsSVGUtils::InvalidateBounds(aOldFrame);

  nsresult rv = nsSVGContainerFrame::RemoveFrame(aListID, aOldFrame);

  if (!(GetStateBits() & (NS_STATE_SVG_NONDISPLAY_CHILD | NS_STATE_IS_OUTER_SVG))) {
    nsSVGUtils::NotifyAncestorsOfFilterRegionChange(this);
  }

  return rv;
}

bool
nsSVGDisplayContainerFrame::IsSVGTransformed(gfxMatrix *aOwnTransform,
                                             gfxMatrix *aFromParentTransform) const
{
  bool foundTransform = false;

  // Check if our parent has children-only transforms:
  nsIFrame *parent = GetParent();
  if (parent &&
      parent->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer)) {
    foundTransform = static_cast<nsSVGContainerFrame*>(parent)->
                       HasChildrenOnlyTransform(aFromParentTransform);
  }

  nsSVGElement *content = static_cast<nsSVGElement*>(mContent);
  if (content->GetAnimatedTransformList() ||
      content->GetAnimateMotionTransform()) {
    if (aOwnTransform) {
      *aOwnTransform = content->PrependLocalTransformsTo(gfxMatrix(),
                                  nsSVGElement::eUserSpaceToParent);
    }
    foundTransform = true;
  }
  return foundTransform;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGDisplayContainerFrame::PaintSVG(nsRenderingContext* aContext,
                                     const nsIntRect *aDirtyRect)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (mState & NS_STATE_SVG_NONDISPLAY_CHILD),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  const nsStyleDisplay *display = mStyleContext->GetStyleDisplay();
  if (display->mOpacity == 0.0)
    return NS_OK;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintFrameWithEffects(aContext, aDirtyRect, kid);
  }

  return NS_OK;
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGDisplayContainerFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
               (mState & NS_STATE_SVG_NONDISPLAY_CHILD),
               "If display lists are enabled, only hit-testing of a "
               "clipPath's contents should take this code path");
  return nsSVGUtils::HitTestChildren(this, aPoint);
}

NS_IMETHODIMP_(nsRect)
nsSVGDisplayContainerFrame::GetCoveredRegion()
{
  return nsSVGUtils::GetCoveredRegion(mFrames);
}

void
nsSVGDisplayContainerFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                    "ReflowSVG mechanism not designed for this");

  NS_ABORT_IF_FALSE(GetType() != nsGkAtoms::svgOuterSVGFrame,
                    "Do not call on outer-<svg>");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  // If the NS_FRAME_FIRST_REFLOW bit has been removed from our parent frame,
  // then our outer-<svg> has previously had its initial reflow. In that case
  // we need to make sure that that bit has been removed from ourself _before_
  // recursing over our children to ensure that they know too. Otherwise, we
  // need to remove it _after_ recursing over our children so that they know
  // the initial reflow is currently underway.

  bool outerSVGHasHadFirstReflow =
    (GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW) == 0;

  if (outerSVGHasHadFirstReflow) {
    mState &= ~NS_FRAME_FIRST_REFLOW; // tell our children
  }

  nsOverflowAreas overflowRects;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      NS_ABORT_IF_FALSE(!(kid->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                        "Check for this explicitly in the |if|, then");
      SVGFrame->ReflowSVG();

      // We build up our child frame overflows here instead of using
      // nsLayoutUtils::UnionChildOverflow since SVG frame's all use the same
      // frame list, and we're iterating over that list now anyway.
      ConsiderChildOverflow(overflowRects, kid);
    }
  }

  // <svg> can create an SVG viewport with an offset due to its
  // x/y/width/height attributes, and <use> can introduce an offset with an
  // empty mRect (any width/height is copied to an anonymous <svg> child).
  // Other than that containers should not set mRect since all other offsets
  // come from transforms, which are accounted for by nsDisplayTransform.
  // Note that we rely on |overflow:visible| to allow display list items to be
  // created for our children.
  NS_ABORT_IF_FALSE(mContent->Tag() == nsGkAtoms::svg ||
                    (mContent->Tag() == nsGkAtoms::use &&
                     mRect.Size() == nsSize(0,0)) ||
                    mRect.IsEqualEdges(nsRect()),
                    "Only inner-<svg>/<use> is expected to have mRect set");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    nsSVGEffects::UpdateEffects(this);
  }

  // We only invalidate if we are dirty, if our outer-<svg> has already had its
  // initial reflow (since if it hasn't, its entire area will be invalidated
  // when it gets that initial reflow), and if our parent is not dirty (since
  // if it is, then it will invalidate its entire new area, which will include
  // our new area).
  bool invalidate = (mState & NS_FRAME_IS_DIRTY) &&
    !(GetParent()->GetStateBits() &
       (NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY));

  FinishAndStoreOverflow(overflowRects, mRect.Size());

  // Remove state bits after FinishAndStoreOverflow so that it doesn't
  // invalidate on first reflow:
  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);

  if (invalidate) {
    // XXXSDL Let FinishAndStoreOverflow do this.
    nsSVGUtils::InvalidateBounds(this, true);
  }
}  

void
nsSVGDisplayContainerFrame::NotifySVGChanged(PRUint32 aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  nsSVGUtils::NotifyChildrenOfSVGChange(this, aFlags);
}

SVGBBox
nsSVGDisplayContainerFrame::GetBBoxContribution(
  const gfxMatrix &aToBBoxUserspace,
  PRUint32 aFlags)
{
  SVGBBox bboxUnion;

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* svgKid = do_QueryFrame(kid);
    if (svgKid) {
      gfxMatrix transform = aToBBoxUserspace;
      nsIContent *content = kid->GetContent();
      if (content->IsSVG()) {
        transform = static_cast<nsSVGElement*>(content)->
                      PrependLocalTransformsTo(aToBBoxUserspace);
      }
      // We need to include zero width/height vertical/horizontal lines, so we have
      // to use UnionEdges.
      bboxUnion.UnionEdges(svgKid->GetBBoxContribution(transform, aFlags));
    }
    kid = kid->GetNextSibling();
  }

  return bboxUnion;
}
