/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsLineBox.h"
#include "nsSpaceManager.h"
#include "nsLineLayout.h"
#include "prprf.h"
#include "nsBlockFrame.h"
#include "nsITextContent.h"
#include "nsLayoutAtoms.h"

#ifdef DEBUG
static PRInt32 ctorCount;
PRInt32 nsLineBox::GetCtorCount() { return ctorCount; }
#endif

MOZ_DECL_CTOR_COUNTER(nsLineBox)

nsLineBox::nsLineBox(nsIFrame* aFrame, PRInt32 aCount, PRBool aIsBlock)
  : mFirstChild(aFrame),
    mBounds(0, 0, 0, 0),
    mMaxElementWidth(0),
    mMaximumWidth(-1),
    mData(nsnull)
{
  MOZ_COUNT_CTOR(nsLineBox);
#ifdef DEBUG
  ++ctorCount;
#endif

  mAllFlags = 0;
#if NS_STYLE_CLEAR_NONE > 0
  mFlags.mBreakType = NS_STYLE_CLEAR_NONE;
#endif
  SetChildCount(aCount);
  MarkDirty();
  mFlags.mBlock = aIsBlock;
}

nsLineBox::~nsLineBox()
{
  MOZ_COUNT_DTOR(nsLineBox);
  Cleanup();
}

nsLineBox*
NS_NewLineBox(nsIPresShell* aPresShell, nsIFrame* aFrame,
              PRInt32 aCount, PRBool aIsBlock)
{
  return new (aPresShell)nsLineBox(aFrame, aCount, aIsBlock);
}

// Overloaded new operator. Uses an arena (which comes from the presShell)
// to perform the allocation.
void* 
nsLineBox::operator new(size_t sz, nsIPresShell* aPresShell) CPP_THROW_NEW
{
  return aPresShell->AllocateFrame(sz);
}

// Overloaded delete operator. Doesn't actually free the memory, because we
// use an arena
void 
nsLineBox::operator delete(void* aPtr, size_t sz)
{
}

void
nsLineBox::Destroy(nsIPresShell* aPresShell)
{
  // Destroy the object. This won't actually free the memory, though
  delete this;

  // Have the pres shell recycle the memory
  aPresShell->FreeFrame(sizeof(*this), (void*)this);
}

void
nsLineBox::Cleanup()
{
  if (mData) {
    if (IsBlock()) {
      delete mBlockData;
    }
    else {
      delete mInlineData;
    }
    mData = nsnull;
  }
}

#ifdef DEBUG
static void
ListFloats(FILE* out, PRInt32 aIndent, const nsFloatCacheList& aFloats)
{
  nsAutoString frameName;
  nsFloatCache* fc = aFloats.Head();
  while (fc) {
    nsFrame::IndentBy(out, aIndent);
    nsPlaceholderFrame* ph = fc->mPlaceholder;
    if (nsnull != ph) {
      fprintf(out, "placeholder@%p ", NS_STATIC_CAST(void*, ph));
      nsIFrame* frame = ph->GetOutOfFlowFrame();
      if (nsnull != frame) {
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->GetFrameName(frameName);
          fputs(NS_LossyConvertUCS2toASCII(frameName).get(), out);
        }
      }
      fprintf(out, " %s region={%d,%d,%d,%d} combinedArea={%d,%d,%d,%d}",
              fc->mIsCurrentLineFloat ? "cl" : "bcl",
              fc->mRegion.x, fc->mRegion.y,
              fc->mRegion.width, fc->mRegion.height,
              fc->mCombinedArea.x, fc->mCombinedArea.y,
              fc->mCombinedArea.width, fc->mCombinedArea.height);

      fprintf(out, "\n");
    }
    fc = fc->Next();
  }
}
#endif

