/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS display:inline objects */

#ifndef nsInlineFrame_h___
#define nsInlineFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsLineLayout;

/**
 * Inline frame class.
 *
 * This class manages a list of child frames that are inline frames. Working with
 * nsLineLayout, the class will reflow and place inline frames on a line.
 */
class nsInlineFrame : public nsContainerFrame
{
public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsInlineFrame)

  friend nsInlineFrame* NS_NewInlineFrame(nsIPresShell* aPresShell,
                                          ComputedStyle* aStyle);

  // nsIFrame overrides
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsDisplayListSet& aLists) override;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    if (aFlags & eSupportsCSSTransforms) {
      return false;
    }
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eBidiInlineContainer | nsIFrame::eLineParticipant));
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0, bool aRebuildDisplayItems = true) override;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0, bool aRebuildDisplayItems = true) override;

  virtual bool IsEmpty() override;
  virtual bool IsSelfEmpty() override;

  virtual FrameSearchResult
  PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                      PeekOffsetCharacterOptions aOptions =
                        PeekOffsetCharacterOptions()) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;
  virtual nsresult StealFrame(nsIFrame* aChild) override;

  // nsIHTMLReflow overrides
  virtual void AddInlineMinISize(gfxContext *aRenderingContext,
                                 InlineMinISizeData *aData) override;
  virtual void AddInlinePrefISize(gfxContext *aRenderingContext,
                                  InlinePrefISizeData *aData) override;
  virtual mozilla::LogicalSize
  ComputeSize(gfxContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              ComputeSizeFlags aFlags) override;
  virtual nsRect ComputeTightBounds(DrawTarget* aDrawTarget) const override;
  virtual void Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual bool CanContinueTextRun() const override;

  virtual void PullOverflowsFromPrevInFlow() override;
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;
  virtual bool DrainSelfOverflowList() override;

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

  // Restyles the block wrappers around our non-inline-outside kids.
  // This will only be called when such wrappers in fact exist.
  void UpdateStyleOfOwnedAnonBoxesForIBSplit(
    mozilla::ServoRestyleState& aRestyleState);

protected:
  // Additional reflow state used during our reflow methods
  struct InlineReflowInput {
    nsIFrame* mPrevFrame;
    nsInlineFrame* mNextInFlow;
    nsIFrame*      mLineContainer;
    nsLineLayout*  mLineLayout;
    bool mSetParentPointer;  // when reflowing child frame first set its
                                     // parent frame pointer

    InlineReflowInput()  {
      mPrevFrame = nullptr;
      mNextInFlow = nullptr;
      mLineContainer = nullptr;
      mLineLayout = nullptr;
      mSetParentPointer = false;
    }
  };

  nsInlineFrame(ComputedStyle* aStyle, ClassID aID)
    : nsContainerFrame(aStyle, aID)
    , mBaseline(NS_INTRINSIC_WIDTH_UNKNOWN)
  {}

  virtual LogicalSides GetLogicalSkipSides(const ReflowInput* aReflowInput = nullptr) const override;

  void ReflowFrames(nsPresContext* aPresContext,
                    const ReflowInput& aReflowInput,
                    InlineReflowInput& rs,
                    ReflowOutput& aMetrics,
                    nsReflowStatus& aStatus);

  void ReflowInlineFrame(nsPresContext* aPresContext,
                         const ReflowInput& aReflowInput,
                         InlineReflowInput& rs,
                         nsIFrame* aFrame,
                         nsReflowStatus& aStatus);

  // Returns whether there's any frame that PullOneFrame would pull from
  // aNextInFlow or any of aNextInFlow's next-in-flows.
  static bool HasFramesToPull(nsInlineFrame* aNextInFlow);

  virtual nsIFrame* PullOneFrame(nsPresContext*, InlineReflowInput&);

  virtual void PushFrames(nsPresContext* aPresContext,
                          nsIFrame* aFromChild,
                          nsIFrame* aPrevSibling,
                          InlineReflowInput& aState);

private:
  explicit nsInlineFrame(ComputedStyle* aStyle)
    : nsInlineFrame(aStyle, kClassID)
  {}

  /**
   * Move any frames on our overflow list to the end of our principal list.
   * @param aInFirstLine whether we're in a first-line frame.
   * @return true if there were any overflow frames
   */
  bool DrainSelfOverflowListInternal(bool aInFirstLine);
protected:
  nscoord mBaseline;
};

//----------------------------------------------------------------------

/**
 * Variation on inline-frame used to manage lines for line layout in
 * special situations (:first-line style in particular).
 */
class nsFirstLineFrame final : public nsInlineFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS(nsFirstLineFrame)

  friend nsFirstLineFrame* NS_NewFirstLineFrame(nsIPresShell* aPresShell,
                                                ComputedStyle* aStyle);

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif
  virtual void Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void PullOverflowsFromPrevInFlow() override;
  virtual bool DrainSelfOverflowList() override;

protected:
  explicit nsFirstLineFrame(ComputedStyle* aStyle)
    : nsInlineFrame(aStyle, kClassID)
  {}

  nsIFrame* PullOneFrame(nsPresContext*, InlineReflowInput&) override;
};

#endif /* nsInlineFrame_h___ */
