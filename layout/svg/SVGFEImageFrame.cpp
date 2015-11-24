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

typedef nsFrame SVGFEImageFrameBase;

class SVGFEImageFrame : public SVGFEImageFrameBase
{
  friend nsIFrame*
  NS_NewSVGFEImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit SVGFEImageFrame(nsStyleContext* aContext)
    : SVGFEImageFrameBase(aContext)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return SVGFEImageFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
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

  virtual bool UpdateOverflow() override {
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
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(SVGFEImageFrameBase::mContent);

  if (imageLoader) {
    imageLoader->FrameDestroyed(this);
    imageLoader
      ->DecrementVisibleCount(nsIImageLoadingContent::ON_NONVISIBLE_NO_ACTION);
  }

  SVGFEImageFrameBase::DestroyFrom(aDestructRoot);
}

void
SVGFEImageFrame::Init(nsIContent*       aContent,
                      nsContainerFrame* aParent,
                      nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::feImage),
               "Trying to construct an SVGFEImageFrame for a "
               "content element that doesn't support the right interfaces");

  SVGFEImageFrameBase::Init(aContent, aParent, aPrevInFlow);
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(SVGFEImageFrameBase::mContent);

  if (imageLoader) {
    // We assume that feImage's are always visible.
    // Increment the visible count before calling FrameCreated so that
    // FrameCreated will actually track the image correctly.
    imageLoader->IncrementVisibleCount();
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

  return SVGFEImageFrameBase::AttributeChanged(aNameSpaceID,
                                               aAttribute, aModType);
}
