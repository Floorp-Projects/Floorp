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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsLineBox.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsLineLayout.h"
#include "prprf.h"

#ifdef DEBUG
#include "nsISizeOfHandler.h"
#include "nsLayoutAtoms.h"
#endif

#ifdef DEBUG
static PRInt32 ctorCount;
PRInt32 nsLineBox::GetCtorCount() { return ctorCount; }
#endif

MOZ_DECL_CTOR_COUNTER(nsLineBox)

nsLineBox::nsLineBox(nsIFrame* aFrame, PRInt32 aCount, PRBool aIsBlock)
  : mFirstChild(aFrame),
    mNext(nsnull),
    mBounds(0, 0, 0, 0),
    mMaxElementWidth(0),
    mData(nsnull),
    mMaximumWidth(-1)
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
nsLineBox::operator new(size_t sz, nsIPresShell* aPresShell)
{
  void* result = nsnull;
  aPresShell->AllocateFrame(sz, &result);
  return result;
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
ListFloaters(FILE* out, PRInt32 aIndent, const nsFloaterCacheList& aFloaters)
{
  nsAutoString frameName;
  nsFloaterCache* fc = aFloaters.Head();
  while (fc) {
    nsFrame::IndentBy(out, aIndent);
    nsPlaceholderFrame* ph = fc->mPlaceholder;
    if (nsnull != ph) {
      fprintf(out, "placeholder@%p ", ph);
      nsIFrame* frame = ph->GetOutOfFlowFrame();
      if (nsnull != frame) {
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->GetFrameName(frameName);
          fputs(frameName, out);
        }
      }
      fprintf(out, " %s region={%d,%d,%d,%d} combinedArea={%d,%d,%d,%d}",
              fc->mIsCurrentLineFloater ? "cl" : "bcl",
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

char*
nsLineBox::StateToString(char* aBuf, PRInt32 aBufSize) const
{
  PR_snprintf(aBuf, aBufSize, "%s,%s,%s,%s[0x%x]",
              IsBlock() ? "block" : "inline",
              IsDirty() ? "dirty" : "clean",
              IsImpactedByFloater() ? "IMPACTED" : "NOT Impacted",
              IsTrimmed() ? "trimmed" : "",
              mAllFlags);
  return aBuf;
}

#ifdef DEBUG
void
nsLineBox::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  char cbuf[100];
  fprintf(out, "line %p: count=%d state=%s ",
          this, GetChildCount(), StateToString(cbuf, sizeof(cbuf)));
  if (0 != GetCarriedOutBottomMargin()) {
    fprintf(out, "bm=%d ", GetCarriedOutBottomMargin());
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
    frame->GetNextSibling(&frame);
  }

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  if (HasFloaters()) {
    fputs("> floaters <\n", out);
    ListFloaters(out, aIndent + 1, mInlineData->mFloaters);
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
  PRInt32 i, n = GetChildCount();
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
nsLineBox::DeleteLineList(nsIPresContext* aPresContext, nsLineBox* aLine)
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

nscoord
nsLineBox::GetCarriedOutBottomMargin() const
{
  return (IsBlock() && mBlockData) ? mBlockData->mCarriedOutBottomMargin : 0;
}

void
nsLineBox::SetCarriedOutBottomMargin(nscoord aValue)
{
  if (IsBlock()) {
    if (aValue) {
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
      if (mInlineData->mFloaters.IsEmpty()) {
        delete mInlineData;
        mInlineData = nsnull;
      }
    }
    else if (0 == mBlockData->mCarriedOutBottomMargin) {
      delete mBlockData;
      mBlockData = nsnull;
    }
  }
}

// XXX get rid of this???
nsFloaterCache*
nsLineBox::GetFirstFloater()
{
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floaters");
  return mInlineData ? mInlineData->mFloaters.Head() : nsnull;
}

// XXX this might be too eager to free memory
void
nsLineBox::FreeFloaters(nsFloaterCacheFreeList& aFreeList)
{
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floaters");
  if (IsInline()) {
    if (mInlineData) {
      aFreeList.Append(mInlineData->mFloaters);
      MaybeFreeData();
    }
  }
}

void
nsLineBox::RemoveFloatersFromSpaceManager(nsISpaceManager* aSpaceManager)
{ 
  if (IsInline()) {
    if (mInlineData) {
      nsFloaterCache* floaterCache = mInlineData->mFloaters.Head();

      while (floaterCache) {
        nsIFrame* floater = floaterCache->mPlaceholder->GetOutOfFlowFrame();
        aSpaceManager->RemoveRegion(floater);
        floaterCache = floaterCache->Next();        
      }
    }
  }
}

void
nsLineBox::AppendFloaters(nsFloaterCacheFreeList& aFreeList)
{ 
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floaters");
  if (IsInline()) {
    if (aFreeList.NotEmpty()) {
      if (!mInlineData) {
        mInlineData = new ExtraInlineData(mBounds);
      }
      if (mInlineData) {
        mInlineData->mFloaters.Append(aFreeList);
      }
    }
  }
}

PRBool
nsLineBox::RemoveFloater(nsIFrame* aFrame)
{
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floaters");
  if (IsInline() && mInlineData) {
    nsFloaterCache* fc = mInlineData->mFloaters.Find(aFrame);
    if (fc) {
      // Note: the placeholder is part of the line's child list
      // and will be removed later.
      fc->mPlaceholder->SetOutOfFlowFrame(nsnull);
      mInlineData->mFloaters.Remove(fc);
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
#ifdef VERY_NOISY_REFLOW
  printf("nsLB::SetCombinedArea(1) %p (%d, %d, %d, %d)\n", 
         this, aCombinedArea.x, aCombinedArea.y, aCombinedArea.width, aCombinedArea.height);
#endif
}

void
nsLineBox::GetCombinedArea(nsRect* aResult)
{
  NS_ASSERTION(aResult, "null arg");
  if (aResult) {
    *aResult = mData ? mData->mCombinedArea : mBounds;
#ifdef VERY_NOISY_REFLOW
    printf("nsLB::SetCombinedArea(1) %p (%d, %d, %d, %d)\n", 
         this, aResult->x, aResult->y, aResult->width, aResult->height);
#endif
  }
}

#ifdef DEBUG
PRInt32
nsLineBox::ListLength(nsLineBox* aLine)
{
  PRInt32 count = 0;
  while (aLine) {
    count++;
    aLine = aLine->mNext;
  }
  return count;
}

nsIAtom*
nsLineBox::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  NS_PRECONDITION(aResult, "null OUT parameter pointer");
  *aResult = sizeof(*this);

  nsIAtom* atom;
  if (IsBlock()) {
    atom = nsLayoutAtoms::lineBoxBlockSmall;
    if (mBlockData) {
      atom = nsLayoutAtoms::lineBoxBlockBig;
      *aResult += sizeof(*mBlockData);
    }
  }
  else {
    atom = nsLayoutAtoms::lineBoxSmall;
    if (mInlineData) {
      atom = nsLayoutAtoms::lineBoxBig;
      *aResult += sizeof(*mInlineData);

      // Add in the size needed for floaters associated with this line
      if (HasFloaters()) {
        PRUint32  floatersSize;
        mInlineData->mFloaters.SizeOf(aHandler, &floatersSize);

        // Base size of embedded object was included in sizeof(*this) above
        floatersSize -= sizeof(mInlineData->mFloaters);
        aHandler->AddSize(nsLayoutAtoms::lineBoxFloaters, floatersSize);
      }
    }
  }

  return atom;
}
#endif

//----------------------------------------------------------------------


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

NS_IMPL_ISUPPORTS2(nsLineIterator, nsILineIterator, nsILineIteratorNavigator)

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
    if (line->IsTrimmed())
      flags |= NS_LINE_FLAG_IS_TRIMMED;
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
  PRInt32 n = line->GetChildCount();
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

NS_IMETHODIMP
nsLineIterator::GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber)
{
  return aFrame->GetNextSibling(&aFrame);
}

//----------------------------------------------------------------------

nsFloaterCacheList::~nsFloaterCacheList()
{
  nsFloaterCache* floater = mHead;
  while (floater) {
    nsFloaterCache* next = floater->mNext;
    delete floater;
    floater = next;
  }
  mHead = nsnull;
}

nsFloaterCache*
nsFloaterCacheList::Tail() const
{
  nsFloaterCache* fc = mHead;
  while (fc) {
    if (!fc->mNext) {
      break;
    }
    fc = fc->mNext;
  }
  return fc;
}

void
nsFloaterCacheList::Append(nsFloaterCacheFreeList& aList)
{
  nsFloaterCache* tail = Tail();
  if (tail) {
    tail->mNext = aList.mHead;
  }
  else {
    mHead = aList.mHead;
  }
  aList.mHead = nsnull;
  aList.mTail = nsnull;
}

nsFloaterCache*
nsFloaterCacheList::Find(nsIFrame* aOutOfFlowFrame)
{
  nsFloaterCache* fc = mHead;
  while (fc) {
    if (fc->mPlaceholder->GetOutOfFlowFrame() == aOutOfFlowFrame) {
      break;
    }
    fc = fc->Next();
  }
  return fc;
}

void
nsFloaterCacheList::Remove(nsFloaterCache* aElement)
{
  nsFloaterCache** fcp = &mHead;
  nsFloaterCache* fc;
  while (nsnull != (fc = *fcp)) {
    if (fc == aElement) {
      *fcp = fc->mNext;
      break;
    }
    fcp = &fc->mNext;
  }
}

#ifdef DEBUG
void
nsFloaterCacheList::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  NS_PRECONDITION(aResult, "null OUT parameter pointer");
  *aResult = sizeof(*this);

  // Add in the space for each floater
  for (nsFloaterCache* cache = Head(); cache; cache = cache->Next()) {
    *aResult += sizeof(*cache);
  }
}
#endif

//----------------------------------------------------------------------

void
nsFloaterCacheFreeList::Append(nsFloaterCacheList& aList)
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

nsFloaterCache*
nsFloaterCacheFreeList::Alloc()
{
  nsFloaterCache* fc = mHead;
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
    fc = new nsFloaterCache();
  }
  return fc;
}

void
nsFloaterCacheFreeList::Append(nsFloaterCache* aFloater)
{
  aFloater->mNext = nsnull;
  if (mTail) {
    mTail->mNext = aFloater;
    mTail = aFloater;
  }
  else {
    mHead = mTail = aFloater;
  }
}

//----------------------------------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsFloaterCache)

nsFloaterCache::nsFloaterCache()
  : mPlaceholder(nsnull),
    mIsCurrentLineFloater(PR_TRUE),
    mMargins(0, 0, 0, 0),
    mOffsets(0, 0, 0, 0),
    mCombinedArea(0, 0, 0, 0),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsFloaterCache);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloaterCache::~nsFloaterCache()
{
  MOZ_COUNT_DTOR(nsFloaterCache);
}
#endif
