/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "::backdrop" */

#ifndef nsBackdropFrame_h___
#define nsBackdropFrame_h___

#include "nsFrame.h"

class nsBackdropFrame final : public nsFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  explicit nsBackdropFrame(nsStyleContext* aContext)
    : nsFrame(aContext) { }

  // nsIFrame overrides
  virtual nsIAtom* GetType() const override;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif
  virtual nsStyleContext*
    GetParentStyleContext(nsIFrame** aProviderFrame) const override;
  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsRect& aDirtyRect,
                                const nsDisplayListSet& aLists) override;
  virtual LogicalSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                      WritingMode aWM,
                                      const LogicalSize& aCBSize,
                                      nscoord aAvailableISize,
                                      const LogicalSize& aMargin,
                                      const LogicalSize& aBorder,
                                      const LogicalSize& aPadding,
                                      bool aShrinkWrap) override;
  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) override;
};

#endif // nsBackdropFrame_h___
