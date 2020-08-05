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

  // nsIFrame overrides
  void Reflow(nsPresContext* aPresContext, ReflowOutput& aReflowOutput,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  // Return our first page frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

 private:
  // Private construtor & destructor, to avoid accidental (non-FrameArena)
  // instantiation/deletion:
  PrintedSheetFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID) {}
  ~PrintedSheetFrame() = default;

  // Note: this will be set before reflow, and it's strongly owned by our
  // nsPageSequenceFrame, which outlives us.
  nsSharedPageData* mPD = nullptr;
};

}  // namespace mozilla

#endif /* LAYOUT_GENERIC_PRINTEDSHEETFRAME_H_ */
