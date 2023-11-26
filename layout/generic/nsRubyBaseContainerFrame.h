/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-base-container" */

#ifndef nsRubyBaseContainerFrame_h___
#define nsRubyBaseContainerFrame_h___

#include "nsContainerFrame.h"
#include "RubyUtils.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

/**
 * Factory function.
 * @return a newly allocated nsRubyBaseContainerFrame (infallible)
 */
nsContainerFrame* NS_NewRubyBaseContainerFrame(mozilla::PresShell* aPresShell,
                                               mozilla::ComputedStyle* aStyle);

class nsRubyBaseContainerFrame final : public nsContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsRubyBaseContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual bool CanContinueTextRun() const override;
  virtual void AddInlineMinISize(gfxContext* aRenderingContext,
                                 InlineMinISizeData* aData) override;
  virtual void AddInlinePrefISize(gfxContext* aRenderingContext,
                                  InlinePrefISizeData* aData) override;
  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;
  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  Maybe<nscoord> GetNaturalBaselineBOffset(
      mozilla::WritingMode aWM, BaselineSharingGroup aBaselineGroup,
      BaselineExportContext) const override;

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
  friend nsContainerFrame* NS_NewRubyBaseContainerFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  explicit nsRubyBaseContainerFrame(ComputedStyle* aStyle,
                                    nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID) {}

  struct RubyReflowInput;
  nscoord ReflowColumns(const RubyReflowInput& aReflowInput,
                        ReflowOutput& aDesiredSize, nsReflowStatus& aStatus);
  nscoord ReflowOneColumn(const RubyReflowInput& aReflowInput,
                          uint32_t aColumnIndex,
                          const mozilla::RubyColumn& aColumn,
                          ReflowOutput& aDesiredSize, nsReflowStatus& aStatus);
  nscoord ReflowSpans(const RubyReflowInput& aReflowInput);

  struct PullFrameState;

  // Pull ruby base and corresponding ruby text frames from
  // continuations after them.
  void PullOneColumn(nsLineLayout* aLineLayout, PullFrameState& aPullFrameState,
                     mozilla::RubyColumn& aColumn, bool& aIsComplete);

  nscoord mBaseline;

  // Leading produced by descendant ruby annotations.
  mozilla::RubyBlockLeadings mDescendantLeadings;
};

#endif /* nsRubyBaseContainerFrame_h___ */
