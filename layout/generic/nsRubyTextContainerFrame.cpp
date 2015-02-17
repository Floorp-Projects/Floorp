/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text-container" */

#include "nsRubyTextContainerFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "WritingModes.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla;

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyTextContainerFrame)
  NS_QUERYFRAME_ENTRY(nsRubyTextContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyTextContainerFrame)

nsContainerFrame*
NS_NewRubyTextContainerFrame(nsIPresShell* aPresShell,
                             nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyTextContainerFrame(aContext);
}


//----------------------------------------------------------------------

// nsRubyTextContainerFrame Method Implementations
// ===============================================

nsIAtom*
nsRubyTextContainerFrame::GetType() const
{
  return nsGkAtoms::rubyTextContainerFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyTextContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("RubyTextContainer"), aResult);
}
#endif

/* virtual */ bool
nsRubyTextContainerFrame::IsFrameOfType(uint32_t aFlags) const
{
  if (aFlags & eSupportsCSSTransforms) {
    return false;
  }
  return nsRubyTextContainerFrameSuper::IsFrameOfType(aFlags);
}

/* virtual */ void
nsRubyTextContainerFrame::SetInitialChildList(ChildListID aListID,
                                              nsFrameList& aChildList)
{
  nsRubyTextContainerFrameSuper::SetInitialChildList(aListID, aChildList);
  UpdateSpanFlag();
}

/* virtual */ void
nsRubyTextContainerFrame::AppendFrames(ChildListID aListID,
                                       nsFrameList& aFrameList)
{
  nsRubyTextContainerFrameSuper::AppendFrames(aListID, aFrameList);
  UpdateSpanFlag();
}

/* virtual */ void
nsRubyTextContainerFrame::InsertFrames(ChildListID aListID,
                                       nsIFrame* aPrevFrame,
                                       nsFrameList& aFrameList)
{
  nsRubyTextContainerFrameSuper::InsertFrames(aListID, aPrevFrame, aFrameList);
  UpdateSpanFlag();
}

/* virtual */ void
nsRubyTextContainerFrame::RemoveFrame(ChildListID aListID,
                                      nsIFrame* aOldFrame)
{
  nsRubyTextContainerFrameSuper::RemoveFrame(aListID, aOldFrame);
  UpdateSpanFlag();
}

void
nsRubyTextContainerFrame::UpdateSpanFlag()
{
  bool isSpan = false;
  // The continuation checks are safe here because spans never break.
  if (!GetPrevContinuation() && !GetNextContinuation()) {
    nsIFrame* onlyChild = mFrames.OnlyChild();
    if (onlyChild && onlyChild->IsPseudoFrame(GetContent())) {
      // Per CSS Ruby spec, if the only child of an rtc frame is
      // a pseudo rt frame, it spans all bases in the segment.
      isSpan = true;
    }
  }

  if (isSpan) {
    AddStateBits(NS_RUBY_TEXT_CONTAINER_IS_SPAN);
  } else {
    RemoveStateBits(NS_RUBY_TEXT_CONTAINER_IS_SPAN);
  }
}

/* virtual */ void
nsRubyTextContainerFrame::Reflow(nsPresContext* aPresContext,
                                 nsHTMLReflowMetrics& aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRubyTextContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // Although a ruby text container may have continuations, returning
  // NS_FRAME_COMPLETE here is still safe, since its parent, ruby frame,
  // ignores the status, and continuations of the ruby base container
  // will take care of our continuations.
  aStatus = NS_FRAME_COMPLETE;
  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();

  nscoord minBCoord = nscoord_MAX;
  nscoord maxBCoord = nscoord_MIN;
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nsIFrame* child = e.get();
    MOZ_ASSERT(child->GetType() == nsGkAtoms::rubyTextFrame);
    // The container width is still unknown yet.
    LogicalRect rect = child->GetLogicalRect(lineWM, 0);
    LogicalMargin margin = child->GetLogicalUsedMargin(lineWM);
    nscoord blockStart = rect.BStart(lineWM) - margin.BStart(lineWM);
    minBCoord = std::min(minBCoord, blockStart);
    nscoord blockEnd = rect.BEnd(lineWM) + margin.BEnd(lineWM);
    maxBCoord = std::max(maxBCoord, blockEnd);
  }

  MOZ_ASSERT(minBCoord <= maxBCoord || mFrames.IsEmpty());
  LogicalSize size(lineWM, mISize, 0);
  if (!mFrames.IsEmpty()) {
    size.BSize(lineWM) = maxBCoord - minBCoord;
    nscoord deltaBCoord = -minBCoord;
    if (lineWM.IsVerticalRL()) {
      deltaBCoord -= size.BSize(lineWM);
    }

    if (deltaBCoord != 0) {
      nscoord containerWidth = size.Width(lineWM);
      for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
        nsIFrame* child = e.get();
        LogicalPoint pos = child->GetLogicalPosition(lineWM, containerWidth);
        pos.B(lineWM) += deltaBCoord;
        // Relative positioning hasn't happened yet.
        // So MovePositionBy should be used here.
        child->SetPosition(lineWM, pos, containerWidth);
        nsContainerFrame::PlaceFrameView(child);
      }
    }
  }

  aDesiredSize.SetSize(lineWM, size);
}
