/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsSelectsAreaFrame.h"

#include "mozilla/PresShell.h"
#include "nsIContent.h"
#include "nsListControlFrame.h"
#include "nsDisplayList.h"
#include "WritingModes.h"

using namespace mozilla;

nsContainerFrame* NS_NewSelectsAreaFrame(PresShell* aShell,
                                         ComputedStyle* aStyle,
                                         nsFrameState aFlags) {
  nsSelectsAreaFrame* it =
      new (aShell) nsSelectsAreaFrame(aStyle, aShell->GetPresContext());

  // We need NS_BLOCK_FLOAT_MGR to ensure that the options inside the select
  // aren't expanded by right floats outside the select.
  it->AddStateBits(aFlags | NS_BLOCK_FLOAT_MGR);

  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsSelectsAreaFrame)

//---------------------------------------------------------
/**
 * This wrapper class lets us redirect mouse hits from the child frame of
 * an option element to the element's own frame.
 * REVIEW: This is what nsSelectsAreaFrame::GetFrameForPoint used to do
 */
class nsDisplayOptionEventGrabber : public nsDisplayWrapList {
 public:
  nsDisplayOptionEventGrabber(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              nsDisplayItem* aItem)
      : nsDisplayWrapList(aBuilder, aFrame, aItem) {}
  nsDisplayOptionEventGrabber(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              nsDisplayList* aList)
      : nsDisplayWrapList(aBuilder, aFrame, aList) {}
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*>* aOutFrames) override;
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }
  NS_DISPLAY_DECL_NAME("OptionEventGrabber", TYPE_OPTION_EVENT_GRABBER)
};

void nsDisplayOptionEventGrabber::HitTest(nsDisplayListBuilder* aBuilder,
                                          const nsRect& aRect,
                                          HitTestState* aState,
                                          nsTArray<nsIFrame*>* aOutFrames) {
  nsTArray<nsIFrame*> outFrames;
  mList.HitTest(aBuilder, aRect, aState, &outFrames);

  for (uint32_t i = 0; i < outFrames.Length(); i++) {
    nsIFrame* selectedFrame = outFrames.ElementAt(i);
    while (selectedFrame &&
           !(selectedFrame->GetContent() &&
             selectedFrame->GetContent()->IsHTMLElement(nsGkAtoms::option))) {
      selectedFrame = selectedFrame->GetParent();
    }
    if (selectedFrame) {
      aOutFrames->AppendElement(selectedFrame);
    } else {
      // keep the original result, which could be this frame
      aOutFrames->AppendElement(outFrames.ElementAt(i));
    }
  }
}

class nsOptionEventGrabberWrapper : public nsDisplayWrapper {
 public:
  nsOptionEventGrabberWrapper() {}
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame,
                                  nsDisplayList* aList) override {
    return MakeDisplayItem<nsDisplayOptionEventGrabber>(aBuilder, aFrame,
                                                        aList);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) override {
    return MakeDisplayItem<nsDisplayOptionEventGrabber>(aBuilder,
                                                        aItem->Frame(), aItem);
  }
};

static nsListControlFrame* GetEnclosingListFrame(nsIFrame* aSelectsAreaFrame) {
  nsIFrame* frame = aSelectsAreaFrame->GetParent();
  while (frame) {
    if (frame->IsListControlFrame())
      return static_cast<nsListControlFrame*>(frame);
    frame = frame->GetParent();
  }
  return nullptr;
}

class nsDisplayListFocus : public nsPaintedDisplayItem {
 public:
  nsDisplayListFocus(nsDisplayListBuilder* aBuilder, nsSelectsAreaFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayListFocus);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayListFocus)

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = false;
    // override bounds because the list item focus ring may extend outside
    // the nsSelectsAreaFrame
    nsListControlFrame* listFrame = GetEnclosingListFrame(Frame());
    return listFrame->GetVisualOverflowRectRelativeToSelf() +
           listFrame->GetOffsetToCrossDoc(ReferenceFrame());
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override {
    nsListControlFrame* listFrame = GetEnclosingListFrame(Frame());
    // listFrame must be non-null or we wouldn't get called.
    listFrame->PaintFocus(aCtx->GetDrawTarget(),
                          aBuilder->ToReferenceFrame(listFrame));
  }
  NS_DISPLAY_DECL_NAME("ListFocus", TYPE_LIST_FOCUS)
};

void nsSelectsAreaFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  if (!aBuilder->IsForEventDelivery()) {
    BuildDisplayListInternal(aBuilder, aLists);
    return;
  }

  nsDisplayListCollection set(aBuilder);
  BuildDisplayListInternal(aBuilder, set);

  nsOptionEventGrabberWrapper wrapper;
  wrapper.WrapLists(aBuilder, this, set, aLists);
}

void nsSelectsAreaFrame::BuildDisplayListInternal(
    nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) {
  nsBlockFrame::BuildDisplayList(aBuilder, aLists);

  nsListControlFrame* listFrame = GetEnclosingListFrame(this);
  if (listFrame && listFrame->IsFocused()) {
    // we can't just associate the display item with the list frame,
    // because then the list's scrollframe won't clip it (the scrollframe
    // only clips contained descendants).
    aLists.Outlines()->AppendNewToTop<nsDisplayListFocus>(aBuilder, this);
  }
}

void nsSelectsAreaFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  nsListControlFrame* list = GetEnclosingListFrame(this);
  NS_ASSERTION(list,
               "Must have an nsListControlFrame!  Frame constructor is "
               "broken");

  bool isInDropdownMode = list->IsInDropDownMode();

  // See similar logic in nsListControlFrame::Reflow and
  // nsListControlFrame::ReflowAsDropdown.  We need to match it here.
  WritingMode wm = aReflowInput.GetWritingMode();
  nscoord oldBSize;
  if (isInDropdownMode) {
    // Store the block size now in case it changes during
    // nsBlockFrame::Reflow for some odd reason.
    if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
      oldBSize = BSize(wm);
    } else {
      oldBSize = NS_UNCONSTRAINEDSIZE;
    }
  }

  nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowInput, aStatus);

  // Check whether we need to suppress scrollbar updates.  We want to do
  // that if we're in a possible first pass and our block size of a row
  // has changed.
  if (list->MightNeedSecondPass()) {
    nscoord newBSizeOfARow = list->CalcBSizeOfARow();
    // We'll need a second pass if our block size of a row changed.  For
    // comboboxes, we'll also need it if our block size changed.  If
    // we're going to do a second pass, suppress scrollbar updates for
    // this pass.
    if (newBSizeOfARow != mBSizeOfARow ||
        (isInDropdownMode &&
         (oldBSize != aDesiredSize.BSize(wm) || oldBSize != BSize(wm)))) {
      mBSizeOfARow = newBSizeOfARow;
      list->SetSuppressScrollbarUpdate(true);
    }
  }
}
