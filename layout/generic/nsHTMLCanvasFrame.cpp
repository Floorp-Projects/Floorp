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

/* rendering object for the HTML <canvas> element */

#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"

#include "nsHTMLCanvasFrame.h"
#include "nsICanvasElement.h"
#include "nsDisplayList.h"

#include "nsTransform2D.h"

nsIFrame*
NS_NewHTMLCanvasFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsHTMLCanvasFrame(aContext);
}

nsHTMLCanvasFrame::~nsHTMLCanvasFrame()
{
}

nsSize
nsHTMLCanvasFrame::GetCanvasSize()
{
  PRUint32 w, h;
  nsresult rv;
  nsCOMPtr<nsICanvasElement> canvas(do_QueryInterface(GetContent()));
  if (canvas) {
    rv = canvas->GetSize(&w, &h);
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  if (NS_FAILED(rv)) {
    NS_NOTREACHED("couldn't get canvas size");
    h = w = 1;
  }

  return nsSize(w, h);
}

/* virtual */ nscoord
nsHTMLCanvasFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  nscoord result = nsPresContext::CSSPixelsToAppUnits(GetCanvasSize().width);
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsHTMLCanvasFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  nscoord result = nsPresContext::CSSPixelsToAppUnits(GetCanvasSize().width);
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

/* virtual */ nsSize
nsHTMLCanvasFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
                               nsSize aCBSize, nscoord aAvailableWidth,
                               nsSize aMargin, nsSize aBorder, nsSize aPadding,
                               PRBool aShrinkWrap)
{
  nsSize size = GetCanvasSize();
  nsSize canvasSize(nsPresContext::CSSPixelsToAppUnits(size.width),
                    nsPresContext::CSSPixelsToAppUnits(size.height));

  return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                            aRenderingContext, this, canvasSize,
                            aCBSize, aBorder, aPadding);
}

NS_IMETHODIMP
nsHTMLCanvasFrame::Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aMetrics,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLCanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsHTMLCanvasFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  aMetrics.width = aReflowState.ComputedWidth();
  aMetrics.height = aReflowState.mComputedHeight;

  // stash this away so we can compute our inner area later
  mBorderPadding   = aReflowState.mComputedBorderPadding;

  aMetrics.width += mBorderPadding.left + mBorderPadding.right;
  aMetrics.height += mBorderPadding.top + mBorderPadding.bottom;

  if (GetPrevInFlow()) {
    nscoord y = GetContinuationOffset(&aMetrics.width);
    aMetrics.height -= y + mBorderPadding.top;
    aMetrics.height = PR_MAX(0, aMetrics.height);
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

void
nsHTMLCanvasFrame::PaintCanvas(nsIRenderingContext& aRenderingContext,
                               const nsRect& aDirtyRect, nsPoint aPt) 
{
  nsRect inner = GetInnerArea() + aPt;

  nsCOMPtr<nsICanvasElement> canvas(do_QueryInterface(GetContent()));
  if (!canvas)
    return;

  nsSize canvasSize = GetCanvasSize();
  nsSize sizeAppUnits(GetPresContext()->DevPixelsToAppUnits(canvasSize.width),
                      GetPresContext()->DevPixelsToAppUnits(canvasSize.height));

  // XXXvlad clip to aDirtyRect!

  if (inner.Size() != sizeAppUnits)
  {
    float sx = inner.width / (float) sizeAppUnits.width;
    float sy = inner.height / (float) sizeAppUnits.height;

    aRenderingContext.PushState();
    aRenderingContext.Translate(inner.x, inner.y);
    aRenderingContext.Scale(sx, sy);

    canvas->RenderContexts(&aRenderingContext);

    aRenderingContext.PopState();
  } else {
    //nsIRenderingContext::AutoPushTranslation(&aRenderingContext, px, py);

    aRenderingContext.PushState();
    aRenderingContext.Translate(inner.x, inner.y);

    canvas->RenderContexts(&aRenderingContext);

    aRenderingContext.PopState();
  }
}

static void PaintCanvas(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                        const nsRect& aDirtyRect, nsPoint aPt)
{
  NS_STATIC_CAST(nsHTMLCanvasFrame*, aFrame)->PaintCanvas(*aCtx, aDirtyRect, aPt);
}

NS_IMETHODIMP
nsHTMLCanvasFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                    const nsRect&           aDirtyRect,
                                    const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aLists.Content()->AppendNewToTop(new (aBuilder)
         nsDisplayGeneric(this, ::PaintCanvas, "Canvas"));
  NS_ENSURE_SUCCESS(rv, rv);

  return DisplaySelectionOverlay(aBuilder, aLists,
                                 nsISelectionDisplay::DISPLAY_IMAGES);
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
  return nsGkAtoms::HTMLCanvasFrame;
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

  if (GetPrevInFlow()) {
    for (nsIFrame* prevInFlow = GetPrevInFlow() ; prevInFlow; prevInFlow = prevInFlow->GetPrevInFlow()) {
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

