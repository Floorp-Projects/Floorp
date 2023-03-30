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
};

}  // namespace mozilla

#endif /* LAYOUT_GENERIC_PRINTEDSHEETFRAME_H_ */
