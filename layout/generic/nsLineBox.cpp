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
      child->DeleteFrame(aPresContext);
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
