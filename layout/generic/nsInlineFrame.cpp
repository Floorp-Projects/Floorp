/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsBlockFrame.h"
#include "nsLineBox.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"

#define nsInlineFrameSuper nsBaseIBFrame

class nsInlineFrame : public nsInlineFrameSuper
{
public:
  friend nsresult NS_NewInlineFrame(nsIFrame*& aNewFrame);

  // nsIFrame overrides
  NS_IMETHOD CreateContinuingFrame(nsIPresContext& aCX,
                                   nsIFrame* aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*& aContinuingFrame);
  NS_IMETHOD GetFrameName(nsString& aResult) const;

  // nsIHTMLReflow overrides
  NS_IMETHOD FindTextRuns(nsLineLayout& aLineLayout);
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace);
  NS_IMETHOD TrimTrailingWhiteSpace(nsIPresContext& aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth);

protected:
  nsInlineFrame();
  ~nsInlineFrame();

  virtual PRIntn GetSkipSides() const;

  struct AdjustData {
    nsIFrame* frame;
    PRBool splittable;
    nsRect bounds;
  };
};

//----------------------------------------------------------------------

nsresult
NS_NewInlineFrame(nsIFrame*& aNewFrame)
{
  nsInlineFrame* it = new nsInlineFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aNewFrame = it;
  return NS_OK;
}

nsInlineFrame::nsInlineFrame()
{
  mFlags = BLOCK_IS_INLINE;
}

nsInlineFrame::~nsInlineFrame()
{
}

NS_IMETHODIMP
nsInlineFrame::FindTextRuns(nsLineLayout& aLineLayout)
{
  nsresult rv = NS_OK;

  // Gather up children from the overflow lists
  DrainOverflowLines();

  nsLineBox* line = mLines;
  while (nsnull != line) {
    if (!line->IsBlock()) {
      // A frame that doesn't implement nsIHTMLReflow isn't text
      // therefore it will end an open text run.
      nsIFrame* frame = line->mFirstChild;
      PRInt32 n = line->ChildCount();
      while (--n >= 0) {
        nsIHTMLReflow* hr;
        if (NS_OK == frame->QueryInterface(kIHTMLReflowIID, (void**)&hr)) {
          rv = hr->FindTextRuns(aLineLayout);
          if (NS_OK != rv) {
            return rv;
          }
        }
        else {
          aLineLayout.EndTextRun();
        }
        frame->GetNextSibling(frame);
      }
    }
    else {
      // Block lines terminate the text-runs
      aLineLayout.EndTextRun();
    }
    line = line->mNext;
  }

  return rv;
}

