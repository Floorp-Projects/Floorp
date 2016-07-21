/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby" */

#include "nsRubyFrame.h"

#include "RubyUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/WritingModes.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsRubyBaseContainerFrame.h"
#include "nsRubyTextContainerFrame.h"
#include "nsStyleContext.h"

using namespace mozilla;

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyFrame)
  NS_QUERYFRAME_ENTRY(nsRubyFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsInlineFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyFrame)

nsContainerFrame*
NS_NewRubyFrame(nsIPresShell* aPresShell,
                nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyFrame(aContext);
}

//----------------------------------------------------------------------

// nsRubyFrame Method Implementations
// ==================================

nsIAtom*
nsRubyFrame::GetType() const
{
  return nsGkAtoms::rubyFrame;
}

/* virtual */ bool
nsRubyFrame::IsFrameOfType(uint32_t aFlags) const
{
  if (aFlags & eBidiInlineContainer) {
    return false;
  }
  return nsInlineFrame::IsFrameOfType(aFlags);
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Ruby"), aResult);
}
#endif

/* virtual */ void
nsRubyFrame::AddInlineMinISize(nsRenderingContext *aRenderingContext,
                               nsIFrame::InlineMinISizeData *aData)
{
  for (nsIFrame* frame = this; frame; frame = frame->GetNextInFlow()) {
    for (RubySegmentEnumerator e(static_cast<nsRubyFrame*>(frame));
         !e.AtEnd(); e.Next()) {
      e.GetBaseContainer()->AddInlineMinISize(aRenderingContext, aData);
    }
  }
}

/* virtual */ void
nsRubyFrame::AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                nsIFrame::InlinePrefISizeData *aData)
{
  for (nsIFrame* frame = this; frame; frame = frame->GetNextInFlow()) {
    for (RubySegmentEnumerator e(static_cast<nsRubyFrame*>(frame));
         !e.AtEnd(); e.Next()) {
      e.GetBaseContainer()->AddInlinePrefISize(aRenderingContext, aData);
    }
  }
}

/* virtual */ void
nsRubyFrame::Reflow(nsPresContext* aPresContext,
                    ReflowOutput& aDesiredSize,
                    const ReflowInput& aReflowInput,
                    nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsRubyFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  
  if (!aReflowInput.mLineLayout) {
    NS_ASSERTION(aReflowInput.mLineLayout,
                 "No line layout provided to RubyFrame reflow method.");
    aStatus = NS_FRAME_COMPLETE;
    return;
  }

  // Grab overflow frames from prev-in-flow and its own.
  MoveOverflowToChildList();

  // Clear leadings
  mBStartLeading = mBEndLeading = 0;

  // Begin the span for the ruby frame
  WritingMode frameWM = aReflowInput.GetWritingMode();
  WritingMode lineWM = aReflowInput.mLineLayout->GetWritingMode();
  LogicalMargin borderPadding = aReflowInput.ComputedLogicalBorderPadding();
  nscoord startEdge = 0;
  const bool boxDecorationBreakClone =
    StyleBorder()->mBoxDecorationBreak == NS_STYLE_BOX_DECORATION_BREAK_CLONE;
  if (boxDecorationBreakClone || !GetPrevContinuation()) {
    startEdge = borderPadding.IStart(frameWM);
  }
  NS_ASSERTION(aReflowInput.AvailableISize() != NS_UNCONSTRAINEDSIZE,
               "should no longer use available widths");
  nscoord availableISize = aReflowInput.AvailableISize();
  availableISize -= startEdge + borderPadding.IEnd(frameWM);
  aReflowInput.mLineLayout->BeginSpan(this, &aReflowInput,
                                      startEdge, availableISize, &mBaseline);

  aStatus = NS_FRAME_COMPLETE;
  for (RubySegmentEnumerator e(this); !e.AtEnd(); e.Next()) {
    ReflowSegment(aPresContext, aReflowInput, e.GetBaseContainer(), aStatus);

    if (NS_INLINE_IS_BREAK(aStatus)) {
      // A break occurs when reflowing the segment.
      // Don't continue reflowing more segments.
      break;
    }
  }

  ContinuationTraversingState pullState(this);
  while (aStatus == NS_FRAME_COMPLETE) {
    nsRubyBaseContainerFrame* baseContainer =
      PullOneSegment(aReflowInput.mLineLayout, pullState);
    if (!baseContainer) {
      // No more continuations after, finish now.
      break;
    }
    ReflowSegment(aPresContext, aReflowInput, baseContainer, aStatus);
  }
  // We never handle overflow in ruby.
  MOZ_ASSERT(!NS_FRAME_OVERFLOW_IS_INCOMPLETE(aStatus));

  aDesiredSize.ISize(lineWM) = aReflowInput.mLineLayout->EndSpan(this);
  if (boxDecorationBreakClone || !GetPrevContinuation()) {
    aDesiredSize.ISize(lineWM) += borderPadding.IStart(frameWM);
  }
  if (boxDecorationBreakClone || NS_FRAME_IS_COMPLETE(aStatus)) {
    aDesiredSize.ISize(lineWM) += borderPadding.IEnd(frameWM);
  }

  nsLayoutUtils::SetBSizeFromFontMetrics(this, aDesiredSize,
                                         borderPadding, lineWM, frameWM);
}

