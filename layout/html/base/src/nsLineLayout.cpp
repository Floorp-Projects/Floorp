/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsLineLayout.h"
#include "nsInlineReflow.h"
#include "nsStyleConsts.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIFontMetrics.h"

nsTextRun::nsTextRun()
{
  mNext = nsnull;
}

nsTextRun::~nsTextRun()
{
}

void
nsTextRun::List(FILE* out, PRInt32 aIndent)
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  PRInt32 n = mArray.Count();
  fprintf(out, "%p: count=%d <", this, n);
  for (i = 0; i < n; i++) {
    nsIFrame* text = (nsIFrame*) mArray.ElementAt(i);
    nsAutoString tmp;
    text->GetFrameName(tmp);
    fputs(tmp, out);
    printf(" ");
  }
  fputs(">\n", out);
}

//----------------------------------------------------------------------

nsLineLayout::nsLineLayout(nsIPresContext& aPresContext,
                           nsISpaceManager* aSpaceManager)
  : mPresContext(aPresContext)
{
  mSpaceManager = aSpaceManager;
  mListPositionOutside = PR_FALSE;
  mLineNumber = 0;
  mColumn = 0;
  mUnderstandsWhiteSpace = PR_FALSE;
  mEndsInWhiteSpace = PR_TRUE;

  mTextRuns = nsnull;
  ResetTextRuns();
}

nsLineLayout::~nsLineLayout()
{
  nsTextRun::DeleteTextRuns(mTextRuns);
}

void
nsLineLayout::ResetTextRuns()
{
  nsTextRun::DeleteTextRuns(mTextRuns);
  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;
}

nsTextRun*
nsLineLayout::TakeTextRuns()
{
  nsTextRun* result = mTextRuns;
  mTextRuns = nsnull;
  ResetTextRuns();
  return result;
}

void
nsLineLayout::EndTextRun()
{
  mNewTextRun = nsnull;
}

nsresult
nsLineLayout::AddText(nsIFrame* aTextFrame)
{
  if (nsnull == mNewTextRun) {
    mNewTextRun = new nsTextRun();
    if (nsnull == mNewTextRun) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    *mTextRunP = mNewTextRun;
    mTextRunP = &mNewTextRun->mNext;
  }
  mNewTextRun->mArray.AppendElement(aTextFrame);
  return NS_OK;/* XXX */
}

nsTextRun*
nsLineLayout::FindTextRunFor(nsIFrame* aFrame)
{
  // Only the first-in-flows are present in the text run list so
  // backup from the argument frame to its first-in-flow.
  for (;;) {
    nsIFrame* prevInFlow;
    aFrame->GetPrevInFlow(prevInFlow);
    if (nsnull == prevInFlow) {
      break;
    }
    aFrame = prevInFlow;
  }

  // Now look for the frame in each run
  nsTextRun* run = mReflowTextRuns;
  while (nsnull != run) {
    PRInt32 ix = run->mArray.IndexOf(aFrame);
    if (ix >= 0) {
      return run;
    }
    run = run->mNext;
  }
  return nsnull;
}

nsIFrame*
nsLineLayout::FindNextText(nsIFrame* aFrame)
{
  // Only the first-in-flows are present in the text run list so
  // backup from the argument frame to its first-in-flow.
  for (;;) {
    nsIFrame* prevInFlow;
    aFrame->GetPrevInFlow(prevInFlow);
    if (nsnull == prevInFlow) {
      break;
    }
    aFrame = prevInFlow;
  }

  // Now look for the frame that follows aFrame's first-in-flow
  nsTextRun* run = mReflowTextRuns;
  while (nsnull != run) {
    PRInt32 ix = run->mArray.IndexOf(aFrame);
    if (ix >= 0) {
      if (ix < run->mArray.Count() - 1) {
        return (nsIFrame*) run->mArray[ix + 1];
      }
    }
    run = run->mNext;
  }
  return nsnull;
}

PRBool
nsLineLayout::IsNextWordFrame(nsIFrame* aFrame)
{
  if (0 != mWordFrames.Count()) {
    nsIFrame* next = (nsIFrame*) mWordFrames[0];
    return next == aFrame;
  }
  return PR_FALSE;
}

PRBool
nsLineLayout::IsLastWordFrame(nsIFrame* aFrame)
{
  PRInt32 n = mWordFrames.Count();
  if (0 != n) {
    nsIFrame* next = (nsIFrame*) mWordFrames[0];
    return (next == aFrame) && (1 == n);
  }
  return PR_FALSE;
}

void
nsLineLayout::ForgetWordFrame(nsIFrame* aFrame)
{
  NS_ASSERTION((void*)aFrame == mWordFrames[0], "forget-word-frame");
  if (0 != mWordFrames.Count()) {
    mWordFrames.RemoveElementAt(0);
  }
}

// XXX move this somewhere else!!!
PRBool
nsLineLayout::TreatFrameAsBlock(const nsStyleDisplay* aDisplay,
                                const nsStylePosition* aPosition)
{
  if (NS_STYLE_POSITION_ABSOLUTE == aPosition->mPosition) {
    return PR_FALSE;
  }
  if (NS_STYLE_FLOAT_NONE != aDisplay->mFloats) {
    return PR_FALSE;
  }
  switch (aDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
  case NS_STYLE_DISPLAY_RUN_IN:
  case NS_STYLE_DISPLAY_COMPACT:
  case NS_STYLE_DISPLAY_TABLE:
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsLineLayout::TreatFrameAsBlock(nsIFrame* aFrame)
{
  const nsStyleDisplay* display;
  const nsStylePosition* position;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
  aFrame->GetStyleData(eStyleStruct_Position,(const nsStyleStruct*&) position);
  return TreatFrameAsBlock(display, position);
}

void
nsLineLayout::UpdateInlines(nscoord aX, nscoord aY,
                            nscoord aWidth, nscoord aHeight,
                            PRBool aIsLeftFloater)
{
  PRInt32 i, n = mInlineStack.Count();
  for (i = 0; i < n; i++) {
    nsInlineReflow* ir = (nsInlineReflow*) mInlineStack[i];
    ir->UpdateBand(aX, aY, aWidth, aHeight, aIsLeftFloater);

    // After the first inline is updated the remainder are relative to
    // their parent therefore zap the x,y coordinates.

    // XXX border/padding adjustments need to be re-applied for inlines
    aX = 0;
    aY = 0;
  }
}