NS_IMETHODIMP
nsInlineFrame::AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace)
{
  // XXX Refactor this
  if (0 >= aExtraSpace) {
    aUsedSpace = 0;
    return NS_OK;
  }

  const int NUM_AD = 50;
  AdjustData adjustMem[NUM_AD];
  AdjustData* ad0 = adjustMem;
  AdjustData* end = ad0 + NUM_AD;
  AdjustData* ad = ad0;
  PRInt32 numAD = NUM_AD;

  // Gather up raw data for justification
  nsIFrame* frame = mLines ? mLines->mFirstChild : nsnull;
  PRInt32 fixed = 0;
  PRInt32 total = 0;
  nscoord fixedWidth = 0;
  for (; nsnull != frame; ad++, total++) {
    // Grow temporary storage if we have to
    if (ad == end) {
      AdjustData* newAD = new AdjustData[numAD + numAD];
      if (nsnull == newAD) {
        if (ad0 != adjustMem) {
          delete [] ad0;
        }
        aUsedSpace = 0;
        return NS_OK;
      }
      nsCRT::memcpy(newAD, ad0, sizeof(AdjustData) * numAD);
      ad = newAD + (ad - ad0);
      if (ad0 != adjustMem) {
        delete [] ad0;
      }
      ad0 = newAD;
      end = ad0 + numAD;
      numAD = numAD + numAD;
    }

    // Record info about the frame
    ad->frame = frame;
    frame->GetRect(ad->bounds);
    nsSplittableType isSplittable = NS_FRAME_NOT_SPLITTABLE;
    frame->IsSplittable(isSplittable);
    if ((0 == ad->bounds.width) ||
        NS_FRAME_IS_NOT_SPLITTABLE(isSplittable)) {
      ad->splittable = PR_FALSE;
      fixed++;
      fixedWidth += ad->bounds.width;
    }
    else {
      ad->splittable = PR_TRUE;
    }

    // Advance to the next frame
    frame->GetNextSibling(frame);
  }

  nscoord totalUsed = 0;
  nscoord variableWidth = mRect.width - fixedWidth;
  if (variableWidth > 0) {
    // Each variable width frame gets a portion of the available extra
    // space that is proportional to the space it takes in the
    // line. The extra space is given to the frame by updating its
    // position and size. The frame is responsible for adjusting the
    // position of its contents on its own (during rendering).
    PRInt32 i, splittable = total - fixed;
    nscoord extraSpace = aExtraSpace;
    nscoord remainingExtra = extraSpace;
    nscoord dx = 0;
    float lineWidth = float(mRect.width);
    ad = ad0;
    for (i = 0; i < total; i++, ad++) {
      nsIFrame* frame = ad->frame;
      nsIHTMLReflow* ihr;
      if (NS_OK == frame->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
        nsRect r;
        if (ad->splittable && (ad->bounds.width > 0)) {
          float pctOfLine = float(ad->bounds.width) / lineWidth;
          nscoord extra = nscoord(pctOfLine * extraSpace);
          if (--splittable == 0) {
            extra = remainingExtra;
          }
          if (0 != extra) {
            nscoord used;
            ihr->AdjustFrameSize(extra, used);
            if (used < extra) {
//              frame->ListTag(); printf(": extra=%d used=%d\n", extra, used);
            }
            totalUsed += used;
            frame->GetRect(r);
            r.x += dx;
            frame->SetRect(r);
            dx += used;
            remainingExtra -= used;
          }
          else if (0 != dx) {
            frame->GetRect(r);
            r.x += dx;
            frame->SetRect(r);
          }
        }
        else if (0 != dx) {
          frame->GetRect(r);
          r.x += dx;
          frame->SetRect(r);
        }
      }
    }
  }
  mRect.width += totalUsed;
  aUsedSpace = totalUsed;

  if (ad0 != adjustMem) {
    delete [] ad0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::TrimTrailingWhiteSpace(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRC,
                                      nscoord& aDeltaWidth)
{
  aDeltaWidth = 0;
  nsLineBox* lastLine = nsLineBox::LastLine(mLines);
  if (nsnull != lastLine) {
    nsIFrame* lastFrame = lastLine->LastChild();
    if (nsnull != lastFrame) {
      nsIHTMLReflow* ihr;
      if (NS_OK == lastFrame->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
        ihr->TrimTrailingWhiteSpace(aPresContext, aRC, aDeltaWidth);
        if (0 != aDeltaWidth) {
          mRect.width -= aDeltaWidth;
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Inline", aResult);
}

NS_IMETHODIMP
nsInlineFrame::CreateContinuingFrame(nsIPresContext& aPresContext,
                                     nsIFrame* aParent,
                                     nsIStyleContext* aStyleContext,
                                     nsIFrame*& aContinuingFrame)
{
  nsInlineFrame* cf = new nsInlineFrame;
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  cf->Init(aPresContext, mContent, aParent, aStyleContext);
  cf->SetFlags(mFlags);
  cf->AppendToFlow(this);
  aContinuingFrame = cf;
  return NS_OK;
}

PRIntn
nsInlineFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_LEFT;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_RIGHT;
  }
  return skip;
}
