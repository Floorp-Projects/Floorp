/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby" */

#include "nsRubyFrame.h"

#include "RubyUtils.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/Maybe.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/WritingModes.h"
#include "nsLayoutUtils.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsContainerFrameInlines.h"
#include "nsRubyBaseContainerFrame.h"
#include "nsRubyTextContainerFrame.h"

using namespace mozilla;

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyFrame)
  NS_QUERYFRAME_ENTRY(nsRubyFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsInlineFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyFrame)

nsContainerFrame* NS_NewRubyFrame(PresShell* aPresShell,
                                  ComputedStyle* aStyle) {
  return new (aPresShell) nsRubyFrame(aStyle, aPresShell->GetPresContext());
}

//----------------------------------------------------------------------

// nsRubyFrame Method Implementations
// ==================================

/* virtual */
bool nsRubyFrame::IsFrameOfType(uint32_t aFlags) const {
  if (aFlags & eBidiInlineContainer) {
    return false;
  }
  return nsInlineFrame::IsFrameOfType(aFlags);
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsRubyFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"Ruby"_ns, aResult);
}
#endif

/* virtual */
void nsRubyFrame::AddInlineMinISize(gfxContext* aRenderingContext,
                                    nsIFrame::InlineMinISizeData* aData) {
  auto handleChildren = [aRenderingContext](auto frame, auto data) {
    for (RubySegmentEnumerator e(static_cast<nsRubyFrame*>(frame)); !e.AtEnd();
         e.Next()) {
      e.GetBaseContainer()->AddInlineMinISize(aRenderingContext, data);
    }
  };
  DoInlineIntrinsicISize(aData, handleChildren);
}

/* virtual */
void nsRubyFrame::AddInlinePrefISize(gfxContext* aRenderingContext,
                                     nsIFrame::InlinePrefISizeData* aData) {
  auto handleChildren = [aRenderingContext](auto frame, auto data) {
    for (RubySegmentEnumerator e(static_cast<nsRubyFrame*>(frame)); !e.AtEnd();
         e.Next()) {
      e.GetBaseContainer()->AddInlinePrefISize(aRenderingContext, data);
    }
  };
  DoInlineIntrinsicISize(aData, handleChildren);
  aData->mLineIsEmpty = false;
}

static nsRubyBaseContainerFrame* FindRubyBaseContainerAncestor(
    nsIFrame* aFrame) {
  for (nsIFrame* ancestor = aFrame->GetParent();
       ancestor && ancestor->IsFrameOfType(nsIFrame::eLineParticipant);
       ancestor = ancestor->GetParent()) {
    if (ancestor->IsRubyBaseContainerFrame()) {
      return static_cast<nsRubyBaseContainerFrame*>(ancestor);
    }
  }
  return nullptr;
}

/* virtual */
void nsRubyFrame::Reflow(nsPresContext* aPresContext,
                         ReflowOutput& aDesiredSize,
                         const ReflowInput& aReflowInput,
                         nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsRubyFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  if (!aReflowInput.mLineLayout) {
    NS_ASSERTION(aReflowInput.mLineLayout,
                 "No line layout provided to RubyFrame reflow method.");
    return;
  }

  // Grab overflow frames from prev-in-flow and its own.
  MoveInlineOverflowToChildList(aReflowInput.mLineLayout->LineContainerFrame());

  // Clear leadings
  mLeadings.Reset();

  // Begin the span for the ruby frame
  WritingMode frameWM = aReflowInput.GetWritingMode();
  WritingMode lineWM = aReflowInput.mLineLayout->GetWritingMode();
  LogicalMargin borderPadding =
      aReflowInput.ComputedLogicalBorderPadding(frameWM);
  nsLayoutUtils::SetBSizeFromFontMetrics(this, aDesiredSize, borderPadding,
                                         lineWM, frameWM);

  nscoord startEdge = 0;
  const bool boxDecorationBreakClone =
      StyleBorder()->mBoxDecorationBreak == StyleBoxDecorationBreak::Clone;
  if (boxDecorationBreakClone || !GetPrevContinuation()) {
    startEdge = borderPadding.IStart(frameWM);
  }
  NS_ASSERTION(aReflowInput.AvailableISize() != NS_UNCONSTRAINEDSIZE,
               "should no longer use available widths");
  nscoord endEdge = aReflowInput.AvailableISize() - borderPadding.IEnd(frameWM);
  aReflowInput.mLineLayout->BeginSpan(this, &aReflowInput, startEdge, endEdge,
                                      &mBaseline);

  for (RubySegmentEnumerator e(this); !e.AtEnd(); e.Next()) {
    ReflowSegment(aPresContext, aReflowInput, aDesiredSize.BlockStartAscent(),
                  aDesiredSize.BSize(lineWM), e.GetBaseContainer(), aStatus);

    if (aStatus.IsInlineBreak()) {
      // A break occurs when reflowing the segment.
      // Don't continue reflowing more segments.
      break;
    }
  }

  ContinuationTraversingState pullState(this);
  while (aStatus.IsEmpty()) {
    nsRubyBaseContainerFrame* baseContainer =
        PullOneSegment(aReflowInput.mLineLayout, pullState);
    if (!baseContainer) {
      // No more continuations after, finish now.
      break;
    }
    ReflowSegment(aPresContext, aReflowInput, aDesiredSize.BlockStartAscent(),
                  aDesiredSize.BSize(lineWM), baseContainer, aStatus);
  }
  // We never handle overflow in ruby.
  MOZ_ASSERT(!aStatus.IsOverflowIncomplete());

  aDesiredSize.ISize(lineWM) = aReflowInput.mLineLayout->EndSpan(this);
  if (boxDecorationBreakClone || !GetPrevContinuation()) {
    aDesiredSize.ISize(lineWM) += borderPadding.IStart(frameWM);
  }
  if (boxDecorationBreakClone || aStatus.IsComplete()) {
    aDesiredSize.ISize(lineWM) += borderPadding.IEnd(frameWM);
  }

  // Update descendant leadings of ancestor ruby base container.
  if (nsRubyBaseContainerFrame* rbc = FindRubyBaseContainerAncestor(this)) {
    rbc->UpdateDescendantLeadings(mLeadings);
  }

  ReflowAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput, aStatus);
}

