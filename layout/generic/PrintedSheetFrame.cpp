/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Rendering object for a printed or print-previewed sheet of paper */

#include "mozilla/PrintedSheetFrame.h"

#include "mozilla/StaticPrefs_print.h"
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
    if (!frame->HasAnyStateBits(NS_PAGE_SKIPPED_BY_CUSTOM_RANGE)) {
      BuildDisplayListForChild(aBuilder, frame, aLists);
    }
  }
}

// If the given page is included in the user's page range, this function
// returns false. Otherwise, it tags the page with the
// NS_PAGE_SKIPPED_BY_CUSTOM_RANGE state bit and returns true.
static bool TagIfSkippedByCustomRange(nsPageFrame* aPageFrame, int32_t aPageNum,
                                      nsSharedPageData* aPD) {
  // Only do page-skipping here if we're using the new tab-modal UI, and only
  // do it for print-preview.  We have separate legacy code that skips pages
  // for actual printing (for now).
  if (!StaticPrefs::print_tab_modal_enabled() ||
      !aPageFrame->PresContext()->IsScreen()) {
    return false;
  }

  if (!aPD->mDoingPageRange ||
      (aPD->mFromPageNum <= aPageNum && aPD->mToPageNum >= aPageNum)) {
    MOZ_ASSERT(!aPageFrame->HasAnyStateBits(NS_PAGE_SKIPPED_BY_CUSTOM_RANGE),
               "page frames in print range shouldn't be tagged with the"
               "NS_PAGE_SKIPPED_BY_CUSTOM_RANGE state bit");
    return false;
  }

  aPageFrame->AddStateBits(NS_PAGE_SKIPPED_BY_CUSTOM_RANGE);
  return true;
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

  // Count the number of pages that are displayed on this sheet (i.e. how many
  // child frames we end up laying out, excluding any pages that are skipped
  // due to not being in the user's page-range selection).
  // (Until bug 1631452, this will always end up at 1, per assertion near
  // the end of this function.)
  uint32_t numPagesOnThisSheet = 0;

  // Target for numPagesOnThisSheet. (bug 1631452 will make this a
  // configurable value instead of a constant.)
  static const uint32_t kDesiredPagesPerSheet = 1;

  // NOTE: I'm intentionally *not* using a range-based 'for' looop here, since
  // we potentially mutate the frame list (appending to the end) during the
  // list, which is not generally safe with range-based 'for' loops.
  for (auto* childFrame = mFrames.FirstChild(); childFrame;
       childFrame = childFrame->GetNextSibling()) {
    MOZ_ASSERT(childFrame->IsPageFrame(),
               "we're only expecting page frames as children");
    auto* pageFrame = static_cast<nsPageFrame*>(childFrame);

    // Be sure our child has a pointer to the nsSharedPageData and knows its
    // page number:
    pageFrame->SetSharedPageData(mPD);
    pageFrame->DeterminePageNum();

    if (!TagIfSkippedByCustomRange(pageFrame, pageFrame->GetPageNum(), mPD)) {
      numPagesOnThisSheet++;  // Page isn't skipped, so we increment count.
    }

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

    // Since we don't support incremental reflow in printed documents (see the
    // early-return in nsPageSequenceFrame::Reflow), we can assume that this
    // was the first time that pageFrame has been reflowed, and so there's no
    // way that it could already have a next-in-flow. If it *did* have a
    // next-in-flow, we would need to handle it in the 'status' logic below.
    NS_ASSERTION(!pageFrame->GetNextInFlow(), "bad child flow list");

    // Did this page complete the document, or do we need to generate
    // another page frame?
    if (status.IsFullyComplete()) {
      // The page we just reflowed is the final page! Record its page number
      // as the number of pages:
      mPD->mRawNumPages = pageFrame->GetPageNum();
    } else {
      // Create a continuation for our page frame. We add the continuation to
      // our child list, and then potentially push it to our overflow list, if
      // it really belongs on the next sheet.
      nsIFrame* continuingPage =
          PresShell()->FrameConstructor()->CreateContinuingFrame(pageFrame,
                                                                 this);
      mFrames.InsertFrame(nullptr, pageFrame, continuingPage);
      const bool isContinuingPageSkipped =
          TagIfSkippedByCustomRange(static_cast<nsPageFrame*>(continuingPage),
                                    pageFrame->GetPageNum() + 1, mPD);

      // If we've already reached the target number of pages for this sheet,
      // and this continuation page that we just created is meant to be
      // dispayed (i.e. it's in the chosen page range), then we need to push it
      // to our overflow list so that it'll go onto a subsequent sheet.
      // Otherwise we leave it on this sheet. This ensures we *only* generate
      // another sheet IFF there's a displayable page that will end up on it.
      if (numPagesOnThisSheet == kDesiredPagesPerSheet &&
          !isContinuingPageSkipped) {
        PushChildrenToOverflow(continuingPage, pageFrame);
        aStatus.SetIncomplete();
      }
    }
  }

  // This should hold for the first sheet, because our UI should prevent the
  // user from creating a 0-length page range; and it should hold for
  // subsequent sheets because we should only create an additional sheet when
  // we discover a displayable (i.e. non-skipped) page that we need to push
  // to that new sheet.
  MOZ_ASSERT(numPagesOnThisSheet > 0 &&
             "Shouldn't create a sheet with no displayable pages on it");
  MOZ_ASSERT(numPagesOnThisSheet <= kDesiredPagesPerSheet,
             "Shouldn't have more than desired number of displayable pages "
             "on this sheet");

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
