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

/* representation of one line within a block frame, a CSS line box */

#include "nsLineBox.h"
#include "nsLineLayout.h"
#include "prprf.h"
#include "nsBlockFrame.h"
#include "nsGkAtoms.h"
#include "nsFrameManager.h"
#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif

#ifdef DEBUG
static PRInt32 ctorCount;
PRInt32 nsLineBox::GetCtorCount() { return ctorCount; }
#endif

nsLineBox::nsLineBox(nsIFrame* aFrame, PRInt32 aCount, PRBool aIsBlock)
  : mFirstChild(aFrame),
    mBounds(0, 0, 0, 0),
    mAscent(0),
    mData(nsnull)
{
  MOZ_COUNT_CTOR(nsLineBox);
#ifdef DEBUG
  ++ctorCount;
  NS_ASSERTION(!aIsBlock || aCount == 1, "Blocks must have exactly one child");
  nsIFrame* f = aFrame;
  for (PRInt32 n = aCount; n > 0; f = f->GetNextSibling(), --n) {
    NS_ASSERTION(aIsBlock == f->GetStyleDisplay()->IsBlockOutside(),
                 "wrong kind of child frame");
  }
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
  return aPresShell->AllocateMisc(sz);
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
  aPresShell->FreeMisc(sizeof(*this), (void*)this);
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
  nsFloatCache* fc = aFloats.Head();
  while (fc) {
    nsFrame::IndentBy(out, aIndent);
    nsIFrame* frame = fc->mFloat;
    fprintf(out, "floatframe@%p ", static_cast<void*>(frame));
    if (frame) {
      nsAutoString frameName;
      frame->GetFrameName(frameName);
      fputs(NS_LossyConvertUTF16toASCII(frameName).get(), out);
    }
    else {
      fputs("\n###!!! NULL out-of-flow frame", out);
    }
    fprintf(out, "\n");
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
  PR_snprintf(aBuf, aBufSize, "%s,%s,%s,%s,%s,before:%s,after:%s[0x%x]",
              IsBlock() ? "block" : "inline",
              IsDirty() ? "dirty" : "clean",
              IsPreviousMarginDirty() ? "prevmargindirty" : "prevmarginclean",
              IsImpactedByFloat() ? "impacted" : "not impacted",
              IsLineWrapped() ? "wrapped" : "not wrapped",
              BreakTypeToString(GetBreakTypeBefore()),
              BreakTypeToString(GetBreakTypeAfter()),
              mAllFlags);
  return aBuf;
}

void
nsLineBox::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  char cbuf[100];
  fprintf(out, "line %p: count=%d state=%s ",
          static_cast<const void*>(this), GetChildCount(),
          StateToString(cbuf, sizeof(cbuf)));
  if (IsBlock() && !GetCarriedOutBottomMargin().IsZero()) {
    fprintf(out, "bm=%d ", GetCarriedOutBottomMargin().get());
  }
  fprintf(out, "{%d,%d,%d,%d} ",
          mBounds.x, mBounds.y, mBounds.width, mBounds.height);
  if (mData) {
    fprintf(out, "vis-overflow={%d,%d,%d,%d} scr-overflow={%d,%d,%d,%d} ",
            mData->mOverflowAreas.VisualOverflow().x,
            mData->mOverflowAreas.VisualOverflow().y,
            mData->mOverflowAreas.VisualOverflow().width,
            mData->mOverflowAreas.VisualOverflow().height,
            mData->mOverflowAreas.ScrollableOverflow().x,
            mData->mOverflowAreas.ScrollableOverflow().y,
            mData->mOverflowAreas.ScrollableOverflow().width,
            mData->mOverflowAreas.ScrollableOverflow().height);
  }
  fprintf(out, "<\n");

  nsIFrame* frame = mFirstChild;
  PRInt32 n = GetChildCount();
  while (--n >= 0) {
    frame->List(out, aIndent + 1);
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
  if (HasBullet()) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsLineBox::CachedIsEmpty()
{
  if (mFlags.mDirty) {
    return IsEmpty();
  }
  
  if (mFlags.mEmptyCacheValid) {
    return mFlags.mEmptyCacheState;
  }

  PRBool result;
  if (IsBlock()) {
    result = mFirstChild->CachedIsEmpty();
  } else {
    PRInt32 n;
    nsIFrame *kid;
    result = PR_TRUE;
    for (n = GetChildCount(), kid = mFirstChild;
         n > 0;
         --n, kid = kid->GetNextSibling())
      {
        if (!kid->CachedIsEmpty()) {
          result = PR_FALSE;
          break;
        }
      }
    if (HasBullet()) {
      result = PR_FALSE;
    }
  }

  mFlags.mEmptyCacheValid = PR_TRUE;
  mFlags.mEmptyCacheState = result;
  return result;
}

void
nsLineBox::DeleteLineList(nsPresContext* aPresContext, nsLineList& aLines,
                          nsIFrame* aDestructRoot)
{
  if (! aLines.empty()) {
    // Delete our child frames before doing anything else. In particular
    // we do all of this before our base class releases it's hold on the
    // view.
#ifdef DEBUG
    PRInt32 numFrames = 0;
#endif
    for (nsIFrame* child = aLines.front()->mFirstChild; child; ) {
      nsIFrame* nextChild = child->GetNextSibling();
      child->SetNextSibling(nsnull);
      child->DestroyFrom((aDestructRoot) ? aDestructRoot : child);
      child = nextChild;
#ifdef DEBUG
      numFrames++;
#endif
    }

    nsIPresShell *shell = aPresContext->PresShell();

    do {
      nsLineBox* line = aLines.front();
#ifdef DEBUG
      numFrames -= line->GetChildCount();
#endif
      aLines.pop_front();
      line->Destroy(shell);
    } while (! aLines.empty());
#ifdef DEBUG
    NS_ASSERTION(numFrames == 0, "number of frames deleted does not match");
#endif
  }
}

PRBool
nsLineBox::RFindLineContaining(nsIFrame* aFrame,
                               const nsLineList::iterator& aBegin,
                               nsLineList::iterator& aEnd,
                               nsIFrame* aLastFrameBeforeEnd,
                               PRInt32* aFrameIndexInLine)
{
  NS_PRECONDITION(aFrame, "null ptr");
  nsIFrame* curFrame = aLastFrameBeforeEnd;
  while (aBegin != aEnd) {
    --aEnd;
    NS_ASSERTION(aEnd->IsLastChild(curFrame), "Unexpected curFrame");
    // i is the index of curFrame in aEnd
    PRInt32 i = aEnd->GetChildCount() - 1;
    while (i >= 0) {
      if (curFrame == aFrame) {
        *aFrameIndexInLine = i;
        return PR_TRUE;
      }
      --i;
      curFrame = curFrame->GetPrevSibling();
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

PRBool
nsLineBox::SetCarriedOutBottomMargin(nsCollapsingMargin aValue)
{
  PRBool changed = PR_FALSE;
  if (IsBlock()) {
    if (!aValue.IsZero()) {
      if (!mBlockData) {
        mBlockData = new ExtraBlockData(mBounds);
      }
      changed = aValue != mBlockData->mCarriedOutBottomMargin;
      mBlockData->mCarriedOutBottomMargin = aValue;
    }
    else if (mBlockData) {
      changed = aValue != mBlockData->mCarriedOutBottomMargin;
      mBlockData->mCarriedOutBottomMargin = aValue;
      MaybeFreeData();
    }
  }
  return changed;
}

void
nsLineBox::MaybeFreeData()
{
  if (mData && mData->mOverflowAreas == nsOverflowAreas(mBounds, mBounds)) {
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
  if (IsInline() && mInlineData) {
    if (mInlineData->mFloats.NotEmpty()) {
      aFreeList.Append(mInlineData->mFloats);
    }
    MaybeFreeData();
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
      mInlineData->mFloats.Append(aFreeList);
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
      mInlineData->mFloats.Remove(fc);
      delete fc;
      MaybeFreeData();
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void
nsLineBox::SetOverflowAreas(const nsOverflowAreas& aOverflowAreas)
{
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    NS_ASSERTION(aOverflowAreas.Overflow(otype).width >= 0,
                 "illegal width for combined area");
    NS_ASSERTION(aOverflowAreas.Overflow(otype).height >= 0,
                 "illegal height for combined area");
  }
  if (!aOverflowAreas.VisualOverflow().IsEqualInterior(mBounds) ||
      !aOverflowAreas.ScrollableOverflow().IsEqualEdges(mBounds)) {
    if (!mData) {
      if (IsInline()) {
        mInlineData = new ExtraInlineData(mBounds);
      }
      else {
        mBlockData = new ExtraBlockData(mBounds);
      }
    }
    mData->mOverflowAreas = aOverflowAreas;
  }
  else if (mData) {
    // Store away new value so that MaybeFreeData compares against
    // the right value.
    mData->mOverflowAreas = aOverflowAreas;
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

/* virtual */ void
nsLineIterator::DisposeLineIterator()
{
  delete this;
}

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

PRInt32
nsLineIterator::GetNumLines()
{
  return mNumLines;
}

PRBool
nsLineIterator::GetDirection()
{
  return mRightToLeft;
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
    if (line->HasBreakAfter())
      flags |= NS_LINE_FLAG_ENDS_IN_BREAK;
  }
  *aLineFlags = flags;

  return NS_OK;
}

PRInt32
nsLineIterator::FindLineContaining(nsIFrame* aFrame, PRInt32 aStartLine)
{
  NS_PRECONDITION(aStartLine <= mNumLines, "Bogus line numbers");
  PRInt32 lineNumber = aStartLine;
  while (lineNumber != mNumLines) {
    nsLineBox* line = mLines[lineNumber];
    if (line->Contains(aFrame)) {
      return lineNumber;
    }
    ++lineNumber;
  }
  return -1;
}

#ifdef IBMBIDI
NS_IMETHODIMP
nsLineIterator::CheckLineOrder(PRInt32                  aLine,
                               PRBool                   *aIsReordered,
                               nsIFrame                 **aFirstVisual,
                               nsIFrame                 **aLastVisual)
{
  NS_ASSERTION (aLine >= 0 && aLine < mNumLines, "aLine out of range!");
  nsLineBox* line = mLines[aLine];

  if (!line->mFirstChild) { // empty line
    *aIsReordered = PR_FALSE;
    *aFirstVisual = nsnull;
    *aLastVisual = nsnull;
    return NS_OK;
  }

  nsIFrame* leftmostFrame;
  nsIFrame* rightmostFrame;
  *aIsReordered = nsBidiPresUtils::CheckLineOrder(line->mFirstChild, line->GetChildCount(), &leftmostFrame, &rightmostFrame);

  // map leftmost/rightmost to first/last according to paragraph direction
  *aFirstVisual = mRightToLeft ? rightmostFrame : leftmostFrame;
  *aLastVisual = mRightToLeft ? leftmostFrame : rightmostFrame;

  return NS_OK;
}
#endif // IBMBIDI

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

  if (line->mBounds.width == 0 && line->mBounds.height == 0)
    return NS_ERROR_FAILURE;

  nsIFrame* frame = line->mFirstChild;
  nsIFrame* closestFromLeft = nsnull;
  nsIFrame* closestFromRight = nsnull;
  PRInt32 n = line->GetChildCount();
  while (n--) {
    nsRect rect = frame->GetRect();
    if (rect.width > 0) {
      // If aX is inside this frame - this is it
      if (rect.x <= aX && rect.XMost() > aX) {
        closestFromLeft = closestFromRight = frame;
        break;
      }
      if (rect.x < aX) {
        if (!closestFromLeft || 
            rect.XMost() > closestFromLeft->GetRect().XMost())
          closestFromLeft = frame;
      }
      else {
        if (!closestFromRight ||
            rect.x < closestFromRight->GetRect().x)
          closestFromRight = frame;
      }
    }
    frame = frame->GetNextSibling();
  }
  if (!closestFromLeft && !closestFromRight) {
    // All frames were zero-width. Just take the first one.
    closestFromLeft = closestFromRight = line->mFirstChild;
  }
  *aXIsBeforeFirstFrame = mRightToLeft ? !closestFromRight : !closestFromLeft;
  *aXIsAfterLastFrame = mRightToLeft ? !closestFromLeft : !closestFromRight;
  if (closestFromLeft == closestFromRight) {
    *aFrameFound = closestFromLeft;
  }
  else if (!closestFromLeft) {
    *aFrameFound = closestFromRight;
  }
  else if (!closestFromRight) {
    *aFrameFound = closestFromLeft;
  }
  else { // we're between two frames
    nscoord delta = closestFromRight->GetRect().x - closestFromLeft->GetRect().XMost();
    if (aX < closestFromLeft->GetRect().XMost() + delta/2)
      *aFrameFound = closestFromLeft;
    else
      *aFrameFound = closestFromRight;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber)
{
  aFrame = aFrame->GetNextSibling();
  return NS_OK;
}

//----------------------------------------------------------------------

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatCacheList::nsFloatCacheList() :
  mHead(nsnull)
{
  MOZ_COUNT_CTOR(nsFloatCacheList);
}
#endif

nsFloatCacheList::~nsFloatCacheList()
{
  DeleteAll();
  MOZ_COUNT_DTOR(nsFloatCacheList);
}

void
nsFloatCacheList::DeleteAll()
{
  nsFloatCache* c = mHead;
  while (c) {
    nsFloatCache* next = c->Next();
    delete c;
    c = next;
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
  NS_PRECONDITION(aList.NotEmpty(), "Appending empty list will fail");
  
  nsFloatCache* tail = Tail();
  if (tail) {
    NS_ASSERTION(!tail->mNext, "Bogus!");
    tail->mNext = aList.mHead;
  }
  else {
    NS_ASSERTION(!mHead, "Bogus!");
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
    if (fc->mFloat == aOutOfFlowFrame) {
      break;
    }
    fc = fc->Next();
  }
  return fc;
}

nsFloatCache*
nsFloatCacheList::RemoveAndReturnPrev(nsFloatCache* aElement)
{
  nsFloatCache* fc = mHead;
  nsFloatCache* prev = nsnull;
  while (fc) {
    if (fc == aElement) {
      if (prev) {
        prev->mNext = fc->mNext;
      } else {
        mHead = fc->mNext;
      }
      return prev;
    }
    prev = fc;
    fc = fc->mNext;
  }
  return nsnull;
}

//----------------------------------------------------------------------

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatCacheFreeList::nsFloatCacheFreeList() :
  mTail(nsnull)
{
  MOZ_COUNT_CTOR(nsFloatCacheFreeList);
}

nsFloatCacheFreeList::~nsFloatCacheFreeList()
{
  MOZ_COUNT_DTOR(nsFloatCacheFreeList);
}
#endif
  
void
nsFloatCacheFreeList::Append(nsFloatCacheList& aList)
{
  NS_PRECONDITION(aList.NotEmpty(), "Appending empty list will fail");
  
  if (mTail) {
    NS_ASSERTION(!mTail->mNext, "Bogus");
    mTail->mNext = aList.mHead;
  }
  else {
    NS_ASSERTION(!mHead, "Bogus");
    mHead = aList.mHead;
  }
  mTail = aList.Tail();
  aList.mHead = nsnull;
}

void
nsFloatCacheFreeList::Remove(nsFloatCache* aElement)
{
  nsFloatCache* prev = nsFloatCacheList::RemoveAndReturnPrev(aElement);
  if (mTail == aElement) {
    mTail = prev;
  }
}

void
nsFloatCacheFreeList::DeleteAll()
{
  nsFloatCacheList::DeleteAll();
  mTail = nsnull;
}

nsFloatCache*
nsFloatCacheFreeList::Alloc(nsIFrame* aFloat)
{
  NS_PRECONDITION(aFloat->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
                  "This is a float cache, why isn't the frame out-of-flow?");
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
  fc->mFloat = aFloat;
  return fc;
}

void
nsFloatCacheFreeList::Append(nsFloatCache* aFloat)
{
  NS_ASSERTION(!aFloat->mNext, "Bogus!");
  aFloat->mNext = nsnull;
  if (mTail) {
    NS_ASSERTION(!mTail->mNext, "Bogus!");
    mTail->mNext = aFloat;
    mTail = aFloat;
  }
  else {
    NS_ASSERTION(!mHead, "Bogus!");
    mHead = mTail = aFloat;
  }
}

//----------------------------------------------------------------------

nsFloatCache::nsFloatCache()
  : mFloat(nsnull),
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