void nsRubyFrame::ReflowSegment(nsPresContext* aPresContext,
                                const ReflowInput& aReflowInput,
                                nscoord aBlockStartAscent, nscoord aBlockSize,
                                nsRubyBaseContainerFrame* aBaseContainer,
                                nsReflowStatus& aStatus) {
  WritingMode lineWM = aReflowInput.mLineLayout->GetWritingMode();
  LogicalSize availSize(lineWM, aReflowInput.AvailableISize(),
                        aReflowInput.AvailableBSize());
  NS_ASSERTION(!GetWritingMode().IsOrthogonalTo(lineWM),
               "Ruby frame writing-mode shouldn't be orthogonal to its line");

  AutoRubyTextContainerArray textContainers(aBaseContainer);
  const uint32_t rtcCount = textContainers.Length();

  ReflowOutput baseMetrics(aReflowInput);
  bool pushedFrame;
  aReflowInput.mLineLayout->ReflowFrame(aBaseContainer, aStatus, &baseMetrics,
                                        pushedFrame);

  if (aStatus.IsInlineBreakBefore()) {
    if (aBaseContainer != mFrames.FirstChild()) {
      // Some segments may have been reflowed before, hence it is not
      // a break-before for the ruby container.
      aStatus.Reset();
      aStatus.SetInlineLineBreakAfter();
      aStatus.SetIncomplete();
      PushChildrenToOverflow(aBaseContainer, aBaseContainer->GetPrevSibling());
      aReflowInput.mLineLayout->SetDirtyNextLine();
    }
    // This base container is not placed at all, we can skip all
    // text containers paired with it.
    return;
  }
  if (aStatus.IsIncomplete()) {
    // It always promise that if the status is incomplete, there is a
    // break occurs. Break before has been processed above. However,
    // it is possible that break after happens with the frame reflow
    // completed. It happens if there is a force break at the end.
    MOZ_ASSERT(aStatus.IsInlineBreakAfter());
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
        MOZ_ASSERT(newTextContainer,
                   "Next-in-flow of rtc should not exist "
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
      PushChildrenToOverflow(lastChild->GetNextSibling(), lastChild);
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
  // lines, so here we reset it to the different between the ascents of
  // the ruby container and the ruby base container, assuming they are
  // aligned with the baseline.
  // XXX We may need to add border/padding here. See bug 1055667.
  baseRect.BStart(lineWM) = aBlockStartAscent - baseMetrics.BlockStartAscent();
  // The rect for offsets of text containers.
  LogicalRect offsetRect = baseRect;
  RubyBlockLeadings descLeadings = aBaseContainer->GetDescendantLeadings();
  offsetRect.BStart(lineWM) -= descLeadings.mStart;
  offsetRect.BSize(lineWM) += descLeadings.mStart + descLeadings.mEnd;
  Maybe<LineRelativeDir> lastLineSide;
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsRubyTextContainerFrame* textContainer = textContainers[i];
    WritingMode rtcWM = textContainer->GetWritingMode();
    nsReflowStatus textReflowStatus;
    ReflowOutput textMetrics(aReflowInput);
    ReflowInput textReflowInput(aPresContext, aReflowInput, textContainer,
                                availSize.ConvertTo(rtcWM, lineWM));
    textContainer->Reflow(aPresContext, textMetrics, textReflowInput,
                          textReflowStatus);
    // Ruby text containers always return complete reflow status even when
    // they have continuations, because the breaking has already been
    // handled when reflowing the base containers.
    NS_ASSERTION(textReflowStatus.IsEmpty(),
                 "Ruby text container must not break itself inside");
    const LogicalSize size = textMetrics.Size(lineWM);
    textContainer->SetSize(lineWM, size);

    nscoord reservedISize = RubyUtils::GetReservedISize(textContainer);
    segmentISize = std::max(segmentISize, size.ISize(lineWM) + reservedISize);

    Maybe<LineRelativeDir> lineSide;
    switch (textContainer->StyleText()->mRubyPosition) {
      case StyleRubyPosition::Over:
        lineSide.emplace(eLineRelativeDirOver);
        break;
      case StyleRubyPosition::Under:
        lineSide.emplace(eLineRelativeDirUnder);
        break;
      case StyleRubyPosition::AlternateOver:
        if (lastLineSide.isSome() &&
            lastLineSide.value() == eLineRelativeDirOver) {
          lineSide.emplace(eLineRelativeDirUnder);
        } else {
          lineSide.emplace(eLineRelativeDirOver);
        }
        break;
      case StyleRubyPosition::AlternateUnder:
        if (lastLineSide.isSome() &&
            lastLineSide.value() == eLineRelativeDirUnder) {
          lineSide.emplace(eLineRelativeDirOver);
        } else {
          lineSide.emplace(eLineRelativeDirUnder);
        }
        break;
      default:
        // XXX inter-character support in bug 1055672
        MOZ_ASSERT_UNREACHABLE("Unsupported ruby-position");
    }
    lastLineSide = lineSide;

    LogicalPoint position(lineWM);
    if (lineSide.isSome()) {
      LogicalSide logicalSide =
          lineWM.LogicalSideForLineRelativeDir(lineSide.value());
      if (StaticPrefs::layout_css_ruby_intercharacter_enabled() &&
          rtcWM.IsVerticalRL() &&
          lineWM.GetInlineDir() == WritingMode::eInlineLTR) {
        // Inter-character ruby annotations are only supported for vertical-rl
        // in ltr horizontal writing. Fall back to non-inter-character behavior
        // otherwise.
        LogicalPoint offset(
            lineWM, offsetRect.ISize(lineWM),
            offsetRect.BSize(lineWM) > size.BSize(lineWM)
                ? (offsetRect.BSize(lineWM) - size.BSize(lineWM)) / 2
                : 0);
        position = offsetRect.Origin(lineWM) + offset;
        aReflowInput.mLineLayout->AdvanceICoord(size.ISize(lineWM));
      } else if (logicalSide == eLogicalSideBStart) {
        offsetRect.BStart(lineWM) -= size.BSize(lineWM);
        offsetRect.BSize(lineWM) += size.BSize(lineWM);
        position = offsetRect.Origin(lineWM);
      } else if (logicalSide == eLogicalSideBEnd) {
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
                      &textReflowInput, lineWM, position, dummyContainerSize,
                      ReflowChildFlags::Default);
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

  // Set block leadings of the base container.
  // The leadings are the difference between the offsetRect and the rect
  // of this ruby container, which has block start zero and block size
  // aBlockSize.
  nscoord startLeading = -offsetRect.BStart(lineWM);
  nscoord endLeading = offsetRect.BEnd(lineWM) - aBlockSize;
  // XXX When bug 765861 gets fixed, this warning should be upgraded.
  NS_WARNING_ASSERTION(startLeading >= 0 && endLeading >= 0,
                       "Leadings should be non-negative (because adding "
                       "ruby annotation can only increase the size)");
  mLeadings.Update(startLeading, endLeading);
}

nsRubyBaseContainerFrame* nsRubyFrame::PullOneSegment(
    const nsLineLayout* aLineLayout, ContinuationTraversingState& aState) {
  // Pull a ruby base container
  nsIFrame* baseFrame = GetNextInFlowChild(aState);
  if (!baseFrame) {
    return nullptr;
  }
  MOZ_ASSERT(baseFrame->IsRubyBaseContainerFrame());

  // Get the float containing block of the base frame before we pull it.
  nsBlockFrame* oldFloatCB = nsLayoutUtils::GetFloatContainingBlock(baseFrame);
  PullNextInFlowChild(aState);

  // Pull all ruby text containers following the base container
  nsIFrame* nextFrame;
  while ((nextFrame = GetNextInFlowChild(aState)) != nullptr &&
         nextFrame->IsRubyTextContainerFrame()) {
    PullNextInFlowChild(aState);
  }

  if (nsBlockFrame* newFloatCB =
          do_QueryFrame(aLineLayout->LineContainerFrame())) {
    if (oldFloatCB && oldFloatCB != newFloatCB) {
      newFloatCB->ReparentFloats(baseFrame, oldFloatCB, true);
    }
  }

  return static_cast<nsRubyBaseContainerFrame*>(baseFrame);
}
