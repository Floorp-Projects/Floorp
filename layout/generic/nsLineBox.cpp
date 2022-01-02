/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of one line within a block frame, a CSS line box */

#include "nsLineBox.h"

#include "mozilla/ArenaObjectID.h"
#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"
#include "mozilla/PresShell.h"
#include "mozilla/Sprintf.h"
#include "mozilla/WritingModes.h"
#include "mozilla/ToString.h"
#include "nsBidiPresUtils.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsPresArena.h"
#include "nsPrintfCString.h"
#include "nsWindowSizes.h"

#ifdef DEBUG
static int32_t ctorCount;
int32_t nsLineBox::GetCtorCount() { return ctorCount; }
#endif

#ifndef _MSC_VER
// static nsLineBox constant; initialized in the header file.
const uint32_t nsLineBox::kMinChildCountForHashtable;
#endif

using namespace mozilla;

nsLineBox::nsLineBox(nsIFrame* aFrame, int32_t aCount, bool aIsBlock)
    : mFirstChild(aFrame),
      mWritingMode(),
      mContainerSize(-1, -1),
      mBounds(WritingMode()),  // mBounds will be initialized with the correct
                               // writing mode when it is set
      mFrames(),
      mAscent(),
      mAllFlags(0),
      mData(nullptr) {
  // Assert that the union elements chosen for initialisation are at
  // least as large as all other elements in their respective unions, so
  // as to ensure that no parts are missed.
  static_assert(sizeof(mFrames) >= sizeof(mChildCount), "nsLineBox init #1");
  static_assert(sizeof(mAllFlags) >= sizeof(mFlags), "nsLineBox init #2");
  static_assert(sizeof(mData) >= sizeof(mBlockData), "nsLineBox init #3");
  static_assert(sizeof(mData) >= sizeof(mInlineData), "nsLineBox init #4");

  MOZ_COUNT_CTOR(nsLineBox);
#ifdef DEBUG
  ++ctorCount;
  NS_ASSERTION(!aIsBlock || aCount == 1, "Blocks must have exactly one child");
  nsIFrame* f = aFrame;
  for (int32_t n = aCount; n > 0; f = f->GetNextSibling(), --n) {
    NS_ASSERTION(aIsBlock == f->IsBlockOutside(), "wrong kind of child frame");
  }
#endif
  static_assert(static_cast<int>(StyleClear::Max) <= 15,
                "FlagBits needs more bits to store the full range of "
                "break type ('clear') values");
  mChildCount = aCount;
  MarkDirty();
  mFlags.mBlock = aIsBlock;
}

nsLineBox::~nsLineBox() {
  MOZ_COUNT_DTOR(nsLineBox);
  if (MOZ_UNLIKELY(mFlags.mHasHashedFrames)) {
    delete mFrames;
  }
  Cleanup();
}

nsLineBox* NS_NewLineBox(PresShell* aPresShell, nsIFrame* aFrame,
                         bool aIsBlock) {
  return new (aPresShell) nsLineBox(aFrame, 1, aIsBlock);
}

nsLineBox* NS_NewLineBox(PresShell* aPresShell, nsLineBox* aFromLine,
                         nsIFrame* aFrame, int32_t aCount) {
  nsLineBox* newLine = new (aPresShell) nsLineBox(aFrame, aCount, false);
  newLine->NoteFramesMovedFrom(aFromLine);
  newLine->mContainerSize = aFromLine->mContainerSize;
  return newLine;
}

void nsLineBox::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  if (mFlags.mHasHashedFrames) {
    aSizes.mLayoutFramePropertiesSize +=
        mFrames->ShallowSizeOfIncludingThis(aSizes.mState.mMallocSizeOf);
  }
}

