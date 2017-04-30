/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFrameList.h"

#include "mozilla/ArenaObjectID.h"
#include "nsBidiPresUtils.h"
#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "nsILineIterator.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"

using namespace mozilla;

namespace mozilla {
namespace layout {
namespace detail {
const AlignedFrameListBytes gEmptyFrameListBytes = { 0 };
} // namespace detail
} // namespace layout
} // namespace mozilla

void*
nsFrameList::operator new(size_t sz, nsIPresShell* aPresShell)
{
  return aPresShell->AllocateByObjectID(eArenaObjectID_nsFrameList, sz);
}

void
nsFrameList::Delete(nsIPresShell* aPresShell)
{
  NS_PRECONDITION(this != &EmptyList(), "Shouldn't Delete() this list");
  NS_ASSERTION(IsEmpty(), "Shouldn't Delete() a non-empty list");

  aPresShell->FreeByObjectID(eArenaObjectID_nsFrameList, this);
}

void
nsFrameList::DestroyFrames()
{
  while (nsIFrame* frame = RemoveFirstChild()) {
    frame->Destroy();
  }
  mLastChild = nullptr;
}

void
nsFrameList::DestroyFramesFrom(nsIFrame* aDestructRoot)
{
  NS_PRECONDITION(aDestructRoot, "Missing destruct root");

  while (nsIFrame* frame = RemoveFirstChild()) {
    frame->DestroyFrom(aDestructRoot);
  }
  mLastChild = nullptr;
}

void
nsFrameList::SetFrames(nsIFrame* aFrameList)
{
  NS_PRECONDITION(!mFirstChild, "Losing frames");

  mFirstChild = aFrameList;
  mLastChild = nsLayoutUtils::GetLastSibling(mFirstChild);
}

void
nsFrameList::RemoveFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");
#ifdef DEBUG_FRAME_LIST
  // ContainsFrame is O(N)
  NS_PRECONDITION(ContainsFrame(aFrame), "wrong list");
#endif

  nsIFrame* nextFrame = aFrame->GetNextSibling();
  if (aFrame == mFirstChild) {
    mFirstChild = nextFrame;
    aFrame->SetNextSibling(nullptr);
    if (!nextFrame) {
      mLastChild = nullptr;
    }
  }
  else {
    nsIFrame* prevSibling = aFrame->GetPrevSibling();
    NS_ASSERTION(prevSibling && prevSibling->GetNextSibling() == aFrame,
                 "Broken frame linkage");
    prevSibling->SetNextSibling(nextFrame);
    aFrame->SetNextSibling(nullptr);
    if (!nextFrame) {
      mLastChild = prevSibling;
    }
  }
}

nsFrameList
nsFrameList::RemoveFramesAfter(nsIFrame* aAfterFrame)
{
  if (!aAfterFrame) {
    nsFrameList result;
    result.InsertFrames(nullptr, nullptr, *this);
    return result;
  }

  NS_PRECONDITION(NotEmpty(), "illegal operation on empty list");
#ifdef DEBUG_FRAME_LIST
  NS_PRECONDITION(ContainsFrame(aAfterFrame), "wrong list");
#endif

  nsIFrame* tail = aAfterFrame->GetNextSibling();
  // if (!tail) return EmptyList();  -- worth optimizing this case?
  nsIFrame* oldLastChild = mLastChild;
  mLastChild = aAfterFrame;
  aAfterFrame->SetNextSibling(nullptr);
  return nsFrameList(tail, tail ? oldLastChild : nullptr);
}

nsIFrame*
nsFrameList::RemoveFirstChild()
{
  if (mFirstChild) {
    nsIFrame* firstChild = mFirstChild;
    RemoveFrame(firstChild);
    return firstChild;
  }
  return nullptr;
}

void
nsFrameList::DestroyFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");
  RemoveFrame(aFrame);
  aFrame->Destroy();
}

