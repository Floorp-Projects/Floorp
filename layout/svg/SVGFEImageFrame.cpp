/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
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
  SVGFEImageFrame(nsStyleContext* aContext)
    : SVGFEImageFrameBase(aContext)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual void Init(nsIContent* aContent,
                    nsIFrame*   aParent,
                    nsIFrame*   aPrevInFlow) MOZ_OVERRIDE;
  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    return SVGFEImageFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGFEImage"), aResult);
  }
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFEImageFrame
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
    imageLoader->DecrementVisibleCount();
  }

  SVGFEImageFrameBase::DestroyFrom(aDestructRoot);
}

void
SVGFEImageFrame::Init(nsIContent* aContent,
                        nsIFrame* aParent,
                        nsIFrame* aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVG(nsGkAtoms::feImage),
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

NS_IMETHODIMP
SVGFEImageFrame::AttributeChanged(int32_t  aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t  aModType)
{
  SVGFEImageElement *element = static_cast<SVGFEImageElement*>(mContent);
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  }
  if (aNameSpaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {

    // Prevent setting image.src by exiting early
    if (nsContentUtils::IsImageSrcSetDisabled()) {
      return NS_OK;
    }

    if (element->mStringAttributes[SVGFEImageElement::HREF].IsExplicitlySet()) {
      element->LoadSVGImage(true, true);
    } else {
      element->CancelImageRequests(true);
    }
  }

  return SVGFEImageFrameBase::AttributeChanged(aNameSpaceID,
                                                 aAttribute, aModType);
}
