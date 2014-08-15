/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text-container" */

#ifndef nsRubyTextContainerFrame_h___
#define nsRubyTextContainerFrame_h___

#include "nsBlockFrame.h"
#include "nsRubyBaseFrame.h"
#include "nsRubyTextFrame.h"
#include "nsLineLayout.h"

/**
 * Factory function.
 * @return a newly allocated nsRubyTextContainerFrame (infallible)
 */
nsContainerFrame* NS_NewRubyTextContainerFrame(nsIPresShell* aPresShell,
                                               nsStyleContext* aContext);

// If this is ever changed to be inline again, the code in
// nsFrame::IsFontSizeInflationContainer should be updated to stop excluding
// this from being considered inline.
class nsRubyTextContainerFrame MOZ_FINAL : public nsBlockFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsRubyTextContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  void ReflowRubyTextFrame(nsRubyTextFrame* rtFrame, nsIFrame* rbFrame,
                           nscoord baseStart, nsPresContext* aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           const nsHTMLReflowState& aReflowState);
  void BeginRTCLineLayout(nsPresContext* aPresContext,
                          const nsHTMLReflowState& aReflowState);
  nsLineLayout* GetLineLayout() { return mLineLayout.get(); };

protected:
  friend nsContainerFrame*
    NS_NewRubyTextContainerFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext);
  nsRubyTextContainerFrame(nsStyleContext* aContext) : nsBlockFrame(aContext) {}
  // This pointer is active only during reflow of the ruby structure. It gets
  // created when the corresponding ruby base container is reflowed, and it is
  // destroyed when the ruby text container itself is reflowed.
  mozilla::UniquePtr<nsLineLayout> mLineLayout;
  // The intended dimensions of the ruby text container. These are modified
  // whenever a ruby text box is reflowed and used when the ruby text container
  // is reflowed.
  nscoord mISize;
};

#endif /* nsRubyTextContainerFrame_h___ */
