/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "nsContainerFrame.h"
#include "nsFrame.h"
#include "nsGkAtoms.h"
#include "nsSVGEffects.h"
#include "nsSVGFilters.h"

class SVGFEUnstyledLeafFrame : public nsFrame
{
  friend nsIFrame*
  NS_NewSVGFEUnstyledLeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit SVGFEUnstyledLeafFrame(nsStyleContext* aContext)
    : nsFrame(aContext, FrameType::SVGFEUnstyledLeaf)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {}

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGFEUnstyledLeaf"), aResult);
  }
#endif

  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t  aModType) override;

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override {
    // We don't maintain a visual overflow rect
    return false;
  }
};

nsIFrame*
NS_NewSVGFEUnstyledLeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGFEUnstyledLeafFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFEUnstyledLeafFrame)

nsresult
SVGFEUnstyledLeafFrame::AttributeChanged(int32_t  aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         int32_t  aModType)
{
  SVGFEUnstyledElement *element = static_cast<SVGFEUnstyledElement*>(mContent);
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    MOZ_ASSERT(GetParent()->GetParent()->IsSVGFilterFrame(),
               "Observers observe the filter, so that's what we must invalidate");
    nsSVGEffects::InvalidateDirectRenderingObservers(GetParent()->GetParent());
  }

  return nsFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}
