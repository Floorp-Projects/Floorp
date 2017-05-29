/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text" */

#ifndef nsRubyTextFrame_h___
#define nsRubyTextFrame_h___

#include "nsRubyContentFrame.h"

/**
 * Factory function.
 * @return a newly allocated nsRubyTextFrame (infallible)
 */
nsContainerFrame* NS_NewRubyTextFrame(nsIPresShell* aPresShell,
                                      nsStyleContext* aContext);

class nsRubyTextFrame final : public nsRubyContentFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(nsRubyTextFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual bool CanContinueTextRun() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual void Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  bool IsAutoHidden() const
  {
    return GetStateBits() & NS_RUBY_TEXT_FRAME_AUTOHIDE;
  }

protected:
  friend nsContainerFrame* NS_NewRubyTextFrame(nsIPresShell* aPresShell,
                                               nsStyleContext* aContext);
  explicit nsRubyTextFrame(nsStyleContext* aContext)
    : nsRubyContentFrame(aContext, kClassID)
  {}
};

#endif /* nsRubyTextFrame_h___ */
