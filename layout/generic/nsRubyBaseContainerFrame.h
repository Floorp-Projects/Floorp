/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-base-container" */

#ifndef nsRubyBaseContainerFrame_h___
#define nsRubyBaseContainerFrame_h___

#include "nsContainerFrame.h"
#include "RubyUtils.h"

/**
 * Factory function.
 * @return a newly allocated nsRubyBaseContainerFrame (infallible)
 */
nsContainerFrame* NS_NewRubyBaseContainerFrame(nsIPresShell* aPresShell,
                                               nsStyleContext* aContext);

class nsRubyBaseContainerFrame final : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsRubyBaseContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
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
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual nscoord
    GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  void UpdateDescendantLeadings(const mozilla::RubyBlockLeadings& aLeadings) {
    mDescendantLeadings.Update(aLeadings);
  }
  mozilla::RubyBlockLeadings GetDescendantLeadings() const {
    return mDescendantLeadings;
  }

protected:
  friend nsContainerFrame*
    NS_NewRubyBaseContainerFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext);

  explicit nsRubyBaseContainerFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext, mozilla::LayoutFrameType::RubyBaseContainer)
  {}

  struct RubyReflowInput;
  nscoord ReflowColumns(const RubyReflowInput& aReflowInput,
                        nsReflowStatus& aStatus);
  nscoord ReflowOneColumn(const RubyReflowInput& aReflowInput,
                          uint32_t aColumnIndex,
                          const mozilla::RubyColumn& aColumn,
                          nsReflowStatus& aStatus);
  nscoord ReflowSpans(const RubyReflowInput& aReflowInput);

  struct PullFrameState;

  // Pull ruby base and corresponding ruby text frames from
  // continuations after them.
  void PullOneColumn(nsLineLayout* aLineLayout,
                     PullFrameState& aPullFrameState,
                     mozilla::RubyColumn& aColumn,
                     bool& aIsComplete);

  nscoord mBaseline;

  // Leading produced by descendant ruby annotations.
  mozilla::RubyBlockLeadings mDescendantLeadings;
};

#endif /* nsRubyBaseContainerFrame_h___ */
