/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
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
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"
#include "nsColor.h"
#include "nsIDocument.h"
#include "nsButtonFrameRenderer.h"
#include "nsFormControlFrame.h"
#include "nsIFrameManager.h"
#include "nsINameSpaceManager.h"
#include "nsReflowPath.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLInputElement.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

#if 0
// see nsHTMLButtonControlFrame::Reflow()
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
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
  mPreviousCursor = eCursor_standard;
  mTranslatedRect = nsRect(0,0,0,0);
  mDidInit = PR_FALSE;

  mCacheSize.width             = -1;
  mCacheSize.height            = -1;
  mCachedMaxElementSize.width  = -1;
  mCachedMaxElementSize.height = -1;
}

nsHTMLButtonControlFrame::~nsHTMLButtonControlFrame()
{
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Destroy(nsIPresContext *aPresContext)
{
  nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  return nsHTMLContainerFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  mRenderer.SetFrame(this,aPresContext);
   // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  mInline = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay);

  PRUint32 flags = NS_BLOCK_SPACE_MGR;
  if (mInline) {
    flags |= NS_BLOCK_SHRINK_WRAP;
  }

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsIFrame* areaFrame;
  NS_NewAreaFrame(shell, &areaFrame, flags);
  mFrames.SetFrames(areaFrame);

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext;
  aPresContext->ResolvePseudoStyleContextFor(mContent,
                                            nsCSSAnonBoxes::buttonContent,
                                            mStyleContext,
                                            &styleContext);
  mFrames.FirstChild()->Init(aPresContext, mContent, this, styleContext, nsnull);
  NS_RELEASE(styleContext);                                           

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
    nsCOMPtr<nsIContent> content;
    GetContent(getter_AddRefs(content));
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
nsHTMLButtonControlFrame::GetType() const
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
nsHTMLButtonControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
// This is no longer called; click events are handled in
// nsHTMLInputElement::HandleDOMEvent().
}

void 
nsHTMLButtonControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
}

void
nsHTMLButtonControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
     presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);

    }
  }
}

void
nsHTMLButtonControlFrame::GetTranslatedRect(nsIPresContext* aPresContext, nsRect& aRect)
{
  nsIView* view;
  nsPoint viewOffset(0,0);
  GetOffsetFromView(aPresContext, viewOffset, &view);
  while (nsnull != view) {
    nsPoint tempOffset;
    view->GetPosition(&tempOffset.x, &tempOffset.y);
    viewOffset += tempOffset;
    view->GetParent(view);
  }
  aRect = nsRect(viewOffset.x, viewOffset.y, mRect.width, mRect.height);
}

            

NS_IMETHODIMP
nsHTMLButtonControlFrame::HandleEvent(nsIPresContext* aPresContext, 
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
nsHTMLButtonControlFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                           const nsPoint& aPoint,
                                           nsFramePaintLayer aWhichLayer,
                                           nsIFrame** aFrame)
{
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND &&
      mRect.Contains(aPoint)) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
      
    if (vis->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  // get the frame manager and the style context of the new parent frame
  // this is used whent he children are reparented below
  // NOTE: the whole reparenting should not need to happen: see bugzilla bug 51767
  //
  nsCOMPtr<nsIPresShell> shell;
  nsCOMPtr<nsIFrameManager> frameManager;
  nsCOMPtr<nsIStyleContext> newParentContext;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (shell) {
    shell->GetFrameManager(getter_AddRefs(frameManager));
  }
  // get the new parent context from the first child: that is the frame that the
  // subsequent children will be made children of
  mFrames.FirstChild()->GetStyleContext(getter_AddRefs(newParentContext));

  // Set the parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(&frame)) {
    frame->SetParent(mFrames.FirstChild());
    // now reparent the contexts for the reparented frame too
    if (frameManager) {
      frameManager->ReParentStyleContext(aPresContext,frame,newParentContext);
    }
  }

  // Queue up the frames for the inline frame
  return mFrames.FirstChild()->SetInitialChildList(aPresContext, nsnull, aChildList);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Paint(nsIPresContext*      aPresContext,
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
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FLOATERS);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FOREGROUND);

