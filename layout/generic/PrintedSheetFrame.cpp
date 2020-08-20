/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Rendering object for a printed or print-previewed sheet of paper */

#include "PrintedSheetFrame.h"

#include "nsCSSFrameConstructor.h"
#include "nsPageFrame.h"
#include "nsPageSequenceFrame.h"

using namespace mozilla;

PrintedSheetFrame* NS_NewPrintedSheetFrame(PresShell* aPresShell,
                                           ComputedStyle* aStyle) {
  return new (aPresShell)
      PrintedSheetFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_QUERYFRAME_HEAD(PrintedSheetFrame)
  NS_QUERYFRAME_ENTRY(PrintedSheetFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(PrintedSheetFrame)

void PrintedSheetFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayListSet& aLists) {
  if (PresContext()->IsScreen()) {
    // Draw the background/shadow/etc. of a blank sheet of paper, for
    // print-preview.
    DisplayBorderBackgroundOutline(aBuilder, aLists);
  }

  // Let each of our children (pages) draw itself:
  for (auto* frame : mFrames) {
    BuildDisplayListForChild(aBuilder, frame, aLists);
  }
}

void PrintedSheetFrame::Reflow(nsPresContext* aPresContext,
                               ReflowOutput& aReflowOutput,
                               const ReflowInput& aReflowInput,
                               nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("PrintedSheetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aReflowOutput, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // If we have a prev-in-flow, take its overflowing content:
  MoveOverflowToChildList();

  const WritingMode wm = aReflowInput.GetWritingMode();

  // Both the sheet and the pages will use this size:
  const nsSize physPageSize = aPresContext->GetPageSize();
  const LogicalSize pageSize(wm, physPageSize);

  // Until bug 1631452, we should only have one child (which we might've just
  // pulled from our prev-in-flow's overflow list). That means the loop below
  // is trivial & will will run exactly once, for now.
  MOZ_ASSERT(mFrames.GetLength() == 1,
             "how did we get more than one page per sheet");
  for (auto* childFrame : mFrames) {
    MOZ_ASSERT(childFrame->IsPageFrame(),
               "we're only expecting page frames as children");
    auto* pageFrame = static_cast<nsPageFrame*>(childFrame);

    // Be sure our child has a pointer to the nsSharedPageData and knows its
    // page number:
    pageFrame->SetSharedPageData(mPD);
    pageFrame->DeterminePageNum();

    ReflowInput pageReflowInput(aPresContext, aReflowInput, pageFrame,
                                pageSize);
    // For now, we'll always position the pageFrame at our origin.  (This will
    // change in bug 1631452 when we support more than 1 page per sheet.)
    LogicalPoint pagePos(wm);

    // Outparams for reflow:
    ReflowOutput pageReflowOutput(pageReflowInput);
    nsReflowStatus status;

    ReflowChild(pageFrame, aPresContext, pageReflowOutput, pageReflowInput, wm,
                pagePos, physPageSize, ReflowChildFlags::Default, status);

    FinishReflowChild(pageFrame, aPresContext, pageReflowOutput,
                      &pageReflowInput, wm, pagePos, physPageSize,
                      ReflowChildFlags::Default);

    // Did this page complete the document, or do we need to generate
    // another page frame?
    nsIFrame* pageNextInFlow = pageFrame->GetNextInFlow();
    if (status.IsFullyComplete()) {
      // The page we just reflowed is the final page! Record its page number
      // as the number of pages:
      mPD->mTotNumPages = pageFrame->GetPageNum();

      // Normally, we (the parent frame) would be responsible for deleting the
      // next-in-flow of our fully-complete children. But since we don't
      // support dynamic changes / incremental reflow for printing (and since
      // the reflow operation is single-pass at this level of granularity), our
      // child *shouldn't have had any opportunities* to end up with a
      // next-in-flow that would need cleaning up here.
      NS_ASSERTION(!pageNextInFlow, "bad child flow list");
    } else {
      // Our page frame child needs a continuation. For now, that makes us need
      // one as well. (This will change in bug 1631452 when we support more
      // than 1 page per sheet; at that point, we'll only want to do this when
      // we max out the number of pages that are allowed on our sheet.)
      aStatus.SetIncomplete();

      if (!pageNextInFlow) {
        // We need to create a continuation for our page frame. We add the
        // continuation to our child list, and then push it to our overflow
        // list so that our own (perhaps yet-to-be-created) continuation can
        // pull it forward.
        nsIFrame* continuingPage =
            PresShell()->FrameConstructor()->CreateContinuingFrame(pageFrame,
                                                                   this);
        mFrames.InsertFrame(nullptr, pageFrame, continuingPage);
        PushChildrenToOverflow(continuingPage, pageFrame);
      }
    }
  }

  // Populate our ReflowOutput outparam -- just use up all the
  // available space, for both our desired size & overflow areas.
  aReflowOutput.ISize(wm) = aReflowInput.AvailableISize();
  if (aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE) {
    aReflowOutput.BSize(wm) = aReflowInput.AvailableBSize();
  }
  aReflowOutput.SetOverflowAreasToDesiredBounds();

  FinishAndStoreOverflow(&aReflowOutput);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aReflowOutput);
}

void PrintedSheetFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  MOZ_ASSERT(mFrames.FirstChild() && mFrames.FirstChild()->IsPageFrame(),
             "PrintedSheetFrame must have a nsPageFrame child");
  // Only append the first child; all our children are expected to be
  // continuations of each other, and our anon box handling always walks
  // continuations.
  aResult.AppendElement(mFrames.FirstChild());
}

#ifdef DEBUG_FRAME_DUMP
nsresult PrintedSheetFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"PrintedSheet"_ns, aResult);
}
#endif

}  // namespace mozilla
