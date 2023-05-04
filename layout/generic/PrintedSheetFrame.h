/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Rendering object for a printed or print-previewed sheet of paper */

#ifndef LAYOUT_GENERIC_PRINTEDSHEETFRAME_H_
#define LAYOUT_GENERIC_PRINTEDSHEETFRAME_H_

#include "nsContainerFrame.h"
#include "nsHTMLParts.h"

class nsSharedPageData;

namespace mozilla {

class PrintedSheetFrame final : public nsContainerFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(PrintedSheetFrame)

  friend PrintedSheetFrame* ::NS_NewPrintedSheetFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  void SetSharedPageData(nsSharedPageData* aPD) { mPD = aPD; }

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

  float GetPagesPerSheetScale() const { return mPagesPerSheetScale; }
  uint32_t GetPagesPerSheetNumCols() const { return mPagesPerSheetNumCols; }
  nsPoint GetPagesPerSheetGridOrigin() const {
    return mPagesPerSheetGridOrigin;
  }
  float GetGridCellWidth() const { return mGridCellWidth; }
  float GetGridCellHeight() const { return mGridCellHeight; }

 private:
  // Private construtor & destructor, to avoid accidental (non-FrameArena)
  // instantiation/deletion:
  PrintedSheetFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID) {}
  ~PrintedSheetFrame() = default;

  // Helper function to populate some pages-per-sheet metrics in our
  // nsSharedPageData.
  void ComputePagesPerSheetOriginAndScale();

  // Note: this will be set before reflow, and it's strongly owned by our
  // nsPageSequenceFrame, which outlives us.
  nsSharedPageData* mPD = nullptr;
  // The number of visible pages in this sheet.
  uint32_t mNumPages = 0;

  // The mPagesPerSheet{...} members are only used if
  // PagesPerSheetInfo()->mNumPages > 1.  They're initialized with reasonable
  // defaults here (which correspond to what we do for the regular
  // 1-page-per-sheet scenario, though we don't actually use these members in
  // that case).  If we're in >1 pages-per-sheet scenario, then these members
  // will be assigned "real" values during the reflow of the first
  // PrintedSheetFrame.
  float mPagesPerSheetScale = 1.0f;
  // Number of "columns" in our pages-per-sheet layout. For example: if we're
  // printing with 6 pages-per-sheet, then this could be either 3 or 2,
  // depending on whether we're printing portrait-oriented pages onto a
  // landscape-oriented sheet (3 cols) vs. if we're printing landscape-oriented
  // pages onto a portrait-oriented sheet (2 cols).
  uint32_t mPagesPerSheetNumCols = 1;

  nsPoint mPagesPerSheetGridOrigin;

  // The size of each cell on the sheet into which pages are to be placed.
  // (The default values are arbitrary.)
  float mGridCellWidth = 1.0f;
  float mGridCellHeight = 1.0f;
};

}  // namespace mozilla

#endif /* LAYOUT_GENERIC_PRINTEDSHEETFRAME_H_ */