#ifdef DEBUG
const char *
BreakTypeToString(PRUint8 aBreakType)
{
  switch (aBreakType) {
  case NS_STYLE_CLEAR_NONE: return "nobr";
  case NS_STYLE_CLEAR_LEFT: return "leftbr";
  case NS_STYLE_CLEAR_RIGHT: return "rightbr";
  case NS_STYLE_CLEAR_LEFT_AND_RIGHT: return "leftbr+rightbr";
  case NS_STYLE_CLEAR_LINE: return "linebr";
  case NS_STYLE_CLEAR_BLOCK: return "blockbr";
  case NS_STYLE_CLEAR_COLUMN: return "columnbr";
  case NS_STYLE_CLEAR_PAGE: return "pagebr";
  default:
    break;
  }
  return "unknown";
}

char*
nsLineBox::StateToString(char* aBuf, PRInt32 aBufSize) const
{
  PR_snprintf(aBuf, aBufSize, "%s,%s,%s,%s,%s,%s[0x%x]",
              IsBlock() ? "block" : "inline",
              IsDirty() ? "dirty" : "clean",
              IsPreviousMarginDirty() ? "prevmargindirty" : "prevmarginclean",
              IsImpactedByFloat() ? "impacted" : "not impacted",
              IsLineWrapped() ? "wrapped" : "not wrapped",
              BreakTypeToString(GetBreakType()),
              mAllFlags);
  return aBuf;
}

void
nsLineBox::List(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  char cbuf[100];
  fprintf(out, "line %p: count=%d state=%s ",
          NS_STATIC_CAST(const void*, this), GetChildCount(),
          StateToString(cbuf, sizeof(cbuf)));
  if (IsBlock() && !GetCarriedOutBottomMargin().IsZero()) {
    fprintf(out, "bm=%d ", GetCarriedOutBottomMargin().get());
  }
  if (0 != mMaxElementWidth) {
    fprintf(out, "mew=%d ", mMaxElementWidth);
  }
  fprintf(out, "{%d,%d,%d,%d} ",
          mBounds.x, mBounds.y, mBounds.width, mBounds.height);
  if (mData) {
    fprintf(out, "ca={%d,%d,%d,%d} ",
            mData->mCombinedArea.x, mData->mCombinedArea.y,
            mData->mCombinedArea.width, mData->mCombinedArea.height);
  }
  fprintf(out, "<\n");

  nsIFrame* frame = mFirstChild;
  PRInt32 n = GetChildCount();
  while (--n >= 0) {
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->List(aPresContext, out, aIndent + 1);
    }
    frame = frame->GetNextSibling();
  }

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  if (HasFloats()) {
    fputs("> floats <\n", out);
    ListFloats(out, aIndent + 1, mInlineData->mFloats);
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
  }
  fputs(">\n", out);
}
#endif

