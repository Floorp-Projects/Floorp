/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGFOREIGNOBJECTFRAME_H__
#define NSSVGFOREIGNOBJECTFRAME_H__

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsContainerFrame.h"
#include "nsIPresShell.h"
#include "nsSVGDisplayableFrame.h"
#include "nsRegion.h"
#include "nsSVGUtils.h"

class gfxContext;

class nsSVGForeignObjectFrame : public nsContainerFrame
                              , public nsSVGDisplayableFrame
{
  friend nsContainerFrame*
  NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit nsSVGForeignObjectFrame(nsStyleContext* aContext);

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;
  virtual nsresult  AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType) override;

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgForeignObjectFrame
   */
  virtual nsIAtom* GetType() const override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eSVG | nsIFrame::eSVGForeignObject));
  }

  virtual bool IsSVGTransformed(Matrix *aOwnTransform,
                                Matrix *aFromParentTransform) const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGForeignObject"), aResult);
  }
#endif

  // nsSVGDisplayableFrame interface:
  virtual DrawResult PaintSVG(gfxContext& aContext,
                              const gfxMatrix& aTransform,
                              const nsIntRect* aDirtyRect = nullptr) override;
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  virtual void ReflowSVG() override;
  virtual void NotifySVGChanged(uint32_t aFlags) override;
  virtual SVGBBox GetBBoxContribution(const Matrix &aToBBoxUserspace,
                                      uint32_t aFlags) override;
  virtual bool IsDisplayContainer() override { return true; }

  gfxMatrix GetCanvasTM();

  nsRect GetInvalidRegion();

  /**
   * Update the style of our ::-moz-svg-foreign-content anonymous box.
   */
  void DoUpdateStyleOfOwnedAnonBoxes(mozilla::ServoStyleSet& aStyleSet,
                                     nsStyleChangeList& aChangeList,
                                     nsChangeHint aHintForThisFrame) override;

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
