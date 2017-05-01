/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text-container" */

#include "nsRubyTextContainerFrame.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/WritingModes.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"

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
  return nsContainerFrame::IsFrameOfType(aFlags);
}

/* virtual */ void
nsRubyTextContainerFrame::SetInitialChildList(ChildListID aListID,
                                              nsFrameList& aChildList)
{
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  if (aListID == kPrincipalList) {
    UpdateSpanFlag();
  }
}

/* virtual */ void
nsRubyTextContainerFrame::AppendFrames(ChildListID aListID,
                                       nsFrameList& aFrameList)
{
  nsContainerFrame::AppendFrames(aListID, aFrameList);
  UpdateSpanFlag();
}

/* virtual */ void
nsRubyTextContainerFrame::InsertFrames(ChildListID aListID,
                                       nsIFrame* aPrevFrame,
                                       nsFrameList& aFrameList)
{
  nsContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
  UpdateSpanFlag();
}

/* virtual */ void
nsRubyTextContainerFrame::RemoveFrame(ChildListID aListID,
                                      nsIFrame* aOldFrame)
{
  nsContainerFrame::RemoveFrame(aListID, aOldFrame);
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
                                 ReflowOutput& aDesiredSize,
                                 const ReflowInput& aReflowInput,
                                 nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsRubyTextContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);

  // Although a ruby text container may have continuations, returning
  // complete reflow status is still safe, since its parent, ruby frame,
  // ignores the status, and continuations of the ruby base container
  // will take care of our continuations.
  aStatus.Reset();
  WritingMode lineWM = aReflowInput.mLineLayout->GetWritingMode();

  nscoord minBCoord = nscoord_MAX;
  nscoord maxBCoord = nscoord_MIN;
  // The container size is not yet known, so we use a dummy (0, 0) size.
  // The block-dir position will be corrected below after containerSize
  // is finalized.
  const nsSize dummyContainerSize;
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nsIFrame* child = e.get();
    MOZ_ASSERT(child->IsRubyTextFrame());
    LogicalRect rect = child->GetLogicalRect(lineWM, dummyContainerSize);
    LogicalMargin margin = child->GetLogicalUsedMargin(lineWM);
    nscoord blockStart = rect.BStart(lineWM) - margin.BStart(lineWM);
    minBCoord = std::min(minBCoord, blockStart);
    nscoord blockEnd = rect.BEnd(lineWM) + margin.BEnd(lineWM);
    maxBCoord = std::max(maxBCoord, blockEnd);
  }

  LogicalSize size(lineWM, mISize, 0);
  if (!mFrames.IsEmpty()) {
    if (MOZ_UNLIKELY(minBCoord > maxBCoord)) {
      // XXX When bug 765861 gets fixed, this warning should be upgraded.
      NS_WARNING("bad block coord");
      minBCoord = maxBCoord = 0;
    }
    size.BSize(lineWM) = maxBCoord - minBCoord;
    nsSize containerSize = size.GetPhysicalSize(lineWM);
    for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
      nsIFrame* child = e.get();
      // We reflowed the child with a dummy container size, as the true size
      // was not yet known at that time.
      LogicalPoint pos = child->GetLogicalPosition(lineWM, dummyContainerSize);
      // Adjust block position to account for minBCoord,
      // then reposition child based on the true container width.
      pos.B(lineWM) -= minBCoord;
      // Relative positioning hasn't happened yet.
      // So MovePositionBy should not be used here.
      child->SetPosition(lineWM, pos, containerSize);
      nsContainerFrame::PlaceFrameView(child);
    }
  }

  aDesiredSize.SetSize(lineWM, size);
}
