/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "nsFrame.h"
#include "nsGkAtoms.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGSVGElement.h"
#include "nsSVGViewElement.h"

typedef nsFrame SVGViewFrameBase;

/**
 * While views are not directly rendered in SVG they can be linked to
 * and thereby override attributes of an <svg> element via a fragment
 * identifier. The SVGViewFrame class passes on any attribute changes
 * the view receives to the overridden <svg> element (if there is one).
 **/
class SVGViewFrame : public SVGViewFrameBase
{
  friend nsIFrame*
  NS_NewSVGViewFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  SVGViewFrame(nsStyleContext* aContext)
    : SVGViewFrameBase(aContext)
  {
    AddStateBits(NS_STATE_SVG_NONDISPLAY_CHILD);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame*   aParent,
                  nsIFrame*   aPrevInFlow);
#endif

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    return SVGViewFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGView"), aResult);
  }
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFELeafFrame
   */
  virtual nsIAtom* GetType() const;

  NS_IMETHOD AttributeChanged(int32_t  aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t  aModType);

  virtual bool UpdateOverflow() {
    // We don't maintain a visual overflow rect
    return false;
  }
};

nsIFrame*
NS_NewSVGViewFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGViewFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGViewFrame)

#ifdef DEBUG
NS_IMETHODIMP
SVGViewFrame::Init(nsIContent* aContent,
                   nsIFrame* aParent,
                   nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGViewElement> elem = do_QueryInterface(aContent);
  NS_ASSERTION(elem, "Content is not an SVG view");

  return SVGViewFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
SVGViewFrame::GetType() const
{
  return nsGkAtoms::svgViewFrame;
}

NS_IMETHODIMP
SVGViewFrame::AttributeChanged(int32_t  aNameSpaceID,
                               nsIAtom* aAttribute,
                               int32_t  aModType)
{
  // Ignore zoomAndPan as it does not cause the <svg> element to re-render
    
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::preserveAspectRatio ||
       aAttribute == nsGkAtoms::viewBox ||
       aAttribute == nsGkAtoms::viewTarget)) {

    nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
    NS_ASSERTION(outerSVGFrame->GetContent()->IsSVG(nsGkAtoms::svg),
                 "Expecting an <svg> element");

    nsSVGSVGElement* svgElement =
      static_cast<nsSVGSVGElement*>(outerSVGFrame->GetContent());

    nsAutoString viewID;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, viewID);

    if (svgElement->IsOverriddenBy(viewID)) {
      // We're the view that's providing overrides, so pretend that the frame
      // we're overriding was updated.
      outerSVGFrame->AttributeChanged(aNameSpaceID, aAttribute, aModType);
    }
  }

  return SVGViewFrameBase::AttributeChanged(aNameSpaceID,
                                            aAttribute, aModType);
}
