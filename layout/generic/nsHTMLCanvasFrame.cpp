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
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Vladimir Vukicevic <vladimir@pobox.com> (original author)
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

#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsLayoutAtoms.h"

#include "nsHTMLCanvasFrame.h"
#include "nsICanvasElement.h"

nsresult
NS_NewHTMLCanvasFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  nsHTMLCanvasFrame* it = new (aPresShell) nsHTMLCanvasFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsHTMLCanvasFrame::nsHTMLCanvasFrame()
{
}

nsHTMLCanvasFrame::~nsHTMLCanvasFrame()
{
}

// We really want a PR_MINMAX to go along with PR_MIN/PR_MAX
#define MINMAX(_value,_min,_max) \
    ((_value) < (_min)           \
     ? (_min)                    \
     : ((_value) > (_max)        \
        ? (_max)                 \
        : (_value)))

NS_IMETHODIMP
nsHTMLCanvasFrame::Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aMetrics,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLCanvasFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsHTMLCanvasFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  nsCOMPtr<nsICanvasElement> canvas(do_QueryInterface(GetContent()));
  NS_ENSURE_TRUE(canvas, NS_ERROR_FAILURE);

  nsresult rv = canvas->GetCanvasImageContainer(getter_AddRefs(mImageContainer));
  NS_ENSURE_SUCCESS(rv, rv);

  float p2t = GetPresContext()->PixelsToTwips();

  if (mImageContainer) {
    PRInt32 w, h;
    mImageContainer->GetWidth(&w);
    mImageContainer->GetHeight(&h);

    mCanvasSize.SizeTo(NSIntPixelsToTwips(w, p2t), NSIntPixelsToTwips(h, p2t));
  } else {
    mCanvasSize.SizeTo(0, 0);
  }

  if (aReflowState.mComputedWidth == NS_INTRINSICSIZE)
    aMetrics.width = mCanvasSize.width;
  else
    aMetrics.width = aReflowState.mComputedWidth;

  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE)
    aMetrics.height = mCanvasSize.height;
  else
    aMetrics.height = aReflowState.mComputedHeight;

  // clamp
  aMetrics.height = MINMAX(aMetrics.height, aReflowState.mComputedMinHeight, aReflowState.mComputedMaxHeight);
  aMetrics.width = MINMAX(aMetrics.width, aReflowState.mComputedMinWidth, aReflowState.mComputedMaxWidth);

  // stash this away so we can compute our inner area later
  mBorderPadding   = aReflowState.mComputedBorderPadding;

  aMetrics.width += mBorderPadding.left + mBorderPadding.right;
  aMetrics.height += mBorderPadding.top + mBorderPadding.bottom;

  if (mPrevInFlow) {
    nscoord y = GetContinuationOffset(&aMetrics.width);
    aMetrics.height -= y + mBorderPadding.top;
    aMetrics.height = PR_MAX(0, aMetrics.height);
  }

  aMetrics.ascent  = aMetrics.height;
  aMetrics.descent = 0;

  if (aMetrics.mComputeMEW) {
    aMetrics.SetMEWToActualWidth(aReflowState.mStylePosition->mWidth.GetUnit());
  }
  
  if (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aMetrics.mMaximumWidth = aMetrics.width;
  }
  aMetrics.mOverflowArea.SetRect(0, 0, aMetrics.width, aMetrics.height);
  FinishAndStoreOverflow(&aMetrics);

  if (mRect.width != aMetrics.width || mRect.height != aMetrics.height) {
    Invalidate(nsRect(0, 0, mRect.width, mRect.height), PR_FALSE);
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsHTMLCanvasFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return NS_OK;
}

// FIXME taken from nsImageFrame, but then had splittable frame stuff
// removed.  That needs to be fixed.
nsRect 
nsHTMLCanvasFrame::GetInnerArea() const
{
  nsRect r;
  r.x = mBorderPadding.left;
  r.y = mBorderPadding.top;
  r.width = mRect.width - mBorderPadding.left - mBorderPadding.right;
  r.height = mRect.height - mBorderPadding.top - mBorderPadding.bottom;
  return r;
}