void
nsRubyFrame::ReflowSegment(nsPresContext* aPresContext,
                           const ReflowInput& aReflowInput,
                           nsRubyBaseContainerFrame* aBaseContainer,
                           nsReflowStatus& aStatus)
{
  WritingMode lineWM = aReflowInput.mLineLayout->GetWritingMode();
  LogicalSize availSize(lineWM, aReflowInput.AvailableISize(),
                        aReflowInput.AvailableBSize());
  WritingMode rubyWM = GetWritingMode();
  NS_ASSERTION(!rubyWM.IsOrthogonalTo(lineWM),
               "Ruby frame writing-mode shouldn't be orthogonal to its line");

  AutoRubyTextContainerArray textContainers(aBaseContainer);
  const uint32_t rtcCount = textContainers.Length();

  ReflowOutput baseMetrics(aReflowInput);
  bool pushedFrame;
  aReflowInput.mLineLayout->ReflowFrame(aBaseContainer, aStatus,
                                        &baseMetrics, pushedFrame);

  if (NS_INLINE_IS_BREAK_BEFORE(aStatus)) {
    if (aBaseContainer != mFrames.FirstChild()) {
      // Some segments may have been reflowed before, hence it is not
      // a break-before for the ruby container.
      aStatus = NS_INLINE_LINE_BREAK_AFTER(NS_FRAME_NOT_COMPLETE);
      PushChildren(aBaseContainer, aBaseContainer->GetPrevSibling());
      aReflowInput.mLineLayout->SetDirtyNextLine();
    }
    // This base container is not placed at all, we can skip all
    // text containers paired with it.
    return;
  }
  if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
    // It always promise that if the status is incomplete, there is a
    // break occurs. Break before has been processed above. However,
    // it is possible that break after happens with the frame reflow
    // completed. It happens if there is a force break at the end.
    MOZ_ASSERT(NS_INLINE_IS_BREAK_AFTER(aStatus));
    // Find the previous sibling which we will
    // insert new continuations after.
    nsIFrame* lastChild;
    if (rtcCount > 0) {
      lastChild = textContainers.LastElement();
    } else {
      lastChild = aBaseContainer;
    }

    // Create continuations for the base container
    nsIFrame* newBaseContainer = CreateNextInFlow(aBaseContainer);
    // newBaseContainer is null if there are existing next-in-flows.
    // We only need to move and push if there were not.
    if (newBaseContainer) {
      // Move the new frame after all the text containers
      mFrames.RemoveFrame(newBaseContainer);
      mFrames.InsertFrame(nullptr, lastChild, newBaseContainer);

      // Create continuations for text containers
      nsIFrame* newLastChild = newBaseContainer;
      for (uint32_t i = 0; i < rtcCount; i++) {
        nsIFrame* newTextContainer = CreateNextInFlow(textContainers[i]);
        MOZ_ASSERT(newTextContainer, "Next-in-flow of rtc should not exist "
                   "if the corresponding rbc does not");
        mFrames.RemoveFrame(newTextContainer);
        mFrames.InsertFrame(nullptr, newLastChild, newTextContainer);
        newLastChild = newTextContainer;
      }
    }
    if (lastChild != mFrames.LastChild()) {
      // Always push the next frame after the last child in this segment.
      // It is possible that we pulled it back before our next-in-flow
      // drain our overflow.
      PushChildren(lastChild->GetNextSibling(), lastChild);
      aReflowInput.mLineLayout->SetDirtyNextLine();
    }
  } else {
    // If the ruby base container is reflowed completely, the line
    // layout will remove the next-in-flows of that frame. But the
    // line layout is not aware of the ruby text containers, hence
    // it is necessary to remove them here.
    for (uint32_t i = 0; i < rtcCount; i++) {
      nsIFrame* nextRTC = textContainers[i]->GetNextInFlow();
      if (nextRTC) {
        nextRTC->GetParent()->DeleteNextInFlowChild(nextRTC, true);
      }
    }
  }

  nscoord segmentISize = baseMetrics.ISize(lineWM);
  const nsSize dummyContainerSize;
  LogicalRect baseRect =
    aBaseContainer->GetLogicalRect(lineWM, dummyContainerSize);
  // We need to position our rtc frames on one side or the other of the
  // base container's rect, using a coordinate space that's relative to
  // the ruby frame. Right now, the base container's rect's block-axis
  // position is relative to the block container frame containing the
  // lines, so we use 0 instead. (i.e. we assume that the base container
  // is adjacent to the ruby frame's block-start edge.)
  // XXX We may need to add border/padding here. See bug 1055667.
  baseRect.BStart(lineWM) = 0;
  // The rect for offsets of text containers.
  LogicalRect offsetRect = baseRect;
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsRubyTextContainerFrame* textContainer = textContainers[i];
    WritingMode rtcWM = textContainer->GetWritingMode();
    nsReflowStatus textReflowStatus;
    ReflowOutput textMetrics(aReflowInput);
    ReflowInput textReflowInput(aPresContext, aReflowInput, textContainer,
                                      availSize.ConvertTo(rtcWM, lineWM));
    // FIXME We probably shouldn't be using the same nsLineLayout for
    //       the text containers. But it should be fine now as we are
    //       not actually using this line layout to reflow something,
    //       but just read the writing mode from it.
    textReflowInput.mLineLayout = aReflowInput.mLineLayout;
    textContainer->Reflow(aPresContext, textMetrics,
                          textReflowInput, textReflowStatus);
    // Ruby text containers always return NS_FRAME_COMPLETE even when
    // they have continuations, because the breaking has already been
    // handled when reflowing the base containers.
    NS_ASSERTION(textReflowStatus == NS_FRAME_COMPLETE,
                 "Ruby text container must not break itself inside");
    // The metrics is initialized with reflow state of this ruby frame,
    // hence the writing-mode is tied to rubyWM instead of rtcWM.
    LogicalSize size = textMetrics.Size(rubyWM).ConvertTo(lineWM, rubyWM);
    textContainer->SetSize(lineWM, size);

    nscoord reservedISize = RubyUtils::GetReservedISize(textContainer);
    segmentISize = std::max(segmentISize, size.ISize(lineWM) + reservedISize);

    uint8_t rubyPosition = textContainer->StyleText()->mRubyPosition;
    MOZ_ASSERT(rubyPosition == NS_STYLE_RUBY_POSITION_OVER ||
               rubyPosition == NS_STYLE_RUBY_POSITION_UNDER);
    Maybe<LogicalSide> side;
    if (rubyPosition == NS_STYLE_RUBY_POSITION_OVER) {
      side.emplace(lineWM.LogicalSideForLineRelativeDir(eLineRelativeDirOver));
    } else if (rubyPosition == NS_STYLE_RUBY_POSITION_UNDER) {
      side.emplace(lineWM.LogicalSideForLineRelativeDir(eLineRelativeDirUnder));
    } else {
      // XXX inter-character support in bug 1055672
      MOZ_ASSERT_UNREACHABLE("Unsupported ruby-position");
    }

    LogicalPoint position(lineWM);
    if (side.isSome()) {
      if (side.value() == eLogicalSideBStart) {
        offsetRect.BStart(lineWM) -= size.BSize(lineWM);
        offsetRect.BSize(lineWM) += size.BSize(lineWM);
        position = offsetRect.Origin(lineWM);
      } else if (side.value() == eLogicalSideBEnd) {
        position = offsetRect.Origin(lineWM) +
          LogicalPoint(lineWM, 0, offsetRect.BSize(lineWM));
        offsetRect.BSize(lineWM) += size.BSize(lineWM);
      } else {
        MOZ_ASSERT_UNREACHABLE("???");
      }
    }
    // Using a dummy container-size here, so child positioning may not be
    // correct. We will fix it in nsLineLayout after the whole line is
    // reflowed.
    FinishReflowChild(textContainer, aPresContext, textMetrics,
                      &textReflowInput, lineWM, position, dummyContainerSize, 0);
  }
  MOZ_ASSERT(baseRect.ISize(lineWM) == offsetRect.ISize(lineWM),
             "Annotations should only be placed on the block directions");

  nscoord deltaISize = segmentISize - baseMetrics.ISize(lineWM);
  if (deltaISize <= 0) {
    RubyUtils::ClearReservedISize(aBaseContainer);
  } else {
    RubyUtils::SetReservedISize(aBaseContainer, deltaISize);
    aReflowInput.mLineLayout->AdvanceICoord(deltaISize);
  }

  // Set block leadings of the base container
  nscoord startLeading = baseRect.BStart(lineWM) - offsetRect.BStart(lineWM);
  nscoord endLeading = offsetRect.BEnd(lineWM) - baseRect.BEnd(lineWM);
  // XXX When bug 765861 gets fixed, this warning should be upgraded.
  NS_WARN_IF_FALSE(startLeading >= 0 && endLeading >= 0,
                   "Leadings should be non-negative (because adding "
                   "ruby annotation can only increase the size)");
  mBStartLeading = std::max(mBStartLeading, startLeading);
  mBEndLeading = std::max(mBEndLeading, endLeading);
}

