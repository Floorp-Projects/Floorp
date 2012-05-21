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

using namespace mozilla;

typedef nsFrame SVGFEImageFrameBase;

class SVGFEImageFrame : public SVGFEImageFrameBase
{
  friend nsIFrame*
  NS_NewSVGFEImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  SVGFEImageFrame(nsStyleContext* aContext) : SVGFEImageFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame*   aParent,
                  nsIFrame*   aPrevInFlow);
  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return SVGFEImageFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGFEImage"), aResult);
  }
#endif

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFEImageFrame
   */
  virtual nsIAtom* GetType() const;

  NS_IMETHOD AttributeChanged(PRInt32  aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32  aModType);
};

nsIFrame*
NS_NewSVGFEImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGFEImageFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFEImageFrame)

/* virtual */ void
SVGFEImageFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  SVGFEImageFrameBase::DidSetStyleContext(aOldStyleContext);
  nsSVGEffects::InvalidateRenderingObservers(this);
}

/* virtual */ void
SVGFEImageFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(SVGFEImageFrameBase::mContent);

  if (imageLoader) {
    imageLoader->FrameDestroyed(this);
  }

  SVGFEImageFrameBase::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
SVGFEImageFrame::Init(nsIContent* aContent,
                        nsIFrame* aParent,
                        nsIFrame* aPrevInFlow)
{
#ifdef DEBUG
  nsCOMPtr<nsIDOMSVGFEImageElement> elem = do_QueryInterface(aContent);
  NS_ASSERTION(elem,
               "Trying to construct an SVGFEImageFrame for a "
               "content element that doesn't support the right interfaces");
#endif /* DEBUG */

  SVGFEImageFrameBase::Init(aContent, aParent, aPrevInFlow);
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(SVGFEImageFrameBase::mContent);

  if (imageLoader) {
    imageLoader->FrameCreated(this);
  }

  return NS_OK;
}

nsIAtom *
SVGFEImageFrame::GetType() const
{
  return nsGkAtoms::svgFEImageFrame;
}

NS_IMETHODIMP
SVGFEImageFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  PRInt32  aModType)
{
  nsSVGFEImageElement *element = static_cast<nsSVGFEImageElement*>(mContent);
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  }
  if (aNameSpaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {

    // Prevent setting image.src by exiting early
    if (nsContentUtils::IsImageSrcSetDisabled()) {
      return NS_OK;
    }

    if (element->mStringAttributes[nsSVGFEImageElement::HREF].IsExplicitlySet()) {
      element->LoadSVGImage(true, true);
    } else {
      element->CancelImageRequests(true);
    }
  }

  return SVGFEImageFrameBase::AttributeChanged(aNameSpaceID,
                                                 aAttribute, aModType);
}
