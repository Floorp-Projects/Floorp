/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-base-container" */

#include "nsRubyBaseContainerFrame.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/WritingModes.h"
#include "nsContentUtils.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleStructInlines.h"
#include "nsTextFrame.h"
#include "RubyUtils.h"

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

/**
 * Ruby column is a unit consists of one ruby base and all ruby
 * annotations paired with it.
 * See http://dev.w3.org/csswg/css-ruby/#ruby-pairing
 */
struct MOZ_STACK_CLASS mozilla::RubyColumn
{
  nsRubyBaseFrame* mBaseFrame;
  nsAutoTArray<nsRubyTextFrame*, RTC_ARRAY_SIZE> mTextFrames;
  bool mIsIntraLevelWhitespace;
  RubyColumn() : mBaseFrame(nullptr), mIsIntraLevelWhitespace(false) { }
};

class MOZ_STACK_CLASS RubyColumnEnumerator
{
public:
  RubyColumnEnumerator(nsRubyBaseContainerFrame* aRBCFrame,
                       const nsTArray<nsRubyTextContainerFrame*>& aRTCFrames);

  void Next();
  bool AtEnd() const;

  uint32_t GetLevelCount() const { return mFrames.Length(); }
  nsRubyContentFrame* GetFrameAtLevel(uint32_t aIndex) const;
  void GetColumn(RubyColumn& aColumn) const;

private:
  // Frames in this array are NOT necessary part of the current column.
  // When in doubt, use GetFrameAtLevel to access it.
  // See GetFrameAtLevel() and Next() for more info.
  nsAutoTArray<nsRubyContentFrame*, RTC_ARRAY_SIZE + 1> mFrames;
  // Whether we are on a column for intra-level whitespaces
  bool mAtIntraLevelWhitespace;
};

RubyColumnEnumerator::RubyColumnEnumerator(
  nsRubyBaseContainerFrame* aBaseContainer,
  const nsTArray<nsRubyTextContainerFrame*>& aTextContainers)
  : mAtIntraLevelWhitespace(false)
{
  const uint32_t rtcCount = aTextContainers.Length();
  mFrames.SetCapacity(rtcCount + 1);

  nsIFrame* rbFrame = aBaseContainer->GetFirstPrincipalChild();
  MOZ_ASSERT(!rbFrame || rbFrame->GetType() == nsGkAtoms::rubyBaseFrame);
  mFrames.AppendElement(static_cast<nsRubyContentFrame*>(rbFrame));
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsRubyTextContainerFrame* container = aTextContainers[i];
    // If the container is for span, leave a nullptr here.
    // Spans do not take part in pairing.
    nsIFrame* rtFrame = !container->IsSpanContainer() ?
      container->GetFirstPrincipalChild() : nullptr;
    MOZ_ASSERT(!rtFrame || rtFrame->GetType() == nsGkAtoms::rubyTextFrame);
    mFrames.AppendElement(static_cast<nsRubyContentFrame*>(rtFrame));
  }

  // We have to init mAtIntraLevelWhitespace to be correct for the
  // first column. There are two ways we could end up with intra-level
  // whitespace in our first colum:
  // 1. The current segment itself is an inter-segment whitespace;
  // 2. If our ruby segment is split across multiple lines, and some
  //    intra-level whitespace happens to fall right after a line-break.
  //    Each line will get its own nsRubyBaseContainerFrame, and the
  //    container right after the line-break will end up with its first
  //    column containing that intra-level whitespace.
  for (uint32_t i = 0, iend = mFrames.Length(); i < iend; i++) {
    nsRubyContentFrame* frame = mFrames[i];
    if (frame && frame->IsIntraLevelWhitespace()) {
      mAtIntraLevelWhitespace = true;
      break;
    }
  }
}

void
RubyColumnEnumerator::Next()
{
  bool advancingToIntraLevelWhitespace = false;
  for (uint32_t i = 0, iend = mFrames.Length(); i < iend; i++) {
    nsRubyContentFrame* frame = mFrames[i];
    // If we've got intra-level whitespace frames at some levels in the
    // current ruby column, we "faked" an anonymous box for all other
    // levels for this column. So when we advance off this column, we
    // don't advance any of the frames in those levels, because we're
    // just advancing across the "fake" frames.
    if (frame && (!mAtIntraLevelWhitespace ||
                  frame->IsIntraLevelWhitespace())) {
      nsIFrame* nextSibling = frame->GetNextSibling();
      MOZ_ASSERT(!nextSibling || nextSibling->GetType() == frame->GetType(),
                 "Frame type should be identical among a level");
      mFrames[i] = frame = static_cast<nsRubyContentFrame*>(nextSibling);
      if (!advancingToIntraLevelWhitespace &&
          frame && frame->IsIntraLevelWhitespace()) {
        advancingToIntraLevelWhitespace = true;
      }
    }
  }
  MOZ_ASSERT(!advancingToIntraLevelWhitespace || !mAtIntraLevelWhitespace,
             "Should never have adjacent intra-level whitespace columns");
  mAtIntraLevelWhitespace = advancingToIntraLevelWhitespace;
}

