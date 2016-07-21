/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby" */

#ifndef nsRubyFrame_h___
#define nsRubyFrame_h___

#include "nsInlineFrame.h"

class nsRubyBaseContainerFrame;

/**
 * Factory function.
 * @return a newly allocated nsRubyFrame (infallible)
 */
nsContainerFrame* NS_NewRubyFrame(nsIPresShell* aPresShell,
                                  nsStyleContext* aContext);

class nsRubyFrame final : public nsInlineFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsRubyFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual nsIAtom* GetType() const override;
  virtual bool IsFrameOfType(uint32_t aFlags) const override;
  virtual void AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                 InlineMinISizeData *aData) override;
  virtual void AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                  InlinePrefISizeData *aData) override;
  virtual void Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  void GetBlockLeadings(nscoord& aStartLeading, nscoord& aEndLeading)
  {
    aStartLeading = mBStartLeading;
    aEndLeading = mBEndLeading;
  }

protected:
  friend nsContainerFrame* NS_NewRubyFrame(nsIPresShell* aPresShell,
                                           nsStyleContext* aContext);
  explicit nsRubyFrame(nsStyleContext* aContext)
    : nsInlineFrame(aContext) {}

  void ReflowSegment(nsPresContext* aPresContext,
                     const ReflowInput& aReflowInput,
                     nsRubyBaseContainerFrame* aBaseContainer,
                     nsReflowStatus& aStatus);

  nsRubyBaseContainerFrame* PullOneSegment(const nsLineLayout* aLineLayout,
                                           ContinuationTraversingState& aState);

  // The leading required to put the annotations.
  // They are not initialized until the first reflow.
  nscoord mBStartLeading;
  nscoord mBEndLeading;
};

#endif /* nsRubyFrame_h___ */
