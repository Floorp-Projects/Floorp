/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Rendering object for a printed or print-previewed sheet of paper */

#ifndef LAYOUT_GENERIC_PRINTEDSHEETFRAME_H_
#define LAYOUT_GENERIC_PRINTEDSHEETFRAME_H_

#include "mozilla/gfx/Point.h"
#include "nsContainerFrame.h"
#include "nsHTMLParts.h"

class nsSharedPageData;

namespace mozilla {

class PrintedSheetFrame final : public nsContainerFrame {
 public:
  using IntSize = mozilla::gfx::IntSize;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(PrintedSheetFrame)

  friend PrintedSheetFrame* ::NS_NewPrintedSheetFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  void SetSharedPageData(nsSharedPageData* aPD) { mPD = aPD; }

  // XXX: this needs a better name, since it also updates style.
  // Invokes MoveOverflowToChildList.
  // This is intended for use by callers that need to be able to get our first/
  // only nsPageFrame from our child list to examine its computed style just
  // **prior** to us being reflowed. (If our first nsPageFrame will come from
  // our prev-in-flow, we won't otherwise take ownership of it until we are
  // reflowed.)
  void ClaimPageFrameFromPrevInFlow();

  // nsIFrame overrides
  void Reflow(nsPresContext* aPresContext, ReflowOutput& aReflowOutput,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  uint32_t GetNumPages() const { return mNumPages; }

  // These methods provide information about the grid that pages should be
  // placed into in the case that there are multiple pages-per-sheet.
  uint32_t GetGridNumCols() const { return mGridNumCols; }
  nsPoint GetGridOrigin() const { return mGridOrigin; }
  nscoord GetGridCellWidth() const { return mGridCellWidth; }
  nscoord GetGridCellHeight() const { return mGridCellHeight; }

  nsSize ComputeSheetSize(const nsPresContext* aPresContext);

  /**
   * When we're printing one page-per-sheet and `page-orientation` on our
   * single nsPageFrame child should cause the page to rotate, then we want to
   * essentially rotate the sheet. We implement that by switching the
   * dimensions of this sheet (changing its orientation), sizing the
   * nsPageFrame to the original dimensions, and then applying the rotation to
   * the nsPageFrame child.
   *
   * This returns the dimensions that this frame would have without any
   * dimension swap we may have done to implement `page-orientation`. If
   * there is no rotation caused by `page-orientation`, then the value returned
   * and mRect.Size() are identical.
   */
  nsSize GetSizeForChildren() const { return mSizeForChildren; }

  /**
   * This method returns the dimensions of the physical page that the target
   * [pseudo-]printer should create. This may be different from our own
   * dimensions in the case where CSS `page-orientation` causes us to be
   * rotated, but we only support that if the PrintTarget backend supports
   * different page sizes/orientations. That's only the case for our Save-to-PDF
   * backends (possibly other save-to-file outputs in future).
   *
   * The dimensions returned are expected to be passed to
   * nsDeviceContext::BeginPage, which will pass them on to
   * PrintTarget::BeginPage to use as the physical dimensions of the page.
   */
  IntSize GetPrintTargetSizeInPoints(
      const int32_t aAppUnitsPerPhysicalInch) const;

 private:
  // Private construtor & destructor, to avoid accidental (non-FrameArena)
  // instantiation/deletion:
  PrintedSheetFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID) {}
  ~PrintedSheetFrame() = default;

  // Helper function to populate some pages-per-sheet metrics in our
  // nsSharedPageData.
  // XXXjwatt: We should investigate sharing this function for the single
  // page-per-sheet case (bug 1835782). The logic for that case
  // (nsPageFrame::ComputePageSizeScale) is somewhat different though, since
  // that case uses no sheet margins and uses the user/CSS specified margins on
  // the page, with any page scaling reverted to keep the margins unchanged.
  // We, on the other hand, use the unwriteable margins for the sheet, unscaled,
  // and use the user/CSS margins on the pages and allow them to be scaled
  // along with any pages-per-sheet scaling. (This behavior makes maximum use
  // of the sheet and, by scaling the default on the pages, results in a
  // a sensible amount of spacing between pages.)
  void ComputePagesPerSheetGridMetrics(const nsSize& aSheetSize);

  // See GetSizeForChildren.
  nsSize mSizeForChildren;

  // Note: this will be set before reflow, and it's strongly owned by our
  // nsPageSequenceFrame, which outlives us.
  nsSharedPageData* mPD = nullptr;

  // The number of visible pages in this sheet.
  uint32_t mNumPages = 0;

  // Number of "columns" in our pages-per-sheet layout. For example: if we're
  // printing with 6 pages-per-sheet, then this could be either 3 or 2,
  // depending on whether we're printing portrait-oriented pages onto a
  // landscape-oriented sheet (3 cols) vs. if we're printing landscape-oriented
  // pages onto a portrait-oriented sheet (2 cols).
  uint32_t mGridNumCols = 1;

  // The offset of the start of the multiple pages-per-sheet grid from the
  // top-left of the sheet.
  nsPoint mGridOrigin;

  // The size of each cell on the sheet into which pages are to be placed.
  // (The default values are arbitrary.)
  nscoord mGridCellWidth = 1;
  nscoord mGridCellHeight = 1;
};

}  // namespace mozilla

#endif /* LAYOUT_GENERIC_PRINTEDSHEETFRAME_H_ */
