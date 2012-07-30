/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "gfxMatrix.h"
#include "nsSVGAElement.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGTSpanFrame.h"
#include "nsSVGUtils.h"
#include "SVGLengthList.h"

// <a> elements can contain text. nsSVGGlyphFrames expect to have
// a class derived from nsSVGTextContainerFrame as a parent. We
// also need something that implements nsISVGGlyphFragmentNode to get
// the text DOM to work.

using namespace mozilla;

typedef nsSVGTSpanFrame nsSVGAFrameBase;

class nsSVGAFrame : public nsSVGAFrameBase
{
  friend nsIFrame*
  NS_NewSVGAFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGAFrame(nsStyleContext* aContext) :
    nsSVGAFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  // nsIFrame:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgAFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGA"), aResult);
  }
#endif
  // nsISVGChildFrame interface:
  virtual void NotifySVGChanged(PRUint32 aFlags);
  
  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM(PRUint32 aFor);

  // nsSVGTextContainerFrame methods:
  virtual void GetXY(mozilla::SVGUserUnitList *aX, mozilla::SVGUserUnitList *aY);
  virtual void GetDxDy(mozilla::SVGUserUnitList *aDx, mozilla::SVGUserUnitList *aDy);
  virtual const SVGNumberList* GetRotate() {
    return nullptr;
  }

private:
  nsAutoPtr<gfxMatrix> mCanvasTM;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGAFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGAFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGAFrame)

//----------------------------------------------------------------------
// nsIFrame methods
#ifdef DEBUG
NS_IMETHODIMP
nsSVGAFrame::Init(nsIContent* aContent,
                  nsIFrame* aParent,
                  nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGAElement> elem = do_QueryInterface(aContent);
  NS_ASSERTION(elem,
               "Trying to construct an SVGAFrame for a "
               "content element that doesn't support the right interfaces");

  return nsSVGAFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

NS_IMETHODIMP
nsSVGAFrame::AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::transform) {
    nsSVGUtils::InvalidateAndScheduleReflowSVG(this);
    NotifySVGChanged(TRANSFORM_CHANGED);
  }

 return NS_OK;
}

nsIAtom *
nsSVGAFrame::GetType() const
{
  return nsGkAtoms::svgAFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

void
nsSVGAFrame::NotifySVGChanged(PRUint32 aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  if (aFlags & TRANSFORM_CHANGED) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nullptr;
  }

  nsSVGAFrameBase::NotifySVGChanged(aFlags);
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGAFrame::GetCanvasTM(PRUint32 aFor)
{
  if (!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
    if ((aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
  }
  if (!mCanvasTM) {
    NS_ASSERTION(mParent, "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);
    nsSVGAElement *content = static_cast<nsSVGAElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformsTo(parent->GetCanvasTM(aFor));

    mCanvasTM = new gfxMatrix(tm);
  }

  return *mCanvasTM;
}

//----------------------------------------------------------------------
// nsSVGTextContainerFrame methods:

void
nsSVGAFrame::GetXY(SVGUserUnitList *aX, SVGUserUnitList *aY)
{
  aX->Clear();
  aY->Clear();
}

void
nsSVGAFrame::GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy)
{
  aDx->Clear();
  aDy->Clear();
}
