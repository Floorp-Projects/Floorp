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

#include "nsHTMLButtonControlFrame.h"

#include "nsCOMPtr.h"
#include "nsHTMLContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"

#include "nsIRenderingContext.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsISupports.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsIImage.h"
#include "nsStyleConsts.h"
#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIDocument.h"
#include "nsButtonFrameRenderer.h"
#include "nsFormControlFrame.h"
#include "nsFrameManager.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsStyleSet.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsDisplayList.h"

nsIFrame*
NS_NewHTMLButtonControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsHTMLButtonControlFrame(aContext);
}

nsHTMLButtonControlFrame::nsHTMLButtonControlFrame(nsStyleContext* aContext)
  : nsHTMLContainerFrame(aContext)
{
}

nsHTMLButtonControlFrame::~nsHTMLButtonControlFrame()
{
}

void
nsHTMLButtonControlFrame::Destroy()
{
  nsFormControlFrame::RegUnRegAccessKey(NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  nsHTMLContainerFrame::Destroy();
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Init(
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
  if (NS_SUCCEEDED(rv)) {
    mRenderer.SetFrame(this, GetPresContext());
  }
  return rv;
}

nsrefcnt nsHTMLButtonControlFrame::AddRef(void)
{
  NS_WARNING("not supported");
  return 1;
}

nsrefcnt nsHTMLButtonControlFrame::Release(void)
{
  NS_WARNING("not supported");
  return 1;
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsHTMLButtonControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsHTMLButtonControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    nsIContent* content = GetContent();
    nsCOMPtr<nsIDOMHTMLButtonElement> buttonElement(do_QueryInterface(content));
    if (buttonElement) //If turned XBL-base form control off, the frame contains HTML 4 button
      return accService->CreateHTML4ButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(content));
    if (inputElement) //If turned XBL-base form control on, the frame contains normal HTML button
      return accService->CreateHTMLButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

nsIAtom*
nsHTMLButtonControlFrame::GetType() const
{
  return nsGkAtoms::HTMLButtonControlFrame;
}

PRBool
nsHTMLButtonControlFrame::IsReset(PRInt32 type)
{
  if (NS_FORM_BUTTON_RESET == type) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

PRBool
nsHTMLButtonControlFrame::IsSubmit(PRInt32 type)
{
  if (NS_FORM_BUTTON_SUBMIT == type) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

void 
nsHTMLButtonControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent*     aEvent,
                                      nsEventStatus*  aEventStatus)
{
  // if disabled do nothing
  if (mRenderer.isDisabled()) {
    return NS_OK;
  }

  // mouse clicks are handled by content
  // we don't want our children to get any events. So just pass it to frame.
  return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


NS_IMETHODIMP
nsHTMLButtonControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists)
{
  nsDisplayList onTop;
  if (IsVisibleForPainting(aBuilder)) {
    nsresult rv = mRenderer.DisplayButton(aBuilder, aLists.BorderBackground(), &onTop);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  nsDisplayListCollection set;
  // Do not allow the child subtree to receive events.
  if (!aBuilder->IsForEventDelivery()) {
    nsresult rv =
      BuildDisplayListForChild(aBuilder, mFrames.FirstChild(), aDirtyRect, set,
                               DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT);
    NS_ENSURE_SUCCESS(rv, rv);
    // That should put the display items in set.Content()
  }
  
  // Put the foreground outline and focus rects on top of the children
  set.Content()->AppendToTop(&onTop);

    // XXX This is temporary
  // clips to its size minus the border 
  // but the real problem is the FirstChild (the AreaFrame)
  // isn't being constrained properly
  // Bug #17474
  nsMargin border = GetStyleBorder()->GetBorder();
  nsRect rect(aBuilder->ToReferenceFrame(this), GetSize());
  rect.Deflate(border);
  
  nsresult rv = OverflowClip(aBuilder, set, aLists, rect);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = DisplayOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  // to draw border when selected in editor
  return DisplaySelectionOverlay(aBuilder, aLists);
}

nscoord
nsHTMLButtonControlFrame::GetMinWidth(nsIRenderingContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                kid,
                                                nsLayoutUtils::MIN_WIDTH);

  result += mRenderer.GetAddedButtonBorderAndPadding().LeftRight();

  return result;
}

nscoord
nsHTMLButtonControlFrame::GetPrefWidth(nsIRenderingContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  
  nsIFrame* kid = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                kid,
                                                nsLayoutUtils::PREF_WIDTH);
  result += mRenderer.GetAddedButtonBorderAndPadding().LeftRight();
  return result;
}

NS_IMETHODIMP 
nsHTMLButtonControlFrame::Reflow(nsPresContext* aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLButtonControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_PRECONDITION(aReflowState.ComputedWidth() != NS_INTRINSICSIZE,
                  "Should have real computed width by now");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
  }

  // Reflow the child
  nsIFrame* firstKid = mFrames.FirstChild();

  // XXXbz Eventually we may want to check-and-bail if
  // !aReflowState.ShouldReflowAllKids() &&
  // !(firstKid->GetStateBits() &
  //   (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)).
  // We'd need to cache our ascent for that, of course.
  
  nsMargin focusPadding = mRenderer.GetAddedButtonBorderAndPadding();
  
  // Reflow the contents of the button.
  ReflowButtonContents(aPresContext, aDesiredSize, aReflowState, firstKid,
                       focusPadding, aStatus);

  aDesiredSize.width = aReflowState.ComputedWidth();

  // If computed use the computed value.
  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE) 
    aDesiredSize.height = aReflowState.mComputedHeight;
  else
    aDesiredSize.height += focusPadding.TopBottom();
  
  aDesiredSize.width += aReflowState.mComputedBorderPadding.LeftRight();
  aDesiredSize.height += aReflowState.mComputedBorderPadding.TopBottom();

  // Make sure we obey min/max-height.  Note that we do this after adjusting
  // for borderpadding, since buttons have border-box sizing...

  // XXXbz unless someone overrides that, of course!  We should really consider
  // exposing nsHTMLReflowState::AdjustComputed* or something.
  aDesiredSize.height = NS_CSS_MINMAX(aDesiredSize.height,
                                      aReflowState.mComputedMinHeight,
                                      aReflowState.mComputedMaxHeight);

  aDesiredSize.ascent +=
    aReflowState.mComputedBorderPadding.top + focusPadding.top;

  aDesiredSize.mOverflowArea =
    nsRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  ConsiderChildOverflow(aDesiredSize.mOverflowArea, firstKid);
  FinishAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

void
nsHTMLButtonControlFrame::ReflowButtonContents(nsPresContext* aPresContext,
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               const nsHTMLReflowState& aReflowState,
                                               nsIFrame* aFirstKid,
                                               nsMargin aFocusPadding,
                                               nsReflowStatus& aStatus)
{
  nsSize availSize(aReflowState.ComputedWidth(), NS_INTRINSICSIZE);

  // Indent the child inside us by the focus border. We must do this separate
  // from the regular border.
  availSize.width -= aFocusPadding.LeftRight();
  availSize.width = PR_MAX(availSize.width,0);
  
  // See whether out availSize's width is big enough.  If it's smaller than our
  // intrinsic min width, that means that the kid wouldn't really fit; for a
  // better look in such cases we adjust the available width and our left
  // offset to allow the kid to spill left into our padding.
  nscoord xoffset = aFocusPadding.left + aReflowState.mComputedBorderPadding.left;
  nscoord extrawidth = GetMinWidth(aReflowState.rendContext) -
    aReflowState.ComputedWidth();
  if (extrawidth > 0) {
    nscoord extraleft = extrawidth / 2;
    nscoord extraright = extrawidth - extraleft;
    NS_ASSERTION(extraright >=0, "How'd that happen?");
    
    // Do not allow the extras to be bigger than the relevant padding
    extraleft = PR_MIN(extraleft, aReflowState.mComputedPadding.left);
    extraright = PR_MIN(extraright, aReflowState.mComputedPadding.right);
    xoffset -= extraleft;
    availSize.width += extraleft + extraright;
  }
  
  nsHTMLReflowState reflowState(aPresContext, aReflowState, aFirstKid,
                                availSize);

  ReflowChild(aFirstKid, aPresContext, aDesiredSize, reflowState,
              xoffset,
              aFocusPadding.top + aReflowState.mComputedBorderPadding.top,
              0, aStatus);
  
  // calculate the min internal height so the contents gets centered correctly.
  // XXXbz this assumes border-box sizing.
  nscoord minInternalHeight = aReflowState.mComputedMinHeight -
    aReflowState.mComputedBorderPadding.TopBottom();
  minInternalHeight = PR_MAX(minInternalHeight, 0);

  // center child vertically
  nscoord yoff = 0;
  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE) {
    yoff = (aReflowState.mComputedHeight - aDesiredSize.height)/2;
    if (yoff < 0) {
      yoff = 0;
    }
  } else if (aDesiredSize.height < minInternalHeight) {
    yoff = (minInternalHeight - aDesiredSize.height) / 2;
  }

  // Place the child
  FinishReflowChild(aFirstKid, aPresContext, &reflowState, aDesiredSize,
                    xoffset,
                    yoff + aFocusPadding.top + aReflowState.mComputedBorderPadding.top, 0);

  if (aDesiredSize.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE)
    aDesiredSize.ascent = aFirstKid->GetBaseline();

  // Adjust the baseline by our offset (since we moved the child's
  // baseline by that much).
  aDesiredSize.ascent += yoff;
}

