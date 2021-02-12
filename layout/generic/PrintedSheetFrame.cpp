/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Rendering object for a printed or print-previewed sheet of paper */

#include "mozilla/PrintedSheetFrame.h"

#include <tuple>

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

std::tuple<uint32_t, uint32_t> GetRowAndColFromIdx(uint32_t aIdxOnSheet,
                                                   uint32_t aNumCols) {
  // Compute the row index by *dividing* the item's ordinal position by how
  // many items fit in each row (i.e. the number of columns), and flooring.
  // Compute the column index by getting the remainder of that division:
  // Notably, mNumRows is irrelevant to this computation; that's because
  // we're adding new items column-by-column rather than row-by-row.
  return {aIdxOnSheet / aNumCols, aIdxOnSheet % aNumCols};
}

// Helper for BuildDisplayList:
gfx::Matrix4x4 ComputePagesPerSheetTransform(nsIFrame* aFrame,
                                             float aAppUnitsPerPixel) {
  MOZ_ASSERT(aFrame->IsPageFrame());
  auto* pageFrame = static_cast<nsPageFrame*>(aFrame);

  // Variables that we use in our transform (initialized with reasonable
  // defaults that work for the regular one-page-per-sheet scenario):
  float scale = 1.0f;
  nsPoint gridOrigin;
  uint32_t rowIdx = 0;
  uint32_t colIdx = 0;

  nsSharedPageData* pd = pageFrame->GetSharedPageData();
  if (pd) {
    const auto* ppsInfo = pd->PagesPerSheetInfo();
    if (ppsInfo->mNumPages > 1) {
      scale = pd->mPagesPerSheetScale;
      gridOrigin = pd->mPagesPerSheetGridOrigin;
      std::tie(rowIdx, colIdx) = GetRowAndColFromIdx(pageFrame->IndexOnSheet(),
                                                     pd->mPagesPerSheetNumCols);
    }
  }

  // Scale down the page based on the above-computed scale:
  auto transform = gfx::Matrix4x4::Scaling(scale, scale, 1);

  // Draw the page at an offset, to get it in its pages-per-sheet "cell":
  nsSize pageSize = pageFrame->PresContext()->GetPageSize();
  transform.PreTranslate(
      NSAppUnitsToFloatPixels(colIdx * pageSize.width, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(rowIdx * pageSize.height, aAppUnitsPerPixel), 0);

  // Also add the grid origin as an offset (so that we're not drawing into the
  // sheet's unwritable area). Note that this is a PostTranslate operation
  // (vs. PreTranslate above), since gridOrigin is an offset on the sheet
  // itself, whereas the offset above was in the scaled coordinate space of the
  // pages.
  return transform.PostTranslate(
      NSAppUnitsToFloatPixels(gridOrigin.x, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(gridOrigin.y, aAppUnitsPerPixel), 0);
}

void PrintedSheetFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayListSet& aLists) {
  if (PresContext()->IsScreen()) {
    // Draw the background/shadow/etc. of a blank sheet of paper, for
    // print-preview.
    DisplayBorderBackgroundOutline(aBuilder, aLists);
  }

  // Let each of our children (pages) draw itself, with a supplemental
  // transform to shrink it & place it in its pages-per-sheet cell:
  for (auto* frame : mFrames) {
    if (!frame->HasAnyStateBits(NS_PAGE_SKIPPED_BY_CUSTOM_RANGE)) {
      // We'll be drawing our nsPageFrame children with a (usually-trivial)
      // N-pages-per-sheet transform applied, so our passed-in visible rect
      // isn't meaningful while we're drawing our children, because the
      // transform could scale down content whose coordinates are off-screen
      // such that it ends up on-screen. So: we temporarily update the visible
      // rect to be the child nsPageFrame's whole frame-rect (represented in
      // this PrintedSheetFrame's coordinate space.
      nsDisplayList content;
      {
        nsRect visibleRect = frame->GetRect();
        nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
            aBuilder, this, visibleRect, visibleRect);

        frame->BuildDisplayListForStackingContext(aBuilder, &content);
      }
      content.AppendNewToTop<nsDisplayTransform>(aBuilder, frame, &content,
                                                 content.GetBuildingRect(),
                                                 ComputePagesPerSheetTransform);

      aLists.Content()->AppendToTop(&content);
    }
  }
}

