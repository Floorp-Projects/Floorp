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
#include "nsLineBox.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsLineLayout.h"
#include "prprf.h"

nsLineBox::nsLineBox(nsIFrame* aFrame, PRInt32 aCount, PRUint16 flags)
{
  mFirstChild = aFrame;
  mChildCount = aCount;
  mState = LINE_IS_DIRTY | LINE_NEED_DID_REFLOW | flags;
  mFloaters = nsnull;
  mNext = nsnull;
  mBounds.SetRect(0,0,0,0);
  mCombinedArea.SetRect(0,0,0,0);
//XXX  mCarriedOutTopMargin = 0;
  mCarriedOutBottomMargin = 0;
  mBreakType = NS_STYLE_CLEAR_NONE;
  mMaxElementWidth = 0;
}

nsLineBox::~nsLineBox()
{
  if (nsnull != mFloaters) {
    delete mFloaters;
  }
}

static void
ListFloaters(FILE* out, PRInt32 aIndent, nsVoidArray* aFloaters)
{
  nsAutoString frameName;
  PRInt32 j, i, n = aFloaters->Count();
  for (i = 0; i < n; i++) {
    for (j = aIndent; --j >= 0; ) fputs("  ", out);
    nsPlaceholderFrame* ph = (nsPlaceholderFrame*) aFloaters->ElementAt(i);
    if (nsnull != ph) {
      fprintf(out, "placeholder@%p ", ph);
      nsIFrame* frame = ph->GetOutOfFlowFrame();
      if (nsnull != frame) {
        frame->GetFrameName(frameName);
        fputs(frameName, out);
      }
      fprintf(out, "\n");
    }
  }
}

char*
nsLineBox::StateToString(char* aBuf, PRInt32 aBufSize) const
{
  PR_snprintf(aBuf, aBufSize, "%s,%s[0x%x]",
              (mState & LINE_IS_DIRTY) ? "dirty" : "clean",
              (mState & LINE_IS_BLOCK) ? "block" : "inline",
              mState);
  return aBuf;
}

void
nsLineBox::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  char cbuf[100];
  fprintf(out, "line %p: count=%d state=%s ",
          this, ChildCount(), StateToString(cbuf, sizeof(cbuf)));
#if XXX
  if (0 != mCarriedOutTopMargin) {
    fprintf(out, "tm=%d ", mCarriedOutTopMargin);
  }
#endif
  if (0 != mCarriedOutBottomMargin) {
    fprintf(out, "bm=%d ", mCarriedOutBottomMargin);
  }
  fprintf(out, "{%d,%d,%d,%d} ca={%d,%d,%d,%d}",
          mBounds.x, mBounds.y, mBounds.width, mBounds.height,
          mCombinedArea.x, mCombinedArea.y,
          mCombinedArea.width, mCombinedArea.height);
  fprintf(out, " <\n");

  nsIFrame* frame = mFirstChild;
  PRInt32 n = ChildCount();
  while (--n >= 0) {
    frame->List(out, aIndent + 1);
    frame->GetNextSibling(&frame);
  }

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  if (nsnull != mFloaters) {
    fputs("> floaters <\n", out);
    ListFloaters(out, aIndent + 1, mFloaters);
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
  }
  fputs(">\n", out);
}

nsIFrame*
nsLineBox::LastChild() const
{
  nsIFrame* frame = mFirstChild;
  PRInt32 n = ChildCount() - 1;
  while (--n >= 0) {
    frame->GetNextSibling(&frame);
  }
  return frame;
}

PRBool
nsLineBox::IsLastChild(nsIFrame* aFrame) const
{
  nsIFrame* lastFrame = LastChild();
  return aFrame == lastFrame;
}

PRInt32
nsLineBox::IndexOf(nsIFrame* aFrame) const
{
  PRInt32 i, n = ChildCount();
  nsIFrame* frame = mFirstChild;
  for (i = 0; i < n; i++) {
    if (frame == aFrame) {
      return i;
    }
    frame->GetNextSibling(&frame);
  }
  return -1;
}

