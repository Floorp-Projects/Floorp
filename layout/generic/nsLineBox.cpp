/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of one line within a block frame, a CSS line box */

#include "nsLineBox.h"
#include "nsLineLayout.h"
#include "prprf.h"
#include "nsBlockFrame.h"
#include "nsIFrame.h"
#include "nsPresArena.h"
#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif
#include "nsStyleStructInlines.h"
#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"

#ifdef DEBUG
static int32_t ctorCount;
int32_t nsLineBox::GetCtorCount() { return ctorCount; }
#endif

#ifndef _MSC_VER
// static nsLineBox constant; initialized in the header file.
const uint32_t nsLineBox::kMinChildCountForHashtable;
#endif

nsLineBox::nsLineBox(nsIFrame* aFrame, int32_t aCount, bool aIsBlock)
  : mFirstChild(aFrame)
// NOTE: memory is already zeroed since we allocate with AllocateByObjectID.
{
  MOZ_COUNT_CTOR(nsLineBox);
#ifdef DEBUG
  ++ctorCount;
  NS_ASSERTION(!aIsBlock || aCount == 1, "Blocks must have exactly one child");
  nsIFrame* f = aFrame;
  for (int32_t n = aCount; n > 0; f = f->GetNextSibling(), --n) {
    NS_ASSERTION(aIsBlock == f->IsBlockOutside(),
                 "wrong kind of child frame");
  }
#endif

  static_assert(NS_STYLE_CLEAR_LAST_VALUE <= 15,
                "FlagBits needs more bits to store the full range of "
                "break type ('clear') values");
#if NS_STYLE_CLEAR_NONE > 0
  mFlags.mBreakType = NS_STYLE_CLEAR_NONE;
#endif
  mChildCount = aCount;
  MarkDirty();
  mFlags.mBlock = aIsBlock;
}

nsLineBox::~nsLineBox()
{
  MOZ_COUNT_DTOR(nsLineBox);
  if (MOZ_UNLIKELY(mFlags.mHasHashedFrames)) {
    delete mFrames;
  }  
  Cleanup();
}

nsLineBox*
NS_NewLineBox(nsIPresShell* aPresShell, nsIFrame* aFrame, bool aIsBlock)
{
  return new (aPresShell) nsLineBox(aFrame, 1, aIsBlock);
}

nsLineBox*
NS_NewLineBox(nsIPresShell* aPresShell, nsLineBox* aFromLine,
              nsIFrame* aFrame, int32_t aCount)
{
  nsLineBox* newLine = new (aPresShell) nsLineBox(aFrame, aCount, false);
  newLine->NoteFramesMovedFrom(aFromLine);
  return newLine;
}

void
nsLineBox::StealHashTableFrom(nsLineBox* aFromLine, uint32_t aFromLineNewCount)
{
  MOZ_ASSERT(!mFlags.mHasHashedFrames);
  MOZ_ASSERT(GetChildCount() >= int32_t(aFromLineNewCount));
  mFrames = aFromLine->mFrames;
  mFlags.mHasHashedFrames = 1;
  aFromLine->mFlags.mHasHashedFrames = 0;
  aFromLine->mChildCount = aFromLineNewCount;
  // remove aFromLine's frames that aren't on this line
  nsIFrame* f = aFromLine->mFirstChild;
  for (uint32_t i = 0; i < aFromLineNewCount; f = f->GetNextSibling(), ++i) {
    mFrames->RemoveEntry(f);
  }
}

