/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"
#include "nsLayoutAtoms.h"

//FOR SELECTION
#include "nsIContent.h"
#include "nsIFrameSelection.h"
//END INCLUDES FOR SELECTION

class BRFrame : public nsFrame {
public:
  // nsIFrame
#ifdef NS_DEBUG
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
#endif
  NS_IMETHOD GetContentAndOffsetsFromPoint(nsIPresContext* aCX,
                         const nsPoint& aPoint,
                         nsIContent** aNewContent,
                         PRInt32& aContentOffset,
                         PRInt32& aContentOffsetEnd,
                         PRBool&  aBeginFrameContent);
  NS_IMETHOD PeekOffset(nsIPresContext* aPresContext, 
                         nsPeekOffsetStruct *aPos);

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
protected:
  virtual ~BRFrame();
};

nsresult
NS_NewBRFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* frame = new (aPresShell) BRFrame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = frame;
  return NS_OK;
}

BRFrame::~BRFrame()
{
}

#ifdef NS_DEBUG
NS_IMETHODIMP
BRFrame::Paint(nsIPresContext*      aPresContext,
               nsIRenderingContext& aRenderingContext,
               const nsRect&        aDirtyRect,
               nsFramePaintLayer    aWhichLayer,
               PRUint32             aFlags)
{
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    float p2t;
    aPresContext->GetPixelsToTwips(&p2t);
    nscoord five = NSIntPixelsToTwips(5, p2t);
    aRenderingContext.SetColor(NS_RGB(0, 255, 255));
    aRenderingContext.FillRect(0, 0, five, five*2);
  }
  return NS_OK;
}
#endif

NS_IMETHODIMP
BRFrame::Reflow(nsIPresContext* aPresContext,
                nsHTMLReflowMetrics& aMetrics,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("BRFrame", aReflowState.reason);
  if (aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = 0;
    aMetrics.maxElementSize->height = 0;
  }
  aMetrics.height = 0; // BR frames with height 0 are ignored in quirks
                       // mode by nsLineLayout::VerticalAlignFrames .
                       // However, it's not always 0.  See below.
  aMetrics.width = 0;
  aMetrics.ascent = 0;
  aMetrics.descent = 0;

  // Only when the BR is operating in a line-layout situation will it
  // behave like a BR.
  nsLineLayout* ll = aReflowState.mLineLayout;
  if (ll) {
    if ( ll->CanPlaceFloaterNow() || ll->InStrictMode() ) {
      // If we can place a floater on the line now it means that the
      // line is effectively empty (there may be zero sized compressed
      // white-space frames on the line, but they are to be ignored).
      //
      // If this frame is going to terminate the line we know
      // that nothing else will go on the line. Therefore, in this
      // case, we provide some height for the BR frame so that it
      // creates some vertical whitespace.  It's necessary to use the
      // line-height rather than the font size because the
      // quirks-mode fix that doesn't apply the block's min
      // line-height makes this necessary to make BR cause a line
      // of the full line-height

      // We also do this in strict mode because BR should act like a
      // normal inline frame.  That line-height is used is important
      // here for cases where the line-height is less that 1.
      const nsStyleFont* font = (const nsStyleFont*)
        mStyleContext->GetStyleData(eStyleStruct_Font);
      aReflowState.rendContext->SetFont(font->mFont);
      nsCOMPtr<nsIFontMetrics> fm;
      aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));
      if (fm) {
        nscoord ascent, descent;
        fm->GetMaxAscent(ascent);
        fm->GetMaxDescent(descent);
        nscoord logicalHeight =
          aReflowState.CalcLineHeight(aPresContext,
                                       aReflowState.rendContext,
                                       this);
        nscoord leading = logicalHeight - ascent - descent;
        aMetrics.height = logicalHeight;
        aMetrics.ascent = ascent + (leading/2);
        aMetrics.descent = logicalHeight - aMetrics.ascent;
                      // = descent + (leading/2), but without rounding error
      }
      else {
        aMetrics.ascent = aMetrics.height = 0;
      }

      // XXX temporary until I figure out a better solution; see the
      // code in nsLineLayout::VerticalAlignFrames that zaps minY/maxY
      // if the width is zero.
      // XXX This also fixes bug 10036!
      aMetrics.width = 1;

      // Update max-element-size to keep us honest
      if (aMetrics.maxElementSize) {
        if (aMetrics.width > aMetrics.maxElementSize->width) {
          aMetrics.maxElementSize->width = aMetrics.width;
        }
        if (aMetrics.height > aMetrics.maxElementSize->height) {
          aMetrics.maxElementSize->height = aMetrics.height;
        }
      }
    }

    // Return our reflow status
    PRUint32 breakType = aReflowState.mStyleDisplay->mBreakType;
    if (NS_STYLE_CLEAR_NONE == breakType) {
      breakType = NS_STYLE_CLEAR_LINE;
    }

    aStatus = NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |
      NS_INLINE_MAKE_BREAK_TYPE(breakType);
    ll->SetLineEndsInBR(PR_TRUE);
  }
  else {
    aStatus = NS_FRAME_COMPLETE;
  }
  return NS_OK;
}

NS_IMETHODIMP
BRFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::brFrame;
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP BRFrame::GetContentAndOffsetsFromPoint(nsIPresContext* aCX,
                                                     const nsPoint&  aPoint,
                                                     nsIContent **   aContent,
                                                     PRInt32&        aOffsetBegin,
                                                     PRInt32&        aOffsetEnd,
                                                     PRBool&         aBeginFrameContent)
{
  if (!mContent)
    return NS_ERROR_NULL_POINTER;
  nsresult result = mContent->GetParent(*aContent);
  if (NS_SUCCEEDED(result) && *aContent)
    result = (*aContent)->IndexOf(mContent, aOffsetBegin);
  aOffsetEnd = aOffsetBegin;
  aBeginFrameContent = PR_TRUE;
  return result;
}

NS_IMETHODIMP BRFrame::PeekOffset(nsIPresContext* aPresContext, nsPeekOffsetStruct *aPos)
{
  if (!aPos)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContent> parentContent;
  PRInt32 offsetBegin; //offset of this content in its parents child list. base 0

  nsresult result = mContent->GetParent(*getter_AddRefs(parentContent));


  if (NS_SUCCEEDED(result) && parentContent)
    result = parentContent->IndexOf(mContent, offsetBegin);

  if (aPos->mAmount != eSelectLine && aPos->mAmount != eSelectBeginLine 
      && aPos->mAmount != eSelectEndLine) //then we must do the adjustment to make sure we leave this frame
  {
    if (aPos->mDirection == eDirNext)
      aPos->mStartOffset = offsetBegin +1;//go to end to make sure we jump to next node.
    else
      aPos->mStartOffset = offsetBegin; //we start at beginning to make sure we leave this frame.
  }
  return nsFrame::PeekOffset(aPresContext, aPos);//now we let the default take over.
}
