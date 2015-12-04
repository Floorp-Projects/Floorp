/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-base-container" */

#ifndef nsRubyBaseContainerFrame_h___
#define nsRubyBaseContainerFrame_h___

#include "nsContainerFrame.h"

/**
 * Factory function.
 * @return a newly allocated nsRubyBaseContainerFrame (infallible)
 */
nsContainerFrame* NS_NewRubyBaseContainerFrame(nsIPresShell* aPresShell,
                                               nsStyleContext* aContext);

namespace mozilla {
struct RubyColumn;
} // namespace mozilla

class nsRubyBaseContainerFrame final : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsRubyBaseContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual nsIAtom* GetType() const override;
  virtual bool IsFrameOfType(uint32_t aFlags) const override;
  virtual bool CanContinueTextRun() const override;
  virtual void AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                 InlineMinISizeData *aData) override;
  virtual void AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                  InlinePrefISizeData *aData) override;
  virtual mozilla::LogicalSize
    ComputeSize(nsRenderingContext *aRenderingContext,
                mozilla::WritingMode aWritingMode,
                const mozilla::LogicalSize& aCBSize,
                nscoord aAvailableISize,
                const mozilla::LogicalSize& aMargin,
                const mozilla::LogicalSize& aBorder,
                const mozilla::LogicalSize& aPadding,
                ComputeSizeFlags aFlags) override;
  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) override;

  virtual nscoord
    GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  void SetBlockLeadings(nscoord aStart, nscoord aEnd)
  {
    mBStartLeading = aStart;
    mBEndLeading = aEnd;
  }
  void GetBlockLeadings(nscoord* aStart, nscoord* aEnd) const
  {
    *aStart = mBStartLeading;
    *aEnd = mBEndLeading;
  }

protected:
  friend nsContainerFrame*
    NS_NewRubyBaseContainerFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext);
  explicit nsRubyBaseContainerFrame(nsStyleContext* aContext) : nsContainerFrame(aContext) {}

  struct ReflowState;
  nscoord ReflowColumns(const ReflowState& aReflowState,
                        nsReflowStatus& aStatus);
  nscoord ReflowOneColumn(const ReflowState& aReflowState,
                          uint32_t aColumnIndex,
                          const mozilla::RubyColumn& aColumn,
                          nsReflowStatus& aStatus);
  nscoord ReflowSpans(const ReflowState& aReflowState);

  struct PullFrameState;

  // Pull ruby base and corresponding ruby text frames from
  // continuations after them.
  void PullOneColumn(nsLineLayout* aLineLayout,
                     PullFrameState& aPullFrameState,
                     mozilla::RubyColumn& aColumn,
                     bool& aIsComplete);

  nscoord mBaseline;

  // The leading required to put the annotations. These fields are not
  // initialized until the parent frame finishes its first reflow.
  nscoord mBStartLeading;
  nscoord mBEndLeading;
};

#endif /* nsRubyBaseContainerFrame_h___ */
