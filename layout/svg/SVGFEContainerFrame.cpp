/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsLiteralString.h"
#include "nsSVGEffects.h"
#include "nsSVGFilters.h"

typedef nsContainerFrame SVGFEContainerFrameBase;

/*
 * This frame is used by filter primitive elements that
 * have special child elements that provide parameters.
 */
class SVGFEContainerFrame : public SVGFEContainerFrameBase
{
  friend nsIFrame*
  NS_NewSVGFEContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit SVGFEContainerFrame(nsStyleContext* aContext)
    : SVGFEContainerFrameBase(aContext)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return SVGFEContainerFrameBase::IsFrameOfType(
            aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGContainer));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGFEContainer"), aResult);
  }
#endif

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;
#endif
  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFEContainerFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t  aModType) MOZ_OVERRIDE;

  virtual bool UpdateOverflow() MOZ_OVERRIDE {
    // We don't maintain a visual overflow rect
    return false;
  }
};

nsIFrame*
NS_NewSVGFEContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGFEContainerFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFEContainerFrame)

#ifdef DEBUG
void
SVGFEContainerFrame::Init(nsIContent*       aContent,
                          nsContainerFrame* aParent,
                          nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eFILTER),
               "Trying to construct an SVGFEContainerFrame for a "
               "content element that doesn't support the right interfaces");

  SVGFEContainerFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
SVGFEContainerFrame::GetType() const
{
  return nsGkAtoms::svgFEContainerFrame;
}

nsresult
SVGFEContainerFrame::AttributeChanged(int32_t  aNameSpaceID,
                                      nsIAtom* aAttribute,
                                      int32_t  aModType)
{
  nsSVGFE *element = static_cast<nsSVGFE*>(mContent);
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  }

  return SVGFEContainerFrameBase::AttributeChanged(aNameSpaceID,
                                                     aAttribute, aModType);
}
