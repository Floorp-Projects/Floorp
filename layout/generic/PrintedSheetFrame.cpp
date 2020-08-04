/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Rendering object for a printed or print-previewed sheet of paper */

#include "PrintedSheetFrame.h"

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
  // TODO: Implement me
}

void PrintedSheetFrame::Reflow(nsPresContext* aPresContext,
                               ReflowOutput& aReflowOutput,
                               const ReflowInput& aReflowInput,
                               nsReflowStatus& aStatus) {
  // TODO: Implement me
}

#ifdef DEBUG_FRAME_DUMP
nsresult PrintedSheetFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"PrintedSheet"_ns, aResult);
}
#endif

}  // namespace mozilla
