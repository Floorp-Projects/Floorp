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
#include "nsFormControlHelper.h"
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
#include "nsHTMLAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsIImage.h"
#include "nsStyleConsts.h"
#include "nsIHTMLContent.h"
#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIDocument.h"
#include "nsButtonFrameRenderer.h"
#include "nsFormControlFrame.h"
#include "nsFrameManager.h"
#include "nsINameSpaceManager.h"
#include "nsReflowPath.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsStyleSet.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

nsresult
NS_NewHTMLButtonControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLButtonControlFrame* it = new (aPresShell) nsHTMLButtonControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsHTMLButtonControlFrame::nsHTMLButtonControlFrame()
  : nsHTMLContainerFrame()
{
  mInline = PR_TRUE;

  mCacheSize.width             = -1;
  mCacheSize.height            = -1;
  mCachedMaxElementWidth       = -1;
}

nsHTMLButtonControlFrame::~nsHTMLButtonControlFrame()
{
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Destroy(nsPresContext *aPresContext)
{
  nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  return nsHTMLContainerFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Init(nsPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  mRenderer.SetFrame(this,aPresContext);
   // cache our display type
  mInline = (NS_STYLE_DISPLAY_BLOCK != GetStyleDisplay()->mDisplay);

  PRUint32 flags = NS_BLOCK_SPACE_MGR;
  if (mInline) {
    flags |= NS_BLOCK_SHRINK_WRAP;
  }

  nsIFrame* areaFrame;
  nsIPresShell *shell = aPresContext->PresShell();
  NS_NewAreaFrame(shell, &areaFrame, flags);
  mFrames.SetFrames(areaFrame);

  // Resolve style and initialize the frame
  nsRefPtr<nsStyleContext> styleContext;
  styleContext = shell->StyleSet()->ResolvePseudoStyleFor(mContent,
                                                          nsCSSAnonBoxes::buttonContent,
                                                          mStyleContext);
  mFrames.FirstChild()->Init(aPresContext, mContent, this, styleContext, nsnull);

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


NS_IMETHODIMP_(PRInt32)
nsHTMLButtonControlFrame::GetFormControlType() const
{
  return nsFormControlHelper::GetType(mContent);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetName(nsAString* aResult)
{
  return nsFormControlHelper::GetName(mContent, aResult);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetValue(nsAString* aResult)
{
  return nsFormControlHelper::GetValueAttr(mContent, aResult);
}

void
nsHTMLButtonControlFrame::ReParentFrameList(nsFrameManager* aFrameManager,
                                            nsIFrame* aFrameList)
{
  // get the new parent context from the first child: that is the
  // frame that the subsequent children will be made children of
  nsStyleContext* newParentContext = mFrames.FirstChild()->GetStyleContext();

  // Set the parent for each of the child frames
  for (nsIFrame* frame = aFrameList; frame; frame = frame->GetNextSibling()) {
    frame->SetParent(mFrames.FirstChild());
    // now reparent the contexts for the reparented frame too
    aFrameManager->ReParentStyleContext(frame, newParentContext);
  }
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
nsHTMLButtonControlFrame::MouseClicked(nsPresContext* aPresContext) 
{
// This is no longer called; click events are handled in
// nsHTMLInputElement::HandleDOMEvent().
}

void 
nsHTMLButtonControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
}

void
nsHTMLButtonControlFrame::ScrollIntoView(nsPresContext* aPresContext)
{
  if (aPresContext) {
    nsIPresShell *presShell = aPresContext->GetPresShell();
    if (presShell) {
     presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);

    }
  }
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
nsHTMLButtonControlFrame::GetFrameForPoint(nsPresContext* aPresContext,
                                           const nsPoint& aPoint,
                                           nsFramePaintLayer aWhichLayer,
                                           nsIFrame** aFrame)
{
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND &&
      mRect.Contains(aPoint)) {
    if (GetStyleVisibility()->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::SetInitialChildList(nsPresContext* aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  // NOTE: the whole reparenting should not need to happen: see bugzilla bug 51767
  ReParentFrameList(aPresContext->FrameManager(), aChildList);
  
  return mFrames.FirstChild()->SetInitialChildList(aPresContext, nsnull, aChildList);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Paint(nsPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer,
                                PRUint32             aFlags)
{
  PRBool isVisible;
  if (aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND ||
      (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible)) {
    return NS_OK;
  }

  nsRect rect(0, 0, mRect.width, mRect.height);
  mRenderer.PaintButton(aPresContext, aRenderingContext, aDirtyRect, rect);

#if 0 // old way
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_BACKGROUND);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FLOATS);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FOREGROUND);

#else // temporary
    // XXX This is temporary
  // clips to it's size minus the border 
  // but the real problem is the FirstChild (the AreaFrame)
  // isn't being constrained properly
  // Bug #17474
  const nsStyleBorder* borderStyle = GetStyleBorder();
  nsMargin border;
  border.SizeTo(0, 0, 0, 0);
  borderStyle->CalcBorderFor(this, border);

  rect.Deflate(border);
  aRenderingContext.PushState();

  aRenderingContext.SetClipRect(rect, nsClipCombine_kIntersect);

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_BACKGROUND);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FLOATS);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FOREGROUND);

  aRenderingContext.PopState();

#endif

  // to draw border when selected in editor
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

NS_IMETHODIMP 
nsHTMLButtonControlFrame::AddComputedBorderPaddingToDesiredSize(nsHTMLReflowMetrics& aDesiredSize,
                                                                const nsHTMLReflowState& aSuggestedReflowState)
{
  aDesiredSize.width  += aSuggestedReflowState.mComputedBorderPadding.left + aSuggestedReflowState.mComputedBorderPadding.right;
  aDesiredSize.height += aSuggestedReflowState.mComputedBorderPadding.top + aSuggestedReflowState.mComputedBorderPadding.bottom;
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLButtonControlFrame::Reflow(nsPresContext* aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLButtonControlFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  if (eReflowReason_Initial == aReflowState.reason) {
    nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
  }

#if 0
  nsresult skiprv = nsFormControlFrame::SkipResizeReflow(mCacheSize, mCachedMaxElementWidth, aPresContext, 
                                                         aDesiredSize, aReflowState, aStatus);
  if (NS_SUCCEEDED(skiprv)) {
    return skiprv;
  }
#endif

  // Reflow the child
  nsIFrame* firstKid = mFrames.FirstChild();
  nsSize availSize(aReflowState.mComputedWidth, NS_INTRINSICSIZE);

  // Indent the child inside us by the the focus border. We must do this separate from the
  // regular border.
  nsMargin focusPadding = mRenderer.GetAddedButtonBorderAndPadding();

  
  if (NS_INTRINSICSIZE != availSize.width) {
    availSize.width -= focusPadding.left + focusPadding.right;
    availSize.width = PR_MAX(availSize.width,0);
  }
  if (NS_AUTOHEIGHT != availSize.height) {
    availSize.height -= focusPadding.top + focusPadding.bottom;
    availSize.height = PR_MAX(availSize.height,0);
  }
  
  // XXX Proper handling of incremental reflow...
  nsReflowReason reason = aReflowState.reason;
  if (eReflowReason_Incremental == reason) {
    // See if it's targeted at us
    nsHTMLReflowCommand *command = aReflowState.path->mReflowCommand;
    if (command) {
      // I'm not sure what exactly this Invalidate is for
      Invalidate(nsRect(0,0,mRect.width,mRect.height), PR_FALSE);

      nsReflowType  reflowType;
      command->GetType(reflowType);
      if (eReflowType_StyleChanged == reflowType) {
        reason = eReflowReason_StyleChange;
      }
      else {
        reason = eReflowReason_Resize;
      }
    }
  }

  // Reflow the contents of the button a first time.
  ReflowButtonContents(aPresContext, aDesiredSize, aReflowState, firstKid,
                       availSize, reason, focusPadding, aStatus);

  // If we just performed the first pass of a shrink-wrap reflow (which will have
  // found the requested size of the children, but not placed them), perform
  // the second pass of a shrink-wrap reflow (which places the children
  // within this parent button which has now shrink-wrapped around them).
  if (availSize.width == NS_SHRINKWRAPWIDTH) {
    nsSize newAvailSize(aDesiredSize.width, NS_INTRINSICSIZE);

    ReflowButtonContents(aPresContext, aDesiredSize, aReflowState, firstKid,
                         newAvailSize, eReflowReason_Resize, focusPadding, aStatus);
  }

  // If computed use the computed values.
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE) 
    aDesiredSize.width = aReflowState.mComputedWidth;
  else 
    aDesiredSize.width  += focusPadding.left + focusPadding.right;

  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE) 
    aDesiredSize.height = aReflowState.mComputedHeight;
  else
    aDesiredSize.height += focusPadding.top + focusPadding.bottom;

  AddComputedBorderPaddingToDesiredSize(aDesiredSize, aReflowState);

  //aDesiredSize.width  += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
  //aDesiredSize.height += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;

  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.SetMEWToActualWidth(aReflowState.mStylePosition->mWidth.GetUnit());
  }

  // Make sure we obey min/max-width and min/max-height
  if (aDesiredSize.width > aReflowState.mComputedMaxWidth) {
    aDesiredSize.width = aReflowState.mComputedMaxWidth;
  }
  if (aDesiredSize.width < aReflowState.mComputedMinWidth) {
    aDesiredSize.width = aReflowState.mComputedMinWidth;
  } 

  if (aDesiredSize.height > aReflowState.mComputedMaxHeight) {
    aDesiredSize.height = aReflowState.mComputedMaxHeight;
  }
  if (aDesiredSize.height < aReflowState.mComputedMinHeight) {
    aDesiredSize.height = aReflowState.mComputedMinHeight;
  }

  aDesiredSize.ascent  += aReflowState.mComputedBorderPadding.top + focusPadding.top;
  aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;

  aStatus = NS_FRAME_COMPLETE;

  nsFormControlFrame::SetupCachedSizes(mCacheSize, mCachedAscent,
                                       mCachedMaxElementWidth, aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

void
nsHTMLButtonControlFrame::ReflowButtonContents(nsPresContext* aPresContext,
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               const nsHTMLReflowState& aReflowState,
                                               nsIFrame* aFirstKid,
                                               const nsSize& aAvailSize,
                                               nsReflowReason aReason,
                                               nsMargin aFocusPadding,
                                               nsReflowStatus& aStatus)
{
  nsHTMLReflowState reflowState(aPresContext, aReflowState, aFirstKid, aAvailSize, aReason);

  ReflowChild(aFirstKid, aPresContext, aDesiredSize, reflowState,
              aFocusPadding.left + aReflowState.mComputedBorderPadding.left,
              aFocusPadding.top + aReflowState.mComputedBorderPadding.top,
              0, aStatus);
  
  // calculate the min internal size so the contents gets centered correctly
  // minInternalWidth is not being used at all and causes a warning--commenting
  // out until someone wants it.
  //  nscoord minInternalWidth  = aReflowState.mComputedMinWidth  == 0?0:aReflowState.mComputedMinWidth - 
  //    (aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right);
  nscoord minInternalHeight = aReflowState.mComputedMinHeight == 0?0:aReflowState.mComputedMinHeight - 
    (aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom);

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

  // Adjust the ascent by our offset (since we moved the child's
  // baseline by that much).
  aDesiredSize.ascent += yoff;
  
  // Place the child.  If we have a non-intrinsic width, we want to
  // reduce the left padding as needed to try and fit the text in the
  // button
  nscoord xoffset = aFocusPadding.left + aReflowState.mComputedBorderPadding.left;
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE) {
    // First, how much did we "overflow"?  This is the width of our
    // kid plus our special focus stuff (which did not get accounted
    // for in calculating aReflowState.mComputedWidth minus the width
    // we're forced to be.
    nscoord extrawidth =
      aDesiredSize.width + aFocusPadding.left + aFocusPadding.right
      - aReflowState.mComputedWidth;
    if (extrawidth > 0) {
      // Split it evenly between right and left
      extrawidth /= 2;
      // But do not shoot out the left side of the button, please
      extrawidth = PR_MIN(extrawidth, aReflowState.mComputedPadding.left);
      xoffset -= extrawidth;
    }
  }
  
  // Place the child
  FinishReflowChild(aFirstKid, aPresContext, &reflowState, aDesiredSize,
                    xoffset,
                    yoff + aFocusPadding.top + aReflowState.mComputedBorderPadding.top, 0);
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

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetFormContent(nsIContent*& aContent) const
{
  aContent = GetContent();
  NS_IF_ADDREF(aContent);
  return NS_OK;
}

nscoord 
nsHTMLButtonControlFrame::GetVerticalInsidePadding(nsPresContext* aPresContext,
                                                   float aPixToTwip, 
                                                   nscoord aInnerHeight) const
{
   return 0;
}

nscoord 
nsHTMLButtonControlFrame::GetHorizontalInsidePadding(nsPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}

NS_IMETHODIMP nsHTMLButtonControlFrame::SetProperty(nsPresContext* aPresContext,
                                                    nsIAtom* aName, const nsAString& aValue)
{
  if (nsHTMLAtoms::value == aName) {
    nsCOMPtr<nsIHTMLContent> formControl(do_QueryInterface(mContent));
    if (formControl) {
      return formControl->SetAttr(kNameSpaceID_None, nsHTMLAtoms::value, aValue, PR_TRUE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonControlFrame::GetProperty(nsIAtom* aName, nsAString& aValue)
{
  if (nsHTMLAtoms::value == aName) {
    nsCOMPtr<nsIHTMLContent> formControl(do_QueryInterface(mContent));
    if (formControl) {
      formControl->GetAttr(kNameSpaceID_None, nsHTMLAtoms::value, aValue);
    }
  }
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


NS_IMETHODIMP nsHTMLButtonControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
//  mSuggestedWidth = aWidth;
//  mSuggestedHeight = aHeight;
  return NS_OK;
}



NS_IMETHODIMP 
nsHTMLButtonControlFrame::AppendFrames(nsPresContext* aPresContext,
                                       nsIPresShell&   aPresShell,
                                       nsIAtom*        aListName,
                                       nsIFrame*       aFrameList)
{
  ReParentFrameList(aPresContext->FrameManager(), aFrameList);
  return mFrames.FirstChild()->AppendFrames(aPresContext,
                                            aPresShell,
                                            aListName,
                                            aFrameList);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::InsertFrames(nsPresContext* aPresContext,
                                       nsIPresShell&   aPresShell,
                                       nsIAtom*        aListName,
                                       nsIFrame*       aPrevFrame,
                                       nsIFrame*       aFrameList)
{
  ReParentFrameList(aPresContext->FrameManager(), aFrameList);
  return mFrames.FirstChild()->InsertFrames(aPresContext,
                                            aPresShell,
                                            aListName,
                                            aPrevFrame,
                                            aFrameList);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::RemoveFrame(nsPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aOldFrame)
{
  return mFrames.FirstChild()->RemoveFrame(aPresContext,
                                           aPresShell,
                                           aListName,
                                           aOldFrame);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::ReplaceFrame(nsPresContext* aPresContext,
                                       nsIPresShell&   aPresShell,
                                       nsIAtom*        aListName,
                                       nsIFrame*       aOldFrame,
                                       nsIFrame*       aNewFrame)
{
  ReParentFrameList(aPresContext->FrameManager(), aNewFrame);
  return mFrames.FirstChild()->ReplaceFrame(aPresContext,
                                            aPresShell,
                                            aListName,
                                            aOldFrame,
                                            aNewFrame);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::OnContentReset()
{
  return NS_OK;
}
