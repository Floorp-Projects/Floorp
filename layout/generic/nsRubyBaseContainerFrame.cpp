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

#define RTC_ARRAY_SIZE 1

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
  void GetTextFrames(nsTArray<nsIFrame*>& aFrames) const;

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
PairEnumerator::GetTextFrames(nsTArray<nsIFrame*>& aFrames) const
{
  aFrames.ClearAndRetainStorage();
  for (uint32_t i = 1, iend = mFrames.Length(); i < iend; i++) {
    aFrames.AppendElement(mFrames[i]);
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

  nsIFrame* onlyChild = rtcFrame->PrincipalChildList().OnlyChild();
  if (onlyChild && onlyChild->IsPseudoFrame(rtcFrame->GetContent())) {
    // Per CSS Ruby spec, if the only child of an rtc frame is
    // a pseudo rt frame, it spans all bases in the segment.
    mSpanContainers.AppendElement(rtcFrame);
  } else {
    mTextContainers.AppendElement(rtcFrame);
  }
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

  aStatus = NS_FRAME_COMPLETE;
  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  WritingMode frameWM = aReflowState.GetWritingMode();
  LogicalMargin borderPadding = aReflowState.ComputedLogicalBorderPadding();
  nscoord startEdge = borderPadding.IStart(frameWM);
  nscoord endEdge = aReflowState.AvailableISize() - borderPadding.IEnd(frameWM);

  aReflowState.mLineLayout->BeginSpan(this, &aReflowState,
                                      startEdge, endEdge, &mBaseline);

  LogicalSize availSize(lineWM, aReflowState.AvailableWidth(),
                        aReflowState.AvailableHeight());

  const uint32_t rtcCount = mTextContainers.Length();
  const uint32_t spanCount = mSpanContainers.Length();
  const uint32_t totalCount = rtcCount + spanCount;
  // We have a reflow state and a line layout for each RTC.
  // They are conceptually the state of the RTCs, but we don't actually
  // reflow those RTCs in this code. These two arrays are holders of
  // the reflow states and line layouts.
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
    nsLineLayout* lineLayout = new nsLineLayout(
      aPresContext, reflowState->mFloatManager, reflowState, nullptr);
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
  }

  // Reflow non-span annotations and bases
  nscoord pairsISize = ReflowPairs(aPresContext, aReflowState,
                                   rtcReflowStates, aStatus);
  nscoord isize = pairsISize;

  // Reflow spans
  nscoord spanISize = ReflowSpans(aPresContext, aReflowState,
                                  spanReflowStates, aStatus);
  if (isize < spanISize) {
    aReflowState.mLineLayout->AdvanceICoord(spanISize - isize);
    isize = spanISize;
  }

  DebugOnly<nscoord> lineSpanSize = aReflowState.mLineLayout->EndSpan(this);
  // When there are no frames inside the ruby base container, EndSpan
  // will return 0. However, in this case, the actual width of the
  // container could be non-zero because of non-empty ruby annotations.
  MOZ_ASSERT(isize == lineSpanSize || mFrames.IsEmpty());
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

nscoord
nsRubyBaseContainerFrame::ReflowPairs(nsPresContext* aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsTArray<nsHTMLReflowState*>& aReflowStates,
                                      nsReflowStatus& aStatus)
{
  nscoord istart = aReflowState.mLineLayout->GetCurrentICoord();
  nscoord icoord = istart;

  nsAutoTArray<nsIFrame*, RTC_ARRAY_SIZE> textFrames;
  textFrames.SetCapacity(mTextContainers.Length());
  for (PairEnumerator e(this, mTextContainers); !e.AtEnd(); e.Next()) {
    e.GetTextFrames(textFrames);
    icoord += ReflowOnePair(aPresContext, aReflowState, aReflowStates,
                            e.GetBaseFrame(), textFrames, aStatus);
  }

  return icoord - istart;
}

/* static */ nscoord
nsRubyBaseContainerFrame::ReflowOnePair(nsPresContext* aPresContext,
                                        const nsHTMLReflowState& aReflowState,
                                        nsTArray<nsHTMLReflowState*>& aReflowStates,
                                        nsIFrame* aBaseFrame,
                                        const nsTArray<nsIFrame*>& aTextFrames,
                                        nsReflowStatus& aStatus)
{
  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  const uint32_t rtcCount = aTextFrames.Length();
  MOZ_ASSERT(aReflowStates.Length() == rtcCount);
  nscoord istart = aReflowState.mLineLayout->GetCurrentICoord();
  nscoord pairISize = 0;

  for (uint32_t i = 0; i < rtcCount; i++) {
    if (aTextFrames[i]) {
      MOZ_ASSERT(aTextFrames[i]->GetType() == nsGkAtoms::rubyTextFrame);
      nsReflowStatus reflowStatus;
      nsHTMLReflowMetrics metrics(*aReflowStates[i]);

      bool pushedFrame;
      aReflowStates[i]->mLineLayout->ReflowFrame(aTextFrames[i], reflowStatus,
                                                 &metrics, pushedFrame);
      NS_ASSERTION(!NS_INLINE_IS_BREAK(reflowStatus),
                   "Ruby line breaking is not yet implemented");
      NS_ASSERTION(!pushedFrame, "Ruby line breaking is not yet implemented");
      pairISize = std::max(pairISize, metrics.ISize(lineWM));
    }
  }

  if (aBaseFrame) {
    MOZ_ASSERT(aBaseFrame->GetType() == nsGkAtoms::rubyBaseFrame);
    nsReflowStatus reflowStatus;
    nsHTMLReflowMetrics metrics(aReflowState);

    bool pushedFrame;
    aReflowState.mLineLayout->ReflowFrame(aBaseFrame, reflowStatus,
                                          &metrics, pushedFrame);
    NS_ASSERTION(!NS_INLINE_IS_BREAK(reflowStatus),
                 "Ruby line breaking is not yet implemented");
    NS_ASSERTION(!pushedFrame, "Ruby line breaking is not yet implemented");
    pairISize = std::max(pairISize, metrics.ISize(lineWM));
  }

  // Align all the line layout to the new coordinate.
  nscoord icoord = istart + pairISize;
  aReflowState.mLineLayout->AdvanceICoord(
    icoord - aReflowState.mLineLayout->GetCurrentICoord());
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsLineLayout* lineLayout = aReflowStates[i]->mLineLayout;
    lineLayout->AdvanceICoord(icoord - lineLayout->GetCurrentICoord());
  }

  return pairISize;
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
    NS_ASSERTION(!NS_INLINE_IS_BREAK(reflowStatus),
                 "Ruby line breaking is not yet implemented");
    NS_ASSERTION(!pushedFrame, "Ruby line breaking is not yet implemented");
    spanISize = std::max(spanISize, metrics.ISize(lineWM));
  }

  return spanISize;
}
