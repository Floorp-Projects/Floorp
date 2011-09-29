/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsFrameList.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"

#ifdef IBMBIDI
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsILineIterator.h"
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

const nsFrameList* nsFrameList::sEmptyList;

/* static */
void
nsFrameList::Init()
{
  NS_PRECONDITION(!sEmptyList, "Shouldn't be allocated");

  sEmptyList = new nsFrameList();
}

void
nsFrameList::Destroy()
{
  NS_PRECONDITION(this != sEmptyList, "Shouldn't Destroy() sEmptyList");

  DestroyFrames();
  delete this;
}

void
nsFrameList::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_PRECONDITION(this != sEmptyList, "Shouldn't Destroy() sEmptyList");

  DestroyFramesFrom(aDestructRoot);
  delete this;
}

void
nsFrameList::DestroyFrames()
{
  while (nsIFrame* frame = RemoveFirstChild()) {
    frame->Destroy();
  }
  mLastChild = nsnull;
}

void
nsFrameList::DestroyFramesFrom(nsIFrame* aDestructRoot)
{
  NS_PRECONDITION(aDestructRoot, "Missing destruct root");

  while (nsIFrame* frame = RemoveFirstChild()) {
    frame->DestroyFrom(aDestructRoot);
  }
  mLastChild = nsnull;
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
    aFrame->SetNextSibling(nsnull);
    if (!nextFrame) {
      mLastChild = nsnull;
    }
  }
  else {
    nsIFrame* prevSibling = aFrame->GetPrevSibling();
    NS_ASSERTION(prevSibling && prevSibling->GetNextSibling() == aFrame,
                 "Broken frame linkage");
    prevSibling->SetNextSibling(nextFrame);
    aFrame->SetNextSibling(nsnull);
    if (!nextFrame) {
      mLastChild = prevSibling;
    }
  }
}

