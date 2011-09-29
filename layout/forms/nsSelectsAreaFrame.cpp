/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsSelectsAreaFrame.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIContent.h"
#include "nsListControlFrame.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewSelectsAreaFrame(nsIPresShell* aShell, nsStyleContext* aContext, PRUint32 aFlags)
{
  nsSelectsAreaFrame* it = new (aShell) nsSelectsAreaFrame(aContext);

  if (it) {
    // We need NS_BLOCK_FLOAT_MGR to ensure that the options inside the select
    // aren't expanded by right floats outside the select.
    it->SetFlags(aFlags | NS_BLOCK_FLOAT_MGR);
  }

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

  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem);
};

void nsDisplayOptionEventGrabber::HitTest(nsDisplayListBuilder* aBuilder,
    const nsRect& aRect, HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsTArray<nsIFrame*> outFrames;
  mList.HitTest(aBuilder, aRect, aState, &outFrames);

  for (PRUint32 i = 0; i < outFrames.Length(); i++) {
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

nsDisplayWrapList* nsDisplayOptionEventGrabber::WrapWithClone(
    nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  return new (aBuilder)
    nsDisplayOptionEventGrabber(aBuilder, aItem->GetUnderlyingFrame(), aItem);
}

class nsOptionEventGrabberWrapper : public nsDisplayWrapper
{
public:
  nsOptionEventGrabberWrapper() {}
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) {
    // We can't specify the underlying frame here. We need this list to be
    // exploded if sorted.
    return new (aBuilder) nsDisplayOptionEventGrabber(aBuilder, nsnull, aList);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {
    return new (aBuilder) nsDisplayOptionEventGrabber(aBuilder, aItem->GetUnderlyingFrame(), aItem);
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
  return nsnull;
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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder) {
    // override bounds because the list item focus ring may extend outside
    // the nsSelectsAreaFrame
    nsListControlFrame* listFrame = GetEnclosingListFrame(GetUnderlyingFrame());
    return listFrame->GetVisualOverflowRect() +
           aBuilder->ToReferenceFrame(listFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) {
    nsListControlFrame* listFrame = GetEnclosingListFrame(GetUnderlyingFrame());
    // listFrame must be non-null or we wouldn't get called.
    listFrame->PaintFocus(*aCtx, aBuilder->ToReferenceFrame(listFrame));
  }
  NS_DISPLAY_DECL_NAME("ListFocus", TYPE_LIST_FOCUS)
};

NS_IMETHODIMP
nsSelectsAreaFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  if (!aBuilder->IsForEventDelivery())
    return BuildDisplayListInternal(aBuilder, aDirtyRect, aLists);
    
  nsDisplayListCollection set;
  nsresult rv = BuildDisplayListInternal(aBuilder, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsOptionEventGrabberWrapper wrapper;
  return wrapper.WrapLists(aBuilder, this, set, aLists);
}

nsresult
nsSelectsAreaFrame::BuildDisplayListInternal(nsDisplayListBuilder*   aBuilder,
                                             const nsRect&           aDirtyRect,
                                             const nsDisplayListSet& aLists)
{
  nsresult rv = nsBlockFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsListControlFrame* listFrame = GetEnclosingListFrame(this);
  if (listFrame && listFrame->IsFocused()) {
    // we can't just associate the display item with the list frame,
    // because then the list's scrollframe won't clip it (the scrollframe
    // only clips contained descendants).
    return aLists.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayListFocus(aBuilder, this));
  }
  
  return NS_OK;
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

  // Check whether we need to suppress scrolbar updates.  We want to do that if
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
      list->SetSuppressScrollbarUpdate(PR_TRUE);
    }
  }

  return rv;
}
