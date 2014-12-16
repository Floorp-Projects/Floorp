/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-base-container" */

#include "nsRubyBaseContainerFrame.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "WritingModes.h"

using namespace mozilla;

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyBaseContainerFrame)
  NS_QUERYFRAME_ENTRY(nsRubyBaseContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyBaseContainerFrame)

nsContainerFrame*
NS_NewRubyBaseContainerFrame(nsIPresShell* aPresShell,
                             nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyBaseContainerFrame(aContext);
}


//----------------------------------------------------------------------

// nsRubyBaseContainerFrame Method Implementations
// ===============================================

nsIAtom*
nsRubyBaseContainerFrame::GetType() const
{
  return nsGkAtoms::rubyBaseContainerFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyBaseContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("RubyBaseContainer"), aResult);
}
#endif

class MOZ_STACK_CLASS PairEnumerator
{
public:
  PairEnumerator(nsRubyBaseContainerFrame* aRBCFrame,
                 const nsTArray<nsRubyTextContainerFrame*>& aRTCFrames);

  void Next();
  bool AtEnd() const;

  uint32_t GetLevelCount() const { return mFrames.Length(); }
  nsIFrame* GetFrame(uint32_t aIndex) const { return mFrames[aIndex]; }
  nsIFrame* GetBaseFrame() const { return GetFrame(0); }
  nsIFrame* GetTextFrame(uint32_t aIndex) const { return GetFrame(aIndex + 1); }
  void GetFrames(nsIFrame*& aBaseFrame, nsTArray<nsIFrame*>& aTextFrames) const;

private:
  nsAutoTArray<nsIFrame*, RTC_ARRAY_SIZE + 1> mFrames;
};

PairEnumerator::PairEnumerator(
    nsRubyBaseContainerFrame* aBaseContainer,
    const nsTArray<nsRubyTextContainerFrame*>& aTextContainers)
{
  const uint32_t rtcCount = aTextContainers.Length();
  mFrames.SetCapacity(rtcCount + 1);
  mFrames.AppendElement(aBaseContainer->GetFirstPrincipalChild());
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsIFrame* rtFrame = aTextContainers[i]->GetFirstPrincipalChild();
    mFrames.AppendElement(rtFrame);
  }
}

void
PairEnumerator::Next()
{
  for (uint32_t i = 0, iend = mFrames.Length(); i < iend; i++) {
    if (mFrames[i]) {
      mFrames[i] = mFrames[i]->GetNextSibling();
    }
  }
}

bool
PairEnumerator::AtEnd() const
{
  for (uint32_t i = 0, iend = mFrames.Length(); i < iend; i++) {
    if (mFrames[i]) {
      return false;
    }
  }
  return true;
}

void
PairEnumerator::GetFrames(nsIFrame*& aBaseFrame,
                          nsTArray<nsIFrame*>& aTextFrames) const
{
  aBaseFrame = mFrames[0];
  aTextFrames.ClearAndRetainStorage();
  for (uint32_t i = 1, iend = mFrames.Length(); i < iend; i++) {
    aTextFrames.AppendElement(mFrames[i]);
  }
}

nscoord
nsRubyBaseContainerFrame::CalculateMaxSpanISize(
    nsRenderingContext* aRenderingContext)
{
  nscoord max = 0;
  uint32_t spanCount = mSpanContainers.Length();
  for (uint32_t i = 0; i < spanCount; i++) {
    nsIFrame* frame = mSpanContainers[i]->GetFirstPrincipalChild();
    nscoord isize = frame->GetPrefISize(aRenderingContext);
    max = std::max(max, isize);
  }
  return max;
}

static nscoord
CalculatePairPrefISize(nsRenderingContext* aRenderingContext,
                       const PairEnumerator& aEnumerator)
{
  nscoord max = 0;
  uint32_t levelCount = aEnumerator.GetLevelCount();
  for (uint32_t i = 0; i < levelCount; i++) {
    nsIFrame* frame = aEnumerator.GetFrame(i);
    if (frame) {
      max = std::max(max, frame->GetPrefISize(aRenderingContext));
    }
  }
  return max;
}