nsFrameList::Slice
nsFrameList::InsertFrames(nsContainerFrame* aParent, nsIFrame* aPrevSibling,
                          nsFrameList& aFrameList)
{
  NS_PRECONDITION(aFrameList.NotEmpty(), "Unexpected empty list");

  if (aParent) {
    aFrameList.ApplySetParent(aParent);
  }

  NS_ASSERTION(IsEmpty() ||
               FirstChild()->GetParent() == aFrameList.FirstChild()->GetParent(),
               "frame to add has different parent");
  NS_ASSERTION(!aPrevSibling ||
               aPrevSibling->GetParent() == aFrameList.FirstChild()->GetParent(),
               "prev sibling has different parent");
#ifdef DEBUG_FRAME_LIST
  // ContainsFrame is O(N)
  NS_ASSERTION(!aPrevSibling || ContainsFrame(aPrevSibling),
               "prev sibling is not on this list");
#endif

  nsIFrame* firstNewFrame = aFrameList.FirstChild();
  nsIFrame* nextSibling;
  if (aPrevSibling) {
    nextSibling = aPrevSibling->GetNextSibling();
    aPrevSibling->SetNextSibling(firstNewFrame);
  }
  else {
    nextSibling = mFirstChild;
    mFirstChild = firstNewFrame;
  }

  nsIFrame* lastNewFrame = aFrameList.LastChild();
  lastNewFrame->SetNextSibling(nextSibling);
  if (!nextSibling) {
    mLastChild = lastNewFrame;
  }

  VerifyList();

  aFrameList.Clear();
  return Slice(*this, firstNewFrame, nextSibling);
}

nsFrameList
nsFrameList::ExtractHead(FrameLinkEnumerator& aLink)
{
  NS_PRECONDITION(&aLink.List() == this, "Unexpected list");
  NS_PRECONDITION(!aLink.PrevFrame() ||
                  aLink.PrevFrame()->GetNextSibling() ==
                    aLink.NextFrame(),
                  "Unexpected PrevFrame()");
  NS_PRECONDITION(aLink.PrevFrame() ||
                  aLink.NextFrame() == FirstChild(),
                  "Unexpected NextFrame()");
  NS_PRECONDITION(!aLink.PrevFrame() ||
                  aLink.NextFrame() != FirstChild(),
                  "Unexpected NextFrame()");
  NS_PRECONDITION(aLink.mEnd == nullptr,
                  "Unexpected mEnd for frame link enumerator");

  nsIFrame* prev = aLink.PrevFrame();
  nsIFrame* newFirstFrame = nullptr;
  if (prev) {
    // Truncate the list after |prev| and hand the first part to our new list.
    prev->SetNextSibling(nullptr);
    newFirstFrame = mFirstChild;
    mFirstChild = aLink.NextFrame();
    if (!mFirstChild) { // we handed over the whole list
      mLastChild = nullptr;
    }

    // Now make sure aLink doesn't point to a frame we no longer have.
    aLink.mPrev = nullptr;
  }
  // else aLink is pointing to before our first frame.  Nothing to do.

  return nsFrameList(newFirstFrame, prev);
}

nsFrameList
nsFrameList::ExtractTail(FrameLinkEnumerator& aLink)
{
  NS_PRECONDITION(&aLink.List() == this, "Unexpected list");
  NS_PRECONDITION(!aLink.PrevFrame() ||
                  aLink.PrevFrame()->GetNextSibling() ==
                    aLink.NextFrame(),
                  "Unexpected PrevFrame()");
  NS_PRECONDITION(aLink.PrevFrame() ||
                  aLink.NextFrame() == FirstChild(),
                  "Unexpected NextFrame()");
  NS_PRECONDITION(!aLink.PrevFrame() ||
                  aLink.NextFrame() != FirstChild(),
                  "Unexpected NextFrame()");
  NS_PRECONDITION(aLink.mEnd == nullptr,
                  "Unexpected mEnd for frame link enumerator");

  nsIFrame* prev = aLink.PrevFrame();
  nsIFrame* newFirstFrame;
  nsIFrame* newLastFrame;
  if (prev) {
    // Truncate the list after |prev| and hand the second part to our new list
    prev->SetNextSibling(nullptr);
    newFirstFrame = aLink.NextFrame();
    newLastFrame = newFirstFrame ? mLastChild : nullptr;
    mLastChild = prev;
  } else {
    // Hand the whole list over to our new list
    newFirstFrame = mFirstChild;
    newLastFrame = mLastChild;
    Clear();
  }

  // Now make sure aLink doesn't point to a frame we no longer have.
  aLink.mFrame = nullptr;

  NS_POSTCONDITION(aLink.AtEnd(), "What's going on here?");

  return nsFrameList(newFirstFrame, newLastFrame);
}

nsIFrame*
nsFrameList::FrameAt(int32_t aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid arg");
  if (aIndex < 0) return nullptr;
  nsIFrame* frame = mFirstChild;
  while ((aIndex-- > 0) && frame) {
    frame = frame->GetNextSibling();
  }
  return frame;
}

int32_t
nsFrameList::IndexOf(nsIFrame* aFrame) const
{
  int32_t count = 0;
  for (nsIFrame* f = mFirstChild; f; f = f->GetNextSibling()) {
    if (f == aFrame)
      return count;
    ++count;
  }
  return -1;
}