bool
RubyColumnEnumerator::AtEnd() const
{
  for (uint32_t i = 0, iend = mFrames.Length(); i < iend; i++) {
    if (mFrames[i]) {
      return false;
    }
  }
  return true;
}

nsRubyContentFrame*
RubyColumnEnumerator::GetFrameAtLevel(uint32_t aIndex) const
{
  // If the current ruby column is for intra-level whitespaces, we
  // return nullptr for any levels that do not have an actual intra-
  // level whitespace frame in this column.  This nullptr represents
  // an anonymous empty intra-level whitespace box.  (In this case,
  // it's important that we NOT return mFrames[aIndex], because it's
  // really part of the next column, not the current one.)
  nsRubyContentFrame* frame = mFrames[aIndex];
  return !mAtIntraLevelWhitespace ||
         (frame && frame->IsIntraLevelWhitespace()) ? frame : nullptr;
}

void
RubyColumnEnumerator::GetColumn(RubyColumn& aColumn) const
{
  nsRubyContentFrame* rbFrame = GetFrameAtLevel(0);
  MOZ_ASSERT(!rbFrame || rbFrame->GetType() == nsGkAtoms::rubyBaseFrame);
  aColumn.mBaseFrame = static_cast<nsRubyBaseFrame*>(rbFrame);
  aColumn.mTextFrames.ClearAndRetainStorage();
  for (uint32_t i = 1, iend = mFrames.Length(); i < iend; i++) {
    nsRubyContentFrame* rtFrame = GetFrameAtLevel(i);
    MOZ_ASSERT(!rtFrame || rtFrame->GetType() == nsGkAtoms::rubyTextFrame);
    aColumn.mTextFrames.AppendElement(static_cast<nsRubyTextFrame*>(rtFrame));
  }
  aColumn.mIsIntraLevelWhitespace = mAtIntraLevelWhitespace;
}

static gfxBreakPriority
LineBreakBefore(nsIFrame* aFrame,
                nsRenderingContext* aRenderingContext,
                nsIFrame* aLineContainerFrame,
                const nsLineList::iterator* aLine)
{
  for (nsIFrame* child = aFrame; child;
       child = child->GetFirstPrincipalChild()) {
    if (!child->CanContinueTextRun()) {
      // It is not an inline element. We can break before it.
      return gfxBreakPriority::eNormalBreak;
    }
    if (child->GetType() != nsGkAtoms::textFrame) {
      continue;
    }

    auto textFrame = static_cast<nsTextFrame*>(child);
    gfxSkipCharsIterator iter =
      textFrame->EnsureTextRun(nsTextFrame::eInflated,
                               aRenderingContext->ThebesContext(),
                               aLineContainerFrame, aLine);
    iter.SetOriginalOffset(textFrame->GetContentOffset());
    uint32_t pos = iter.GetSkippedOffset();
    gfxTextRun* textRun = textFrame->GetTextRun(nsTextFrame::eInflated);
    if (pos >= textRun->GetLength()) {
      // The text frame contains no character at all.
      return gfxBreakPriority::eNoBreak;
    }
    // Return whether we can break before the first character.
    if (textRun->CanBreakLineBefore(pos)) {
      return gfxBreakPriority::eNormalBreak;
    }
    // Check whether we can wrap word here.
    const nsStyleText* textStyle = textFrame->StyleText();
    if (textStyle->WordCanWrap(textFrame) && textRun->IsClusterStart(pos)) {
      return gfxBreakPriority::eWordWrapBreak;
    }
    // We cannot break before.
    return gfxBreakPriority::eNoBreak;
  }
  // Neither block, nor text frame is found as a leaf. We won't break
  // before this base frame. It is the behavior of empty spans.
  return gfxBreakPriority::eNoBreak;
}

