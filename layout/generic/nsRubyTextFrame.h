/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text" */

#ifndef nsRubyTextFrame_h___
#define nsRubyTextFrame_h___

#include "nsRubyContentFrame.h"

typedef nsRubyContentFrame nsRubyTextFrameSuper;

/**
 * Factory function.
 * @return a newly allocated nsRubyTextFrame (infallible)
 */
nsContainerFrame* NS_NewRubyTextFrame(nsIPresShell* aPresShell,
                                      nsStyleContext* aContext);

class nsRubyTextFrame MOZ_FINAL : public nsRubyTextFrameSuper
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsRubyTextFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual bool CanContinueTextRun() const MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) MOZ_OVERRIDE;

protected:
  friend nsContainerFrame* NS_NewRubyTextFrame(nsIPresShell* aPresShell,
                                               nsStyleContext* aContext);
  explicit nsRubyTextFrame(nsStyleContext* aContext)
    : nsRubyTextFrameSuper(aContext) {}
};

#endif /* nsRubyTextFrame_h___ */