bool
nsFrameList::RemoveFrameIfPresent(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");

  for (Enumerator e(*this); !e.AtEnd(); e.Next()) {
    if (e.get() == aFrame) {
      RemoveFrame(aFrame);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsFrameList
nsFrameList::RemoveFramesAfter(nsIFrame* aAfterFrame)
{
  if (!aAfterFrame) {
    nsFrameList result;
    result.InsertFrames(nsnull, nsnull, *this);
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
  aAfterFrame->SetNextSibling(nsnull);
  return nsFrameList(tail, tail ? oldLastChild : nsnull);
}

nsIFrame*
nsFrameList::RemoveFirstChild()
{
  if (mFirstChild) {
    nsIFrame* firstChild = mFirstChild;
    RemoveFrame(firstChild);
    return firstChild;
  }
  return nsnull;
}

void
nsFrameList::DestroyFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");
  RemoveFrame(aFrame);
  aFrame->Destroy();
}

bool
nsFrameList::DestroyFrameIfPresent(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");

  if (RemoveFrameIfPresent(aFrame)) {
    aFrame->Destroy();
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsFrameList::Slice
nsFrameList::InsertFrames(nsIFrame* aParent, nsIFrame* aPrevSibling,
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
  NS_PRECONDITION(aLink.mEnd == nsnull,
                  "Unexpected mEnd for frame link enumerator");

  nsIFrame* prev = aLink.PrevFrame();
  nsIFrame* newFirstFrame = nsnull;
  if (prev) {
    // Truncate the list after |prev| and hand the first part to our new list.
    prev->SetNextSibling(nsnull);
    newFirstFrame = mFirstChild;
    mFirstChild = aLink.NextFrame();
    if (!mFirstChild) { // we handed over the whole list
      mLastChild = nsnull;
    }

    // Now make sure aLink doesn't point to a frame we no longer have.
    aLink.mPrev = nsnull;
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
  NS_PRECONDITION(aLink.mEnd == nsnull,
                  "Unexpected mEnd for frame link enumerator");

  nsIFrame* prev = aLink.PrevFrame();
  nsIFrame* newFirstFrame;
  nsIFrame* newLastFrame;
  if (prev) {
    // Truncate the list after |prev| and hand the second part to our new list
    prev->SetNextSibling(nsnull);
    newFirstFrame = aLink.NextFrame();
    newLastFrame = newFirstFrame ? mLastChild : nsnull;
    mLastChild = prev;
  } else {
    // Hand the whole list over to our new list
    newFirstFrame = mFirstChild;
    newLastFrame = mLastChild;
    Clear();
  }

  // Now make sure aLink doesn't point to a frame we no longer have.
  aLink.mFrame = nsnull;

  NS_POSTCONDITION(aLink.AtEnd(), "What's going on here?");

  return nsFrameList(newFirstFrame, newLastFrame);
}

nsIFrame*
nsFrameList::FrameAt(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid arg");
  if (aIndex < 0) return nsnull;
  nsIFrame* frame = mFirstChild;
  while ((aIndex-- > 0) && frame) {
    frame = frame->GetNextSibling();
  }
  return frame;
}

PRInt32
nsFrameList::IndexOf(nsIFrame* aFrame) const
{
  PRInt32 count = 0;
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
      return PR_TRUE;
    }
    frame = frame->GetNextSibling();
  }
  return PR_FALSE;
}

PRInt32
nsFrameList::GetLength() const
{
  PRInt32 count = 0;
  nsIFrame* frame = mFirstChild;
  while (frame) {
    count++;
    frame = frame->GetNextSibling();
  }
  return count;
}

static int CompareByContentOrder(const nsIFrame* aF1, const nsIFrame* aF2)
{
  if (aF1->GetContent() != aF2->GetContent()) {
    return nsLayoutUtils::CompareTreePosition(aF1->GetContent(), aF2->GetContent());
  }

  if (aF1 == aF2) {
    return 0;
  }

  const nsIFrame* f;
  for (f = aF2; f; f = f->GetPrevInFlow()) {
    if (f == aF1) {
      // f1 comes before f2 in the flow
      return -1;
    }
  }
  for (f = aF1; f; f = f->GetPrevInFlow()) {
    if (f == aF2) {
      // f1 comes after f2 in the flow
      return 1;
    }
  }

  NS_ASSERTION(PR_FALSE, "Frames for same content but not in relative flow order");
  return 0;
}

class CompareByContentOrderComparator
{
  public:
  bool Equals(const nsIFrame* aA, const nsIFrame* aB) const {
    return aA == aB;
  }
  bool LessThan(const nsIFrame* aA, const nsIFrame* aB) const {
    return CompareByContentOrder(aA, aB) < 0;
  }
};

void
nsFrameList::ApplySetParent(nsIFrame* aParent) const
{
  NS_ASSERTION(aParent, "null ptr");

  for (nsIFrame* f = FirstChild(); f; f = f->GetNextSibling()) {
    f->SetParent(aParent);
  }
}

#ifdef DEBUG
void
nsFrameList::List(FILE* out) const
{
  fputs("<\n", out);
  for (nsIFrame* frame = mFirstChild; frame;
       frame = frame->GetNextSibling()) {
    frame->List(out, 1);
  }
  fputs(">\n", out);
}
#endif

#ifdef IBMBIDI
nsIFrame*
nsFrameList::GetPrevVisualFor(nsIFrame* aFrame) const
{
  if (!mFirstChild)
    return nsnull;
  
  nsIFrame* parent = mFirstChild->GetParent();
  if (!parent)
    return aFrame ? aFrame->GetPrevSibling() : LastChild();

  nsBidiLevel baseLevel = nsBidiPresUtils::GetFrameBaseLevel(mFirstChild);  

  nsAutoLineIterator iter = parent->GetLineIterator();
  if (!iter) { 
    // Parent is not a block Frame
    if (parent->GetType() == nsGkAtoms::lineFrame) {
      // Line frames are not bidi-splittable, so need to consider bidi reordering
      if (baseLevel == NSBIDI_LTR) {
        return nsBidiPresUtils::GetFrameToLeftOf(aFrame, mFirstChild, -1);
      } else { // RTL
        return nsBidiPresUtils::GetFrameToRightOf(aFrame, mFirstChild, -1);
      }
    } else {
      // Just get the next or prev sibling, depending on block and frame direction.
      nsBidiLevel frameEmbeddingLevel = nsBidiPresUtils::GetFrameEmbeddingLevel(mFirstChild);
      if ((frameEmbeddingLevel & 1) == (baseLevel & 1)) {
        return aFrame ? aFrame->GetPrevSibling() : LastChild();
      } else {
        return aFrame ? aFrame->GetNextSibling() : mFirstChild;
      }    
    }
  }

  // Parent is a block frame, so use the LineIterator to find the previous visual 
  // sibling on this line, or the last one on the previous line.

  PRInt32 thisLine;
  if (aFrame) {
    thisLine = iter->FindLineContaining(aFrame);
    if (thisLine < 0)
      return nsnull;
  } else {
    thisLine = iter->GetNumLines();
  }

  nsIFrame* frame = nsnull;
  nsIFrame* firstFrameOnLine;
  PRInt32 numFramesOnLine;
  nsRect lineBounds;
  PRUint32 lineFlags;

  if (aFrame) {
    iter->GetLine(thisLine, &firstFrameOnLine, &numFramesOnLine, lineBounds, &lineFlags);

    if (baseLevel == NSBIDI_LTR) {
      frame = nsBidiPresUtils::GetFrameToLeftOf(aFrame, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = nsBidiPresUtils::GetFrameToRightOf(aFrame, firstFrameOnLine, numFramesOnLine);
    }
  }

  if (!frame && thisLine > 0) {
    // Get the last frame of the previous line
    iter->GetLine(thisLine - 1, &firstFrameOnLine, &numFramesOnLine, lineBounds, &lineFlags);

    if (baseLevel == NSBIDI_LTR) {
      frame = nsBidiPresUtils::GetFrameToLeftOf(nsnull, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = nsBidiPresUtils::GetFrameToRightOf(nsnull, firstFrameOnLine, numFramesOnLine);
    }
  }
  return frame;
}

nsIFrame*
nsFrameList::GetNextVisualFor(nsIFrame* aFrame) const
{
  if (!mFirstChild)
    return nsnull;
  
  nsIFrame* parent = mFirstChild->GetParent();
  if (!parent)
    return aFrame ? aFrame->GetPrevSibling() : mFirstChild;

  nsBidiLevel baseLevel = nsBidiPresUtils::GetFrameBaseLevel(mFirstChild);
  
  nsAutoLineIterator iter = parent->GetLineIterator();
  if (!iter) { 
    // Parent is not a block Frame
    if (parent->GetType() == nsGkAtoms::lineFrame) {
      // Line frames are not bidi-splittable, so need to consider bidi reordering
      if (baseLevel == NSBIDI_LTR) {
        return nsBidiPresUtils::GetFrameToRightOf(aFrame, mFirstChild, -1);
      } else { // RTL
        return nsBidiPresUtils::GetFrameToLeftOf(aFrame, mFirstChild, -1);
      }
    } else {
      // Just get the next or prev sibling, depending on block and frame direction.
      nsBidiLevel frameEmbeddingLevel = nsBidiPresUtils::GetFrameEmbeddingLevel(mFirstChild);
      if ((frameEmbeddingLevel & 1) == (baseLevel & 1)) {
        return aFrame ? aFrame->GetNextSibling() : mFirstChild;
      } else {
        return aFrame ? aFrame->GetPrevSibling() : LastChild();
      }
    }
  }

  // Parent is a block frame, so use the LineIterator to find the next visual 
  // sibling on this line, or the first one on the next line.
  
  PRInt32 thisLine;
  if (aFrame) {
    thisLine = iter->FindLineContaining(aFrame);
    if (thisLine < 0)
      return nsnull;
  } else {
    thisLine = -1;
  }

  nsIFrame* frame = nsnull;
  nsIFrame* firstFrameOnLine;
  PRInt32 numFramesOnLine;
  nsRect lineBounds;
  PRUint32 lineFlags;

  if (aFrame) {
    iter->GetLine(thisLine, &firstFrameOnLine, &numFramesOnLine, lineBounds, &lineFlags);
    
    if (baseLevel == NSBIDI_LTR) {
      frame = nsBidiPresUtils::GetFrameToRightOf(aFrame, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = nsBidiPresUtils::GetFrameToLeftOf(aFrame, firstFrameOnLine, numFramesOnLine);
    }
  }
  
  PRInt32 numLines = iter->GetNumLines();
  if (!frame && thisLine < numLines - 1) {
    // Get the first frame of the next line
    iter->GetLine(thisLine + 1, &firstFrameOnLine, &numFramesOnLine, lineBounds, &lineFlags);
    
    if (baseLevel == NSBIDI_LTR) {
      frame = nsBidiPresUtils::GetFrameToRightOf(nsnull, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = nsBidiPresUtils::GetFrameToLeftOf(nsnull, firstFrameOnLine, numFramesOnLine);
    }
  }
  return frame;
}
#endif

#ifdef DEBUG_FRAME_LIST
void
nsFrameList::VerifyList() const
{
  NS_ASSERTION((mFirstChild == nsnull) == (mLastChild == nsnull),
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
