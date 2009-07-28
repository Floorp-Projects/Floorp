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

/* class for maintaining a linked list of child frames */

#include "nsFrameList.h"
#include "nsIFrame.h"
#ifdef DEBUG
#include "nsIFrameDebug.h"
#endif
#include "nsLayoutUtils.h"

#ifdef IBMBIDI
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsILineIterator.h"
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

const nsFrameList* nsFrameList::sEmptyList;

/* static */
nsresult
nsFrameList::Init()
{
  NS_PRECONDITION(!sEmptyList, "Shouldn't be allocated");
  sEmptyList = new nsFrameList();
  if (!sEmptyList)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

void
nsFrameList::Destroy()
{
  DestroyFrames();
  delete this;
}

void
nsFrameList::DestroyFrames()
{
  nsIFrame* next;
  for (nsIFrame* frame = mFirstChild; frame; frame = next) {
    next = frame->GetNextSibling();
    frame->Destroy();
    mFirstChild = next;
  }
}

PRBool
nsFrameList::RemoveFrame(nsIFrame* aFrame, nsIFrame* aPrevSiblingHint)
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (aFrame) {
    nsIFrame* nextFrame = aFrame->GetNextSibling();
    if (aFrame == mFirstChild) {
      mFirstChild = nextFrame;
      aFrame->SetNextSibling(nsnull);
      return PR_TRUE;
    }
    else {
      nsIFrame* prevSibling = aPrevSiblingHint;
      if (!prevSibling || prevSibling->GetNextSibling() != aFrame) {
        prevSibling = GetPrevSiblingFor(aFrame);
      }
      if (prevSibling) {
        prevSibling->SetNextSibling(nextFrame);
        aFrame->SetNextSibling(nsnull);
        return PR_TRUE;
      }
    }
  }
  // aFrame was not in the list. 
  return PR_FALSE;
}

