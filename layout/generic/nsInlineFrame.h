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

/**  In Bidi left (or right) margin/padding/border should be applied to left
 *  (or right) most frame (or a continuation frame).
 *  This state value shows if this frame is left (or right) most continuation
 *  or not.
 */
#define NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET     NS_FRAME_STATE_BIT(21)

#define NS_INLINE_FRAME_BIDI_VISUAL_IS_LEFT_MOST     NS_FRAME_STATE_BIT(22)

#define NS_INLINE_FRAME_BIDI_VISUAL_IS_RIGHT_MOST    NS_FRAME_STATE_BIT(23)

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

  friend nsIFrame* NS_NewInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  // nsIFrame overrides
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    if (aFlags & eSupportsCSSTransforms) {
      return false;
    }
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eBidiInlineContainer | nsIFrame::eLineParticipant));
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0);
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0);

  virtual bool IsEmpty() MOZ_OVERRIDE;
  virtual bool IsSelfEmpty() MOZ_OVERRIDE;

  virtual bool PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                     bool aRespectClusters = true) MOZ_OVERRIDE;
  
  // nsIHTMLReflow overrides
  virtual void AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData) MOZ_OVERRIDE;
  virtual void AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData) MOZ_OVERRIDE;
  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             uint32_t aFlags) MOZ_OVERRIDE;
  virtual nsRect ComputeTightBounds(gfxContext* aContext) const;
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus) MOZ_OVERRIDE;

  virtual bool CanContinueTextRun() const MOZ_OVERRIDE;

  virtual void PullOverflowsFromPrevInFlow();
  virtual nscoord GetBaseline() const MOZ_OVERRIDE;

  /**
   * Return true if the frame is leftmost frame or continuation.
   */
  bool IsLeftMost() const {
    // If the frame's bidi visual state is set, return is-leftmost state
    // else return true if it's the first continuation.
    return (GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET)
             ? !!(GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_IS_LEFT_MOST)
             : (!GetPrevInFlow());
  }

  /**
   * Return true if the frame is rightmost frame or continuation.
   */
  bool IsRightMost() const {
    // If the frame's bidi visual state is set, return is-rightmost state
    // else return true if it's the last continuation.
    return (GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET)
             ? !!(GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_IS_RIGHT_MOST)
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

  virtual int GetSkipSides() const MOZ_OVERRIDE;

  nsresult ReflowFrames(nsPresContext* aPresContext,
                        const nsHTMLReflowState& aReflowState,
                        InlineReflowState& rs,
                        nsHTMLReflowMetrics& aMetrics,
                        nsReflowStatus& aStatus);

  nsresult ReflowInlineFrame(nsPresContext* aPresContext,
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

  nscoord mBaseline;
};

//----------------------------------------------------------------------

/**
 * Variation on inline-frame used to manage lines for line layout in
 * special situations (:first-line style in particular).
 */
class nsFirstLineFrame : public nsInlineFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewFirstLineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus) MOZ_OVERRIDE;

  virtual void Init(nsIContent* aContent, nsIFrame* aParent,
                    nsIFrame* aPrevInFlow) MOZ_OVERRIDE;
  virtual void PullOverflowsFromPrevInFlow() MOZ_OVERRIDE;

protected:
  nsFirstLineFrame(nsStyleContext* aContext) : nsInlineFrame(aContext) {}

  virtual nsIFrame* PullOneFrame(nsPresContext* aPresContext,
                                 InlineReflowState& rs,
                                 bool* aIsComplete) MOZ_OVERRIDE;
};

#endif /* nsInlineFrame_h___ */
