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
NS_NewSelectsAreaFrame(nsIPresShell* aShell, PRUint32 aFlags)
{
  nsSelectsAreaFrame* it = new (aShell) nsSelectsAreaFrame;

  if (it) {
    // We need NS_BLOCK_SPACE_MGR to ensure that the options inside the select
    // aren't expanded by right floats outside the select.
    it->SetFlags(aFlags | NS_BLOCK_SPACE_MGR);
  }

  return it;
}

/*NS_IMETHODIMP
nsSelectsAreaFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kAreaFrameIID)) {
    nsIAreaFrame* tmp = (nsIAreaFrame*)this;
    *aInstancePtr = (void*)tmp;
    return NS_OK;
  }
  return nsAreaFrame::QueryInterface(aIID, aInstancePtr);
}
*/

//---------------------------------------------------------
PRBool 
nsSelectsAreaFrame::IsOptionElement(nsIContent* aContent)
{
  PRBool result = PR_FALSE;
 
  nsCOMPtr<nsIDOMHTMLOptionElement> optElem;
  if (NS_SUCCEEDED(aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLOptionElement),(void**) getter_AddRefs(optElem)))) {      
    if (optElem != nsnull) {
      result = PR_TRUE;
    }
  }
 
  return result;
}

//---------------------------------------------------------
PRBool 
nsSelectsAreaFrame::IsOptionElementFrame(nsIFrame *aFrame)
{
  nsIContent *content = aFrame->GetContent();
  if (content) {
    return IsOptionElement(content);
  }
  return PR_FALSE;
}

/**
 * This wrapper class lets us redirect mouse hits from the child frame of
 * an option element to the element's own frame.
 * REVIEW: This is what nsSelectsAreaFrame::GetFrameForPoint used to do
 */
class nsDisplayOptionEventGrabber : public nsDisplayWrapList {
public:
  nsDisplayOptionEventGrabber(nsIFrame* aFrame, nsDisplayItem* aItem)
    : nsDisplayWrapList(aFrame, aItem) {}
  nsDisplayOptionEventGrabber(nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aFrame, aList) {}
  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt);
  NS_DISPLAY_DECL_NAME("OptionEventGrabber")

  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem);
};

nsIFrame* nsDisplayOptionEventGrabber::HitTest(nsDisplayListBuilder* aBuilder,
    nsPoint aPt)
{
  nsIFrame* frame = mList.HitTest(aBuilder, aPt);

  if (frame) {
    nsIFrame* selectedFrame = frame;
    while (selectedFrame &&
           !nsSelectsAreaFrame::IsOptionElementFrame(selectedFrame)) {
      selectedFrame = selectedFrame->GetParent();
    }
    if (selectedFrame) {
      return selectedFrame;
    }
    // else, keep the original result, which could be this frame
  }

  return frame;
}

nsDisplayWrapList* nsDisplayOptionEventGrabber::WrapWithClone(
    nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  return new (aBuilder) nsDisplayOptionEventGrabber(aItem->GetUnderlyingFrame(), aItem);
}

class nsOptionEventGrabberWrapper : public nsDisplayWrapper
{
public:
  nsOptionEventGrabberWrapper() {}
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) {
    // We can't specify the underlying frame here. We need this list to be
    // exploded if sorted.
    return new (aBuilder) nsDisplayOptionEventGrabber(nsnull, aList);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {
    return new (aBuilder) nsDisplayOptionEventGrabber(aItem->GetUnderlyingFrame(), aItem);
  }
};

static nsListControlFrame* GetEnclosingListFrame(nsIFrame* aSelectsAreaFrame)
{
  nsIFrame* frame = aSelectsAreaFrame->GetParent();
  while (frame) {
    if (frame->GetType() == nsLayoutAtoms::listControlFrame)
      return NS_STATIC_CAST(nsListControlFrame*, frame);
    frame = frame->GetParent();
  }
  return nsnull;
}

static void PaintListFocus(nsIFrame* aFrame,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect, nsPoint aPt)
{
  nsListControlFrame* listFrame = GetEnclosingListFrame(aFrame);
  // listFrame must be non-null or we wouldn't get called.
  listFrame->PaintFocus(*aCtx, aPt - aFrame->GetOffsetTo(listFrame));
}

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
  nsresult rv = nsAreaFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsListControlFrame* listFrame = GetEnclosingListFrame(this);
  if (listFrame && listFrame->IsFocused()) {
    // we can't just associate the display item with the list frame,
    // because then the list's scrollframe won't clip it (the scrollframe
    // only clips contained descendants).
    return aLists.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayGeneric(this, PaintListFocus, "ListFocus"));
  }
  
  return NS_OK;
}