void
nsLineBox::NoteFramesMovedFrom(nsLineBox* aFromLine)
{
  uint32_t fromCount = aFromLine->GetChildCount();
  uint32_t toCount = GetChildCount();
  MOZ_ASSERT(toCount <= fromCount, "moved more frames than aFromLine has");
  uint32_t fromNewCount = fromCount - toCount;
  if (MOZ_LIKELY(!aFromLine->mFlags.mHasHashedFrames)) {
    aFromLine->mChildCount = fromNewCount;
    MOZ_ASSERT(toCount < kMinChildCountForHashtable);
  } else if (fromNewCount < kMinChildCountForHashtable) {
    // aFromLine has a hash table but will not have it after moving the frames
    // so this line can steal the hash table if it needs it.
    if (toCount >= kMinChildCountForHashtable) {
      StealHashTableFrom(aFromLine, fromNewCount);
    } else {
      delete aFromLine->mFrames;
      aFromLine->mFlags.mHasHashedFrames = 0;
      aFromLine->mChildCount = fromNewCount;
    }
  } else {
    // aFromLine still needs a hash table.
    if (toCount < kMinChildCountForHashtable) {
      // remove the moved frames from it
      nsIFrame* f = mFirstChild;
      for (uint32_t i = 0; i < toCount; f = f->GetNextSibling(), ++i) {
        aFromLine->mFrames->RemoveEntry(f);
      }
    } else if (toCount <= fromNewCount) {
      // This line needs a hash table, allocate a hash table for it since that
      // means fewer hash ops.
      nsIFrame* f = mFirstChild;
      for (uint32_t i = 0; i < toCount; f = f->GetNextSibling(), ++i) {
        aFromLine->mFrames->RemoveEntry(f); // toCount RemoveEntry
      }
      SwitchToHashtable(); // toCount PutEntry
    } else {
      // This line needs a hash table, but it's fewer hash ops to steal
      // aFromLine's hash table and allocate a new hash table for that line.
      StealHashTableFrom(aFromLine, fromNewCount); // fromNewCount RemoveEntry
      aFromLine->SwitchToHashtable(); // fromNewCount PutEntry
    }
  }
}

// Overloaded new operator. Uses an arena (which comes from the presShell)
// to perform the allocation.
void*
nsLineBox::operator new(size_t sz, nsIPresShell* aPresShell) CPP_THROW_NEW
{
  return aPresShell->AllocateByObjectID(nsPresArena::nsLineBox_id, sz);
}

void
nsLineBox::Destroy(nsIPresShell* aPresShell)
{
  this->nsLineBox::~nsLineBox();
  aPresShell->FreeByObjectID(nsPresArena::nsLineBox_id, this);
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
    mData = nullptr;
  }
}

#ifdef DEBUG
static void
ListFloats(FILE* out, int32_t aIndent, const nsFloatCacheList& aFloats)
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
BreakTypeToString(uint8_t aBreakType)
{
  switch (aBreakType) {
  case NS_STYLE_CLEAR_NONE: return "nobr";
  case NS_STYLE_CLEAR_LEFT: return "leftbr";
  case NS_STYLE_CLEAR_RIGHT: return "rightbr";
  case NS_STYLE_CLEAR_LEFT_AND_RIGHT: return "leftbr+rightbr";
  case NS_STYLE_CLEAR_LINE: return "linebr";
  default:
    break;
  }
  return "unknown";
}