/* virtual */ void
nsRubyBaseContainerFrame::AddInlineMinISize(
    nsRenderingContext *aRenderingContext, nsIFrame::InlineMinISizeData *aData)
{
  if (!mSpanContainers.IsEmpty()) {
    // Since spans are not breakable internally, use our pref isize
    // directly if there is any span.
    aData->currentLine += GetPrefISize(aRenderingContext);
    return;
  }

  nscoord max = 0;
  PairEnumerator enumerator(this, mTextContainers);
  for (; !enumerator.AtEnd(); enumerator.Next()) {
    // We use *pref* isize for computing the min isize of pairs
    // because ruby bases and texts are unbreakable internally.
    max = std::max(max, CalculatePairPrefISize(aRenderingContext, enumerator));
  }
  aData->currentLine += max;
}

/* virtual */ void
nsRubyBaseContainerFrame::AddInlinePrefISize(
    nsRenderingContext *aRenderingContext, nsIFrame::InlinePrefISizeData *aData)
{
  nscoord sum = 0;
  PairEnumerator enumerator(this, mTextContainers);
  for (; !enumerator.AtEnd(); enumerator.Next()) {
    sum += CalculatePairPrefISize(aRenderingContext, enumerator);
  }
  sum = std::max(sum, CalculateMaxSpanISize(aRenderingContext));
  aData->currentLine += sum;
}

/* virtual */ bool 
nsRubyBaseContainerFrame::IsFrameOfType(uint32_t aFlags) const 
{
  return nsContainerFrame::IsFrameOfType(aFlags & 
         ~(nsIFrame::eLineParticipant));
}

void nsRubyBaseContainerFrame::AppendTextContainer(nsIFrame* aFrame)
{
  nsRubyTextContainerFrame* rtcFrame = do_QueryFrame(aFrame);
  MOZ_ASSERT(rtcFrame, "Must provide a ruby text container.");

  nsTArray<nsRubyTextContainerFrame*>* containers = &mTextContainers;
  if (!GetPrevContinuation() && !GetNextContinuation()) {
    nsIFrame* onlyChild = rtcFrame->PrincipalChildList().OnlyChild();
    if (onlyChild && onlyChild->IsPseudoFrame(rtcFrame->GetContent())) {
      // Per CSS Ruby spec, if the only child of an rtc frame is
      // a pseudo rt frame, it spans all bases in the segment.
      containers = &mSpanContainers;
    }
  }
  containers->AppendElement(rtcFrame);
}

void nsRubyBaseContainerFrame::ClearTextContainers() {
  mSpanContainers.Clear();
  mTextContainers.Clear();
}

/* virtual */ bool
nsRubyBaseContainerFrame::CanContinueTextRun() const
{
  return true;
}

