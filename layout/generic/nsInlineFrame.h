/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS display:inline objects */

#ifndef nsInlineFrame_h___
#define nsInlineFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsLineLayout;

typedef nsContainerFrame nsInlineFrameBase;

/**
 * Inline frame class.
 *
 * This class manages a list of child frames that are inline frames. Working with
 * nsLineLayout, the class will reflow and place inline frames on a line.
 */
class nsInlineFrame : public nsInlineFrameBase
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsInlineFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  friend nsInlineFrame* NS_NewInlineFrame(nsIPresShell* aPresShell,
                                          nsStyleContext* aContext);

  // nsIFrame overrides
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    if (aFlags & eSupportsCSSTransforms) {
      return false;
    }
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eBidiInlineContainer | nsIFrame::eLineParticipant));
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;

  virtual bool IsEmpty() MOZ_OVERRIDE;
  virtual bool IsSelfEmpty() MOZ_OVERRIDE;

  virtual FrameSearchResult PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                     bool aRespectClusters = true) MOZ_OVERRIDE;
  
  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  // nsIHTMLReflow overrides
  virtual void AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                 InlineMinISizeData *aData) MOZ_OVERRIDE;
  virtual void AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                  InlinePrefISizeData *aData) MOZ_OVERRIDE;
  virtual mozilla::LogicalSize
  ComputeSize(nsRenderingContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              uint32_t aFlags) MOZ_OVERRIDE;
  virtual nsRect ComputeTightBounds(gfxContext* aContext) const MOZ_OVERRIDE;
  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) MOZ_OVERRIDE;

  virtual bool CanContinueTextRun() const MOZ_OVERRIDE;

  virtual void PullOverflowsFromPrevInFlow() MOZ_OVERRIDE;
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const MOZ_OVERRIDE;
  virtual bool DrainSelfOverflowList() MOZ_OVERRIDE;

  /**
   * Return true if the frame is first visual frame or first continuation
   */
  bool IsFirst() const {
    // If the frame's bidi visual state is set, return is-first state
    // else return true if it's the first continuation.
    return (GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET)
             ? !!(GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_IS_FIRST)
             : (!GetPrevInFlow());
  }

  /**
   * Return true if the frame is last visual frame or last continuation.
   */
  bool IsLast() const {
    // If the frame's bidi visual state is set, return is-last state
    // else return true if it's the last continuation.
    return (GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET)
             ? !!(GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_IS_LAST)
             : (!GetNextInFlow());
  }

protected:
  // Additional reflow state used during our reflow methods
  struct InlineReflowState {
    nsIFrame* mPrevFrame;
    nsInlineFrame* mNextInFlow;
    nsIFrame*      mLineContainer;
    nsLineLayout*  mLineLayout;
    bool mSetParentPointer;  // when reflowing child frame first set its
                                     // parent frame pointer

    InlineReflowState()  {
      mPrevFrame = nullptr;
      mNextInFlow = nullptr;
      mLineContainer = nullptr;
      mLineLayout = nullptr;
      mSetParentPointer = false;
    }
  };

  nsInlineFrame(nsStyleContext* aContext) : nsContainerFrame(aContext) {}

  virtual LogicalSides GetLogicalSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const MOZ_OVERRIDE;

  void ReflowFrames(nsPresContext* aPresContext,
                    const nsHTMLReflowState& aReflowState,
                    InlineReflowState& rs,
                    nsHTMLReflowMetrics& aMetrics,
                    nsReflowStatus& aStatus);

  void ReflowInlineFrame(nsPresContext* aPresContext,
                         const nsHTMLReflowState& aReflowState,
                         InlineReflowState& rs,
                         nsIFrame* aFrame,
                         nsReflowStatus& aStatus);

  /**
   * Reparent floats whose placeholders are inline descendants of aFrame from
   * whatever block they're currently parented by to aOurBlock.
   * @param aReparentSiblings if this is true, we follow aFrame's
   * GetNextSibling chain reparenting them all
   */
  void ReparentFloatsForInlineChild(nsIFrame* aOurBlock, nsIFrame* aFrame,
                                    bool aReparentSiblings);

  virtual nsIFrame* PullOneFrame(nsPresContext* aPresContext,
                                 InlineReflowState& rs,
                                 bool* aIsComplete);

  virtual void PushFrames(nsPresContext* aPresContext,
                          nsIFrame* aFromChild,
                          nsIFrame* aPrevSibling,
                          InlineReflowState& aState);

private:
  // Helper method for DrainSelfOverflowList() to deal with lazy parenting
  // (which we only do for nsInlineFrame, not nsFirstLineFrame).
  enum DrainFlags {
    eDontReparentFrames = 1, // skip reparenting the overflow list frames
    eInFirstLine = 2, // the request is for an inline descendant of a nsFirstLineFrame
    eForDestroy = 4, // the request is from DestroyFrom; in this case we do the
                     // minimal work required since the frame is about to be
                     // destroyed (just fixup parent pointers)
  };
  /**
   * Move any frames on our overflow list to the end of our principal list.
   * @param aFlags one or more of the above DrainFlags
   * @param aLineContainer the nearest line container ancestor
   * @return true if there were any overflow frames
   */
  bool DrainSelfOverflowListInternal(DrainFlags aFlags,
                                     nsIFrame* aLineContainer);
protected:
  nscoord mBaseline;
};

//----------------------------------------------------------------------

/**
 * Variation on inline-frame used to manage lines for line layout in
 * special situations (:first-line style in particular).
 */
class nsFirstLineFrame MOZ_FINAL : public nsInlineFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsFirstLineFrame* NS_NewFirstLineFrame(nsIPresShell* aPresShell,
                                                nsStyleContext* aContext);

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) MOZ_OVERRIDE;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;
  virtual void PullOverflowsFromPrevInFlow() MOZ_OVERRIDE;
  virtual bool DrainSelfOverflowList() MOZ_OVERRIDE;

protected:
  nsFirstLineFrame(nsStyleContext* aContext) : nsInlineFrame(aContext) {}

  virtual nsIFrame* PullOneFrame(nsPresContext* aPresContext,
                                 InlineReflowState& rs,
                                 bool* aIsComplete) MOZ_OVERRIDE;
};

#endif /* nsInlineFrame_h___ */