static void
GetIsLineBreakAllowed(nsIFrame* aFrame, bool aIsLineBreakable,
                      bool* aAllowInitialLineBreak, bool* aAllowLineBreak)
{
  nsIFrame* parent = aFrame->GetParent();
  bool lineBreakSuppressed = parent->StyleContext()->ShouldSuppressLineBreak();
  // Allow line break between ruby bases when white-space allows,
  // we are not inside a nested ruby, and there is no span.
  bool allowLineBreak = !lineBreakSuppressed &&
                        aFrame->StyleText()->WhiteSpaceCanWrap(aFrame);
  bool allowInitialLineBreak = allowLineBreak;
  if (!aFrame->GetPrevInFlow()) {
    allowInitialLineBreak = !lineBreakSuppressed &&
                            parent->StyleText()->WhiteSpaceCanWrap(parent);
  }
  if (!aIsLineBreakable) {
    allowInitialLineBreak = false;
  }
  *aAllowInitialLineBreak = allowInitialLineBreak;
  *aAllowLineBreak = allowLineBreak;
}

static nscoord
CalculateColumnPrefISize(nsRenderingContext* aRenderingContext,
                         const RubyColumnEnumerator& aEnumerator)
{
  nscoord max = 0;
  uint32_t levelCount = aEnumerator.GetLevelCount();
  for (uint32_t i = 0; i < levelCount; i++) {
    nsIFrame* frame = aEnumerator.GetFrameAtLevel(i);
    if (frame) {
      nsIFrame::InlinePrefISizeData data;
      frame->AddInlinePrefISize(aRenderingContext, &data);
      MOZ_ASSERT(data.prevLines == 0, "Shouldn't have prev lines");
      max = std::max(max, data.currentLine);
    }
  }
  return max;
}

// FIXME Currently we use pref isize of ruby content frames for
//       computing min isize of ruby frame, which may cause problem.
//       See bug 1134945.
/* virtual */ void
nsRubyBaseContainerFrame::AddInlineMinISize(
  nsRenderingContext *aRenderingContext, nsIFrame::InlineMinISizeData *aData)
{
  AutoTextContainerArray textContainers;
  GetTextContainers(textContainers);

  for (uint32_t i = 0, iend = textContainers.Length(); i < iend; i++) {
    if (textContainers[i]->IsSpanContainer()) {
      // Since spans are not breakable internally, use our pref isize
      // directly if there is any span.
      nsIFrame::InlinePrefISizeData data;
      AddInlinePrefISize(aRenderingContext, &data);
      aData->currentLine += data.currentLine;
      if (data.currentLine > 0) {
        aData->atStartOfLine = false;
      }
      return;
    }
  }

  bool firstFrame = true;
  bool allowInitialLineBreak, allowLineBreak;
  GetIsLineBreakAllowed(this, !aData->atStartOfLine,
                        &allowInitialLineBreak, &allowLineBreak);
  for (nsIFrame* frame = this; frame; frame = frame->GetNextInFlow()) {
    RubyColumnEnumerator enumerator(
      static_cast<nsRubyBaseContainerFrame*>(frame), textContainers);
    for (; !enumerator.AtEnd(); enumerator.Next()) {
      if (firstFrame ? allowInitialLineBreak : allowLineBreak) {
        nsIFrame* baseFrame = enumerator.GetFrameAtLevel(0);
        if (baseFrame) {
          gfxBreakPriority breakPriority =
            LineBreakBefore(baseFrame, aRenderingContext, nullptr, nullptr);
          if (breakPriority != gfxBreakPriority::eNoBreak) {
            aData->OptionallyBreak(aRenderingContext);
          }
        }
      }
      firstFrame = false;
      nscoord isize = CalculateColumnPrefISize(aRenderingContext, enumerator);
      aData->currentLine += isize;
      if (isize > 0) {
        aData->atStartOfLine = false;
      }
    }
  }
}

/* virtual */ void
nsRubyBaseContainerFrame::AddInlinePrefISize(
  nsRenderingContext *aRenderingContext, nsIFrame::InlinePrefISizeData *aData)
{
  AutoTextContainerArray textContainers;
  GetTextContainers(textContainers);

  nscoord sum = 0;
  for (nsIFrame* frame = this; frame; frame = frame->GetNextInFlow()) {
    RubyColumnEnumerator enumerator(
      static_cast<nsRubyBaseContainerFrame*>(frame), textContainers);
    for (; !enumerator.AtEnd(); enumerator.Next()) {
      sum += CalculateColumnPrefISize(aRenderingContext, enumerator);
    }
  }
  for (uint32_t i = 0, iend = textContainers.Length(); i < iend; i++) {
    if (textContainers[i]->IsSpanContainer()) {
      nsIFrame* frame = textContainers[i]->GetFirstPrincipalChild();
      nsIFrame::InlinePrefISizeData data;
      frame->AddInlinePrefISize(aRenderingContext, &data);
      MOZ_ASSERT(data.prevLines == 0, "Shouldn't have prev lines");
      sum = std::max(sum, data.currentLine);
    }
  }
  aData->currentLine += sum;
}