/* virtual */ LogicalSize
nsRubyBaseContainerFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                                      WritingMode aWM,
                                      const LogicalSize& aCBSize,
                                      nscoord aAvailableISize,
                                      const LogicalSize& aMargin,
                                      const LogicalSize& aBorder,
                                      const LogicalSize& aPadding,
                                      ComputeSizeFlags aFlags)
{
  // Ruby base container frame is inline,
  // hence don't compute size before reflow.
  return LogicalSize(aWM, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

/* virtual */ nscoord
nsRubyBaseContainerFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  return mBaseline;
}

// Check whether the given extra isize can fit in the line in base level.
static bool
ShouldBreakBefore(const nsHTMLReflowState& aReflowState, nscoord aExtraISize)
{
  nsLineLayout* lineLayout = aReflowState.mLineLayout;
  int32_t offset;
  gfxBreakPriority priority;
  nscoord icoord = lineLayout->GetCurrentICoord();
  return icoord + aExtraISize > aReflowState.AvailableISize() &&
         lineLayout->GetLastOptionalBreakPosition(&offset, &priority);
}

/* virtual */ void
nsRubyBaseContainerFrame::Reflow(nsPresContext* aPresContext,
                                 nsHTMLReflowMetrics& aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRubyBaseContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  if (!aReflowState.mLineLayout) {
    NS_ASSERTION(
      aReflowState.mLineLayout,
      "No line layout provided to RubyBaseContainerFrame reflow method.");
    aStatus = NS_FRAME_COMPLETE;
    return;
  }

  MoveOverflowToChildList();
  // Ask text containers to drain overflows
  const uint32_t rtcCount = mTextContainers.Length();
  for (uint32_t i = 0; i < rtcCount; i++) {
    mTextContainers[i]->MoveOverflowToChildList();
  }

  aStatus = NS_FRAME_COMPLETE;
  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  LogicalSize availSize(lineWM, aReflowState.AvailableWidth(),
                        aReflowState.AvailableHeight());

  const uint32_t spanCount = mSpanContainers.Length();
  const uint32_t totalCount = rtcCount + spanCount;
  // We have a reflow state and a line layout for each RTC.
  // They are conceptually the state of the RTCs, but we don't actually
  // reflow those RTCs in this code. These two arrays are holders of
  // the reflow states and line layouts.
  // Since there are pointers refer to reflow states and line layouts,
  // it is necessary to guarantee that they won't be moved. For this
  // reason, they are wrapped in UniquePtr here.
  nsAutoTArray<UniquePtr<nsHTMLReflowState>, RTC_ARRAY_SIZE> reflowStates;
  nsAutoTArray<UniquePtr<nsLineLayout>, RTC_ARRAY_SIZE> lineLayouts;
  reflowStates.SetCapacity(totalCount);
  lineLayouts.SetCapacity(totalCount);

  nsAutoTArray<nsHTMLReflowState*, RTC_ARRAY_SIZE> rtcReflowStates;
  nsAutoTArray<nsHTMLReflowState*, RTC_ARRAY_SIZE> spanReflowStates;
  rtcReflowStates.SetCapacity(rtcCount);
  spanReflowStates.SetCapacity(spanCount);

  // Begin the line layout for each ruby text container in advance.
  for (uint32_t i = 0; i < totalCount; i++) {
    nsIFrame* textContainer;
    nsTArray<nsHTMLReflowState*>* reflowStateArray;
    if (i < rtcCount) {
      textContainer = mTextContainers[i];
      reflowStateArray = &rtcReflowStates;
    } else {
      textContainer = mSpanContainers[i - rtcCount];
      reflowStateArray = &spanReflowStates;
    }
    nsHTMLReflowState* reflowState = new nsHTMLReflowState(
      aPresContext, *aReflowState.parentReflowState, textContainer, availSize);
    reflowStates.AppendElement(reflowState);
    reflowStateArray->AppendElement(reflowState);
    nsLineLayout* lineLayout = new nsLineLayout(aPresContext,
                                                reflowState->mFloatManager,
                                                reflowState, nullptr,
                                                aReflowState.mLineLayout);
    lineLayouts.AppendElement(lineLayout);

    // Line number is useless for ruby text
    // XXX nullptr here may cause problem, see comments for
    //     nsLineLayout::mBlockRS and nsLineLayout::AddFloat
    lineLayout->Init(nullptr, reflowState->CalcLineHeight(), -1);
    reflowState->mLineLayout = lineLayout;

    LogicalMargin borderPadding = reflowState->ComputedLogicalBorderPadding();
    nscoord containerWidth =
      reflowState->ComputedWidth() + borderPadding.LeftRight(lineWM);

    lineLayout->BeginLineReflow(borderPadding.IStart(lineWM),
                                borderPadding.BStart(lineWM),
                                reflowState->ComputedISize(),
                                NS_UNCONSTRAINEDSIZE,
                                false, false, lineWM, containerWidth);
    lineLayout->AttachRootFrameToBaseLineLayout();
  }

  WritingMode frameWM = aReflowState.GetWritingMode();
  LogicalMargin borderPadding = aReflowState.ComputedLogicalBorderPadding();
  nscoord startEdge = borderPadding.IStart(frameWM);
  nscoord endEdge = aReflowState.AvailableISize() - borderPadding.IEnd(frameWM);
  aReflowState.mLineLayout->BeginSpan(this, &aReflowState,
                                      startEdge, endEdge, &mBaseline);

  // Reflow pairs excluding any span
  nscoord pairsISize = ReflowPairs(aPresContext, aReflowState,
                                   rtcReflowStates, aStatus);
  nscoord isize = pairsISize;

  // If there exists any span, the pairs must either be completely
  // reflowed, or be not reflowed at all.
  MOZ_ASSERT(NS_INLINE_IS_BREAK_BEFORE(aStatus) ||
             NS_FRAME_IS_COMPLETE(aStatus) || mSpanContainers.IsEmpty());
  if (!NS_INLINE_IS_BREAK_BEFORE(aStatus) &&
      NS_FRAME_IS_COMPLETE(aStatus) && !mSpanContainers.IsEmpty()) {
    // Reflow spans
    nscoord spanISize = ReflowSpans(aPresContext, aReflowState,
                                    spanReflowStates, aStatus);
    // ReflowSpans never reports break or incomplete, but we still need
    // to check if it exceed the line.
    MOZ_ASSERT(aStatus == NS_FRAME_COMPLETE);
    if (isize < spanISize) {
      nscoord delta = spanISize - isize;
      if (ShouldBreakBefore(aReflowState, delta)) {
        aStatus = NS_INLINE_LINE_BREAK_BEFORE();
      } else {
        aReflowState.mLineLayout->AdvanceICoord(delta);
        isize = spanISize;
      }
    }
  }

  DebugOnly<nscoord> lineSpanSize = aReflowState.mLineLayout->EndSpan(this);
  // When there are no frames inside the ruby base container, EndSpan
  // will return 0. However, in this case, the actual width of the
  // container could be non-zero because of non-empty ruby annotations.
  MOZ_ASSERT(NS_INLINE_IS_BREAK_BEFORE(aStatus) ||
             isize == lineSpanSize || mFrames.IsEmpty());
  for (uint32_t i = 0; i < totalCount; i++) {
    // It happens before the ruby text container is reflowed, and that
    // when it is reflowed, it will just use this size.
    nsRubyTextContainerFrame* textContainer = i < rtcCount ?
      mTextContainers[i] : mSpanContainers[i - rtcCount];
    textContainer->SetISize(isize);
    lineLayouts[i]->EndLineReflow();
  }

  aDesiredSize.ISize(lineWM) = isize;
  nsLayoutUtils::SetBSizeFromFontMetrics(this, aDesiredSize, aReflowState,
                                         borderPadding, lineWM, frameWM);
}

/**
 * This struct stores the continuations after this frame and
 * corresponding text containers. It is used to speed up looking
 * ahead for nonempty continuations.
 */
struct MOZ_STACK_CLASS nsRubyBaseContainerFrame::PullFrameState
{
  ContinuationTraversingState mBase;
  nsAutoTArray<ContinuationTraversingState, RTC_ARRAY_SIZE> mTexts;

  explicit PullFrameState(nsRubyBaseContainerFrame* aFrame);
};

nscoord
nsRubyBaseContainerFrame::ReflowPairs(nsPresContext* aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsTArray<nsHTMLReflowState*>& aReflowStates,
                                      nsReflowStatus& aStatus)
{
  nsLineLayout* lineLayout = aReflowState.mLineLayout;
  if (!lineLayout->LineIsEmpty()) {
    // Record break position only if the line is not empty.
    if (lineLayout->NotifyOptionalBreakPosition(
          this, 0, true, gfxBreakPriority::eNormalBreak)) {
      aStatus = NS_INLINE_LINE_BREAK_BEFORE();
      return 0;
    }
  }

  const uint32_t rtcCount = mTextContainers.Length();
  nscoord istart = lineLayout->GetCurrentICoord();
  nscoord icoord = istart;
  nsReflowStatus reflowStatus = NS_FRAME_COMPLETE;
  aStatus = NS_FRAME_COMPLETE;

  mPairCount = 0;
  nsIFrame* baseFrame = nullptr;
  nsAutoTArray<nsIFrame*, RTC_ARRAY_SIZE> textFrames;
  textFrames.SetCapacity(rtcCount);
  PairEnumerator e(this, mTextContainers);
  for (; !e.AtEnd(); e.Next()) {
    e.GetFrames(baseFrame, textFrames);
    icoord += ReflowOnePair(aPresContext, aReflowState, aReflowStates,
                            baseFrame, textFrames, reflowStatus);
    if (NS_INLINE_IS_BREAK(reflowStatus)) {
      break;
    }
    // We are not handling overflow here.
    MOZ_ASSERT(reflowStatus == NS_FRAME_COMPLETE);
  }

  bool isComplete = false;
  PullFrameState pullFrameState(this);
  while (!NS_INLINE_IS_BREAK(reflowStatus)) {
    // We are not handling overflow here.
    MOZ_ASSERT(reflowStatus == NS_FRAME_COMPLETE);

    // Try pull some frames from next continuations. This call replaces
    // |baseFrame| and |textFrames| with the frame pulled in each level.
    PullOnePair(lineLayout, pullFrameState, baseFrame, textFrames, isComplete);
    if (isComplete) {
      // No more frames can be pulled.
      break;
    }
    icoord += ReflowOnePair(aPresContext, aReflowState, aReflowStates,
                            baseFrame, textFrames, reflowStatus);
  }

  if (!e.AtEnd() && NS_INLINE_IS_BREAK_AFTER(reflowStatus)) {
    // The current pair has been successfully placed.
    // Skip to the next pair and mark break before.
    e.Next();
    e.GetFrames(baseFrame, textFrames);
    reflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
  }
  if (!e.AtEnd() || (GetNextInFlow() && !isComplete)) {
    NS_FRAME_SET_INCOMPLETE(aStatus);
  } else {
    // Completely reflow the whole segment, record a break position.
    // We record an extra break position here because ReflowOnePair
    // won't record any break position if there exist spans.
    if (lineLayout->NotifyOptionalBreakPosition(
          this, INT32_MAX, true, gfxBreakPriority::eNormalBreak)) {
      reflowStatus = NS_INLINE_LINE_BREAK_AFTER(reflowStatus);
    }
  }

  if (NS_INLINE_IS_BREAK_BEFORE(reflowStatus)) {
    if (!mPairCount || !mSpanContainers.IsEmpty()) {
      // If no pair has been placed yet, or we have any span,
      // the whole container should be in the next line.
      aStatus = NS_INLINE_LINE_BREAK_BEFORE();
      return 0;
    }
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
    MOZ_ASSERT(NS_FRAME_IS_COMPLETE(aStatus) || mSpanContainers.IsEmpty());

    if (baseFrame) {
      PushChildren(baseFrame, baseFrame->GetPrevSibling());
    }
    for (uint32_t i = 0; i < rtcCount; i++) {
      nsIFrame* textFrame = textFrames[i];
      if (textFrame) {
        mTextContainers[i]->PushChildren(textFrame,
                                         textFrame->GetPrevSibling());
      }
    }
  } else if (NS_INLINE_IS_BREAK_AFTER(reflowStatus)) {
    // |reflowStatus| being break after here may only happen when
    // there is a break after the pair just pulled, or the whole
    // segment has been completely reflowed. In those cases, we do
    // not need to push anything.
    MOZ_ASSERT(e.AtEnd());
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
  }

  return icoord - istart;
}

nscoord
nsRubyBaseContainerFrame::ReflowOnePair(nsPresContext* aPresContext,
                                        const nsHTMLReflowState& aReflowState,
                                        nsTArray<nsHTMLReflowState*>& aReflowStates,
                                        nsIFrame* aBaseFrame,
                                        const nsTArray<nsIFrame*>& aTextFrames,
                                        nsReflowStatus& aStatus)
{
  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  const uint32_t rtcCount = mTextContainers.Length();
  MOZ_ASSERT(aTextFrames.Length() == rtcCount);
  MOZ_ASSERT(aReflowStates.Length() == rtcCount);
  nscoord istart = aReflowState.mLineLayout->GetCurrentICoord();
  nscoord pairISize = 0;

  // Reflow text frames
  for (uint32_t i = 0; i < rtcCount; i++) {
    if (aTextFrames[i]) {
      MOZ_ASSERT(aTextFrames[i]->GetType() == nsGkAtoms::rubyTextFrame);
      nsReflowStatus reflowStatus;
      nsHTMLReflowMetrics metrics(*aReflowStates[i]);

      bool pushedFrame;
      aReflowStates[i]->mLineLayout->ReflowFrame(aTextFrames[i], reflowStatus,
                                                 &metrics, pushedFrame);
      if (NS_INLINE_IS_BREAK(reflowStatus)) {
        // If any breaking occurs when reflowing a ruby text frame,
        // we should abandon reflowing this pair.
        aStatus = NS_INLINE_LINE_BREAK_BEFORE();
        return 0;
      }
      MOZ_ASSERT(!pushedFrame, "Shouldn't push frame if there is no break");
      pairISize = std::max(pairISize, metrics.ISize(lineWM));
    }
  }
  if (ShouldBreakBefore(aReflowState, pairISize)) {
    // Since ruby text container uses an independent line layout, it
    // may successfully place a frame because the line is empty while
    // the line of base container is not.
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return 0;
  }

  // Reflow the base frame
  if (aBaseFrame) {
    MOZ_ASSERT(aBaseFrame->GetType() == nsGkAtoms::rubyBaseFrame);
    nsReflowStatus reflowStatus;
    nsHTMLReflowMetrics metrics(aReflowState);

    bool pushedFrame;
    aReflowState.mLineLayout->ReflowFrame(aBaseFrame, reflowStatus,
                                          &metrics, pushedFrame);
    if (NS_INLINE_IS_BREAK(reflowStatus)) {
      // We cannot place the ruby base frame. Abandon this pair.
      aStatus = NS_INLINE_LINE_BREAK_BEFORE();
      return 0;
    }
    MOZ_ASSERT(!pushedFrame, "Shouldn't push frame if there is no break");
    pairISize = std::max(pairISize, metrics.ISize(lineWM));
  }

  // Align all the line layout to the new coordinate.
  nscoord icoord = istart + pairISize;
  aReflowState.mLineLayout->AdvanceICoord(
    icoord - aReflowState.mLineLayout->GetCurrentICoord());
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsLineLayout* lineLayout = aReflowStates[i]->mLineLayout;
    lineLayout->AdvanceICoord(icoord - lineLayout->GetCurrentICoord());
    if (aBaseFrame && aTextFrames[i]) {
      lineLayout->AttachLastFrameToBaseLineLayout();
    }
  }

  mPairCount++;
  // We only break between bases if there is no span.
  if (mSpanContainers.IsEmpty()) {
    if (aReflowState.mLineLayout->NotifyOptionalBreakPosition(
          this, mPairCount, icoord <= aReflowState.AvailableISize(),
          gfxBreakPriority::eNormalBreak)) {
      aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
    }
  }

  return pairISize;
}