/* virtual */ PRBool
nsHTMLButtonControlFrame::IsContainingBlock() const
{
  return PR_TRUE;
}

PRIntn
nsHTMLButtonControlFrame::GetSkipSides() const
{
  return 0;
}

nsresult nsHTMLButtonControlFrame::SetFormProperty(nsIAtom* aName, const nsAString& aValue)
{
  if (nsGkAtoms::value == aName) {
    return mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::value,
                             aValue, PR_TRUE);
  }
  return NS_OK;
}

nsresult nsHTMLButtonControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  if (nsGkAtoms::value == aName)
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue);

  return NS_OK;
}

nsStyleContext*
nsHTMLButtonControlFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  return mRenderer.GetStyleContext(aIndex);
}

void
nsHTMLButtonControlFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                                    nsStyleContext* aStyleContext)
{
  mRenderer.SetStyleContext(aIndex, aStyleContext);
}

NS_IMETHODIMP 
nsHTMLButtonControlFrame::AppendFrames(nsIAtom*        aListName,
                                       nsIFrame*       aFrameList)
{
  NS_NOTREACHED("unsupported operation");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::InsertFrames(nsIAtom*        aListName,
                                       nsIFrame*       aPrevFrame,
                                       nsIFrame*       aFrameList)
{
  NS_NOTREACHED("unsupported operation");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::RemoveFrame(nsIAtom*        aListName,
                                      nsIFrame*       aOldFrame)
{
  NS_NOTREACHED("unsupported operation");
  return NS_ERROR_UNEXPECTED;
}