/* virtual */ bool 
nsRubyBaseContainerFrame::IsFrameOfType(uint32_t aFlags) const 
{
  if (aFlags & eSupportsCSSTransforms) {
    return false;
  }
  return nsContainerFrame::IsFrameOfType(aFlags &
         ~(nsIFrame::eLineParticipant));
}

void
nsRubyBaseContainerFrame::GetTextContainers(TextContainerArray& aTextContainers)
{
  MOZ_ASSERT(aTextContainers.IsEmpty());
  for (RubyTextContainerIterator iter(this); !iter.AtEnd(); iter.Next()) {
    aTextContainers.AppendElement(iter.GetTextContainer());
  }
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

struct nsRubyBaseContainerFrame::ReflowState
{
  bool mAllowInitialLineBreak;
  bool mAllowLineBreak;
  const TextContainerArray& mTextContainers;
  const nsHTMLReflowState& mBaseReflowState;
  const nsTArray<UniquePtr<nsHTMLReflowState>>& mTextReflowStates;
};

/* virtual */ void
nsRubyBaseContainerFrame::Reflow(nsPresContext* aPresContext,
                                 nsHTMLReflowMetrics& aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsRubyBaseContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aStatus = NS_FRAME_COMPLETE;

  if (!aReflowState.mLineLayout) {
    NS_ASSERTION(
      aReflowState.mLineLayout,
      "No line layout provided to RubyBaseContainerFrame reflow method.");
    return;
  }

  AutoTextContainerArray textContainers;
  GetTextContainers(textContainers);

  MoveOverflowToChildList();
  // Ask text containers to drain overflows
  const uint32_t rtcCount = textContainers.Length();
  for (uint32_t i = 0; i < rtcCount; i++) {
    textContainers[i]->MoveOverflowToChildList();
  }

  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  LogicalSize availSize(lineWM, aReflowState.AvailableISize(),
                        aReflowState.AvailableBSize());

  // We have a reflow state and a line layout for each RTC.
  // They are conceptually the state of the RTCs, but we don't actually
  // reflow those RTCs in this code. These two arrays are holders of
  // the reflow states and line layouts.
  // Since there are pointers refer to reflow states and line layouts,
  // it is necessary to guarantee that they won't be moved. For this
  // reason, they are wrapped in UniquePtr here.
  nsAutoTArray<UniquePtr<nsHTMLReflowState>, RTC_ARRAY_SIZE> reflowStates;
  nsAutoTArray<UniquePtr<nsLineLayout>, RTC_ARRAY_SIZE> lineLayouts;
  reflowStates.SetCapacity(rtcCount);
  lineLayouts.SetCapacity(rtcCount);

  // Begin the line layout for each ruby text container in advance.
  bool hasSpan = false;
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsRubyTextContainerFrame* textContainer = textContainers[i];
    if (textContainer->IsSpanContainer()) {
      hasSpan = true;
    }

    nsHTMLReflowState* reflowState = new nsHTMLReflowState(
      aPresContext, *aReflowState.parentReflowState, textContainer,
      availSize.ConvertTo(textContainer->GetWritingMode(), lineWM));
    reflowStates.AppendElement(reflowState);
    nsLineLayout* lineLayout = new nsLineLayout(aPresContext,
                                                reflowState->mFloatManager,
                                                reflowState, nullptr,
                                                aReflowState.mLineLayout);
    lineLayout->SetSuppressLineWrap(true);
    lineLayouts.AppendElement(lineLayout);

    // Line number is useless for ruby text
    // XXX nullptr here may cause problem, see comments for
    //     nsLineLayout::mBlockRS and nsLineLayout::AddFloat
    lineLayout->Init(nullptr, reflowState->CalcLineHeight(), -1);
    reflowState->mLineLayout = lineLayout;

    // Border and padding are suppressed on ruby text containers.
    // If the writing mode is vertical-rl, the horizontal position of
    // rt frames will be updated when reflowing this text container,
    // hence leave container size 0 here for now.
    lineLayout->BeginLineReflow(0, 0, reflowState->ComputedISize(),
                                NS_UNCONSTRAINEDSIZE,
                                false, false, lineWM, nsSize(0, 0));
    lineLayout->AttachRootFrameToBaseLineLayout();
  }

  aReflowState.mLineLayout->BeginSpan(this, &aReflowState,
                                      0, aReflowState.AvailableISize(),
                                      &mBaseline);

  bool allowInitialLineBreak, allowLineBreak;
  GetIsLineBreakAllowed(this, aReflowState.mLineLayout->LineIsBreakable(),
                        &allowInitialLineBreak, &allowLineBreak);

  nscoord isize = 0;
  // Reflow columns excluding any span
  ReflowState reflowState = {
    allowInitialLineBreak, allowLineBreak && !hasSpan,
    textContainers, aReflowState, reflowStates
  };
  isize = ReflowColumns(reflowState, aStatus);
  DebugOnly<nscoord> lineSpanSize = aReflowState.mLineLayout->EndSpan(this);
  aDesiredSize.ISize(lineWM) = isize;
  // When there are no frames inside the ruby base container, EndSpan
  // will return 0. However, in this case, the actual width of the
  // container could be non-zero because of non-empty ruby annotations.
  // XXX When bug 765861 gets fixed, this warning should be upgraded.
  NS_WARN_IF_FALSE(NS_INLINE_IS_BREAK(aStatus) ||
                   isize == lineSpanSize || mFrames.IsEmpty(), "bad isize");

  // If there exists any span, the columns must either be completely
  // reflowed, or be not reflowed at all.
  MOZ_ASSERT(NS_INLINE_IS_BREAK_BEFORE(aStatus) ||
             NS_FRAME_IS_COMPLETE(aStatus) || !hasSpan);
  if (!NS_INLINE_IS_BREAK_BEFORE(aStatus) &&
      NS_FRAME_IS_COMPLETE(aStatus) && hasSpan) {
    // Reflow spans
    ReflowState reflowState = {
      false, false, textContainers, aReflowState, reflowStates
    };
    nscoord spanISize = ReflowSpans(reflowState);
    isize = std::max(isize, spanISize);
    if (isize > aReflowState.AvailableISize() &&
        aReflowState.mLineLayout->HasOptionalBreakPosition()) {
      aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    }
  }

  for (uint32_t i = 0; i < rtcCount; i++) {
    // It happens before the ruby text container is reflowed, and that
    // when it is reflowed, it will just use this size.
    nsRubyTextContainerFrame* textContainer = textContainers[i];
    nsLineLayout* lineLayout = lineLayouts[i].get();

    RubyUtils::ClearReservedISize(textContainer);
    nscoord rtcISize = lineLayout->GetCurrentICoord();
    // Only span containers and containers with collapsed annotations
    // need reserving isize. For normal ruby text containers, their
    // children will be expanded properly. We only need to expand their
    // own size.
    if (!textContainer->IsSpanContainer()) {
      rtcISize = isize;
    } else if (isize > rtcISize) {
      RubyUtils::SetReservedISize(textContainer, isize - rtcISize);
    }

    lineLayout->VerticalAlignLine();
    textContainer->SetISize(rtcISize);
    lineLayout->EndLineReflow();
  }

  // Border and padding are suppressed on ruby base container,
  // create a fake borderPadding for setting BSize.
  WritingMode frameWM = aReflowState.GetWritingMode();
  LogicalMargin borderPadding(frameWM);
  nsLayoutUtils::SetBSizeFromFontMetrics(this, aDesiredSize,
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
  const TextContainerArray& mTextContainers;

  PullFrameState(nsRubyBaseContainerFrame* aBaseContainer,
                 const TextContainerArray& aTextContainers);
};

nscoord
nsRubyBaseContainerFrame::ReflowColumns(const ReflowState& aReflowState,
                                        nsReflowStatus& aStatus)
{
  nsLineLayout* lineLayout = aReflowState.mBaseReflowState.mLineLayout;
  const uint32_t rtcCount = aReflowState.mTextContainers.Length();
  nscoord icoord = lineLayout->GetCurrentICoord();
  MOZ_ASSERT(icoord == 0, "border/padding of rbc should have been suppressed");
  nsReflowStatus reflowStatus = NS_FRAME_COMPLETE;
  aStatus = NS_FRAME_COMPLETE;

  uint32_t columnIndex = 0;
  RubyColumn column;
  column.mTextFrames.SetCapacity(rtcCount);
  RubyColumnEnumerator e(this, aReflowState.mTextContainers);
  for (; !e.AtEnd(); e.Next()) {
    e.GetColumn(column);
    icoord += ReflowOneColumn(aReflowState, columnIndex, column, reflowStatus);
    if (!NS_INLINE_IS_BREAK_BEFORE(reflowStatus)) {
      columnIndex++;
    }
    if (NS_INLINE_IS_BREAK(reflowStatus)) {
      break;
    }
    // We are not handling overflow here.
    MOZ_ASSERT(reflowStatus == NS_FRAME_COMPLETE);
  }

  bool isComplete = false;
  PullFrameState pullFrameState(this, aReflowState.mTextContainers);
  while (!NS_INLINE_IS_BREAK(reflowStatus)) {
    // We are not handling overflow here.
    MOZ_ASSERT(reflowStatus == NS_FRAME_COMPLETE);

    // Try pull some frames from next continuations. This call replaces
    // frames in |column| with the frame pulled in each level.
    PullOneColumn(lineLayout, pullFrameState, column, isComplete);
    if (isComplete) {
      // No more frames can be pulled.
      break;
    }
    icoord += ReflowOneColumn(aReflowState, columnIndex, column, reflowStatus);
    if (!NS_INLINE_IS_BREAK_BEFORE(reflowStatus)) {
      columnIndex++;
    }
  }

  if (!e.AtEnd() && NS_INLINE_IS_BREAK_AFTER(reflowStatus)) {
    // The current column has been successfully placed.
    // Skip to the next column and mark break before.
    e.Next();
    e.GetColumn(column);
    reflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
  }
  if (!e.AtEnd() || (GetNextInFlow() && !isComplete)) {
    NS_FRAME_SET_INCOMPLETE(aStatus);
  }

  if (NS_INLINE_IS_BREAK_BEFORE(reflowStatus)) {
    if (!columnIndex || !aReflowState.mAllowLineBreak) {
      // If no column has been placed yet, or we have any span,
      // the whole container should be in the next line.
      aStatus = NS_INLINE_LINE_BREAK_BEFORE();
      return 0;
    }
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
    MOZ_ASSERT(NS_FRAME_IS_COMPLETE(aStatus) || aReflowState.mAllowLineBreak);

    // If we are on an intra-level whitespace column, null values in
    // column.mBaseFrame and column.mTextFrames don't represent the
    // end of the frame-sibling-chain at that level -- instead, they
    // represent an anonymous empty intra-level whitespace box. It is
    // likely that there are frames in the next column (which can't be
    // intra-level whitespace). Those frames should be pushed as well.
    Maybe<RubyColumn> nextColumn;
    if (column.mIsIntraLevelWhitespace && !e.AtEnd()) {
      e.Next();
      nextColumn.emplace();
      e.GetColumn(nextColumn.ref());
    }
    nsIFrame* baseFrame = column.mBaseFrame;
    if (!baseFrame & nextColumn.isSome()) {
      baseFrame = nextColumn->mBaseFrame;
    }
    if (baseFrame) {
      PushChildren(baseFrame, baseFrame->GetPrevSibling());
    }
    for (uint32_t i = 0; i < rtcCount; i++) {
      nsRubyTextFrame* textFrame = column.mTextFrames[i];
      if (!textFrame && nextColumn.isSome()) {
        textFrame = nextColumn->mTextFrames[i];
      }
      if (textFrame) {
        aReflowState.mTextContainers[i]->PushChildren(
          textFrame, textFrame->GetPrevSibling());
      }
    }
  } else if (NS_INLINE_IS_BREAK_AFTER(reflowStatus)) {
    // |reflowStatus| being break after here may only happen when
    // there is a break after the column just pulled, or the whole
    // segment has been completely reflowed. In those cases, we do
    // not need to push anything.
    MOZ_ASSERT(e.AtEnd());
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
  }

  return icoord;
}

nscoord
nsRubyBaseContainerFrame::ReflowOneColumn(const ReflowState& aReflowState,
                                          uint32_t aColumnIndex,
                                          const RubyColumn& aColumn,
                                          nsReflowStatus& aStatus)
{
  const nsHTMLReflowState& baseReflowState = aReflowState.mBaseReflowState;
  const auto& textReflowStates = aReflowState.mTextReflowStates;
  nscoord istart = baseReflowState.mLineLayout->GetCurrentICoord();

  if (aColumn.mBaseFrame) {
    bool allowBreakBefore = aColumnIndex ?
      aReflowState.mAllowLineBreak : aReflowState.mAllowInitialLineBreak;
    if (allowBreakBefore) {
      gfxBreakPriority breakPriority = LineBreakBefore(
        aColumn.mBaseFrame, baseReflowState.rendContext,
        baseReflowState.mLineLayout->LineContainerFrame(),
        baseReflowState.mLineLayout->GetLine());
      if (breakPriority != gfxBreakPriority::eNoBreak) {
        gfxBreakPriority lastBreakPriority =
          baseReflowState.mLineLayout->LastOptionalBreakPriority();
        if (breakPriority >= lastBreakPriority) {
          // Either we have been overflow, or we are forced
          // to break here, do break before.
          if (istart > baseReflowState.AvailableISize() ||
              baseReflowState.mLineLayout->NotifyOptionalBreakPosition(
                aColumn.mBaseFrame, 0, true, breakPriority)) {
            aStatus = NS_INLINE_LINE_BREAK_BEFORE();
            return 0;
          }
        }
      }
    }
  }

  const uint32_t rtcCount = aReflowState.mTextContainers.Length();
  MOZ_ASSERT(aColumn.mTextFrames.Length() == rtcCount);
  MOZ_ASSERT(textReflowStates.Length() == rtcCount);
  nscoord columnISize = 0;

  nsAutoString baseText;
  if (aColumn.mBaseFrame) {
    if (!nsContentUtils::GetNodeTextContent(aColumn.mBaseFrame->GetContent(),
                                            true, baseText)) {
      NS_RUNTIMEABORT("OOM");
    }
  }

  // Reflow text frames
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsRubyTextFrame* textFrame = aColumn.mTextFrames[i];
    if (textFrame) {
      nsAutoString annotationText;
      if (!nsContentUtils::GetNodeTextContent(textFrame->GetContent(),
                                              true, annotationText)) {
        NS_RUNTIMEABORT("OOM");
      }
      // Per CSS Ruby spec, the content comparison for auto-hiding
      // takes place prior to white spaces collapsing (white-space)
      // and text transformation (text-transform), and ignores elements
      // (considers only the textContent of the boxes). Which means
      // using the content tree text comparison is correct.
      if (annotationText.Equals(baseText)) {
        textFrame->AddStateBits(NS_RUBY_TEXT_FRAME_AUTOHIDE);
      } else {
        textFrame->RemoveStateBits(NS_RUBY_TEXT_FRAME_AUTOHIDE);
      }
      RubyUtils::ClearReservedISize(textFrame);

      bool pushedFrame;
      nsReflowStatus reflowStatus;
      nsLineLayout* lineLayout = textReflowStates[i]->mLineLayout;
      nscoord textIStart = lineLayout->GetCurrentICoord();
      lineLayout->ReflowFrame(textFrame, reflowStatus, nullptr, pushedFrame);
      if (MOZ_UNLIKELY(NS_INLINE_IS_BREAK(reflowStatus) || pushedFrame)) {
        MOZ_ASSERT_UNREACHABLE(
            "Any line break inside ruby box should have been suppressed");
        // For safety, always drain the overflow list, so that
        // no frames are left there after reflow.
        textFrame->DrainSelfOverflowList();
      }
      nscoord textISize = lineLayout->GetCurrentICoord() - textIStart;
      columnISize = std::max(columnISize, textISize);
    }
  }

  // Reflow the base frame
  if (aColumn.mBaseFrame) {
    RubyUtils::ClearReservedISize(aColumn.mBaseFrame);

    bool pushedFrame;
    nsReflowStatus reflowStatus;
    nsLineLayout* lineLayout = baseReflowState.mLineLayout;
    nscoord baseIStart = lineLayout->GetCurrentICoord();
    lineLayout->ReflowFrame(aColumn.mBaseFrame, reflowStatus,
                            nullptr, pushedFrame);
    if (MOZ_UNLIKELY(NS_INLINE_IS_BREAK(reflowStatus) || pushedFrame)) {
      MOZ_ASSERT_UNREACHABLE(
        "Any line break inside ruby box should have been suppressed");
      // For safety, always drain the overflow list, so that
      // no frames are left there after reflow.
      aColumn.mBaseFrame->DrainSelfOverflowList();
    }
    nscoord baseISize = lineLayout->GetCurrentICoord() - baseIStart;
    columnISize = std::max(columnISize, baseISize);
  }

  // Align all the line layout to the new coordinate.
  nscoord icoord = istart + columnISize;
  nscoord deltaISize = icoord - baseReflowState.mLineLayout->GetCurrentICoord();
  if (deltaISize > 0) {
    baseReflowState.mLineLayout->AdvanceICoord(deltaISize);
    if (aColumn.mBaseFrame) {
      RubyUtils::SetReservedISize(aColumn.mBaseFrame, deltaISize);
    }
  }
  for (uint32_t i = 0; i < rtcCount; i++) {
    if (aReflowState.mTextContainers[i]->IsSpanContainer()) {
      continue;
    }
    nsLineLayout* lineLayout = textReflowStates[i]->mLineLayout;
    nsRubyTextFrame* textFrame = aColumn.mTextFrames[i];
    nscoord deltaISize = icoord - lineLayout->GetCurrentICoord();
    if (deltaISize > 0) {
      lineLayout->AdvanceICoord(deltaISize);
      if (textFrame && !textFrame->IsAutoHidden()) {
        RubyUtils::SetReservedISize(textFrame, deltaISize);
      }
    }
    if (aColumn.mBaseFrame && textFrame) {
      lineLayout->AttachLastFrameToBaseLineLayout();
    }
  }

  return columnISize;
}

