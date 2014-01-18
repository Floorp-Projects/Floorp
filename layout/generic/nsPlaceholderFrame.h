/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

nsIFrame* NS_NewPlaceholderFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext,
                                 nsFrameState aTypeBit);

#define PLACEHOLDER_TYPE_MASK    (PLACEHOLDER_FOR_FLOAT | \
                                  PLACEHOLDER_FOR_ABSPOS | \
                                  PLACEHOLDER_FOR_FIXEDPOS | \
                                  PLACEHOLDER_FOR_POPUP)

/**
 * Implementation of a frame that's used as a placeholder for a frame that
 * has been moved out of the flow.
 */
class nsPlaceholderFrame MOZ_FINAL : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS
#ifdef DEBUG
  NS_DECL_QUERYFRAME_TARGET(nsPlaceholderFrame)
  NS_DECL_QUERYFRAME
#endif

  /**
   * Create a new placeholder frame.  aTypeBit must be one of the
   * PLACEHOLDER_FOR_* constants above.
   */
  friend nsIFrame* NS_NewPlaceholderFrame(nsIPresShell* aPresShell,
                                          nsStyleContext* aContext,
                                          nsFrameState aTypeBit);
  nsPlaceholderFrame(nsStyleContext* aContext, nsFrameState aTypeBit)
    : nsFrame(aContext)
  {
    NS_PRECONDITION(aTypeBit == PLACEHOLDER_FOR_FLOAT ||
                    aTypeBit == PLACEHOLDER_FOR_ABSPOS ||
                    aTypeBit == PLACEHOLDER_FOR_FIXEDPOS ||
                    aTypeBit == PLACEHOLDER_FOR_POPUP,
                    "Unexpected type bit");
    AddStateBits(aTypeBit);
  }

  // Get/Set the associated out of flow frame
  nsIFrame*  GetOutOfFlowFrame() const { return mOutOfFlowFrame; }
  void       SetOutOfFlowFrame(nsIFrame* aFrame) {
               NS_ASSERTION(!aFrame || !aFrame->GetPrevContinuation(),
                            "OOF must be first continuation");
               mOutOfFlowFrame = aFrame;
             }

  // nsIFrame overrides
  // We need to override GetMinSize and GetPrefSize because XUL uses
  // placeholders not within lines.
  virtual void AddInlineMinWidth(nsRenderingContext* aRenderingContext,
                                 InlineMinWidthData* aData) MOZ_OVERRIDE;
  virtual void AddInlinePrefWidth(nsRenderingContext* aRenderingContext,
                                  InlinePrefWidthData* aData) MOZ_OVERRIDE;
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

#if defined(DEBUG) || (defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF))
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;
#endif // DEBUG || (MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF)
  
#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out, int32_t aIndent, uint32_t aFlags = 0) const MOZ_OVERRIDE;
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif // DEBUG

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::placeholderFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsEmpty() MOZ_OVERRIDE { return true; }
  virtual bool IsSelfEmpty() MOZ_OVERRIDE { return true; }

  virtual bool CanContinueTextRun() const MOZ_OVERRIDE;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE
  {
    nsIFrame* realFrame = GetRealFrameForPlaceholder(this);
    return realFrame ? realFrame->AccessibleType() :
                       nsFrame::AccessibleType();
  }
#endif

  virtual nsIFrame* GetParentStyleContextFrame() const MOZ_OVERRIDE;

  /**
   * @return the out-of-flow for aFrame if aFrame is a placeholder; otherwise
   * aFrame
   */
  static nsIFrame* GetRealFrameFor(nsIFrame* aFrame) {
    NS_PRECONDITION(aFrame, "Must have a frame to work with");
    if (aFrame->GetType() == nsGkAtoms::placeholderFrame) {
      return GetRealFrameForPlaceholder(aFrame);
    }
    return aFrame;
  }

  /**
   * @return the out-of-flow for aFrame, which is known to be a placeholder
   */
  static nsIFrame* GetRealFrameForPlaceholder(nsIFrame* aFrame) {
    NS_PRECONDITION(aFrame->GetType() == nsGkAtoms::placeholderFrame,
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
