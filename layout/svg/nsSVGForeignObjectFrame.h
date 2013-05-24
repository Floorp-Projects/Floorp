/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGFOREIGNOBJECTFRAME_H__
#define NSSVGFOREIGNOBJECTFRAME_H__

#include "mozilla/Attributes.h"
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
  virtual void Init(nsIContent* aContent,
                    nsIFrame*   aParent,
                    nsIFrame*   aPrevInFlow) MOZ_OVERRIDE;
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  NS_IMETHOD  AttributeChanged(int32_t         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               int32_t         aModType);

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;

  virtual nsIFrame* GetContentInsertionFrame() {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgForeignObjectFrame
   */
  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    return nsSVGForeignObjectFrameBase::IsFrameOfType(aFlags &
      ~(nsIFrame::eSVG | nsIFrame::eSVGForeignObject));
  }

  virtual bool IsSVGTransformed(gfxMatrix *aOwnTransform,
                                gfxMatrix *aFromParentTransform) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGForeignObject"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsRenderingContext *aContext,
                      const nsIntRect *aDirtyRect) MOZ_OVERRIDE;
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint) MOZ_OVERRIDE;
  NS_IMETHOD_(nsRect) GetCoveredRegion() MOZ_OVERRIDE;
  virtual void ReflowSVG() MOZ_OVERRIDE;
  virtual void NotifySVGChanged(uint32_t aFlags) MOZ_OVERRIDE;
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      uint32_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsDisplayContainer() MOZ_OVERRIDE { return true; }

  gfxMatrix GetCanvasTM(uint32_t aFor);

  nsRect GetInvalidRegion();

protected:
  // implementation helpers:
  void DoReflow();
  void RequestReflow(nsIPresShell::IntrinsicDirty aType);

  // If width or height is less than or equal to zero we must disable rendering
  bool IsDisabled() const { return mRect.width <= 0 || mRect.height <= 0; }

  nsAutoPtr<gfxMatrix> mCanvasTM;

  bool mInReflow;
};

#endif
