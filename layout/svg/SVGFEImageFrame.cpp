/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "nsContainerFrame.h"
#include "nsContentUtils.h"
#include "nsFrame.h"
#include "nsGkAtoms.h"
#include "nsLiteralString.h"
#include "nsSVGEffects.h"
#include "nsSVGFilters.h"
#include "mozilla/dom/SVGFEImageElement.h"

using namespace mozilla;
using namespace mozilla::dom;

class SVGFEImageFrame : public nsFrame
{
  friend nsIFrame*
  NS_NewSVGFEImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit SVGFEImageFrame(nsStyleContext* aContext)
    : nsFrame(aContext)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);

    // This frame isn't actually displayed, but it contains an image and we want
    // to use the nsImageLoadingContent machinery for managing images, which
    // requires visibility tracking, so we enable visibility tracking and
    // forcibly mark it visible below.
    EnableVisibilityTracking();
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGFEImage"), aResult);
  }
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFEImageFrame
   */
  virtual nsIAtom* GetType() const override;

  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t  aModType) override;

  void OnVisibilityChange(Visibility aOldVisibility,
                          Visibility aNewVisibility,
                          Maybe<OnNonvisible> aNonvisibleAction = Nothing()) override;

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override {
    // We don't maintain a visual overflow rect
    return false;
  }
};

nsIFrame*
NS_NewSVGFEImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGFEImageFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFEImageFrame)

/* virtual */ void
SVGFEImageFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  DecVisibilityCount(VisibilityCounter::IN_DISPLAYPORT);

  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(nsFrame::mContent);
  if (imageLoader) {
    imageLoader->FrameDestroyed(this);
  }

  nsFrame::DestroyFrom(aDestructRoot);
}

void
SVGFEImageFrame::Init(nsIContent*       aContent,
                      nsContainerFrame* aParent,
                      nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::feImage),
               "Trying to construct an SVGFEImageFrame for a "
               "content element that doesn't support the right interfaces");

  nsFrame::Init(aContent, aParent, aPrevInFlow);

  // We assume that feImage's are always visible.
  IncVisibilityCount(VisibilityCounter::IN_DISPLAYPORT);

  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(nsFrame::mContent);
  if (imageLoader) {
    imageLoader->FrameCreated(this);
  }
}

nsIAtom *
SVGFEImageFrame::GetType() const
{
  return nsGkAtoms::svgFEImageFrame;
}

nsresult
SVGFEImageFrame::AttributeChanged(int32_t  aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t  aModType)
{
  SVGFEImageElement *element = static_cast<SVGFEImageElement*>(mContent);
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    MOZ_ASSERT(GetParent()->GetType() == nsGkAtoms::svgFilterFrame,
               "Observers observe the filter, so that's what we must invalidate");
    nsSVGEffects::InvalidateDirectRenderingObservers(GetParent());
  }
  if (aNameSpaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {
    if (element->mStringAttributes[SVGFEImageElement::HREF].IsExplicitlySet()) {
      element->LoadSVGImage(true, true);
    } else {
      element->CancelImageRequests(true);
    }
  }

  return nsFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

void
SVGFEImageFrame::OnVisibilityChange(Visibility aOldVisibility,
                                    Visibility aNewVisibility,
                                    Maybe<OnNonvisible> aNonvisibleAction)
{
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(nsFrame::mContent);
  if (!imageLoader) {
    MOZ_ASSERT_UNREACHABLE("Should have an nsIImageLoadingContent");
    nsFrame::OnVisibilityChange(aOldVisibility, aNewVisibility,
                                aNonvisibleAction);
    return;
  }

  imageLoader->OnVisibilityChange(aOldVisibility, aNewVisibility,
                                  aNonvisibleAction);

  nsFrame::OnVisibilityChange(aOldVisibility, aNewVisibility,
                              aNonvisibleAction);
}