nsRubyBaseContainerFrame*
nsRubyFrame::PullOneSegment(const nsLineLayout* aLineLayout,
                            ContinuationTraversingState& aState)
{
  // Pull a ruby base container
  nsIFrame* baseFrame = GetNextInFlowChild(aState);
  if (!baseFrame) {
    return nullptr;
  }
  MOZ_ASSERT(baseFrame->GetType() == nsGkAtoms::rubyBaseContainerFrame);

  // Get the float containing block of the base frame before we pull it.
  nsBlockFrame* oldFloatCB =
    nsLayoutUtils::GetFloatContainingBlock(baseFrame);
  PullNextInFlowChild(aState);

  // Pull all ruby text containers following the base container
  nsIFrame* nextFrame;
  while ((nextFrame = GetNextInFlowChild(aState)) != nullptr &&
         nextFrame->GetType() == nsGkAtoms::rubyTextContainerFrame) {
    PullNextInFlowChild(aState);
  }

  if (nsBlockFrame* newFloatCB =
      nsLayoutUtils::GetAsBlock(aLineLayout->LineContainerFrame())) {
    if (oldFloatCB && oldFloatCB != newFloatCB) {
      newFloatCB->ReparentFloats(baseFrame, oldFloatCB, true);
    }
  }

  return static_cast<nsRubyBaseContainerFrame*>(baseFrame);
}