bool
nsFrameList::ContainsFrame(const nsIFrame* aFrame) const
{
  NS_PRECONDITION(aFrame, "null ptr");

  nsIFrame* frame = mFirstChild;
  while (frame) {
    if (frame == aFrame) {
      return true;
    }
    frame = frame->GetNextSibling();
  }
  return false;
}

int32_t
nsFrameList::GetLength() const
{
  int32_t count = 0;
  nsIFrame* frame = mFirstChild;
  while (frame) {
    count++;
    frame = frame->GetNextSibling();
  }
  return count;
}

void
nsFrameList::ApplySetParent(nsContainerFrame* aParent) const
{
  NS_ASSERTION(aParent, "null ptr");

  for (nsIFrame* f = FirstChild(); f; f = f->GetNextSibling()) {
    f->SetParent(aParent);
  }
}

/* static */ void
nsFrameList::UnhookFrameFromSiblings(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame->GetPrevSibling() && aFrame->GetNextSibling());
  nsIFrame* const nextSibling = aFrame->GetNextSibling();
  nsIFrame* const prevSibling = aFrame->GetPrevSibling();
  aFrame->SetNextSibling(nullptr);
  prevSibling->SetNextSibling(nextSibling);
  MOZ_ASSERT(!aFrame->GetPrevSibling() && !aFrame->GetNextSibling());
}

#ifdef DEBUG_FRAME_DUMP
void
nsFrameList::List(FILE* out) const
{
  fprintf_stderr(out, "<\n");
  for (nsIFrame* frame = mFirstChild; frame;
       frame = frame->GetNextSibling()) {
    frame->List(out, "  ");
  }
  fprintf_stderr(out, ">\n");
}
#endif