void nsLineBox::StealHashTableFrom(nsLineBox* aFromLine,
                                   uint32_t aFromLineNewCount) {
  MOZ_ASSERT(!mFlags.mHasHashedFrames);
  MOZ_ASSERT(GetChildCount() >= int32_t(aFromLineNewCount));
  mFrames = aFromLine->mFrames;
  mFlags.mHasHashedFrames = 1;
  aFromLine->mFlags.mHasHashedFrames = 0;
  aFromLine->mChildCount = aFromLineNewCount;
  // remove aFromLine's frames that aren't on this line
  nsIFrame* f = aFromLine->mFirstChild;
  for (uint32_t i = 0; i < aFromLineNewCount; f = f->GetNextSibling(), ++i) {
    mFrames->Remove(f);
  }
}

void nsLineBox::NoteFramesMovedFrom(nsLineBox* aFromLine) {
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
        aFromLine->mFrames->Remove(f);
      }
    } else if (toCount <= fromNewCount) {
      // This line needs a hash table, allocate a hash table for it since that
      // means fewer hash ops.
      nsIFrame* f = mFirstChild;
      for (uint32_t i = 0; i < toCount; f = f->GetNextSibling(), ++i) {
        aFromLine->mFrames->Remove(f);  // toCount RemoveEntry
      }
      SwitchToHashtable();  // toCount PutEntry
    } else {
      // This line needs a hash table, but it's fewer hash ops to steal
      // aFromLine's hash table and allocate a new hash table for that line.
      StealHashTableFrom(aFromLine, fromNewCount);  // fromNewCount RemoveEntry
      aFromLine->SwitchToHashtable();               // fromNewCount PutEntry
    }
  }
}

void* nsLineBox::operator new(size_t sz, PresShell* aPresShell) {
  return aPresShell->AllocateByObjectID(eArenaObjectID_nsLineBox, sz);
}

void nsLineBox::Destroy(PresShell* aPresShell) {
  this->nsLineBox::~nsLineBox();
  aPresShell->FreeByObjectID(eArenaObjectID_nsLineBox, this);
}

void nsLineBox::Cleanup() {
  if (mData) {
    if (IsBlock()) {
      delete mBlockData;
    } else {
      delete mInlineData;
    }
    mData = nullptr;
  }
}

#ifdef DEBUG_FRAME_DUMP
static void ListFloats(FILE* out, const char* aPrefix,
                       const nsFloatCacheList& aFloats) {
  nsFloatCache* fc = aFloats.Head();
  while (fc) {
    nsCString str(aPrefix);
    nsIFrame* frame = fc->mFloat;
    str += nsPrintfCString("floatframe@%p ", static_cast<void*>(frame));
    if (frame) {
      nsAutoString frameName;
      frame->GetFrameName(frameName);
      str += NS_ConvertUTF16toUTF8(frameName).get();
    } else {
      str += "\n###!!! NULL out-of-flow frame";
    }
    fprintf_stderr(out, "%s\n", str.get());
    fc = fc->Next();
  }
}

/* static */ const char* nsLineBox::BreakTypeToString(StyleClear aBreakType) {
  switch (aBreakType) {
    case StyleClear::None:
      return "nobr";
    case StyleClear::Left:
      return "leftbr";
    case StyleClear::Right:
      return "rightbr";
    case StyleClear::Both:
      return "leftbr+rightbr";
    case StyleClear::Line:
      return "linebr";
    case StyleClear::Max:
      return "leftbr+rightbr+linebr";
  }
  return "unknown";
}

char* nsLineBox::StateToString(char* aBuf, int32_t aBufSize) const {
  snprintf(aBuf, aBufSize, "%s,%s,%s,%s,%s,before:%s,after:%s[0x%x]",
           IsBlock() ? "block" : "inline", IsDirty() ? "dirty" : "clean",
           IsPreviousMarginDirty() ? "prevmargindirty" : "prevmarginclean",
           IsImpactedByFloat() ? "impacted" : "not-impacted",
           IsLineWrapped() ? "wrapped" : "not-wrapped",
           BreakTypeToString(GetBreakTypeBefore()),
           BreakTypeToString(GetBreakTypeAfter()), mAllFlags);
  return aBuf;
}