PRBool
nsFrameList::RemoveFirstChild()
{
  if (mFirstChild) {
    RemoveFrame(mFirstChild);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsFrameList::DestroyFrame(nsIFrame* aFrame, nsIFrame* aPrevSiblingHint)
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (RemoveFrame(aFrame, aPrevSiblingHint)) {
    aFrame->Destroy();
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsFrameList::InsertFrames(nsIFrame* aParent,
                          nsIFrame* aPrevSibling,
                          nsIFrame* aFrameList)
{
  // XXXbz once we have fast access to last frames (and have an nsFrameList
  // here, not an nsIFrame*), we should clean up this method significantly.
  NS_PRECONDITION(nsnull != aFrameList, "null ptr");
  if (nsnull != aFrameList) {
    nsIFrame* lastNewFrame = nsnull;
    if (aParent) {
      for (nsIFrame* frame = aFrameList; frame;
           frame = frame->GetNextSibling()) {
        frame->SetParent(aParent);
        lastNewFrame = frame;
      }
    }

    // Get the last new frame if necessary
    if (!lastNewFrame &&
        ((aPrevSibling && aPrevSibling->GetNextSibling()) ||
         mFirstChild)) {
      nsFrameList tmp(aFrameList);
      lastNewFrame = tmp.LastChild();
    }

    // Link the new frames into the child list
    if (!aPrevSibling) {
      NS_ASSERTION(lastNewFrame || !mFirstChild,
                   "Should have lastNewFrame here");
      if (lastNewFrame) {
        lastNewFrame->SetNextSibling(mFirstChild);
      }
      mFirstChild = aFrameList;
    }
    else {
      NS_ASSERTION(aFrameList->GetParent() == aPrevSibling->GetParent(),
                   "prev sibling has different parent");
      nsIFrame* nextFrame = aPrevSibling->GetNextSibling();
      NS_ASSERTION(!nextFrame ||
                   aFrameList->GetParent() == nextFrame->GetParent(),
                   "next sibling has different parent");
      aPrevSibling->SetNextSibling(aFrameList);
      NS_ASSERTION(lastNewFrame || !nextFrame, "Should have lastNewFrame here");
      if (lastNewFrame) {
        lastNewFrame->SetNextSibling(nextFrame);
      }
    }
  }
#ifdef DEBUG
  CheckForLoops();
#endif
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

    // Now make sure aLink doesn't point to a frame we no longer have.
    aLink.mPrev = nsnull;
  }
  // else aLink is pointing to before our first frame.  Nothing to do.

  return nsFrameList(newFirstFrame);
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
  if (prev) {
    // Truncate the list after |prev| and hand the second part to our new list
    prev->SetNextSibling(nsnull);
    newFirstFrame = aLink.NextFrame();
  } else {
    // Hand the whole list over to our new list
    newFirstFrame = mFirstChild;
    mFirstChild = nsnull;
  }

  // Now make sure aLink doesn't point to a frame we no longer have.
  aLink.mFrame = nsnull;

  NS_POSTCONDITION(aLink.AtEnd(), "What's going on here?");

  return nsFrameList(newFirstFrame);
}

nsIFrame*
nsFrameList::LastChild() const
{
  nsIFrame* frame = mFirstChild;
  if (!frame) {
    return nsnull;
  }

  nsIFrame* next = frame->GetNextSibling();
  while (next) {
    frame = next;
    next = frame->GetNextSibling();
  }
  return frame;
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

PRBool
nsFrameList::ContainsFrame(const nsIFrame* aFrame) const
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  nsIFrame* frame = mFirstChild;
  while (frame) {
    if (frame == aFrame) {
      return PR_TRUE;
    }
    frame = frame->GetNextSibling();
  }
  return PR_FALSE;
}

PRBool
nsFrameList::ContainsFrameBefore(const nsIFrame* aFrame, const nsIFrame* aEnd) const
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  nsIFrame* frame = mFirstChild;
  while (frame) {
    if (frame == aEnd) {
      return PR_FALSE;
    }
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
  PRBool Equals(const nsIFrame* aA, const nsIFrame* aB) const {
    return aA == aB;
  }
  PRBool LessThan(const nsIFrame* aA, const nsIFrame* aB) const {
    return CompareByContentOrder(aA, aB) < 0;
  }
};

void
nsFrameList::SortByContentOrder()
{
  if (!mFirstChild)
    return;

  nsAutoTArray<nsIFrame*, 8> array;
  nsIFrame* f;
  for (f = mFirstChild; f; f = f->GetNextSibling()) {
    array.AppendElement(f);
  }
  array.Sort(CompareByContentOrderComparator());
  f = mFirstChild = array.ElementAt(0);
  for (PRUint32 i = 1; i < array.Length(); ++i) {
    nsIFrame* ff = array.ElementAt(i);
    f->SetNextSibling(ff);
    f = ff;
  }
  f->SetNextSibling(nsnull);
}

nsIFrame*
nsFrameList::GetPrevSiblingFor(nsIFrame* aFrame) const
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (aFrame == mFirstChild) {
    return nsnull;
  }
  nsIFrame* frame = mFirstChild;
  while (frame) {
    nsIFrame* next = frame->GetNextSibling();
    if (next == aFrame) {
      break;
    }
    frame = next;
  }
  return frame;
}

#ifdef DEBUG
void
nsFrameList::List(FILE* out) const
{
  fputs("<\n", out);
  for (nsIFrame* frame = mFirstChild; frame;
       frame = frame->GetNextSibling()) {
    nsIFrameDebug *frameDebug = do_QueryFrame(frame);
    if (frameDebug) {
      frameDebug->List(out, 1);
    }
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
    return aFrame ? GetPrevSiblingFor(aFrame) : LastChild();

  nsBidiLevel baseLevel = nsBidiPresUtils::GetFrameBaseLevel(mFirstChild);  
  nsBidiPresUtils* bidiUtils = mFirstChild->PresContext()->GetBidiUtils();

  nsAutoLineIterator iter = parent->GetLineIterator();
  if (!iter) { 
    // Parent is not a block Frame
    if (parent->GetType() == nsGkAtoms::lineFrame) {
      // Line frames are not bidi-splittable, so need to consider bidi reordering
      if (baseLevel == NSBIDI_LTR) {
        return bidiUtils->GetFrameToLeftOf(aFrame, mFirstChild, -1);
      } else { // RTL
        return bidiUtils->GetFrameToRightOf(aFrame, mFirstChild, -1);
      }
    } else {
      // Just get the next or prev sibling, depending on block and frame direction.
      nsBidiLevel frameEmbeddingLevel = nsBidiPresUtils::GetFrameEmbeddingLevel(mFirstChild);
      if ((frameEmbeddingLevel & 1) == (baseLevel & 1)) {
        return aFrame ? GetPrevSiblingFor(aFrame) : LastChild();
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
      frame = bidiUtils->GetFrameToLeftOf(aFrame, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = bidiUtils->GetFrameToRightOf(aFrame, firstFrameOnLine, numFramesOnLine);
    }
  }

  if (!frame && thisLine > 0) {
    // Get the last frame of the previous line
    iter->GetLine(thisLine - 1, &firstFrameOnLine, &numFramesOnLine, lineBounds, &lineFlags);

    if (baseLevel == NSBIDI_LTR) {
      frame = bidiUtils->GetFrameToLeftOf(nsnull, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = bidiUtils->GetFrameToRightOf(nsnull, firstFrameOnLine, numFramesOnLine);
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
    return aFrame ? GetPrevSiblingFor(aFrame) : mFirstChild;

  nsBidiLevel baseLevel = nsBidiPresUtils::GetFrameBaseLevel(mFirstChild);
  nsBidiPresUtils* bidiUtils = mFirstChild->PresContext()->GetBidiUtils();
  
  nsAutoLineIterator iter = parent->GetLineIterator();
  if (!iter) { 
    // Parent is not a block Frame
    if (parent->GetType() == nsGkAtoms::lineFrame) {
      // Line frames are not bidi-splittable, so need to consider bidi reordering
      if (baseLevel == NSBIDI_LTR) {
        return bidiUtils->GetFrameToRightOf(aFrame, mFirstChild, -1);
      } else { // RTL
        return bidiUtils->GetFrameToLeftOf(aFrame, mFirstChild, -1);
      }
    } else {
      // Just get the next or prev sibling, depending on block and frame direction.
      nsBidiLevel frameEmbeddingLevel = nsBidiPresUtils::GetFrameEmbeddingLevel(mFirstChild);
      if ((frameEmbeddingLevel & 1) == (baseLevel & 1)) {
        return aFrame ? aFrame->GetNextSibling() : mFirstChild;
      } else {
        return aFrame ? GetPrevSiblingFor(aFrame) : LastChild();
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
      frame = bidiUtils->GetFrameToRightOf(aFrame, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = bidiUtils->GetFrameToLeftOf(aFrame, firstFrameOnLine, numFramesOnLine);
    }
  }
  
  PRInt32 numLines = iter->GetNumLines();
  if (!frame && thisLine < numLines - 1) {
    // Get the first frame of the next line
    iter->GetLine(thisLine + 1, &firstFrameOnLine, &numFramesOnLine, lineBounds, &lineFlags);
    
    if (baseLevel == NSBIDI_LTR) {
      frame = bidiUtils->GetFrameToRightOf(nsnull, firstFrameOnLine, numFramesOnLine);
    } else { // RTL
      frame = bidiUtils->GetFrameToLeftOf(nsnull, firstFrameOnLine, numFramesOnLine);
    }
  }
  return frame;
}
#endif

#ifdef DEBUG
void
nsFrameList::CheckForLoops()
{
  if (!mFirstChild) {
    return;
  }
  
  // Simple algorithm to find a loop in a linked list -- advance pointers
  // through it at speeds of 1 and 2, and if they ever get to be equal bail
  nsIFrame *first = mFirstChild, *second = mFirstChild;
  do {
    first = first->GetNextSibling();
    second = second->GetNextSibling();
    if (!second) {
      break;
    }
    second = second->GetNextSibling();
    if (first == second) {
      // Loop detected!  Since second advances faster, they can't both be null;
      // we would have broken out of the loop long ago.
      NS_ERROR("loop in frame list.  This will probably hang soon.");
      break;
    }                           
  } while (first && second);
}
#endif
