/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Robert Longson
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Longson <longsonr@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Keep in (case-insensitive) order:
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsSVGEffects.h"
#include "nsSVGGFrame.h"
#include "nsSVGSwitchElement.h"
#include "nsSVGUtils.h"

class nsRenderingContext;

typedef nsSVGGFrame nsSVGSwitchFrameBase;

class nsSVGSwitchFrame : public nsSVGSwitchFrameBase
{
  friend nsIFrame*
  NS_NewSVGSwitchFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGSwitchFrame(nsStyleContext* aContext) :
    nsSVGSwitchFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgSwitchFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGSwitch"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsRenderingContext* aContext, const nsIntRect *aDirtyRect);
  NS_IMETHODIMP_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint);
  NS_IMETHODIMP_(nsRect) GetCoveredRegion();
  virtual void UpdateBounds();
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      PRUint32 aFlags);

private:
  nsIFrame *GetActiveChildFrame();
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGSwitchFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{  
  return new (aPresShell) nsSVGSwitchFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGSwitchFrame)

#ifdef DEBUG
NS_IMETHODIMP
nsSVGSwitchFrame::Init(nsIContent* aContent,
                       nsIFrame* aParent,
                       nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGSwitchElement> svgSwitch = do_QueryInterface(aContent);
  NS_ASSERTION(svgSwitch, "Content is not an SVG switch\n");

  return nsSVGSwitchFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGSwitchFrame::GetType() const
{
  return nsGkAtoms::svgSwitchFrame;
}

NS_IMETHODIMP
nsSVGSwitchFrame::PaintSVG(nsRenderingContext* aContext,
                           const nsIntRect *aDirtyRect)
{
  const nsStyleDisplay *display = mStyleContext->GetStyleDisplay();
  if (display->mOpacity == 0.0)
    return NS_OK;

  nsIFrame *kid = GetActiveChildFrame();
  if (kid) {
    nsSVGUtils::PaintFrameWithEffects(aContext, aDirtyRect, kid);
  }
  return NS_OK;
}


NS_IMETHODIMP_(nsIFrame*)
nsSVGSwitchFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  nsIFrame *kid = GetActiveChildFrame();
  if (kid) {
    nsISVGChildFrame* svgFrame = do_QueryFrame(kid);
    if (svgFrame) {
      return svgFrame->GetFrameForPoint(aPoint);
    }
  }

  return nsnull;
}

NS_IMETHODIMP_(nsRect)
nsSVGSwitchFrame::GetCoveredRegion()
{
  nsRect rect;

  nsIFrame *kid = GetActiveChildFrame();
  if (kid) {
    nsISVGChildFrame* child = do_QueryFrame(kid);
    if (child) {
      rect = child->GetCoveredRegion();
    }
  }
  return rect;
}

void
nsSVGSwitchFrame::UpdateBounds()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingUpdateBounds(this),
               "This call is probaby a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                    "UpdateBounds mechanism not designed for this");

  if (!nsSVGUtils::NeedsUpdatedBounds(this)) {
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

  nsIFrame *child = GetActiveChildFrame();
  if (child) {
    nsISVGChildFrame* svgChild = do_QueryFrame(child);
    if (svgChild) {
      NS_ABORT_IF_FALSE(!(child->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                        "Check for this explicitly in the |if|, then");
      svgChild->UpdateBounds();

      // We build up our child frame overflows here instead of using
      // nsLayoutUtils::UnionChildOverflow since SVG frame's all use the same
      // frame list, and we're iterating over that list now anyway.
      ConsiderChildOverflow(overflowRects, child);
    }
  }

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    nsSVGEffects::UpdateEffects(this);
  }

  FinishAndStoreOverflow(overflowRects, mRect.Size());

  // Remove state bits after FinishAndStoreOverflow so that it doesn't
  // invalidate on first reflow:
  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);

  // XXXSDL Make Invalidate() call nsSVGUtils::InvalidateBounds(this)
  // so that we invalidate under FinishAndStoreOverflow().
}

SVGBBox
nsSVGSwitchFrame::GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      PRUint32 aFlags)
{
  nsIFrame* kid = GetActiveChildFrame();
  if (kid) {
    nsISVGChildFrame* svgKid = do_QueryFrame(kid);
    if (svgKid) {
      nsIContent *content = kid->GetContent();
      gfxMatrix transform = aToBBoxUserspace;
      if (content->IsSVG()) {
        transform = static_cast<nsSVGElement*>(content)->
                      PrependLocalTransformsTo(aToBBoxUserspace);
      }
      return svgKid->GetBBoxContribution(transform, aFlags);
    }
  }
  return SVGBBox();
}

nsIFrame *
nsSVGSwitchFrame::GetActiveChildFrame()
{
  nsIContent *activeChild =
    static_cast<nsSVGSwitchElement*>(mContent)->GetActiveChild();

  if (activeChild) {
    for (nsIFrame* kid = mFrames.FirstChild(); kid;
         kid = kid->GetNextSibling()) {

      if (activeChild == kid->GetContent()) {
        return kid;
      }
    }
  }
  return nsnull;
}