nsIFrame*
nsFrameList::GetPrevVisualFor(nsIFrame* aFrame) const
{
  if (!mFirstChild)
    return nullptr;

  nsIFrame* parent = mFirstChild->GetParent();
  if (!parent)
    return aFrame ? aFrame->GetPrevSibling() : LastChild();

  nsBidiDirection paraDir = nsBidiPresUtils::ParagraphDirection(mFirstChild);

  nsAutoLineIterator iter = parent->GetLineIterator();
  if (!iter) {
    // Parent is not a block Frame
    if (parent->IsLineFrame()) {
      // Line frames are not bidi-splittable, so need to consider bidi reordering
      if (paraDir == NSBIDI_LTR) {
        return nsBidiPresUtils::GetFrameToLeftOf(aFrame, mFirstChild, -1);
      } else { // RTL
        return nsBidiPresUtils::GetFrameToRightOf(aFrame, mFirstChild, -1);
      }
    } else {
      // Just get the next or prev sibling, depending on block and frame direction.
      if (nsBidiPresUtils::IsFrameInParagraphDirection(mFirstChild)) {
        return aFrame ? aFrame->GetPrevSibling() : LastChild();
      } else {
        return aFrame ? aFrame->GetNextSibling() : mFirstChild;
      }
    }
  }

  // Parent is a block frame, so use the LineIterator to find the previous visual 
  // sibling on this line, or the last one on the previous line.

  int32_t thisLine;
  if (aFrame) {
    thisLine = iter->FindLineContaining(aFrame);
    if (thisLine < 0)
      return nullptr;
  } else {
    thisLine = iter->GetNumLines();
  }

  nsIFrame* frame = nullptr;
  nsIFrame* firstFrameOnLine;
  int32_t numFramesOnLine;
  nsRect lineBounds;

  if (aFrame) {
    iter->GetLine(thisLine, &firstFrameOnLine, &numFramesOnLine, lineBounds);

    if (paraDir == NSBIDI_LTR) {
      frame = nsBidiPresUtils::GetFrameToLeftOf(aFrame, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = nsBidiPresUtils::GetFrameToRightOf(aFrame, firstFrameOnLine, numFramesOnLine);
    }
  }

  if (!frame && thisLine > 0) {
    // Get the last frame of the previous line
    iter->GetLine(thisLine - 1, &firstFrameOnLine, &numFramesOnLine, lineBounds);

    if (paraDir == NSBIDI_LTR) {
      frame = nsBidiPresUtils::GetFrameToLeftOf(nullptr, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = nsBidiPresUtils::GetFrameToRightOf(nullptr, firstFrameOnLine, numFramesOnLine);
    }
  }
  return frame;
}

nsIFrame*
nsFrameList::GetNextVisualFor(nsIFrame* aFrame) const
{
  if (!mFirstChild)
    return nullptr;
  
  nsIFrame* parent = mFirstChild->GetParent();
  if (!parent)
    return aFrame ? aFrame->GetPrevSibling() : mFirstChild;

  nsBidiDirection paraDir = nsBidiPresUtils::ParagraphDirection(mFirstChild);

  nsAutoLineIterator iter = parent->GetLineIterator();
  if (!iter) { 
    // Parent is not a block Frame
    if (parent->IsLineFrame()) {
      // Line frames are not bidi-splittable, so need to consider bidi reordering
      if (paraDir == NSBIDI_LTR) {
        return nsBidiPresUtils::GetFrameToRightOf(aFrame, mFirstChild, -1);
      } else { // RTL
        return nsBidiPresUtils::GetFrameToLeftOf(aFrame, mFirstChild, -1);
      }
    } else {
      // Just get the next or prev sibling, depending on block and frame direction.
      if (nsBidiPresUtils::IsFrameInParagraphDirection(mFirstChild)) {
        return aFrame ? aFrame->GetNextSibling() : mFirstChild;
      } else {
        return aFrame ? aFrame->GetPrevSibling() : LastChild();
      }
    }
  }

  // Parent is a block frame, so use the LineIterator to find the next visual 
  // sibling on this line, or the first one on the next line.
  
  int32_t thisLine;
  if (aFrame) {
    thisLine = iter->FindLineContaining(aFrame);
    if (thisLine < 0)
      return nullptr;
  } else {
    thisLine = -1;
  }

  nsIFrame* frame = nullptr;
  nsIFrame* firstFrameOnLine;
  int32_t numFramesOnLine;
  nsRect lineBounds;

  if (aFrame) {
    iter->GetLine(thisLine, &firstFrameOnLine, &numFramesOnLine, lineBounds);

    if (paraDir == NSBIDI_LTR) {
      frame = nsBidiPresUtils::GetFrameToRightOf(aFrame, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = nsBidiPresUtils::GetFrameToLeftOf(aFrame, firstFrameOnLine, numFramesOnLine);
    }
  }

  int32_t numLines = iter->GetNumLines();
  if (!frame && thisLine < numLines - 1) {
    // Get the first frame of the next line
    iter->GetLine(thisLine + 1, &firstFrameOnLine, &numFramesOnLine, lineBounds);

    if (paraDir == NSBIDI_LTR) {
      frame = nsBidiPresUtils::GetFrameToRightOf(nullptr, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = nsBidiPresUtils::GetFrameToLeftOf(nullptr, firstFrameOnLine, numFramesOnLine);
    }
  }
  return frame;
}

#ifdef DEBUG_FRAME_LIST
void
nsFrameList::VerifyList() const
{
  NS_ASSERTION((mFirstChild == nullptr) == (mLastChild == nullptr),
               "bad list state");

  if (IsEmpty()) {
    return;
  }

  // Simple algorithm to find a loop in a linked list -- advance pointers
  // through it at speeds of 1 and 2, and if they ever get to be equal bail
  NS_ASSERTION(!mFirstChild->GetPrevSibling(), "bad prev sibling pointer");
  nsIFrame *first = mFirstChild, *second = mFirstChild;
  for (;;) {
    first = first->GetNextSibling();
    second = second->GetNextSibling();
    if (!second) {
      break;
    }
    NS_ASSERTION(second->GetPrevSibling()->GetNextSibling() == second,
                 "bad prev sibling pointer");
    second = second->GetNextSibling();
    if (first == second) {
      // Loop detected!  Since second advances faster, they can't both be null;
      // we would have broken out of the loop long ago.
      NS_ERROR("loop in frame list.  This will probably hang soon.");
      return;
    }                           
    if (!second) {
      break;
    }
    NS_ASSERTION(second->GetPrevSibling()->GetNextSibling() == second,
                 "bad prev sibling pointer");
  }

  NS_ASSERTION(mLastChild == nsLayoutUtils::GetLastSibling(mFirstChild),
               "bogus mLastChild");
  // XXX we should also assert that all GetParent() are either null or
  // the same non-null value, but nsCSSFrameConstructor::nsFrameItems
  // prevents that, e.g. table captions.
}
#endif

namespace mozilla {
namespace layout {

AutoFrameListPtr::~AutoFrameListPtr()
{
  if (mFrameList) {
    mFrameList->Delete(mPresContext->PresShell());
  }
}

} // namespace layout
} // namespace mozilla