char*
nsLineBox::StateToString(char* aBuf, int32_t aBufSize) const
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
nsLineBox::List(FILE* out, int32_t aIndent, uint32_t aFlags) const
{
  nsFrame::IndentBy(out, aIndent);
  char cbuf[100];
  fprintf(out, "line %p: count=%d state=%s ",
          static_cast<const void*>(this), GetChildCount(),
          StateToString(cbuf, sizeof(cbuf)));
  if (IsBlock() && !GetCarriedOutBottomMargin().IsZero()) {
    fprintf(out, "bm=%d ", GetCarriedOutBottomMargin().get());
  }
  fprintf(out, "{%d,%d,%d,%d} ",
          mBounds.x, mBounds.y, mBounds.width, mBounds.height);
  if (mData &&
      (!mData->mOverflowAreas.VisualOverflow().IsEqualEdges(mBounds) ||
       !mData->mOverflowAreas.ScrollableOverflow().IsEqualEdges(mBounds))) {
    fprintf(out, "vis-overflow=%d,%d,%d,%d scr-overflow=%d,%d,%d,%d ",
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
  int32_t n = GetChildCount();
  while (--n >= 0) {
    frame->List(out, aIndent + 1, aFlags);
    frame = frame->GetNextSibling();
  }

  if (HasFloats()) {
    nsFrame::IndentBy(out, aIndent);
    fputs("> floats <\n", out);
    ListFloats(out, aIndent + 1, mInlineData->mFloats);
  }
  nsFrame::IndentBy(out, aIndent);
  fputs(">\n", out);
}
#endif

#ifdef DEBUG
nsIFrame*
nsLineBox::LastChild() const
{
  nsIFrame* frame = mFirstChild;
  int32_t n = GetChildCount() - 1;
  while (--n >= 0) {
    frame = frame->GetNextSibling();
  }
  return frame;
}
#endif

int32_t
nsLineBox::IndexOf(nsIFrame* aFrame) const
{
  int32_t i, n = GetChildCount();
  nsIFrame* frame = mFirstChild;
  for (i = 0; i < n; i++) {
    if (frame == aFrame) {
      return i;
    }
    frame = frame->GetNextSibling();
  }
  return -1;
}

bool
nsLineBox::IsEmpty() const
{
  if (IsBlock())
    return mFirstChild->IsEmpty();

  int32_t n;
  nsIFrame *kid;
  for (n = GetChildCount(), kid = mFirstChild;
       n > 0;
       --n, kid = kid->GetNextSibling())
  {
    if (!kid->IsEmpty())
      return false;
  }
  if (HasBullet()) {
    return false;
  }
  return true;
}

bool
nsLineBox::CachedIsEmpty()
{
  if (mFlags.mDirty) {
    return IsEmpty();
  }
  
  if (mFlags.mEmptyCacheValid) {
    return mFlags.mEmptyCacheState;
  }

  bool result;
  if (IsBlock()) {
    result = mFirstChild->CachedIsEmpty();
  } else {
    int32_t n;
    nsIFrame *kid;
    result = true;
    for (n = GetChildCount(), kid = mFirstChild;
         n > 0;
         --n, kid = kid->GetNextSibling())
      {
        if (!kid->CachedIsEmpty()) {
          result = false;
          break;
        }
      }
    if (HasBullet()) {
      result = false;
    }
  }

  mFlags.mEmptyCacheValid = true;
  mFlags.mEmptyCacheState = result;
  return result;
}

void
nsLineBox::DeleteLineList(nsPresContext* aPresContext, nsLineList& aLines,
                          nsIFrame* aDestructRoot, nsFrameList* aFrames)
{
  nsIPresShell* shell = aPresContext->PresShell();

  // Keep our line list and frame list up to date as we
  // remove frames, in case something wants to traverse the
  // frame tree while we're destroying.
  while (!aLines.empty()) {
    nsLineBox* line = aLines.front();
    if (MOZ_UNLIKELY(line->mFlags.mHasHashedFrames)) {
      line->SwitchToCounter();  // Avoid expensive has table removals.
    }
    while (line->GetChildCount() > 0) {
      nsIFrame* child = aFrames->RemoveFirstChild();
      MOZ_ASSERT(child == line->mFirstChild, "Lines out of sync");
      line->mFirstChild = aFrames->FirstChild();
      line->NoteFrameRemoved(child);
      child->DestroyFrom(aDestructRoot);
    }

    aLines.pop_front();
    line->Destroy(shell);
  }
}

bool
nsLineBox::RFindLineContaining(nsIFrame* aFrame,
                               const nsLineList::iterator& aBegin,
                               nsLineList::iterator& aEnd,
                               nsIFrame* aLastFrameBeforeEnd,
                               int32_t* aFrameIndexInLine)
{
  NS_PRECONDITION(aFrame, "null ptr");

  nsIFrame* curFrame = aLastFrameBeforeEnd;
  while (aBegin != aEnd) {
    --aEnd;
    NS_ASSERTION(aEnd->LastChild() == curFrame, "Unexpected curFrame");
    if (MOZ_UNLIKELY(aEnd->mFlags.mHasHashedFrames) &&
        !aEnd->Contains(aFrame)) {
      if (aEnd->mFirstChild) {
        curFrame = aEnd->mFirstChild->GetPrevSibling();
      }
      continue;
    }
    // i is the index of curFrame in aEnd
    int32_t i = aEnd->GetChildCount() - 1;
    while (i >= 0) {
      if (curFrame == aFrame) {
        *aFrameIndexInLine = i;
        return true;
      }
      --i;
      curFrame = curFrame->GetPrevSibling();
    }
    MOZ_ASSERT(!aEnd->mFlags.mHasHashedFrames, "Contains lied to us!");
  }
  *aFrameIndexInLine = -1;
  return false;
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

bool
nsLineBox::SetCarriedOutBottomMargin(nsCollapsingMargin aValue)
{
  bool changed = false;
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
        mInlineData = nullptr;
      }
    }
    else if (mBlockData->mCarriedOutBottomMargin.IsZero()) {
      delete mBlockData;
      mBlockData = nullptr;
    }
  }
}

