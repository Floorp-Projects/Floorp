/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for the point that anchors out-of-flow rendering
 * objects such as floats and absolutely positioned elements
 */

/*
 * Destruction of a placeholder and its out-of-flow must observe the
 * following constraints:
 *
 * - The mapping from the out-of-flow to the placeholder must be
 *   removed from the frame manager before the placeholder is destroyed.
 * - The mapping from the out-of-flow to the placeholder must be
 *   removed from the frame manager before the out-of-flow is destroyed.
 * - The placeholder must be removed from the frame tree, or have the
 *   mapping from it to its out-of-flow cleared, before the out-of-flow
 *   is destroyed (so that the placeholder will not point to a destroyed
 *   frame while it's in the frame tree).
 *
 * Furthermore, some code assumes that placeholders point to something
 * useful, so placeholders without an associated out-of-flow should not
 * remain in the tree.
 *
 * The placeholder's Destroy() implementation handles the destruction of
 * the placeholder and its out-of-flow. To avoid crashes, frame removal
 * and destruction code that works with placeholders must not assume
 * that the placeholder points to its out-of-flow.
 */

#ifndef nsPlaceholderFrame_h___
#define nsPlaceholderFrame_h___

#include "mozilla/Attributes.h"
#include "nsFrame.h"
#include "nsGkAtoms.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsPlaceholderFrame;
nsPlaceholderFrame* NS_NewPlaceholderFrame(mozilla::PresShell* aPresShell,
                                           mozilla::ComputedStyle* aStyle,
                                           nsFrameState aTypeBits);

#define PLACEHOLDER_TYPE_MASK                                                  \
  (PLACEHOLDER_FOR_FLOAT | PLACEHOLDER_FOR_ABSPOS | PLACEHOLDER_FOR_FIXEDPOS | \
   PLACEHOLDER_FOR_POPUP | PLACEHOLDER_FOR_TOPLAYER)

/**
 * Implementation of a frame that's used as a placeholder for a frame that
 * has been moved out of the flow.
 */
class nsPlaceholderFrame final : public nsFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsPlaceholderFrame)
#ifdef DEBUG
  NS_DECL_QUERYFRAME
#endif

  /**
   * Create a new placeholder frame.  aTypeBit must be one of the
   * PLACEHOLDER_FOR_* constants above.
   */
  friend nsPlaceholderFrame* NS_NewPlaceholderFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle,
      nsFrameState aTypeBits);

  nsPlaceholderFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                     nsFrameState aTypeBits)
      : nsFrame(aStyle, aPresContext, kClassID), mOutOfFlowFrame(nullptr) {
    MOZ_ASSERT(
        aTypeBits == PLACEHOLDER_FOR_FLOAT ||
            aTypeBits == PLACEHOLDER_FOR_ABSPOS ||
            aTypeBits == PLACEHOLDER_FOR_FIXEDPOS ||
            aTypeBits == PLACEHOLDER_FOR_POPUP ||
            aTypeBits == (PLACEHOLDER_FOR_TOPLAYER | PLACEHOLDER_FOR_ABSPOS) ||
            aTypeBits == (PLACEHOLDER_FOR_TOPLAYER | PLACEHOLDER_FOR_FIXEDPOS),
        "Unexpected type bit");
    AddStateBits(aTypeBits);
  }

  // Get/Set the associated out of flow frame
  nsIFrame* GetOutOfFlowFrame() const { return mOutOfFlowFrame; }
  void SetOutOfFlowFrame(nsIFrame* aFrame) {
    NS_ASSERTION(!aFrame || !aFrame->GetPrevContinuation(),
                 "OOF must be first continuation");
    mOutOfFlowFrame = aFrame;
  }

  // nsIFrame overrides
  // We need to override GetXULMinSize and GetXULPrefSize because XUL uses
  // placeholders not within lines.
  virtual void AddInlineMinISize(gfxContext* aRenderingContext,
                                 InlineMinISizeData* aData) override;
  virtual void AddInlinePrefISize(gfxContext* aRenderingContext,
                                  InlinePrefISizeData* aData) override;
  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

#if defined(DEBUG) || (defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF))
  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;
#endif  // DEBUG || (MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF)

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out = stderr, const char* aPrefix = "",
            uint32_t aFlags = 0) const override;
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif  // DEBUG

  virtual bool IsEmpty() override { return true; }
  virtual bool IsSelfEmpty() override { return true; }

  virtual bool CanContinueTextRun() const override;

  void SetLineIsEmptySoFar(bool aValue) {
    AddOrRemoveStateBits(PLACEHOLDER_LINE_IS_EMPTY_SO_FAR, aValue);
    AddStateBits(PLACEHOLDER_HAVE_LINE_IS_EMPTY_SO_FAR);
  }
  bool GetLineIsEmptySoFar(bool* aResult) const {
    bool haveValue = HasAnyStateBits(PLACEHOLDER_HAVE_LINE_IS_EMPTY_SO_FAR);
    if (haveValue) {
      *aResult = HasAnyStateBits(PLACEHOLDER_LINE_IS_EMPTY_SO_FAR);
    }
    return haveValue;
  }
  void ForgetLineIsEmptySoFar() {
    RemoveStateBits(PLACEHOLDER_HAVE_LINE_IS_EMPTY_SO_FAR);
  }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override {
    nsIFrame* realFrame = GetRealFrameForPlaceholder(this);
    return realFrame ? realFrame->AccessibleType() : nsFrame::AccessibleType();
  }
#endif

  ComputedStyle* GetParentComputedStyleForOutOfFlow(
      nsIFrame** aProviderFrame) const;

  // Like GetParentComputedStyleForOutOfFlow, but ignores display:contents bits.
  ComputedStyle* GetLayoutParentStyleForOutOfFlow(
      nsIFrame** aProviderFrame) const;

  bool RenumberFrameAndDescendants(int32_t* aOrdinal, int32_t aDepth,
                                   int32_t aIncrement,
                                   bool aForCounting) override {
    return mOutOfFlowFrame->RenumberFrameAndDescendants(
        aOrdinal, aDepth, aIncrement, aForCounting);
  }

  /**
   * @return the out-of-flow for aFrame if aFrame is a placeholder; otherwise
   * aFrame
   */
  static nsIFrame* GetRealFrameFor(nsIFrame* aFrame) {
    MOZ_ASSERT(aFrame, "Must have a frame to work with");
    if (aFrame->IsPlaceholderFrame()) {
      return GetRealFrameForPlaceholder(aFrame);
    }
    return aFrame;
  }

  /**
   * @return the out-of-flow for aFrame, which is known to be a placeholder
   */
  static nsIFrame* GetRealFrameForPlaceholder(nsIFrame* aFrame) {
    MOZ_ASSERT(aFrame->IsPlaceholderFrame(),
               "Must have placeholder frame as input");
    nsIFrame* outOfFlow =
        static_cast<nsPlaceholderFrame*>(aFrame)->GetOutOfFlowFrame();
    NS_ASSERTION(outOfFlow, "Null out-of-flow for placeholder?");
    return outOfFlow;
  }

 protected:
  nsIFrame* mOutOfFlowFrame;
};

#endif /* nsPlaceholderFrame_h___ */