void nsLineBox::List(FILE* out, int32_t aIndent,
                     nsIFrame::ListFlags aFlags) const {
  nsCString str;
  while (aIndent-- > 0) {
    str += "  ";
  }
  List(out, str.get(), aFlags);
}

void nsLineBox::List(FILE* out, const char* aPrefix,
                     nsIFrame::ListFlags aFlags) const {
  nsCString str(aPrefix);
  char cbuf[100];
  str += nsPrintfCString("line@%p count=%d state=%s ",
                         static_cast<const void*>(this), GetChildCount(),
                         StateToString(cbuf, sizeof(cbuf)));
  if (IsBlock() && !GetCarriedOutBEndMargin().IsZero()) {
    const nscoord bm = GetCarriedOutBEndMargin().get();
    str += nsPrintfCString("bm=%s ",
                           nsIFrame::ConvertToString(bm, aFlags).c_str());
  }
  nsRect bounds = GetPhysicalBounds();
  str +=
      nsPrintfCString("%s ", nsIFrame::ConvertToString(bounds, aFlags).c_str());
  if (mWritingMode.IsVertical() || mWritingMode.IsBidiRTL()) {
    str += nsPrintfCString(
        "wm=%s cs=(%s) logical-rect=%s ", ToString(mWritingMode).c_str(),
        nsIFrame::ConvertToString(mContainerSize, aFlags).c_str(),
        nsIFrame::ConvertToString(mBounds, mWritingMode, aFlags).c_str());
  }
  if (mData) {
    const nsRect vo = mData->mOverflowAreas.InkOverflow();
    const nsRect so = mData->mOverflowAreas.ScrollableOverflow();
    if (!vo.IsEqualEdges(bounds) || !so.IsEqualEdges(bounds)) {
      str += nsPrintfCString("ink-overflow=%s scr-overflow=%s ",
                             nsIFrame::ConvertToString(vo, aFlags).c_str(),
                             nsIFrame::ConvertToString(so, aFlags).c_str());
    }
  }
  fprintf_stderr(out, "%s<\n", str.get());

  nsIFrame* frame = mFirstChild;
  int32_t n = GetChildCount();
  nsCString pfx(aPrefix);
  pfx += "  ";
  while (--n >= 0) {
    frame->List(out, pfx.get(), aFlags);
    frame = frame->GetNextSibling();
  }

  if (HasFloats()) {
    fprintf_stderr(out, "%s> floats <\n", aPrefix);
    ListFloats(out, pfx.get(), mInlineData->mFloats);
  }
  fprintf_stderr(out, "%s>\n", aPrefix);
}

nsIFrame* nsLineBox::LastChild() const {
  nsIFrame* frame = mFirstChild;
  int32_t n = GetChildCount() - 1;
  while (--n >= 0) {
    frame = frame->GetNextSibling();
  }
  return frame;
}
#endif

int32_t nsLineBox::IndexOf(nsIFrame* aFrame) const {
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

int32_t nsLineBox::RIndexOf(nsIFrame* aFrame,
                            nsIFrame* aLastFrameInLine) const {
  nsIFrame* frame = aLastFrameInLine;
  for (int32_t i = GetChildCount() - 1; i >= 0; --i) {
    MOZ_ASSERT(i != 0 || frame == mFirstChild,
               "caller provided incorrect last frame");
    if (frame == aFrame) {
      return i;
    }
    frame = frame->GetPrevSibling();
  }
  return -1;
}

bool nsLineBox::IsEmpty() const {
  if (IsBlock()) return mFirstChild->IsEmpty();

  int32_t n;
  nsIFrame* kid;
  for (n = GetChildCount(), kid = mFirstChild; n > 0;
       --n, kid = kid->GetNextSibling()) {
    if (!kid->IsEmpty()) return false;
  }
  if (HasMarker()) {
    return false;
  }
  return true;
}

bool nsLineBox::CachedIsEmpty() {
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
    nsIFrame* kid;
    result = true;
    for (n = GetChildCount(), kid = mFirstChild; n > 0;
         --n, kid = kid->GetNextSibling()) {
      if (!kid->CachedIsEmpty()) {
        result = false;
        break;
      }
    }
    if (HasMarker()) {
      result = false;
    }
  }

  mFlags.mEmptyCacheValid = true;
  mFlags.mEmptyCacheState = result;
  return result;
}