nsRubyBaseContainerFrame::PullFrameState::PullFrameState(
    nsRubyBaseContainerFrame* aFrame)
  : mBase(aFrame)
{
  const uint32_t rtcCount = aFrame->mTextContainers.Length();
  for (uint32_t i = 0; i < rtcCount; i++) {
    mTexts.AppendElement(aFrame->mTextContainers[i]);
  }
}

void
nsRubyBaseContainerFrame::PullOnePair(nsLineLayout* aLineLayout,
                                      PullFrameState& aPullFrameState,
                                      nsIFrame*& aBaseFrame,
                                      nsTArray<nsIFrame*>& aTextFrames,
                                      bool& aIsComplete)
{
  const uint32_t rtcCount = mTextContainers.Length();

  aBaseFrame = PullNextInFlowChild(aPullFrameState.mBase);
  aIsComplete = !aBaseFrame;

  aTextFrames.ClearAndRetainStorage();
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsIFrame* nextText =
      mTextContainers[i]->PullNextInFlowChild(aPullFrameState.mTexts[i]);
    aTextFrames.AppendElement(nextText);
    // If there exists any frame in continations, we haven't
    // completed the reflow process.
    aIsComplete = aIsComplete && !nextText;
  }

  if (!aIsComplete) {
    // We pulled frames from the next line, hence mark it dirty.
    aLineLayout->SetDirtyNextLine();
  }
}

nscoord
nsRubyBaseContainerFrame::ReflowSpans(nsPresContext* aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsTArray<nsHTMLReflowState*>& aReflowStates,
                                      nsReflowStatus& aStatus)
{
  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  const uint32_t spanCount = mSpanContainers.Length();
  nscoord spanISize = 0;

  for (uint32_t i = 0; i < spanCount; i++) {
    nsRubyTextContainerFrame* container = mSpanContainers[i];
    nsIFrame* rtFrame = container->GetFirstPrincipalChild();
    nsReflowStatus reflowStatus;
    nsHTMLReflowMetrics metrics(*aReflowStates[i]);
    bool pushedFrame;
    aReflowStates[i]->mLineLayout->ReflowFrame(rtFrame, reflowStatus,
                                               &metrics, pushedFrame);
    // It should never cause break, since it is always
    // at the beginning in its line layout.
    MOZ_ASSERT(!NS_INLINE_IS_BREAK(reflowStatus) && !pushedFrame);
    spanISize = std::max(spanISize, metrics.ISize(lineWM));
  }

  return spanISize;
}