// XXX get rid of this???
nsFloatCache*
nsLineBox::GetFirstFloat()
{
  NS_ABORT_IF_FALSE(IsInline(), "block line can't have floats");
  return mInlineData ? mInlineData->mFloats.Head() : nullptr;
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

bool
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
      return true;
    }
  }
  return false;
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
  mRightToLeft = false;
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
nsLineIterator::Init(nsLineList& aLines, bool aRightToLeft)
{
  mRightToLeft = aRightToLeft;

  // Count the lines
  int32_t numLines = aLines.size();
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

int32_t
nsLineIterator::GetNumLines()
{
  return mNumLines;
}

bool
nsLineIterator::GetDirection()
{
  return mRightToLeft;
}

NS_IMETHODIMP
nsLineIterator::GetLine(int32_t aLineNumber,
                        nsIFrame** aFirstFrameOnLine,
                        int32_t* aNumFramesOnLine,
                        nsRect& aLineBounds,
                        uint32_t* aLineFlags)
{
  NS_ENSURE_ARG_POINTER(aFirstFrameOnLine);
  NS_ENSURE_ARG_POINTER(aNumFramesOnLine);
  NS_ENSURE_ARG_POINTER(aLineFlags);

  if ((aLineNumber < 0) || (aLineNumber >= mNumLines)) {
    *aFirstFrameOnLine = nullptr;
    *aNumFramesOnLine = 0;
    aLineBounds.SetRect(0, 0, 0, 0);
    return NS_OK;
  }
  nsLineBox* line = mLines[aLineNumber];
  *aFirstFrameOnLine = line->mFirstChild;
  *aNumFramesOnLine = line->GetChildCount();
  aLineBounds = line->mBounds;

  uint32_t flags = 0;
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

int32_t
nsLineIterator::FindLineContaining(nsIFrame* aFrame, int32_t aStartLine)
{
  NS_PRECONDITION(aStartLine <= mNumLines, "Bogus line numbers");
  int32_t lineNumber = aStartLine;
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
nsLineIterator::CheckLineOrder(int32_t                  aLine,
                               bool                     *aIsReordered,
                               nsIFrame                 **aFirstVisual,
                               nsIFrame                 **aLastVisual)
{
  NS_ASSERTION (aLine >= 0 && aLine < mNumLines, "aLine out of range!");
  nsLineBox* line = mLines[aLine];

  if (!line->mFirstChild) { // empty line
    *aIsReordered = false;
    *aFirstVisual = nullptr;
    *aLastVisual = nullptr;
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
nsLineIterator::FindFrameAt(int32_t aLineNumber,
                            nscoord aX,
                            nsIFrame** aFrameFound,
                            bool* aXIsBeforeFirstFrame,
                            bool* aXIsAfterLastFrame)
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
    *aFrameFound = nullptr;
    *aXIsBeforeFirstFrame = true;
    *aXIsAfterLastFrame = false;
    return NS_OK;
  }

  if (line->mBounds.width == 0 && line->mBounds.height == 0)
    return NS_ERROR_FAILURE;

  nsIFrame* frame = line->mFirstChild;
  nsIFrame* closestFromLeft = nullptr;
  nsIFrame* closestFromRight = nullptr;
  int32_t n = line->GetChildCount();
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
nsLineIterator::GetNextSiblingOnLine(nsIFrame*& aFrame, int32_t aLineNumber)
{
  aFrame = aFrame->GetNextSibling();
  return NS_OK;
}

//----------------------------------------------------------------------

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatCacheList::nsFloatCacheList() :
  mHead(nullptr)
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
  mHead = nullptr;
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
  aList.mHead = nullptr;
  aList.mTail = nullptr;
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
  nsFloatCache* prev = nullptr;
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
  return nullptr;
}

//----------------------------------------------------------------------

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatCacheFreeList::nsFloatCacheFreeList() :
  mTail(nullptr)
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
  aList.mHead = nullptr;
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
  mTail = nullptr;
}

nsFloatCache*
nsFloatCacheFreeList::Alloc(nsIFrame* aFloat)
{
  NS_PRECONDITION(aFloat->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
                  "This is a float cache, why isn't the frame out-of-flow?");
  nsFloatCache* fc = mHead;
  if (mHead) {
    if (mHead == mTail) {
      mHead = mTail = nullptr;
    }
    else {
      mHead = fc->mNext;
    }
    fc->mNext = nullptr;
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
  aFloat->mNext = nullptr;
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
  : mFloat(nullptr),
    mNext(nullptr)
{
  MOZ_COUNT_CTOR(nsFloatCache);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatCache::~nsFloatCache()
{
  MOZ_COUNT_DTOR(nsFloatCache);
}
#endif