void
nsLineBox::DeleteLineList(nsIPresContext& aPresContext, nsLineBox* aLine)
{
  if (nsnull != aLine) {
    // Delete our child frames before doing anything else. In particular
    // we do all of this before our base class releases it's hold on the
    // view.
    for (nsIFrame* child = aLine->mFirstChild; child; ) {
      nsIFrame* nextChild;
      child->GetNextSibling(&nextChild);
      child->Destroy(aPresContext);
      child = nextChild;
    }

    while (nsnull != aLine) {
      nsLineBox* next = aLine->mNext;
      delete aLine;
      aLine = next;
    }
  }
}

nsLineBox*
nsLineBox::LastLine(nsLineBox* aLine)
{
  if (nsnull != aLine) {
    while (nsnull != aLine->mNext) {
      aLine = aLine->mNext;
    }
  }
  return aLine;
}

nsLineBox*
nsLineBox::FindLineContaining(nsLineBox* aLine, nsIFrame* aFrame,
                              PRInt32* aFrameIndexInLine)
{
  NS_PRECONDITION(aFrameIndexInLine && aLine && aFrame, "null ptr");
  while (nsnull != aLine) {
    PRInt32 ix = aLine->IndexOf(aFrame);
    if (ix >= 0) {
      *aFrameIndexInLine = ix;
      return aLine;
    }
    aLine = aLine->mNext;
  }
  *aFrameIndexInLine = -1;
  return nsnull;
}

#ifdef NS_DEBUG
PRBool
nsLineBox::CheckIsBlock() const
{
  PRBool isBlock = nsLineLayout::TreatFrameAsBlock(mFirstChild);
  return isBlock == IsBlock();
}
#endif

//----------------------------------------------------------------------

static NS_DEFINE_IID(kILineIteratorIID, NS_ILINE_ITERATOR_IID);

static nsLineBox* gDummyLines[1];

nsLineIterator::nsLineIterator()
{
  NS_INIT_REFCNT();
  mLines = gDummyLines;
  mNumLines = 0;
  mIndex = 0;
  mRightToLeft = PR_FALSE;
}

nsLineIterator::~nsLineIterator()
{
  if (mLines != gDummyLines) {
    delete [] mLines;
  }
}

NS_IMPL_ISUPPORTS(nsLineIterator, kILineIteratorIID)