#else // temporary
    // XXX This is temporary
  // clips to it's size minus the border 
  // but the real problem is the FirstChild (the AreaFrame)
  // isn't being constrained properly
  // Bug #17474
  const nsStyleBorder* borderStyle;
  GetStyleData(eStyleStruct_Border,  (const nsStyleStruct *&)borderStyle);
  nsMargin border;
  border.SizeTo(0, 0, 0, 0);
  borderStyle->CalcBorderFor(this, border);

  rect.Deflate(border);
  aRenderingContext.PushState();
  PRBool clipEmpty;

  aRenderingContext.SetClipRect(rect, nsClipCombine_kIntersect, clipEmpty);

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_BACKGROUND);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FLOATERS);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FOREGROUND);

  aRenderingContext.PopState(clipEmpty);

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
nsHTMLButtonControlFrame::Reflow(nsIPresContext* aPresContext,
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
  nsresult skiprv = nsFormControlFrame::SkipResizeReflow(mCacheSize, mCachedMaxElementSize, aPresContext, 
                                                         aDesiredSize, aReflowState, aStatus);
  if (NS_SUCCEEDED(skiprv)) {
    return skiprv;
  }
#endif

  // commenting this out for now. We need a view to do mouse grabbing but
  // it doesn't really seem to work correctly. When you press the only event
  // you can get after that is a release. You need mouse enter and exit.
  // the view also breaks the outline code. For some reason you can not reset 
  // the clip rect to draw outside you bounds if you have a view. And you need to
  // because the outline must be drawn outside of our bounds according to CSS. -EDV

  // XXX If you do decide you need a view, then create it in the Init() function
  // and not here...
#if 0
  if (!mDidInit) {
    // create our view, we need a view to grab the mouse 
    nsIView* view;
    GetView(&view);
    if (!view) {
      nsresult result = nsComponentManager::CreateInstance(kViewCID, nsnull, NS_GET_IID(nsIView), (void **)&view);
	    nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
	    nsCOMPtr<nsIViewManager> viewMan;
      presShell->GetViewManager(getter_AddRefs(viewMan));

      nsIFrame* parWithView;
	    nsIView *parView;
      GetParentWithView(&parWithView);
	    parWithView->GetView(&parView);
      // the view's size is not know yet, but its size will be kept in synch with our frame.
      nsRect boundBox(0, 0, 500, 500); 
      result = view->Init(viewMan, boundBox, parView, nsnull);
      viewMan->InsertChild(parView, view, 0);
      SetView(view);

      const nsStyleColor* color = (const nsStyleColor*) mStyleContext->GetStyleData(eStyleStruct_Color);
      // set the opacity
      viewMan->SetViewOpacity(view, color->mOpacity);

    }
    mDidInit = PR_TRUE;
  }
#endif

  // reflow the child
  nsIFrame* firstKid = mFrames.FirstChild();
  nsSize availSize(aReflowState.mComputedWidth, NS_INTRINSICSIZE);

  // indent the child inside us by the the focus border. We must do this separate from the
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
      Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);

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

  nsHTMLReflowState reflowState(aPresContext, aReflowState, firstKid, availSize, reason);
  //reflowState.computedWidth = availSize;

  ReflowChild(firstKid, aPresContext, aDesiredSize, reflowState,
              focusPadding.left + aReflowState.mComputedBorderPadding.left,
              focusPadding.top + aReflowState.mComputedBorderPadding.top,
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

  // Place the child.  If we have a non-intrinsic width, we want to
  // reduce the left padding as needed to try and fit the text in the
  // button
  nscoord xoffset = focusPadding.left + aReflowState.mComputedBorderPadding.left;
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE) {
    // First, how much did we "overflow"?  This is the width of our
    // kid plus our special focus stuff (which did not get accounted
    // for in calculating aReflowState.mComputedWidth minus the width
    // we're forced to be.
    nscoord extrawidth =
      aDesiredSize.width + focusPadding.left + focusPadding.right
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
  FinishReflowChild(firstKid, aPresContext, &reflowState, aDesiredSize,
                    xoffset,
                    yoff + focusPadding.top + aReflowState.mComputedBorderPadding.top, 0);

  // if computed use the computed values.
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

#if 0
  //adjust our max element size, if necessary
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.AddBorderPaddingToMaxElementSize(aReflowState.mComputedBorderPadding);
  }
#endif

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
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
                                       mCachedMaxElementSize, aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::IsPercentageBase(PRBool& aBase) const
{
  aBase = PR_TRUE;
  return NS_OK;
}


PRIntn
nsHTMLButtonControlFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetFont(nsIPresContext* aPresContext, 
                                  const nsFont*&  aFont)
{
  return nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}

nscoord 
nsHTMLButtonControlFrame::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                                   float aPixToTwip, 
                                                   nscoord aInnerHeight) const
{
   return 0;
}

nscoord 
nsHTMLButtonControlFrame::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}

nsresult nsHTMLButtonControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP nsHTMLButtonControlFrame::SetProperty(nsIPresContext* aPresContext,
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

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetAdditionalStyleContext(PRInt32 aIndex, 
                                                    nsIStyleContext** aStyleContext) const
{
  return mRenderer.GetStyleContext(aIndex, aStyleContext);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                                    nsIStyleContext* aStyleContext)
{
  return mRenderer.SetStyleContext(aIndex, aStyleContext);
}


NS_IMETHODIMP nsHTMLButtonControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
//  mSuggestedWidth = aWidth;
//  mSuggestedHeight = aHeight;
  return NS_OK;
}



NS_IMETHODIMP 
nsHTMLButtonControlFrame::AppendFrames(nsIPresContext* aPresContext,
                                       nsIPresShell&   aPresShell,
                                       nsIAtom*        aListName,
                                       nsIFrame*       aFrameList)
{
  return mFrames.FirstChild()->AppendFrames(aPresContext,
                                     aPresShell,
                                     aListName,
                                     aFrameList);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::OnContentReset()
{
  return NS_OK;
}
