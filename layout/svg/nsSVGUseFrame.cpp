/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGUseFrame.h"
#include "nsContentUtils.h"

#include "mozilla/dom/SVGUseElement.h"
#include "SVGObserverUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGUseFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) nsSVGUseFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGUseFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

void
nsSVGUseFrame::Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::use),
               "Content is not an SVG use!");

  mHasValidDimensions =
    static_cast<SVGUseElement*>(aContent)->HasValidDimensions();

  nsSVGGFrame::Init(aContent, aParent, aPrevInFlow);
}

nsresult
nsSVGUseFrame::AttributeChanged(int32_t aNameSpaceID,
                                nsAtom* aAttribute,
                                int32_t aModType)
{
  auto* useElement = static_cast<SVGUseElement*>(GetContent());

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nullptr;
      nsLayoutUtils::PostRestyleEvent(
        useElement, nsRestyleHint(0),
        nsChangeHint_InvalidateRenderingObservers);
      nsSVGUtils::ScheduleReflowSVG(this);
      nsSVGUtils::NotifyChildrenOfSVGChange(this, TRANSFORM_CHANGED);
    } else if (aAttribute == nsGkAtoms::width ||
               aAttribute == nsGkAtoms::height) {
      bool invalidate = false;
      if (mHasValidDimensions != useElement->HasValidDimensions()) {
        mHasValidDimensions = !mHasValidDimensions;
        invalidate = true;
      }

      // FIXME(emilio): This shouldn't really belong to nsSVGUseFrame, but
      // SVGUseElement.
      if (useElement->OurWidthAndHeightAreUsed()) {
        invalidate = true;
        useElement->SyncWidthOrHeight(aAttribute);
      }

      if (invalidate) {
        nsLayoutUtils::PostRestyleEvent(
          useElement, nsRestyleHint(0),
          nsChangeHint_InvalidateRenderingObservers);
        nsSVGUtils::ScheduleReflowSVG(this);
      }
    }
  }

  if ((aNameSpaceID == kNameSpaceID_XLink ||
       aNameSpaceID == kNameSpaceID_None) &&
      aAttribute == nsGkAtoms::href) {
    // we're changing our nature, clear out the clone information
    nsLayoutUtils::PostRestyleEvent(
      useElement, nsRestyleHint(0),
      nsChangeHint_InvalidateRenderingObservers);
    nsSVGUtils::ScheduleReflowSVG(this);
    useElement->mOriginal = nullptr;
    useElement->UnlinkSource();
    useElement->TriggerReclone();
  }

  return nsSVGGFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

//----------------------------------------------------------------------
// nsSVGDisplayableFrame methods

void
nsSVGUseFrame::ReflowSVG()
{
  // We only handle x/y offset here, since any width/height that is in force is
  // handled by the nsSVGOuterSVGFrame for the anonymous <svg> that will be
  // created for that purpose.
  float x, y;
  static_cast<SVGUseElement*>(GetContent())->
    GetAnimatedLengthValues(&x, &y, nullptr);
  mRect.MoveTo(nsLayoutUtils::RoundGfxRectToAppRect(
                 gfxRect(x, y, 0.0, 0.0),
                 PresContext()->AppUnitsPerCSSPixel()).TopLeft());

  // If we have a filter, we need to invalidate ourselves because filter
  // output can change even if none of our descendants need repainting.
  if (StyleEffects()->HasFilters()) {
    InvalidateFrame();
  }

  nsSVGGFrame::ReflowSVG();
}

void
nsSVGUseFrame::NotifySVGChanged(uint32_t aFlags)
{
  if (aFlags & COORD_CONTEXT_CHANGED &&
      !(aFlags & TRANSFORM_CHANGED)) {
    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y'
    SVGUseElement *use = static_cast<SVGUseElement*>(GetContent());
    if (use->mLengthAttributes[SVGUseElement::ATTR_X].IsPercentage() ||
        use->mLengthAttributes[SVGUseElement::ATTR_Y].IsPercentage()) {
      aFlags |= TRANSFORM_CHANGED;
      // Ancestor changes can't affect how we render from the perspective of
      // any rendering observers that we may have, so we don't need to
      // invalidate them. We also don't need to invalidate ourself, since our
      // changed ancestor will have invalidated its entire area, which includes
      // our area.
      // For perf reasons we call this before calling NotifySVGChanged() below.
      nsSVGUtils::ScheduleReflowSVG(this);
    }
  }

  // We don't remove the TRANSFORM_CHANGED flag here if we have a viewBox or
  // non-percentage width/height, since if they're set then they are cloned to
  // an anonymous child <svg>, and its nsSVGInnerSVGFrame will do that.

  nsSVGGFrame::NotifySVGChanged(aFlags);
}