nsresult
nsLineIterator::Init(nsLineBox* aLines, PRBool aRightToLeft)
{
  mRightToLeft = aRightToLeft;

  // Count the lines
  PRInt32 numLines = 0;
  nsLineBox* line = aLines;
  while (line) {
    numLines++;
    line = line->mNext;
  }
  if (0 == numLines) {
    // Use gDummyLines so that we don't need null pointer checks in
    // the accessor methods
    mLines = gDummyLines;
    return NS_OK;
  }

  // Make a linear array of the lines
  mLines = new nsLineBox*[numLines];
  if (!mLines) {
    // Use gDummyLines so that we don't need null pointer checks in
    // the accessor methods
    mLines = gDummyLines;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsLineBox** lp = mLines;
  line = aLines;
  while (line) {
    *lp++ = line;
    line = line->mNext;
  }
  mNumLines = numLines;
  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::GetNumLines(PRInt32* aResult)
{
  NS_PRECONDITION(aResult, "null OUT ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mNumLines;
  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::GetDirection(PRBool* aIsRightToLeft)
{
  NS_PRECONDITION(aIsRightToLeft, "null OUT ptr");
  if (!aIsRightToLeft) {
    return NS_ERROR_NULL_POINTER;
  }
  *aIsRightToLeft = mRightToLeft;
  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::GetLine(PRInt32 aLineNumber,
                        nsIFrame** aFirstFrameOnLine,
                        PRInt32* aNumFramesOnLine,
                        nsRect& aLineBounds)
{
  NS_PRECONDITION(aFirstFrameOnLine && aNumFramesOnLine, "null OUT ptr");
  if (!aFirstFrameOnLine || !aNumFramesOnLine) {
    return NS_ERROR_NULL_POINTER;
  }
  if ((aLineNumber < 0) || (aLineNumber >= mNumLines)) {
    *aFirstFrameOnLine = nsnull;
    *aNumFramesOnLine = 0;
    aLineBounds.SetRect(0, 0, 0, 0);
    return NS_OK;
  }
  nsLineBox* line = mLines[aLineNumber];
  *aFirstFrameOnLine = line->mFirstChild;
  *aNumFramesOnLine = line->mChildCount;
  aLineBounds = line->mBounds;
  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::FindLineContaining(nsIFrame* aFrame,
                                   PRInt32* aLineNumberResult)
{
  nsLineBox* line = mLines[0];
  PRInt32 lineNumber = 0;
  while (line) {
    if (line->Contains(aFrame)) {
      *aLineNumberResult = lineNumber;
      return NS_OK;
    }
    line = line->mNext;
    lineNumber++;
  }
  *aLineNumberResult = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::FindLineAt(nscoord aY,
                           PRInt32* aLineNumberResult)
{
  nsLineBox* line = mLines[0];
  if (!line || (aY < line->mBounds.y)) {
    *aLineNumberResult = -1;
    return NS_OK;
  }
  PRInt32 lineNumber = 0;
  while (line) {
    if ((aY >= line->mBounds.y) && (aY < line->mBounds.YMost())) {
      *aLineNumberResult = lineNumber;
      return NS_OK;
    }
    line = line->mNext;
    lineNumber++;
  }
  *aLineNumberResult = mNumLines;
  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::FindFrameAt(PRInt32 aLineNumber,
                            nscoord aX,
                            nsIFrame** aFrameFound,
                            PRBool* aXIsBeforeFirstFrame,
                            PRBool* aXIsAfterLastFrame)
{
  NS_PRECONDITION(aFrameFound && aXIsBeforeFirstFrame && aXIsAfterLastFrame,
                  "null OUT ptr");
  if (!aFrameFound || !aXIsBeforeFirstFrame || !aXIsAfterLastFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  if ((aLineNumber < 0) || (aLineNumber >= mNumLines)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsLineBox* line = mLines[aLineNumber];
  if (!line) {
    *aFrameFound = nsnull;
    *aXIsBeforeFirstFrame = PR_TRUE;
    *aXIsAfterLastFrame = PR_FALSE;
    return NS_OK;
  }

  if (aX < line->mBounds.x) {
    nsIFrame* frame;
    if (mRightToLeft) {
      frame = line->LastChild();
    }
    else {
      frame = line->mFirstChild;
    }
    *aFrameFound = frame;
    *aXIsBeforeFirstFrame = PR_TRUE;
    *aXIsAfterLastFrame = PR_FALSE;
    return NS_OK;
  }
  else if (aX >= line->mBounds.XMost()) {
    nsIFrame* frame;
    if (mRightToLeft) {
      frame = line->mFirstChild;
    }
    else {
      frame = line->LastChild();
    }
    *aFrameFound = frame;
    *aXIsBeforeFirstFrame = PR_FALSE;
    *aXIsAfterLastFrame = PR_TRUE;
    return NS_OK;
  }

  // Find the frame closest to the X coordinate. Gaps can occur
  // between frames (because of margins) so we split the gap in two
  // when checking.
  *aXIsBeforeFirstFrame = PR_FALSE;
  *aXIsAfterLastFrame = PR_FALSE;
  nsRect r1, r2;
  nsIFrame* frame = line->mFirstChild;
  PRInt32 n = line->mChildCount;
  if (mRightToLeft) {
    while (--n >= 0) {
      nsIFrame* nextFrame;
      frame->GetNextSibling(&nextFrame);
      frame->GetRect(r1);
      if (aX > r1.x) {
        break;
      }
      if (nextFrame) {
        nextFrame->GetRect(r2);
        if (aX > r2.XMost()) {
          nscoord rightEdge = r2.XMost();
          nscoord delta = r1.x - rightEdge;
          if (aX < rightEdge + delta/2) {
            frame = nextFrame;
          }
          break;
        }
      }
      else {
        *aXIsBeforeFirstFrame = PR_TRUE;
      }
      frame = nextFrame;
    }
  }
  else {
    while (--n >= 0) {
      nsIFrame* nextFrame;
      frame->GetNextSibling(&nextFrame);
      frame->GetRect(r1);
      if (aX < r1.XMost()) {
        break;
      }
      if (nextFrame) {
        nextFrame->GetRect(r2);
        if (aX < r2.x) {
          nscoord rightEdge = r1.XMost();
          nscoord delta = r2.x - rightEdge;
          if (aX >= rightEdge + delta/2) {
            frame = nextFrame;
          }
          break;
        }
      }
      else {
        *aXIsAfterLastFrame = PR_TRUE;
      }
      frame = nextFrame;
    }
  }

  *aFrameFound = frame;
  return NS_OK;
}