NS_IMETHODIMP
nsHTMLCanvasFrame::Paint(nsPresContext*       aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect,
                         nsFramePaintLayer    aWhichLayer,
                         PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && 
      isVisible && mRect.width && mRect.height)
  {
    // If painting is suppressed, we need to stop image painting.
    PRBool paintingSuppressed = PR_FALSE;
    aPresContext->PresShell()->IsPaintingSuppressed(&paintingSuppressed);
    if (paintingSuppressed) {
      return NS_OK;
    }

    // make sure that the rendering context has updated the
    // image frame
    nsCOMPtr<nsICanvasElement> canvas(do_QueryInterface(GetContent()));
    NS_ENSURE_TRUE(canvas, NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(canvas->UpdateImageFrame(), NS_ERROR_FAILURE);

    // from nsImageFrame
    // First paint background and borders, which should be in the
    // FOREGROUND or BACKGROUND paint layer if the element is
    // inline-level or block-level, respectively (bug 36710).  (See
    // CSS2 9.5, which is the rationale for paint layers.)
    const nsStyleDisplay* display = GetStyleDisplay();
    nsFramePaintLayer backgroundLayer = display->IsBlockLevel()
      ? NS_FRAME_PAINT_LAYER_BACKGROUND
      : NS_FRAME_PAINT_LAYER_FOREGROUND;

    if (aWhichLayer == backgroundLayer) {
      PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
    }

    if ((aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) && mImageContainer) {
      nsRect inner = GetInnerArea();
      nsRect src(0, 0, mCanvasSize.width, mCanvasSize.height);

      aRenderingContext.DrawImage(mImageContainer, src, inner);
    }
  }

  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, nsISelectionDisplay::DISPLAY_IMAGES);
}

NS_IMETHODIMP
nsHTMLCanvasFrame::CanContinueTextRun(PRBool& aContinueTextRun) const
{
  // stolen from nsImageFrame.cpp
  // images really CAN continue text runs, but the textFrame needs to be 
  // educated before we can indicate that it can. For now, we handle the fixing up 
  // of max element widths in nsLineLayout::VerticalAlignFrames, but hopefully
  // this can be eliminated and the textFrame can be convinced to handle inlines
  // that take up space in text runs.

  aContinueTextRun = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP  
nsHTMLCanvasFrame::GetContentForEvent(nsPresContext* aPresContext,
                                      nsEvent* aEvent,
                                      nsIContent** aContent)
{
  NS_ENSURE_ARG_POINTER(aContent);
  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

nsIAtom*
nsHTMLCanvasFrame::GetType() const
{
  return nsLayoutAtoms::HTMLCanvasFrame;
}

// get the offset into the content area of the image where aImg starts if it is a continuation.
// from nsImageFrame
nscoord 
nsHTMLCanvasFrame::GetContinuationOffset(nscoord* aWidth) const
{
  nscoord offset = 0;
  if (aWidth) {
    *aWidth = 0;
  }

  if (mPrevInFlow) {
    for (nsIFrame* prevInFlow = mPrevInFlow ; prevInFlow; prevInFlow = prevInFlow->GetPrevInFlow()) {
      nsRect rect = prevInFlow->GetRect();
      if (aWidth) {
        *aWidth = rect.width;
      }
      offset += rect.height;
    }
    offset -= mBorderPadding.top;
    offset = PR_MAX(0, offset);
  }
  return offset;
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP
nsHTMLCanvasFrame::GetAccessible(nsIAccessible** aAccessible)
{
  return NS_ERROR_FAILURE;
}
#endif

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLCanvasFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLCanvas"), aResult);
}

NS_IMETHODIMP
nsHTMLCanvasFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
  fputs("\n", out);
  return NS_OK;
}
#endif

