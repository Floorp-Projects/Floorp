/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGFOREIGNOBJECTFRAME_H__
#define NSSVGFOREIGNOBJECTFRAME_H__

#include "nsContainerFrame.h"
#include "nsIPresShell.h"
#include "nsISVGChildFrame.h"
#include "nsRegion.h"
#include "nsSVGUtils.h"

class nsRenderingContext;
class nsSVGOuterSVGFrame;

typedef nsContainerFrame nsSVGForeignObjectFrameBase;

class nsSVGForeignObjectFrame : public nsSVGForeignObjectFrameBase,
                                public nsISVGChildFrame
{
  friend nsIFrame*
  NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGForeignObjectFrame(nsStyleContext* aContext);

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
  NS_IMETHOD  Init(nsIContent* aContent,
                   nsIFrame*   aParent,
                   nsIFrame*   aPrevInFlow);
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;

  virtual nsIFrame* GetContentInsertionFrame() {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgForeignObjectFrame
   */
  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsSVGForeignObjectFrameBase::IsFrameOfType(aFlags &
      ~(nsIFrame::eSVG | nsIFrame::eSVGForeignObject));
  }

  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRUint32 aFlags);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGForeignObject"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsRenderingContext *aContext,
                      const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint);
  NS_IMETHOD_(nsRect) GetCoveredRegion();
  virtual void UpdateBounds();
  virtual void NotifySVGChanged(PRUint32 aFlags);
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      PRUint32 aFlags);
  NS_IMETHOD_(bool) IsDisplayContainer() { return true; }

  gfxMatrix GetCanvasTM(PRUint32 aFor);

protected:
  // implementation helpers:
  void DoReflow();
  void RequestReflow(nsIPresShell::IntrinsicDirty aType);

  void InvalidateDirtyRect(const nsRect& aRect, PRUint32 aFlags,
                           bool aDuringReflowSVG);
  void FlushDirtyRegion(PRUint32 aFlags, bool aDuringReflowSVG);

  // If width or height is less than or equal to zero we must disable rendering
  bool IsDisabled() const { return mRect.width <= 0 || mRect.height <= 0; }

  nsAutoPtr<gfxMatrix> mCanvasTM;

  // Areas dirtied by changes to decendents that are in our document
  nsRegion mSameDocDirtyRegion;

  // Areas dirtied by changes to sub-documents embedded by our decendents
  nsRegion mSubDocDirtyRegion;

  nsRect mCoveredRegion;

  bool mInReflow;
};

#endif