// If the given page is included in the user's page range, this function
// returns false. Otherwise, it tags the page with the
// NS_PAGE_SKIPPED_BY_CUSTOM_RANGE state bit and returns true.
static bool TagIfSkippedByCustomRange(nsPageFrame* aPageFrame, int32_t aPageNum,
                                      nsSharedPageData* aPD) {
  if (!nsIPrintSettings::IsPageSkipped(aPageNum, aPD->mPageRanges)) {
    MOZ_ASSERT(!aPageFrame->HasAnyStateBits(NS_PAGE_SKIPPED_BY_CUSTOM_RANGE),
               "page frames NS_PAGE_SKIPPED_BY_CUSTOM_RANGE state should "
               "only be set if we actually want to skip the page");
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

  // This is the app-unit size of each page (in physical & logical units):
  const nsSize physPageSize = aPresContext->GetPageSize();
  const LogicalSize pageSize(wm, physPageSize);

  // Count the number of pages that are displayed on this sheet (i.e. how many
  // child frames we end up laying out, excluding any pages that are skipped
  // due to not being in the user's page-range selection).
  uint32_t numPagesOnThisSheet = 0;

  // Target for numPagesOnThisSheet.
  const uint32_t desiredPagesPerSheet = mPD->PagesPerSheetInfo()->mNumPages;

  // If we're the first continuation and we're doing >1 pages per sheet,
  // precompute some metrics that we'll use when painting the pages:
  if (desiredPagesPerSheet > 1 && !GetPrevContinuation()) {
    ComputePagesPerSheetOriginAndScale();
  }

  // NOTE: I'm intentionally *not* using a range-based 'for' loop here, since
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
      // The page is going to be displayed on this sheet. Tell it its index
      // among the displayed pages, so we can use that to compute its "cell"
      // when painting.
      pageFrame->SetIndexOnSheet(numPagesOnThisSheet);
      numPagesOnThisSheet++;
    }

    ReflowInput pageReflowInput(aPresContext, aReflowInput, pageFrame,
                                pageSize);

    // For layout purposes, we position *all* our nsPageFrame children at our
    // origin. Then, if we have multiple pages-per-sheet, we'll shrink & shift
    // each one into the right position as a paint-time effect, in
    // BuildDisplayList.
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
      // displayed (i.e. it's in the chosen page range), then we need to push it
      // to our overflow list so that it'll go onto a subsequent sheet.
      // Otherwise we leave it on this sheet. This ensures we *only* generate
      // another sheet IFF there's a displayable page that will end up on it.
      if (numPagesOnThisSheet >= desiredPagesPerSheet &&
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

  // XXXdholbert In certain edge cases (e.g. after a page-orientation-flip that
  // reduces the page count), it's possible for us to be given a page range
  // that is *entirely out-of-bounds* (with "from" & "to" both being larger
  // than our actual page-number count).  This scenario produces a single
  // PrintedSheetFrame with zero displayable pages on it, which is a weird
  // state to be in. This is hopefully a scenario that the frontend code can
  // detect and recover from (e.g. by clamping the range to our reported
  // `rawNumPages`), but it can't do that until *after* we've completed this
  // problematic reflow and can reported an up-to-date `rawNumPages` to the
  // frontend.  So: to give the frontend a chance to intervene and apply some
  // correction/clamping to its print-range parameters, we soften this
  // assertion *specifically for the first printed sheet*.
  if (!GetPrevContinuation()) {
    NS_WARNING_ASSERTION(numPagesOnThisSheet > 0,
                         "Shouldn't create a sheet with no displayable pages "
                         "on it");
  } else {
    MOZ_ASSERT(numPagesOnThisSheet > 0,
               "Shouldn't create a sheet with no displayable pages on it");
  }

  MOZ_ASSERT(numPagesOnThisSheet <= desiredPagesPerSheet,
             "Shouldn't have more than desired number of displayable pages "
             "on this sheet");
  mNumPages = numPagesOnThisSheet;

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

void PrintedSheetFrame::ComputePagesPerSheetOriginAndScale() {
  MOZ_ASSERT(mPD->PagesPerSheetInfo()->mNumPages > 1,
             "Unnecessary to call this in a regular 1-page-per-sheet scenario; "
             "the computed values won't ever be used in that case");
  MOZ_ASSERT(!GetPrevContinuation(),
             "Only needs to be called once, so 1st continuation handles it");

  // The "full-scale" size of a page (if it weren't shrunk down into a grid):
  const nsSize pageSize = PresContext()->GetPageSize();

  // Compute the space available for the pages-per-sheet "page grid" (just
  // subtract the sheet's unwriteable margin area):
  nsSize availSpaceOnSheet = pageSize;
  nsMargin uwm = nsPresContext::CSSTwipsToAppUnits(
      mPD->mPrintSettings->GetUnwriteableMarginInTwips());

  if (mPD->mPrintSettings->HasOrthogonalSheetsAndPages()) {
    // The pages will be rotated to be orthogonal to the physical sheet.  To
    // account for that, we rotate the components of availSpaceOnSheet and uwm,
    // so that we can reason about them here from the perspective of a
    // "pageSize"-oriented *page*.
    std::swap(availSpaceOnSheet.width, availSpaceOnSheet.height);

    // Note that the pages are rotated 90 degrees clockwise when placed onto a
    // sheet (so that, e.g. in a scenario with two side-by-side portait pages
    // that are rotated & placed onto a sheet, the "left" edge of the first
    // page is at the "top" of the sheet and hence comes out of the printer
    // first, etc).  So: given `nsMargin uwm` whose sides correspond to the
    // physical sheet's sides, we have to rotate 90 degrees *counter-clockwise*
    // in order to "cancel out" the page rotation and to represent it in the
    // page's perspective. From a page's perspective, its own "top" side
    // corresponds to the physical sheet's right side, which is why we're
    // passing "uwm.right" as the "top" component here; and so on.
    nsMargin rotated(uwm.right, uwm.bottom, uwm.left, uwm.top);
    uwm = rotated;
  }

  availSpaceOnSheet.width -= uwm.LeftRight();
  availSpaceOnSheet.height -= uwm.TopBottom();
  nsPoint pageGridOrigin(uwm.left, uwm.top);

  // If there are a different number of rows vs. cols, we'll aim to put
  // the larger number of items in the longer axis.
  const auto* ppsInfo = mPD->PagesPerSheetInfo();
  uint32_t smallerNumTracks = ppsInfo->mNumPages / ppsInfo->mLargerNumTracks;
  bool pageSizeIsPortraitLike = pageSize.width > pageSize.height;
  auto numCols =
      pageSizeIsPortraitLike ? smallerNumTracks : ppsInfo->mLargerNumTracks;
  auto numRows =
      pageSizeIsPortraitLike ? ppsInfo->mLargerNumTracks : smallerNumTracks;

  // Compute the full size of the "page grid" that we'll be scaling down &
  // placing onto a given sheet:
  nsSize pageGridFullSize(numCols * pageSize.width, numRows * pageSize.height);

  if (MOZ_UNLIKELY(availSpaceOnSheet.IsEmpty() || pageGridFullSize.IsEmpty())) {
    // Either we have a 0-sized available area, or we have a 0-sized page-grid
    // to draw into the available area. This sort of thing should be rare, but
    // it can happen if there are bizarre page sizes, and/or if there's an
    // unexpectedly large unwritable margin area. Regardless: if we get here,
    // we shouldn't be drawing anything onto the sheet; so let's just use a
    // scale factor of 0, and bail early to avoid division by 0 in hScale &
    // vScale computations below.
    NS_WARNING("Zero area for pages-per-sheet grid, or zero-sized grid");
    mPD->mPagesPerSheetGridOrigin = pageGridOrigin;
    mPD->mPagesPerSheetNumCols = 1;
    mPD->mPagesPerSheetScale = 0.0f;
    return;
  }

  // Compute the scale factors required in each axis:
  float hScale =
      availSpaceOnSheet.width / static_cast<float>(pageGridFullSize.width);
  float vScale =
      availSpaceOnSheet.height / static_cast<float>(pageGridFullSize.height);

  // Choose the more restrictive scale factor (so that we don't overflow the
  // sheet's printable area in either axis). And center the page-grid in the
  // other axis (since it probably ends up with extra space).
  float scale = std::min(hScale, vScale);
  if (hScale < vScale) {
    // hScale is the more restrictive scale-factor, so we'll be using that.
    // Nudge the grid in the vertical axis to center it:
    nscoord extraSpace = availSpaceOnSheet.height -
                         NSToCoordFloor(scale * pageGridFullSize.height);
    if (MOZ_LIKELY(extraSpace > 0)) {
      pageGridOrigin.y += extraSpace / 2;
    }
  } else if (vScale < hScale) {
    // vScale is the more restrictive scale-factor, so we'll be using that.
    // Nudge the grid in the vertical axis to center it:
    nscoord extraSpace = availSpaceOnSheet.width -
                         NSToCoordFloor(scale * pageGridFullSize.width);
    if (MOZ_LIKELY(extraSpace > 0)) {
      pageGridOrigin.x += extraSpace / 2;
    }
  }
  // else, we fit exactly in both axes, with the same scale factor, so there's
  // no extra space in either axis, i.e. no need to center.

  // Update the nsSharedPageData member data:
  mPD->mPagesPerSheetGridOrigin = pageGridOrigin;
  mPD->mPagesPerSheetNumCols = numCols;
  mPD->mPagesPerSheetScale = scale;
}

#ifdef DEBUG_FRAME_DUMP
nsresult PrintedSheetFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"PrintedSheet"_ns, aResult);
}
#endif

}  // namespace mozilla