nsRubyBaseContainerFrame::PullFrameState::PullFrameState(
    nsRubyBaseContainerFrame* aBaseContainer,
    const TextContainerArray& aTextContainers)
  : mBase(aBaseContainer)
  , mTextContainers(aTextContainers)
{
  const uint32_t rtcCount = aTextContainers.Length();
  for (uint32_t i = 0; i < rtcCount; i++) {
    mTexts.AppendElement(aTextContainers[i]);
  }
}

void
nsRubyBaseContainerFrame::PullOneColumn(nsLineLayout* aLineLayout,
                                        PullFrameState& aPullFrameState,
                                        RubyColumn& aColumn,
                                        bool& aIsComplete)
{
  const TextContainerArray& textContainers = aPullFrameState.mTextContainers;
  const uint32_t rtcCount = textContainers.Length();

  nsIFrame* nextBase = GetNextInFlowChild(aPullFrameState.mBase);
  MOZ_ASSERT(!nextBase || nextBase->GetType() == nsGkAtoms::rubyBaseFrame);
  aColumn.mBaseFrame = static_cast<nsRubyBaseFrame*>(nextBase);
  aIsComplete = !aColumn.mBaseFrame;
  bool pullingIntraLevelWhitespace =
    aColumn.mBaseFrame && aColumn.mBaseFrame->IsIntraLevelWhitespace();

  aColumn.mTextFrames.ClearAndRetainStorage();
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsIFrame* nextText =
      textContainers[i]->GetNextInFlowChild(aPullFrameState.mTexts[i]);
    MOZ_ASSERT(!nextText || nextText->GetType() == nsGkAtoms::rubyTextFrame);
    nsRubyTextFrame* textFrame = static_cast<nsRubyTextFrame*>(nextText);
    aColumn.mTextFrames.AppendElement(textFrame);
    // If there exists any frame in continations, we haven't
    // completed the reflow process.
    aIsComplete = aIsComplete && !nextText;
    if (nextText && !pullingIntraLevelWhitespace) {
      pullingIntraLevelWhitespace = textFrame->IsIntraLevelWhitespace();
    }
  }

  aColumn.mIsIntraLevelWhitespace = pullingIntraLevelWhitespace;
  if (pullingIntraLevelWhitespace) {
    // We are pulling an intra-level whitespace. Drop all frames which
    // are not part of this intra-level whitespace column. (Those frames
    // are really part of the *next* column, after the pulled one.)
    if (aColumn.mBaseFrame && !aColumn.mBaseFrame->IsIntraLevelWhitespace()) {
      aColumn.mBaseFrame = nullptr;
    }
    for (uint32_t i = 0; i < rtcCount; i++) {
      nsRubyTextFrame*& textFrame = aColumn.mTextFrames[i];
      if (textFrame && !textFrame->IsIntraLevelWhitespace()) {
        textFrame = nullptr;
      }
    }
  }

  // Pull the frames of this column.
  if (aColumn.mBaseFrame) {
    DebugOnly<nsIFrame*> pulled = PullNextInFlowChild(aPullFrameState.mBase);
    MOZ_ASSERT(pulled == aColumn.mBaseFrame, "pulled a wrong frame?");
  }
  for (uint32_t i = 0; i < rtcCount; i++) {
    if (aColumn.mTextFrames[i]) {
      DebugOnly<nsIFrame*> pulled =
        textContainers[i]->PullNextInFlowChild(aPullFrameState.mTexts[i]);
      MOZ_ASSERT(pulled == aColumn.mTextFrames[i], "pulled a wrong frame?");
    }
  }

  if (!aIsComplete) {
    // We pulled frames from the next line, hence mark it dirty.
    aLineLayout->SetDirtyNextLine();
  }
}

nscoord
nsRubyBaseContainerFrame::ReflowSpans(const ReflowState& aReflowState)
{
  nscoord spanISize = 0;
  for (uint32_t i = 0, iend = aReflowState.mTextContainers.Length();
       i < iend; i++) {
    nsRubyTextContainerFrame* container = aReflowState.mTextContainers[i];
    if (!container->IsSpanContainer()) {
      continue;
    }

    nsIFrame* rtFrame = container->GetFirstPrincipalChild();
    nsReflowStatus reflowStatus;
    bool pushedFrame;
    nsLineLayout* lineLayout = aReflowState.mTextReflowStates[i]->mLineLayout;
    MOZ_ASSERT(lineLayout->GetCurrentICoord() == 0,
               "border/padding of rtc should have been suppressed");
    lineLayout->ReflowFrame(rtFrame, reflowStatus, nullptr, pushedFrame);
    MOZ_ASSERT(!NS_INLINE_IS_BREAK(reflowStatus) && !pushedFrame,
               "Any line break inside ruby box should has been suppressed");
    spanISize = std::max(spanISize, lineLayout->GetCurrentICoord());
  }
  return spanISize;
}