void nsLineBox::DeleteLineList(nsPresContext* aPresContext, nsLineList& aLines,
                               nsIFrame* aDestructRoot, nsFrameList* aFrames,
                               PostDestroyData& aPostDestroyData) {
  PresShell* presShell = aPresContext->PresShell();

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
      MOZ_DIAGNOSTIC_ASSERT(child->PresContext() == aPresContext);
      MOZ_DIAGNOSTIC_ASSERT(child == line->mFirstChild, "Lines out of sync");
      line->mFirstChild = aFrames->FirstChild();
      line->NoteFrameRemoved(child);
      child->DestroyFrom(aDestructRoot, aPostDestroyData);
    }
    MOZ_DIAGNOSTIC_ASSERT(line == aLines.front(),
                          "destroying child frames messed up our lines!");
    aLines.pop_front();
    line->Destroy(presShell);
  }
}

bool nsLineBox::RFindLineContaining(nsIFrame* aFrame,
                                    const nsLineList::iterator& aBegin,
                                    nsLineList::iterator& aEnd,
                                    nsIFrame* aLastFrameBeforeEnd,
                                    int32_t* aFrameIndexInLine) {
  MOZ_ASSERT(aFrame, "null ptr");

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

nsCollapsingMargin nsLineBox::GetCarriedOutBEndMargin() const {
  NS_ASSERTION(IsBlock(), "GetCarriedOutBEndMargin called on non-block line.");
  return (IsBlock() && mBlockData) ? mBlockData->mCarriedOutBEndMargin
                                   : nsCollapsingMargin();
}

bool nsLineBox::SetCarriedOutBEndMargin(nsCollapsingMargin aValue) {
  bool changed = false;
  if (IsBlock()) {
    if (!aValue.IsZero()) {
      if (!mBlockData) {
        mBlockData = new ExtraBlockData(GetPhysicalBounds());
      }
      changed = aValue != mBlockData->mCarriedOutBEndMargin;
      mBlockData->mCarriedOutBEndMargin = aValue;
    } else if (mBlockData) {
      changed = aValue != mBlockData->mCarriedOutBEndMargin;
      mBlockData->mCarriedOutBEndMargin = aValue;
      MaybeFreeData();
    }
  }
  return changed;
}

void nsLineBox::MaybeFreeData() {
  nsRect bounds = GetPhysicalBounds();
  if (mData && mData->mOverflowAreas == OverflowAreas(bounds, bounds)) {
    if (IsInline()) {
      if (mInlineData->mFloats.IsEmpty()) {
        delete mInlineData;
        mInlineData = nullptr;
      }
    } else if (mBlockData->mCarriedOutBEndMargin.IsZero()) {
      delete mBlockData;
      mBlockData = nullptr;
    }
  }
}

// XXX get rid of this???
nsFloatCache* nsLineBox::GetFirstFloat() {
  MOZ_ASSERT(IsInline(), "block line can't have floats");
  return mInlineData ? mInlineData->mFloats.Head() : nullptr;
}

// XXX this might be too eager to free memory
void nsLineBox::FreeFloats(nsFloatCacheFreeList& aFreeList) {
  MOZ_ASSERT(IsInline(), "block line can't have floats");
  if (IsInline() && mInlineData) {
    if (mInlineData->mFloats.NotEmpty()) {
      aFreeList.Append(mInlineData->mFloats);
    }
    MaybeFreeData();
  }
}

void nsLineBox::AppendFloats(nsFloatCacheFreeList& aFreeList) {
  MOZ_ASSERT(IsInline(), "block line can't have floats");
  if (IsInline()) {
    if (aFreeList.NotEmpty()) {
      if (!mInlineData) {
        mInlineData = new ExtraInlineData(GetPhysicalBounds());
      }
      mInlineData->mFloats.Append(aFreeList);
    }
  }
}

bool nsLineBox::RemoveFloat(nsIFrame* aFrame) {
  MOZ_ASSERT(IsInline(), "block line can't have floats");
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

void nsLineBox::SetFloatEdges(nscoord aStart, nscoord aEnd) {
  MOZ_ASSERT(IsInline(), "block line can't have float edges");
  if (!mInlineData) {
    mInlineData = new ExtraInlineData(GetPhysicalBounds());
  }
  mInlineData->mFloatEdgeIStart = aStart;
  mInlineData->mFloatEdgeIEnd = aEnd;
}

void nsLineBox::ClearFloatEdges() {
  MOZ_ASSERT(IsInline(), "block line can't have float edges");
  if (mInlineData) {
    mInlineData->mFloatEdgeIStart = nscoord_MIN;
    mInlineData->mFloatEdgeIEnd = nscoord_MIN;
  }
}

void nsLineBox::SetOverflowAreas(const OverflowAreas& aOverflowAreas) {
#ifdef DEBUG
  for (const auto otype : mozilla::AllOverflowTypes()) {
    NS_ASSERTION(aOverflowAreas.Overflow(otype).width >= 0,
                 "Illegal width for an overflow area!");
    NS_ASSERTION(aOverflowAreas.Overflow(otype).height >= 0,
                 "Illegal height for an overflow area!");
  }
#endif

  nsRect bounds = GetPhysicalBounds();
  if (!aOverflowAreas.InkOverflow().IsEqualInterior(bounds) ||
      !aOverflowAreas.ScrollableOverflow().IsEqualEdges(bounds)) {
    if (!mData) {
      if (IsInline()) {
        mInlineData = new ExtraInlineData(bounds);
      } else {
        mBlockData = new ExtraBlockData(bounds);
      }
    }
    mData->mOverflowAreas = aOverflowAreas;
  } else if (mData) {
    // Store away new value so that MaybeFreeData compares against
    // the right value.
    mData->mOverflowAreas = aOverflowAreas;
    MaybeFreeData();
  }
}

//----------------------------------------------------------------------

Result<nsILineIterator::LineInfo, nsresult> nsLineIterator::GetLine(
    int32_t aLineNumber) {
  const nsLineBox* line = GetLineAt(aLineNumber);
  if (!line) {
    return Err(NS_ERROR_FAILURE);
  }
  LineInfo structure;
  structure.mFirstFrameOnLine = line->mFirstChild;
  structure.mNumFramesOnLine = line->GetChildCount();
  structure.mLineBounds = line->GetPhysicalBounds();
  structure.mIsWrapped = line->IsLineWrapped();
  return structure;
}

int32_t nsLineIterator::FindLineContaining(nsIFrame* aFrame,
                                           int32_t aStartLine) {
  const nsLineBox* line = GetLineAt(aStartLine);
  MOZ_ASSERT(line, "aStartLine out of range");
  while (line) {
    if (line->Contains(aFrame)) {
      return mIndex;
    }
    line = GetNextLine();
  }
  return -1;
}

NS_IMETHODIMP
nsLineIterator::CheckLineOrder(int32_t aLine, bool* aIsReordered,
                               nsIFrame** aFirstVisual,
                               nsIFrame** aLastVisual) {
  const nsLineBox* line = GetLineAt(aLine);
  MOZ_ASSERT(line, "aLine out of range!");

  if (!line || !line->mFirstChild) {  // empty line
    *aIsReordered = false;
    *aFirstVisual = nullptr;
    *aLastVisual = nullptr;
    return NS_OK;
  }

  nsIFrame* leftmostFrame;
  nsIFrame* rightmostFrame;
  *aIsReordered =
      nsBidiPresUtils::CheckLineOrder(line->mFirstChild, line->GetChildCount(),
                                      &leftmostFrame, &rightmostFrame);

  // map leftmost/rightmost to first/last according to paragraph direction
  *aFirstVisual = mRightToLeft ? rightmostFrame : leftmostFrame;
  *aLastVisual = mRightToLeft ? leftmostFrame : rightmostFrame;

  return NS_OK;
}

NS_IMETHODIMP
nsLineIterator::FindFrameAt(int32_t aLineNumber, nsPoint aPos,
                            nsIFrame** aFrameFound,
                            bool* aPosIsBeforeFirstFrame,
                            bool* aPosIsAfterLastFrame) {
  MOZ_ASSERT(aFrameFound && aPosIsBeforeFirstFrame && aPosIsAfterLastFrame,
             "null OUT ptr");

  if (!aFrameFound || !aPosIsBeforeFirstFrame || !aPosIsAfterLastFrame) {
    return NS_ERROR_NULL_POINTER;
  }

  const nsLineBox* line = GetLineAt(aLineNumber);
  if (!line) {
    *aFrameFound = nullptr;
    *aPosIsBeforeFirstFrame = true;
    *aPosIsAfterLastFrame = false;
    return NS_OK;
  }

  if (line->ISize() == 0 && line->BSize() == 0) return NS_ERROR_FAILURE;

  nsIFrame* frame = line->mFirstChild;
  nsIFrame* closestFromStart = nullptr;
  nsIFrame* closestFromEnd = nullptr;

  WritingMode wm = line->mWritingMode;
  nsSize containerSize = line->mContainerSize;

  LogicalPoint pos(wm, aPos, containerSize);

  int32_t n = line->GetChildCount();
  while (n--) {
    LogicalRect rect = frame->GetLogicalRect(wm, containerSize);
    if (rect.ISize(wm) > 0) {
      // If pos.I() is inside this frame - this is it
      if (rect.IStart(wm) <= pos.I(wm) && rect.IEnd(wm) > pos.I(wm)) {
        closestFromStart = closestFromEnd = frame;
        break;
      }
      if (rect.IStart(wm) < pos.I(wm)) {
        if (!closestFromStart ||
            rect.IEnd(wm) >
                closestFromStart->GetLogicalRect(wm, containerSize).IEnd(wm))
          closestFromStart = frame;
      } else {
        if (!closestFromEnd ||
            rect.IStart(wm) <
                closestFromEnd->GetLogicalRect(wm, containerSize).IStart(wm))
          closestFromEnd = frame;
      }
    }
    frame = frame->GetNextSibling();
  }
  if (!closestFromStart && !closestFromEnd) {
    // All frames were zero-width. Just take the first one.
    closestFromStart = closestFromEnd = line->mFirstChild;
  }
  *aPosIsBeforeFirstFrame = mRightToLeft ? !closestFromEnd : !closestFromStart;
  *aPosIsAfterLastFrame = mRightToLeft ? !closestFromStart : !closestFromEnd;
  if (closestFromStart == closestFromEnd) {
    *aFrameFound = closestFromStart;
  } else if (!closestFromStart) {
    *aFrameFound = closestFromEnd;
  } else if (!closestFromEnd) {
    *aFrameFound = closestFromStart;
  } else {  // we're between two frames
    nscoord delta =
        closestFromEnd->GetLogicalRect(wm, containerSize).IStart(wm) -
        closestFromStart->GetLogicalRect(wm, containerSize).IEnd(wm);
    if (pos.I(wm) <
        closestFromStart->GetLogicalRect(wm, containerSize).IEnd(wm) +
            delta / 2) {
      *aFrameFound = closestFromStart;
    } else {
      *aFrameFound = closestFromEnd;
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatCacheList::nsFloatCacheList() : mHead(nullptr) {
  MOZ_COUNT_CTOR(nsFloatCacheList);
}
#endif

nsFloatCacheList::~nsFloatCacheList() {
  DeleteAll();
  MOZ_COUNT_DTOR(nsFloatCacheList);
}

void nsFloatCacheList::DeleteAll() {
  nsFloatCache* c = mHead;
  while (c) {
    nsFloatCache* next = c->Next();
    delete c;
    c = next;
  }
  mHead = nullptr;
}

nsFloatCache* nsFloatCacheList::Tail() const {
  nsFloatCache* fc = mHead;
  while (fc) {
    if (!fc->mNext) {
      break;
    }
    fc = fc->mNext;
  }
  return fc;
}

void nsFloatCacheList::Append(nsFloatCacheFreeList& aList) {
  MOZ_ASSERT(aList.NotEmpty(), "Appending empty list will fail");

  nsFloatCache* tail = Tail();
  if (tail) {
    NS_ASSERTION(!tail->mNext, "Bogus!");
    tail->mNext = aList.mHead;
  } else {
    NS_ASSERTION(!mHead, "Bogus!");
    mHead = aList.mHead;
  }
  aList.mHead = nullptr;
  aList.mTail = nullptr;
}

nsFloatCache* nsFloatCacheList::Find(nsIFrame* aOutOfFlowFrame) {
  nsFloatCache* fc = mHead;
  while (fc) {
    if (fc->mFloat == aOutOfFlowFrame) {
      break;
    }
    fc = fc->Next();
  }
  return fc;
}

nsFloatCache* nsFloatCacheList::RemoveAndReturnPrev(nsFloatCache* aElement) {
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
nsFloatCacheFreeList::nsFloatCacheFreeList() : mTail(nullptr) {
  MOZ_COUNT_CTOR(nsFloatCacheFreeList);
}

nsFloatCacheFreeList::~nsFloatCacheFreeList() {
  MOZ_COUNT_DTOR(nsFloatCacheFreeList);
}
#endif

void nsFloatCacheFreeList::Append(nsFloatCacheList& aList) {
  MOZ_ASSERT(aList.NotEmpty(), "Appending empty list will fail");

  if (mTail) {
    NS_ASSERTION(!mTail->mNext, "Bogus");
    mTail->mNext = aList.mHead;
  } else {
    NS_ASSERTION(!mHead, "Bogus");
    mHead = aList.mHead;
  }
  mTail = aList.Tail();
  aList.mHead = nullptr;
}

void nsFloatCacheFreeList::Remove(nsFloatCache* aElement) {
  nsFloatCache* prev = nsFloatCacheList::RemoveAndReturnPrev(aElement);
  if (mTail == aElement) {
    mTail = prev;
  }
}

void nsFloatCacheFreeList::DeleteAll() {
  nsFloatCacheList::DeleteAll();
  mTail = nullptr;
}

nsFloatCache* nsFloatCacheFreeList::Alloc(nsIFrame* aFloat) {
  MOZ_ASSERT(aFloat->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW),
             "This is a float cache, why isn't the frame out-of-flow?");

  nsFloatCache* fc = mHead;
  if (mHead) {
    if (mHead == mTail) {
      mHead = mTail = nullptr;
    } else {
      mHead = fc->mNext;
    }
    fc->mNext = nullptr;
  } else {
    fc = new nsFloatCache();
  }
  fc->mFloat = aFloat;
  return fc;
}

void nsFloatCacheFreeList::Append(nsFloatCache* aFloat) {
  NS_ASSERTION(!aFloat->mNext, "Bogus!");
  aFloat->mNext = nullptr;
  if (mTail) {
    NS_ASSERTION(!mTail->mNext, "Bogus!");
    mTail->mNext = aFloat;
    mTail = aFloat;
  } else {
    NS_ASSERTION(!mHead, "Bogus!");
    mHead = mTail = aFloat;
  }
}

//----------------------------------------------------------------------

nsFloatCache::nsFloatCache() : mFloat(nullptr), mNext(nullptr) {
  MOZ_COUNT_CTOR(nsFloatCache);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatCache::~nsFloatCache() { MOZ_COUNT_DTOR(nsFloatCache); }
#endif
