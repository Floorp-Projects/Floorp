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
#include "nsPageContentFrame.h"
#include "nsHTMLParts.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIPresShell.h"
#include "nsIDeviceContext.h"
#include "nsReadableUtils.h"
#include "nsSimplePageSequence.h"

#include "nsIView.h"

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
//#define DEBUG_PRINTING
#endif


nsresult
NS_NewPageContentFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");

  nsPageContentFrame* it = new (aPresShell) nsPageContentFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsPageContentFrame::nsPageContentFrame() :
  mClipRect(-1, -1, -1, -1)
{
}

NS_IMETHODIMP nsPageContentFrame::Reflow(nsPresContext*   aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPageContentFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  if (eReflowReason_Incremental != aReflowState.reason) {
    // Resize our frame allowing it only to be as big as we are
    // XXX Pay attention to the page's border and padding...
    if (mFrames.NotEmpty()) {
      nsIFrame* frame = mFrames.FirstChild();
      nsSize  maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame, maxSize);

      mPD->mPageContentSize  = aReflowState.availableWidth;

      // Reflow the page content area to get the child's desired size
      ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, 0, 0, 0, aStatus);

      // The document element's background should cover the entire canvas, so
      // take into account the combined area and any space taken up by
      // absolutely positioned elements
      nsMargin      border(0,0,0,0);
      nsMargin      padding(0,0,0,0);

      // Ignore the return values for these
      // Typically they are zero and if they fail 
      // we should keep going anyway, there impact is small
      kidReflowState.mStyleBorder->GetBorder(border);
      kidReflowState.mStylePadding->GetPadding(padding);

      // First check the combined area
      if (NS_FRAME_OUTSIDE_CHILDREN & frame->GetStateBits()) {
        // The background covers the content area and padding area, so check
        // for children sticking outside the child frame's padding edge
        if (aDesiredSize.mOverflowArea.XMost() > aDesiredSize.width) {
          mPD->mPageContentXMost =  aDesiredSize.mOverflowArea.XMost() + border.right + padding.right;
        }
      }

      // Place and size the child
      FinishReflowChild(frame, aPresContext, &kidReflowState, aDesiredSize, 0, 0, 0);

#ifdef NS_DEBUG
      // Is the frame complete?
      if (NS_FRAME_IS_COMPLETE(aStatus)) {
        nsIFrame* childNextInFlow;

        frame->GetNextInFlow(&childNextInFlow);
        NS_ASSERTION(nsnull == childNextInFlow, "bad child flow list");
      }
#endif

#ifdef DEBUG_PRINTING
      nsRect r = frame->GetRect();
      printf("PCF: Area Frame %p Bounds: %5d,%5d,%5d,%5d\n", frame, r.x, r.y, r.width, r.height);
      nsIView* view = frame->GetView();
      if (view) {
        r = view->GetBounds();
        printf("PCF: Area Frame View Bounds: %5d,%5d,%5d,%5d\n", r.x, r.y, r.width, r.height);
      } else {
        printf("PCF: Area Frame View Bounds: NO VIEW\n");
      }
#endif
    }
    // Reflow our fixed frames 
    mFixedContainer.Reflow(this, aPresContext, aReflowState, aReflowState.availableWidth, 
                           aReflowState.availableHeight);

    // Return our desired size
    aDesiredSize.width = aReflowState.availableWidth;
    if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
      aDesiredSize.height = aReflowState.availableHeight;
    }
  }

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

nsIAtom*
nsPageContentFrame::GetType() const
{
  return nsLayoutAtoms::pageContentFrame; 
}

#ifdef DEBUG
NS_IMETHODIMP
nsPageContentFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("PageContent"), aResult);
}
#endif

/* virtual */ PRBool
nsPageContentFrame::IsContainingBlock() const
{
  return PR_TRUE;
}


//------------------------------------------------------------------------------
NS_IMETHODIMP
nsPageContentFrame::Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags)
{
  aRenderingContext.PushState();

  nsRect rect;
  if (mClipRect.width != -1 || mClipRect.height != -1) {
    rect = mClipRect;
  } else {
    rect = mRect;
    rect.x = 0;
    rect.y = 0;
  }

  aRenderingContext.SetClipRect(rect, nsClipCombine_kReplace);

  nsresult rv = nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nsRect r = mRect;
    r.x = 0;
    r.y = 0;
    aRenderingContext.SetColor(NS_RGB(0, 0, 0));
    aRenderingContext.DrawRect(r);
  }
#endif

  aRenderingContext.PopState();

  return rv;
}