nsIFrame*
nsLineBox::LastChild() const
{
  nsIFrame* frame = mFirstChild;
  PRInt32 n = GetChildCount() - 1;
  while (--n >= 0) {
    frame = frame->GetNextSibling();
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
  PRInt32 i, n = GetChildCount();
  nsIFrame* frame = mFirstChild;
  for (i = 0; i < n; i++) {
    if (frame == aFrame) {
      return i;
    }
    frame = frame->GetNextSibling();
  }
  return -1;
}

PRBool
nsLineBox::IsEmpty() const
{
  if (IsBlock())
    return mFirstChild->IsEmpty();

  PRInt32 n;
  nsIFrame *kid;
  for (n = GetChildCount(), kid = mFirstChild;
       n > 0;
       --n, kid = kid->GetNextSibling())
  {
    if (!kid->IsEmpty())
      return PR_FALSE;
  }
  return PR_TRUE;
}

void
nsLineBox::DeleteLineList(nsPresContext* aPresContext, nsLineList& aLines)
{
  if (! aLines.empty()) {
    // Delete our child frames before doing anything else. In particular
    // we do all of this before our base class releases it's hold on the
    // view.
    for (nsIFrame* child = aLines.front()->mFirstChild; child; ) {
      nsIFrame* nextChild = child->GetNextSibling();
      child->Destroy(aPresContext);
      child = nextChild;
    }

    nsIPresShell *shell = aPresContext->PresShell();

    do {
      nsLineBox* line = aLines.front();
      aLines.pop_front();
      line->Destroy(shell);
    } while (! aLines.empty());
  }
}

nsLineBox*
nsLineBox::FindLineContaining(nsLineList& aLines, nsIFrame* aFrame,
                              PRInt32* aFrameIndexInLine)
{
  NS_PRECONDITION(aFrameIndexInLine && !aLines.empty() && aFrame, "null ptr");
  for (nsLineList::iterator line = aLines.begin(),
                            line_end = aLines.end();
       line != line_end;
       ++line)
  {
    PRInt32 ix = line->IndexOf(aFrame);
    if (ix >= 0) {
      *aFrameIndexInLine = ix;
      return line;
    }
  }
  *aFrameIndexInLine = -1;
  return nsnull;
}

PRBool
nsLineBox::RFindLineContaining(nsIFrame* aFrame,
                               const nsLineList::iterator& aBegin,
                               nsLineList::iterator& aEnd,
                               PRInt32* aFrameIndexInLine)
{
  NS_PRECONDITION(aFrame, "null ptr");
  while (aBegin != aEnd) {
    --aEnd;
    PRInt32 ix = aEnd->IndexOf(aFrame);
    if (ix >= 0) {
      *aFrameIndexInLine = ix;
      return PR_TRUE;
    }
  }
  *aFrameIndexInLine = -1;
  return PR_FALSE;
}

nsCollapsingMargin
nsLineBox::GetCarriedOutBottomMargin() const
{
  NS_ASSERTION(IsBlock(),
               "GetCarriedOutBottomMargin called on non-block line.");
  return (IsBlock() && mBlockData)
    ? mBlockData->mCarriedOutBottomMargin
    : nsCollapsingMargin();
}

void
nsLineBox::SetCarriedOutBottomMargin(nsCollapsingMargin aValue)
{
  if (IsBlock()) {
    if (! aValue.IsZero()) {
      if (!mBlockData) {
        mBlockData = new ExtraBlockData(mBounds);
      }
      if (mBlockData) {
        mBlockData->mCarriedOutBottomMargin = aValue;
      }
    }
    else if (mBlockData) {
      mBlockData->mCarriedOutBottomMargin = aValue;
      MaybeFreeData();
    }
  }
}

void
nsLineBox::MaybeFreeData()
{
  if (mData && (mData->mCombinedArea == mBounds)) {
    if (IsInline()) {
      if (mInlineData->mFloats.IsEmpty()) {
        delete mInlineData;
        mInlineData = nsnull;
      }
    }
    else if (mBlockData->mCarriedOutBottomMargin.IsZero()) {
      delete mBlockData;
      mBlockData = nsnull;
    }
  }
}

// XXX get rid of this???
nsFloatCache*
nsLineBox::GetFirstFloat()
{
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floats");
  return mInlineData ? mInlineData->mFloats.Head() : nsnull;
}

// XXX this might be too eager to free memory
void
nsLineBox::FreeFloats(nsFloatCacheFreeList& aFreeList)
{
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floats");
  if (IsInline()) {
    if (mInlineData) {
      aFreeList.Append(mInlineData->mFloats);
      MaybeFreeData();
    }
  }
}

void
nsLineBox::AppendFloats(nsFloatCacheFreeList& aFreeList)
{ 
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floats");
  if (IsInline()) {
    if (aFreeList.NotEmpty()) {
      if (!mInlineData) {
        mInlineData = new ExtraInlineData(mBounds);
      }
      if (mInlineData) {
        mInlineData->mFloats.Append(aFreeList);
      }
    }
  }
}

PRBool
nsLineBox::RemoveFloat(nsIFrame* aFrame)
{
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floats");
  if (IsInline() && mInlineData) {
    nsFloatCache* fc = mInlineData->mFloats.Find(aFrame);
    if (fc) {
      // Note: the placeholder is part of the line's child list
      // and will be removed later.
      fc->mPlaceholder->SetOutOfFlowFrame(nsnull);
      mInlineData->mFloats.Remove(fc);
      MaybeFreeData();
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void
nsLineBox::SetCombinedArea(const nsRect& aCombinedArea)
{  
  NS_ASSERTION(aCombinedArea.width >= 0, "illegal width for combined area");
  NS_ASSERTION(aCombinedArea.height >= 0, "illegal height for combined area");
  if (aCombinedArea != mBounds) {
    if (mData) {
      mData->mCombinedArea = aCombinedArea;
    }
    else {
      if (IsInline()) {
        mInlineData = new ExtraInlineData(aCombinedArea);
      }
      else {
        mBlockData = new ExtraBlockData(aCombinedArea);
      }
    }
  }
  else {
    if (mData) {
      // Store away new value so that MaybeFreeData compares against
      // the right value.
      mData->mCombinedArea = aCombinedArea;
    }
    MaybeFreeData();
  }
}

//----------------------------------------------------------------------


static nsLineBox* gDummyLines[1];

nsLineIterator::nsLineIterator()
{
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

NS_IMPL_ISUPPORTS2(nsLineIterator, nsILineIterator, nsILineIteratorNavigator)

nsresult
nsLineIterator::Init(nsLineList& aLines, PRBool aRightToLeft)
{
  mRightToLeft = aRightToLeft;

  // Count the lines
  PRInt32 numLines = aLines.size();
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
  for (nsLineList::iterator line = aLines.begin(), line_end = aLines.end() ;
       line != line_end;
       ++line)
  {
    *lp++ = line;
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
                        nsRect& aLineBounds,
                        PRUint32* aLineFlags)
{
  NS_ENSURE_ARG_POINTER(aFirstFrameOnLine);
  NS_ENSURE_ARG_POINTER(aNumFramesOnLine);
  NS_ENSURE_ARG_POINTER(aLineFlags);

  if ((aLineNumber < 0) || (aLineNumber >= mNumLines)) {
    *aFirstFrameOnLine = nsnull;
    *aNumFramesOnLine = 0;
    aLineBounds.SetRect(0, 0, 0, 0);
    return NS_OK;
  }
  nsLineBox* line = mLines[aLineNumber];
  *aFirstFrameOnLine = line->mFirstChild;
  *aNumFramesOnLine = line->GetChildCount();
  aLineBounds = line->mBounds;

  PRUint32 flags = 0;
  if (line->IsBlock()) {
    flags |= NS_LINE_FLAG_IS_BLOCK;
  }
  else {
    if (line->HasBreak())
      flags |= NS_LINE_FLAG_ENDS_IN_BREAK;
  }
  *aLineFlags = flags;

  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::FindLineContaining(nsIFrame* aFrame,
                                   PRInt32* aLineNumberResult)
{
  nsLineBox* line = mLines[0];
  PRInt32 lineNumber = 0;
  while (lineNumber != mNumLines) {
    if (line->Contains(aFrame)) {
      *aLineNumberResult = lineNumber;
      return NS_OK;
    }
    line = mLines[++lineNumber];
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
  while (lineNumber != mNumLines) {
    if ((aY >= line->mBounds.y) && (aY < line->mBounds.YMost())) {
      *aLineNumberResult = lineNumber;
      return NS_OK;
    }
    line = mLines[++lineNumber];
  }
  *aLineNumberResult = mNumLines;
  return NS_OK;
}

#ifdef IBMBIDI
NS_IMETHODIMP
nsLineIterator::CheckLineOrder(PRInt32                  aLine,
                               PRBool                   *aIsReordered,
                               nsIFrame                 **aFirstVisual,
                               nsIFrame                 **aLastVisual)
{
  PRInt32   currentLine, saveLine, testLine;
  nscoord   saveX;
  nsIFrame  *checkFrame;
  nsIFrame  *firstFrame;
  nsIFrame  *leftmostFrame;
  nsIFrame  *rightmostFrame;
  nscoord   minX, maxX;
  PRInt32   lineFrameCount;
  PRUint32  lineFlags;

  nsresult  result = NS_OK;

  // an RTL paragraph is always considered as reordered
  // in an LTR paragraph, find out by examining the coordinates of each frame in the line
  if (mRightToLeft)
    *aIsReordered = PR_TRUE;
  else {
    *aIsReordered = PR_FALSE;

    // Check the preceding and following line, since we might be moving into them
    for (currentLine = PR_MAX(0, aLine-1); currentLine < aLine+1; currentLine++) {

      nsLineBox* line = mLines[currentLine];
      if (!line)
        break;

      checkFrame = line->mFirstChild;

      result = FindLineContaining(checkFrame, &saveLine);
      if (NS_FAILED(result))
        return result;
      saveX = checkFrame->GetRect().x;
      lineFrameCount = line->GetChildCount();

      for (; checkFrame; checkFrame = checkFrame->GetNextSibling()) {
        result = FindLineContaining(checkFrame, &testLine);
        if (NS_FAILED(result))
          return result;
        if (testLine != saveLine) {
          *aIsReordered = PR_TRUE;
          break;
        }

        nsRect checkRect = checkFrame->GetRect();
        // If the origin of any frame is less than the previous frame, the line is reordered
        if (checkRect.x < saveX) {
          *aIsReordered = PR_TRUE;
          break;
        }
        saveX = checkRect.x;
        lineFrameCount--;
        if (0 == lineFrameCount)
          break;
      }
      if (*aIsReordered)
        break;
    }
  }

  // If the line is reordered, identify the first and last frames on the line
  if (*aIsReordered) {
    nsRect nonUsedRect;
    result = GetLine(aLine, &firstFrame, &lineFrameCount, nonUsedRect, &lineFlags);
    if (NS_FAILED(result))
      return result;

    leftmostFrame = rightmostFrame = firstFrame;
    maxX = minX = firstFrame->GetRect().x;

    for (;lineFrameCount > 1;lineFrameCount--) {
      firstFrame = firstFrame->GetNextSibling();

      nsRect checkRect = firstFrame->GetRect();
      if (checkRect.x > maxX) {
        maxX = checkRect.x;
        rightmostFrame = firstFrame;
      }
      if (checkRect.x < minX) {
        minX = checkRect.x;
        leftmostFrame = firstFrame;
      }
    }
    if (mRightToLeft) {
      *aFirstVisual = rightmostFrame;
      *aLastVisual = leftmostFrame;
    }
    else {
      *aFirstVisual = leftmostFrame;
      *aLastVisual = rightmostFrame;
    }
  }
  return result;
}
#endif // IBMBIDI

NS_IMETHODIMP
nsLineIterator::FindFrameAt(PRInt32 aLineNumber,
                            nscoord aX,
#ifdef IBMBIDI
                            PRBool aCouldBeReordered,
#endif // IBMBIDI
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

  if (line->mBounds.width == 0)
    return NS_ERROR_FAILURE;

  nsIFrame *stoppingFrame = nsnull;

  if (aX < line->mBounds.x) {
    nsIFrame* frame;
    if (mRightToLeft) {
      frame = line->LastChild();
    }
    else {
      frame = line->mFirstChild;
    }
    if (frame->GetRect().width > 0)
    {
      *aFrameFound = frame;
      *aXIsBeforeFirstFrame = PR_TRUE;
      *aXIsAfterLastFrame = PR_FALSE;
      return NS_OK;
    }
    else if (mRightToLeft)
      stoppingFrame = frame;
    else
      stoppingFrame = line->LastChild();
  }
  else if (aX >= line->mBounds.XMost()) {
    nsIFrame* frame;
    if (mRightToLeft) {
      frame = line->mFirstChild;
    }
    else {
      frame = line->LastChild();
    }
    if (frame->GetRect().width > 0)
    {
      *aFrameFound = frame;
      *aXIsBeforeFirstFrame = PR_FALSE;
      *aXIsAfterLastFrame = PR_TRUE;
      return NS_OK;
    }
    else if (mRightToLeft)
      stoppingFrame = line->mFirstChild;
    else
      stoppingFrame = frame;
  }

  // Find the frame closest to the X coordinate. Gaps can occur
  // between frames (because of margins) so we split the gap in two
  // when checking.
  *aXIsBeforeFirstFrame = PR_FALSE;
  *aXIsAfterLastFrame = PR_FALSE;
#ifdef IBMBIDI
  PRBool isReordered = PR_FALSE;
  nsIFrame *firstVisual, *lastVisual;
  if (aCouldBeReordered)
    CheckLineOrder(aLineNumber, &isReordered, &firstVisual, &lastVisual);
#endif
  nsIFrame* frame = line->mFirstChild;
#ifdef IBMBIDI
  if (isReordered)
    frame = firstVisual;
#endif // IBMBIDI
  PRInt32 n = line->GetChildCount();
  if (mRightToLeft) {
    while (--n >= 0) {
      nsIFrame* nextFrame;
#ifdef IBMBIDI
      if (!frame)
        break;
      if (isReordered) {
        nscoord maxX, limX;
        PRInt32 testLine;
        nsIFrame* tempFrame;

        maxX = -0x7fffffff;

        limX = frame->GetRect().x;
        tempFrame = line->mFirstChild;
        nextFrame = nsnull;

        while (tempFrame) {
          if (NS_SUCCEEDED(FindLineContaining(tempFrame, &testLine))
              && testLine == aLineNumber) {
            nsRect tempRect = tempFrame->GetRect();
            if (tempRect.x > maxX && tempRect.x < limX) { // we are looking for the highest value less than the current one
              maxX = tempRect.x;
              nextFrame = tempFrame;
            }
          }
          tempFrame = tempFrame->GetNextSibling();
        }
      }
      else
#endif // IBMBIDI
        nextFrame = frame->GetNextSibling();

      nsRect r1 = frame->GetRect();
      if (r1.width && aX > r1.x) {
        break;
      }
      if (nextFrame) {
        nsRect r2 = nextFrame->GetRect();
        if (r2.width && aX > r2.XMost()) {
          nscoord rightEdge = r2.XMost();
          nscoord delta = r1.x - rightEdge;
          if (!r1.width || aX < rightEdge + delta/2) {
            frame = nextFrame;
          }
          break;
        }
      }
      else {
        *aXIsBeforeFirstFrame = PR_TRUE;
      }
      frame = nextFrame;
      if (nextFrame == stoppingFrame)
        break;
    }
  }
  else {
    while (--n >= 0) {
      nsIFrame* nextFrame;
#ifdef IBMBIDI
      if (!frame)
        break;
      if (isReordered) {
        nsIFrame* tempFrame;
        nscoord minX, limX;
        PRInt32 testLine;

        minX = 0x7fffffff;

        limX = frame->GetRect().x;
        tempFrame = line->mFirstChild;
        nextFrame = nsnull;

        while (tempFrame) {
          if (NS_SUCCEEDED(FindLineContaining(tempFrame, &testLine))
              && testLine == aLineNumber) {
            nsRect tempRect = tempFrame->GetRect();
            if (tempRect.width && tempRect.x < minX && tempRect.x > limX) { // we are looking for the lowest value greater than the current one
              minX = tempRect.x;
              nextFrame = tempFrame;
            }
          }
          tempFrame = tempFrame->GetNextSibling();
        }
      }
      else
#endif // IBMBIDI
      nextFrame = frame->GetNextSibling();

      nsRect r1 = frame->GetRect();
      if (r1.width && aX < r1.XMost()) {
        break;
      }
      if (nextFrame) {
        nsRect r2 = nextFrame->GetRect();
        if (r2.width && aX < r2.x) {
          nscoord rightEdge = r1.XMost();
          nscoord delta = r2.x - rightEdge;
          if (!r1.width || aX >= rightEdge + delta/2) {
            frame = nextFrame;
          }
          break;
        }
      }
      else {
        *aXIsAfterLastFrame = PR_TRUE;
      }
      frame = nextFrame;
      if (nextFrame == stoppingFrame)
        break;
    }
  }

  *aFrameFound = frame;
  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber)
{
  aFrame = aFrame->GetNextSibling();
  return NS_OK;
}

//----------------------------------------------------------------------

nsFloatCacheList::~nsFloatCacheList()
{
  nsFloatCache* fc = mHead;
  while (fc) {
    nsFloatCache* next = fc->mNext;
    delete fc;
    fc = next;
  }
  mHead = nsnull;
}

nsFloatCache*
nsFloatCacheList::Tail() const
{
  nsFloatCache* fc = mHead;
  while (fc) {
    if (!fc->mNext) {
      break;
    }
    fc = fc->mNext;
  }
  return fc;
}

void
nsFloatCacheList::Append(nsFloatCacheFreeList& aList)
{
  nsFloatCache* tail = Tail();
  if (tail) {
    tail->mNext = aList.mHead;
  }
  else {
    mHead = aList.mHead;
  }
  aList.mHead = nsnull;
  aList.mTail = nsnull;
}

nsFloatCache*
nsFloatCacheList::Find(nsIFrame* aOutOfFlowFrame)
{
  nsFloatCache* fc = mHead;
  while (fc) {
    if (fc->mPlaceholder->GetOutOfFlowFrame() == aOutOfFlowFrame) {
      break;
    }
    fc = fc->Next();
  }
  return fc;
}

void
nsFloatCacheList::Remove(nsFloatCache* aElement)
{
  nsFloatCache** fcp = &mHead;
  nsFloatCache* fc;
  while (nsnull != (fc = *fcp)) {
    if (fc == aElement) {
      *fcp = fc->mNext;
      break;
    }
    fcp = &fc->mNext;
  }
}

//----------------------------------------------------------------------

void
nsFloatCacheFreeList::Append(nsFloatCacheList& aList)
{
  if (mTail) {
    mTail->mNext = aList.mHead;
  }
  else {
    mHead = aList.mHead;
  }
  mTail = aList.Tail();
  aList.mHead = nsnull;
}

nsFloatCache*
nsFloatCacheFreeList::Alloc()
{
  nsFloatCache* fc = mHead;
  if (mHead) {
    if (mHead == mTail) {
      mHead = mTail = nsnull;
    }
    else {
      mHead = fc->mNext;
    }
    fc->mNext = nsnull;
  }
  else {
    fc = new nsFloatCache();
  }
  return fc;
}

void
nsFloatCacheFreeList::Append(nsFloatCache* aFloat)
{
  aFloat->mNext = nsnull;
  if (mTail) {
    mTail->mNext = aFloat;
    mTail = aFloat;
  }
  else {
    mHead = mTail = aFloat;
  }
}

//----------------------------------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsFloatCache)

nsFloatCache::nsFloatCache()
  : mPlaceholder(nsnull),
    mIsCurrentLineFloat(PR_TRUE),
    mMargins(0, 0, 0, 0),
    mOffsets(0, 0, 0, 0),
    mCombinedArea(0, 0, 0, 0),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsFloatCache);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatCache::~nsFloatCache()
{
  MOZ_COUNT_DTOR(nsFloatCache);
}
#endif
