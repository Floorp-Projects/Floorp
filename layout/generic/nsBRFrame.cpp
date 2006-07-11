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

/* rendering object for HTML <br> elements */

#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsHTMLParts.h"
#include "nsPresContext.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"
#include "nsLayoutAtoms.h"
#include "nsTextTransformer.h"

//FOR SELECTION
#include "nsIContent.h"
#include "nsFrameSelection.h"
//END INCLUDES FOR SELECTION

class BRFrame : public nsFrame {
public:
  friend nsIFrame* NS_NewBRFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual ContentOffsets CalcContentOffsetsFromFramePoint(nsPoint aPoint);

  NS_IMETHOD PeekOffset(nsPresContext* aPresContext, 
                         nsPeekOffsetStruct *aPos);

  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  virtual nsIAtom* GetType() const;
  
protected:
  BRFrame(nsStyleContext* aContext) : nsFrame(aContext) {}
  virtual ~BRFrame();
};

nsIFrame*
NS_NewBRFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) BRFrame(aContext);
}

BRFrame::~BRFrame()
{
}

NS_IMETHODIMP
BRFrame::Reflow(nsPresContext* aPresContext,
                nsHTMLReflowMetrics& aMetrics,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("BRFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  if (aMetrics.mComputeMEW) {
    aMetrics.mMaxElementWidth = 0;
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
    // Note that the compatibility mode check excludes AlmostStandards
    // mode, since this is the inline box model.  See bug 161691.
    if ( ll->CanPlaceFloatNow() ||
         ll->GetCompatMode() == eCompatibility_FullStandards ) {
      // If we can place a float on the line now it means that the
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
      SetFontFromStyle(aReflowState.rendContext, mStyleContext);
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
      // Warning: nsTextControlFrame::CalculateSizeStandard depends on
      // the following line, see bug 228752.
      aMetrics.width = 1;

      // Update max-element-width to keep us honest
      if (aMetrics.mComputeMEW && aMetrics.width > aMetrics.mMaxElementWidth) {
        aMetrics.mMaxElementWidth = aMetrics.width;
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

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return NS_OK;
}

nsIAtom*
BRFrame::GetType() const
{
  return nsLayoutAtoms::brFrame;
}

nsIFrame::ContentOffsets BRFrame::CalcContentOffsetsFromFramePoint(nsPoint aPoint)
{
  ContentOffsets offsets;
  offsets.content = mContent->GetParent();
  if (offsets.content) {
    offsets.offset = offsets.content->IndexOf(mContent);
    offsets.secondaryOffset = offsets.offset;
    offsets.associateWithNext = PR_TRUE;
  }
  return offsets;
}

NS_IMETHODIMP BRFrame::PeekOffset(nsPresContext* aPresContext, nsPeekOffsetStruct *aPos)
{
  if (!aPos)
    return NS_ERROR_NULL_POINTER;

  //BR is also a whitespace, but sometimes GetNextWord() can't handle this
  //See bug 304891
  nsTextTransformer::Initialize();

  if (aPos->mWordMovementType != eDefaultBehavior) {
    // aPos->mWordMovementType possible values:
    //       eEndWord: eat the space if we're moving backwards
    //       eStartWord: eat the space if we're moving forwards
    if ((aPos->mWordMovementType == eEndWord) == (aPos->mDirection == eDirPrevious)) {
      aPos->mEatingWS = PR_TRUE;
    }
  }
  else if (aPos->mDirection == eDirNext && nsTextTransformer::GetWordSelectEatSpaceAfter()) {
    // Use the hidden preference which is based on operating system behavior.
    // This pref only affects whether moving forward by word should go to the end of this word or start of the next word.
    // When going backwards, the start of the word is always used, on every operating system.
    aPos->mEatingWS = PR_TRUE;
  }

 //offset of this content in its parents child list. base 0
  PRInt32 offsetBegin = mContent->GetParent()->IndexOf(mContent);

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
