/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsSelectsAreaFrame.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIContent.h"
#include "nsListControlFrame.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewSelectsAreaFrame(nsIPresShell* aShell, nsStyleContext* aContext, uint32_t aFlags)
{
  nsSelectsAreaFrame* it = new (aShell) nsSelectsAreaFrame(aContext);

  // We need NS_BLOCK_FLOAT_MGR to ensure that the options inside the select
  // aren't expanded by right floats outside the select.
  it->SetFlags(aFlags | NS_BLOCK_FLOAT_MGR);

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
  nsDisplayOptionEventGrabber(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame, nsDisplayItem* aItem)
    : nsDisplayWrapList(aBuilder, aFrame, aItem) {}
  nsDisplayOptionEventGrabber(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList) {}
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  NS_DISPLAY_DECL_NAME("OptionEventGrabber", TYPE_OPTION_EVENT_GRABBER)
};

void nsDisplayOptionEventGrabber::HitTest(nsDisplayListBuilder* aBuilder,
    const nsRect& aRect, HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsTArray<nsIFrame*> outFrames;
  mList.HitTest(aBuilder, aRect, aState, &outFrames);

  for (uint32_t i = 0; i < outFrames.Length(); i++) {
    nsIFrame* selectedFrame = outFrames.ElementAt(i);
    while (selectedFrame &&
           !(selectedFrame->GetContent() &&
             selectedFrame->GetContent()->IsHTML(nsGkAtoms::option))) {
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

class nsOptionEventGrabberWrapper : public nsDisplayWrapper
{
public:
  nsOptionEventGrabberWrapper() {}
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) {
    return new (aBuilder) nsDisplayOptionEventGrabber(aBuilder, aFrame, aList);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {
    return new (aBuilder) nsDisplayOptionEventGrabber(aBuilder, aItem->Frame(), aItem);
  }
};

static nsListControlFrame* GetEnclosingListFrame(nsIFrame* aSelectsAreaFrame)
{
  nsIFrame* frame = aSelectsAreaFrame->GetParent();
  while (frame) {
    if (frame->GetType() == nsGkAtoms::listControlFrame)
      return static_cast<nsListControlFrame*>(frame);
    frame = frame->GetParent();
  }
  return nullptr;
}

class nsDisplayListFocus : public nsDisplayItem {
public:
  nsDisplayListFocus(nsDisplayListBuilder* aBuilder,
                     nsSelectsAreaFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayListFocus);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayListFocus() {
    MOZ_COUNT_DTOR(nsDisplayListFocus);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
    *aSnap = false;
    // override bounds because the list item focus ring may extend outside
    // the nsSelectsAreaFrame
    nsListControlFrame* listFrame = GetEnclosingListFrame(Frame());
    return listFrame->GetVisualOverflowRectRelativeToSelf() +
           listFrame->GetOffsetToCrossDoc(ReferenceFrame());
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) {
    nsListControlFrame* listFrame = GetEnclosingListFrame(Frame());
    // listFrame must be non-null or we wouldn't get called.
    listFrame->PaintFocus(*aCtx, aBuilder->ToReferenceFrame(listFrame));
  }
  NS_DISPLAY_DECL_NAME("ListFocus", TYPE_LIST_FOCUS)
};

void
nsSelectsAreaFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  if (!aBuilder->IsForEventDelivery()) {
    BuildDisplayListInternal(aBuilder, aDirtyRect, aLists);
    return;
  }

  nsDisplayListCollection set;
  BuildDisplayListInternal(aBuilder, aDirtyRect, set);
  
  nsOptionEventGrabberWrapper wrapper;
  wrapper.WrapLists(aBuilder, this, set, aLists);
}

void
nsSelectsAreaFrame::BuildDisplayListInternal(nsDisplayListBuilder*   aBuilder,
                                             const nsRect&           aDirtyRect,
                                             const nsDisplayListSet& aLists)
{
  nsBlockFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);

  nsListControlFrame* listFrame = GetEnclosingListFrame(this);
  if (listFrame && listFrame->IsFocused()) {
    // we can't just associate the display item with the list frame,
    // because then the list's scrollframe won't clip it (the scrollframe
    // only clips contained descendants).
    aLists.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayListFocus(aBuilder, this));
  }
}

NS_IMETHODIMP 
nsSelectsAreaFrame::Reflow(nsPresContext*           aPresContext, 
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState, 
                           nsReflowStatus&          aStatus)
{
  nsListControlFrame* list = GetEnclosingListFrame(this);
  NS_ASSERTION(list,
               "Must have an nsListControlFrame!  Frame constructor is "
               "broken");
  
  bool isInDropdownMode = list->IsInDropDownMode();
  
  // See similar logic in nsListControlFrame::Reflow and
  // nsListControlFrame::ReflowAsDropdown.  We need to match it here.
  nscoord oldHeight;
  if (isInDropdownMode) {
    // Store the height now in case it changes during
    // nsBlockFrame::Reflow for some odd reason.
    if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
      oldHeight = GetSize().height;
    } else {
      oldHeight = NS_UNCONSTRAINEDSIZE;
    }
  }
  
  nsresult rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize,
                                    aReflowState, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check whether we need to suppress scrollbar updates.  We want to do that if
  // we're in a possible first pass and our height of a row has changed.
  if (list->MightNeedSecondPass()) {
    nscoord newHeightOfARow = list->CalcHeightOfARow();
    // We'll need a second pass if our height of a row changed.  For
    // comboboxes, we'll also need it if our height changed.  If we're going
    // to do a second pass, suppress scrollbar updates for this pass.
    if (newHeightOfARow != mHeightOfARow ||
        (isInDropdownMode && (oldHeight != aDesiredSize.height ||
                              oldHeight != GetSize().height))) {
      mHeightOfARow = newHeightOfARow;
      list->SetSuppressScrollbarUpdate(true);
    }
  }

  return rv;
}
