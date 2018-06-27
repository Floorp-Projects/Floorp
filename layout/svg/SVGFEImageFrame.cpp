/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "nsContainerFrame.h"
#include "nsContentUtils.h"
#include "nsFrame.h"
#include "nsGkAtoms.h"
#include "nsLiteralString.h"
#include "SVGObserverUtils.h"
#include "nsSVGFilters.h"
#include "mozilla/dom/SVGFEImageElement.h"
#include "mozilla/dom/MutationEventBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

class SVGFEImageFrame final : public nsFrame
{
  friend nsIFrame*
  NS_NewSVGFEImageFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle);
protected:
  explicit SVGFEImageFrame(ComputedStyle* aStyle)
    : nsFrame(aStyle, kClassID)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);

    // This frame isn't actually displayed, but it contains an image and we want
    // to use the nsImageLoadingContent machinery for managing images, which
    // requires visibility tracking, so we enable visibility tracking and
    // forcibly mark it visible below.
    EnableVisibilityTracking();
  }

public:
  NS_DECL_FRAMEARENA_HELPERS(SVGFEImageFrame)

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;

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

  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsAtom* aAttribute,
                                    int32_t  aModType) override;

  void OnVisibilityChange(Visibility aNewVisibility,
                          const Maybe<OnNonvisible>& aNonvisibleAction = Nothing()) override;

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override {
    // We don't maintain a visual overflow rect
    return false;
  }
};

nsIFrame*
NS_NewSVGFEImageFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) SVGFEImageFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFEImageFrame)

/* virtual */ void
SVGFEImageFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  DecApproximateVisibleCount();

  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(nsFrame::mContent);
  if (imageLoader) {
    imageLoader->FrameDestroyed(this);
  }

  nsFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
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
  // This call must happen before the FrameCreated. This is because the
  // primary frame pointer on our content node isn't set until after this
  // function ends, so there is no way for the resulting OnVisibilityChange
  // notification to get a frame. FrameCreated has a workaround for this in
  // that it passes our frame around so it can be accessed. OnVisibilityChange
  // doesn't have that workaround.
  IncApproximateVisibleCount();

  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(nsFrame::mContent);
  if (imageLoader) {
    imageLoader->FrameCreated(this);
  }
}

nsresult
SVGFEImageFrame::AttributeChanged(int32_t  aNameSpaceID,
                                  nsAtom* aAttribute,
                                  int32_t  aModType)
{
  SVGFEImageElement* element = static_cast<SVGFEImageElement*>(GetContent());
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    MOZ_ASSERT(GetParent()->IsSVGFilterFrame(),
               "Observers observe the filter, so that's what we must invalidate");
    SVGObserverUtils::InvalidateDirectRenderingObservers(GetParent());
  }

  // Currently our SMIL implementation does not modify the DOM attributes. Once
  // we implement the SVG 2 SMIL behaviour this can be removed
  // SVGFEImageElement::AfterSetAttr's implementation will be sufficient.
  if (aModType == MutationEvent_Binding::SMIL &&
      aAttribute == nsGkAtoms::href &&
      (aNameSpaceID == kNameSpaceID_XLink ||
       aNameSpaceID == kNameSpaceID_None)) {
    bool hrefIsSet =
      element->mStringAttributes[SVGFEImageElement::HREF].IsExplicitlySet() ||
      element->mStringAttributes[SVGFEImageElement::XLINK_HREF]
        .IsExplicitlySet();
    if (hrefIsSet) {
      element->LoadSVGImage(true, true);
    } else {
      element->CancelImageRequests(true);
    }
  }

  return nsFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

void
SVGFEImageFrame::OnVisibilityChange(Visibility aNewVisibility,
                                    const Maybe<OnNonvisible>& aNonvisibleAction)
{
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(nsFrame::mContent);
  if (!imageLoader) {
    MOZ_ASSERT_UNREACHABLE("Should have an nsIImageLoadingContent");
    nsFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
    return;
  }

  imageLoader->OnVisibilityChange(aNewVisibility, aNonvisibleAction);

  nsFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
}
