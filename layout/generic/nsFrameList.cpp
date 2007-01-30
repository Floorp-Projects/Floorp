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
#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#endif
#include "nsLayoutUtils.h"

#ifdef IBMBIDI
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsILineIterator.h"
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

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

void
nsFrameList::AppendFrames(nsIFrame* aParent, nsIFrame* aFrameList)
{
  NS_PRECONDITION(nsnull != aFrameList, "null ptr");
  if (nsnull != aFrameList) {
    nsIFrame* lastChild = LastChild();
    if (nsnull == lastChild) {
      mFirstChild = aFrameList;
    }
    else {
      lastChild->SetNextSibling(aFrameList);
    }
    if (aParent) {
      for (nsIFrame* frame = aFrameList; frame;
           frame = frame->GetNextSibling()) {
        frame->SetParent(aParent);
      }
    }
  }
#ifdef DEBUG
  CheckForLoops();
#endif
}

void
nsFrameList::AppendFrame(nsIFrame* aParent, nsIFrame* aFrame)
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (nsnull != aFrame) {
    NS_PRECONDITION(!aFrame->GetNextSibling(), "Can only append one frame here");
    nsIFrame* lastChild = LastChild();
    if (nsnull == lastChild) {
      mFirstChild = aFrame;
    }
    else {
      lastChild->SetNextSibling(aFrame);
    }
    if (nsnull != aParent) {
      aFrame->SetParent(aParent);
    }
  }
#ifdef DEBUG
  CheckForLoops();
#endif
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
    nsIFrame* nextFrame = mFirstChild->GetNextSibling();
    mFirstChild->SetNextSibling(nsnull);
    mFirstChild = nextFrame;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsFrameList::DestroyFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (RemoveFrame(aFrame)) {
    aFrame->Destroy();
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsFrameList::InsertFrame(nsIFrame* aParent,
                         nsIFrame* aPrevSibling,
                         nsIFrame* aNewFrame)
{
  NS_PRECONDITION(nsnull != aNewFrame, "null ptr");
  if (nsnull != aNewFrame) {
    if (aParent) {
      aNewFrame->SetParent(aParent);
    }
    if (nsnull == aPrevSibling) {
      aNewFrame->SetNextSibling(mFirstChild);
      mFirstChild = aNewFrame;
    }
    else {
      NS_ASSERTION(aNewFrame->GetParent() == aPrevSibling->GetParent(),
                   "prev sibling has different parent");
      nsIFrame* nextFrame = aPrevSibling->GetNextSibling();
      NS_ASSERTION(!nextFrame ||
                   aNewFrame->GetParent() == nextFrame->GetParent(),
                   "next sibling has different parent");
      aPrevSibling->SetNextSibling(aNewFrame);
      aNewFrame->SetNextSibling(nextFrame);
    }
  }
#ifdef DEBUG
  CheckForLoops();
#endif
}

void
nsFrameList::InsertFrames(nsIFrame* aParent,
                          nsIFrame* aPrevSibling,
                          nsIFrame* aFrameList)
{
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
    if (!lastNewFrame) {
      nsFrameList tmp(aFrameList);
      lastNewFrame = tmp.LastChild();
    }

    // Link the new frames into the child list
    if (nsnull == aPrevSibling) {
      lastNewFrame->SetNextSibling(mFirstChild);
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
      lastNewFrame->SetNextSibling(nextFrame);
    }
  }
#ifdef DEBUG
  CheckForLoops();
#endif
}

PRBool
nsFrameList::Split(nsIFrame* aAfterFrame, nsIFrame** aNextFrameResult)
{
  NS_PRECONDITION(nsnull != aAfterFrame, "null ptr");
  NS_PRECONDITION(nsnull != aNextFrameResult, "null ptr");
  NS_ASSERTION(ContainsFrame(aAfterFrame), "split after unknown frame");

  if (aNextFrameResult && aAfterFrame) {
    nsIFrame* nextFrame = aAfterFrame->GetNextSibling();
    aAfterFrame->SetNextSibling(nsnull);
    *aNextFrameResult = nextFrame;
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsIFrame*
nsFrameList::PullFrame(nsIFrame* aParent,
                       nsIFrame* aLastChild,
                       nsFrameList& aFromList)
{
  NS_PRECONDITION(nsnull != aParent, "null ptr");

  nsIFrame* pulledFrame = nsnull;
  if (nsnull != aParent) {
    pulledFrame = aFromList.FirstChild();
    if (nsnull != pulledFrame) {
      // Take frame off old list
      aFromList.RemoveFirstChild();

      // Put it on the end of this list
      if (nsnull == aLastChild) {
        NS_ASSERTION(nsnull == mFirstChild, "bad aLastChild");
        mFirstChild = pulledFrame;
      }
      else {
        aLastChild->SetNextSibling(pulledFrame);
      }
      pulledFrame->SetParent(aParent);
    }
  }
#ifdef DEBUG
  CheckForLoops();
#endif
  return pulledFrame;
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

static int PR_CALLBACK CompareByContentOrder(const void* aF1, const void* aF2,
                                             void* aDummy)
{
  const nsIFrame* f1 = NS_STATIC_CAST(const nsIFrame*, aF1);
  const nsIFrame* f2 = NS_STATIC_CAST(const nsIFrame*, aF2);
  if (f1->GetContent() != f2->GetContent()) {
    return nsLayoutUtils::CompareTreePosition(f1->GetContent(), f2->GetContent());
  }

  if (f1 == f2) {
    return 0;
  }

  const nsIFrame* f;
  for (f = f2; f; f = f->GetPrevInFlow()) {
    if (f == f1) {
      // f1 comes before f2 in the flow
      return -1;
    }
  }
  for (f = f1; f; f = f->GetPrevInFlow()) {
    if (f == f2) {
      // f1 comes after f2 in the flow
      return 1;
    }
  }

  NS_ASSERTION(PR_FALSE, "Frames for same content but not in relative flow order");
  return 0;
}

void
nsFrameList::SortByContentOrder()
{
  if (!mFirstChild)
    return;

  nsAutoVoidArray array;
  nsIFrame* f;
  for (f = mFirstChild; f; f = f->GetNextSibling()) {
    array.AppendElement(f);
  }
  array.Sort(CompareByContentOrder, nsnull);
  f = mFirstChild = NS_STATIC_CAST(nsIFrame*, array.FastElementAt(0));
  for (PRInt32 i = 1; i < array.Count(); ++i) {
    nsIFrame* ff = NS_STATIC_CAST(nsIFrame*, array.FastElementAt(i));
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

void
nsFrameList::VerifyParent(nsIFrame* aParent) const
{
#ifdef NS_DEBUG
  for (nsIFrame* frame = mFirstChild; frame;
       frame = frame->GetNextSibling()) {
    NS_ASSERTION(frame->GetParent() == aParent, "bad parent");
  }
#endif
}

#ifdef NS_DEBUG
void
nsFrameList::List(FILE* out) const
{
  fputs("<\n", out);
  for (nsIFrame* frame = mFirstChild; frame;
       frame = frame->GetNextSibling()) {
    nsIFrameDebug*  frameDebug;
    if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
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
  nsILineIterator* iter;

  if (!mFirstChild)
    return nsnull;
  
  nsIFrame* parent = mFirstChild->GetParent();
  if (!parent)
    return aFrame ? GetPrevSiblingFor(aFrame) : LastChild();

  nsBidiLevel baseLevel = nsBidiPresUtils::GetFrameBaseLevel(mFirstChild);  
  nsBidiPresUtils* bidiUtils = mFirstChild->GetPresContext()->GetBidiUtils();

  nsresult result = parent->QueryInterface(NS_GET_IID(nsILineIterator), (void**)&iter);
  if (NS_FAILED(result) || !iter) { 
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
    result = iter->FindLineContaining(aFrame, &thisLine);
    if (NS_FAILED(result) || thisLine < 0)
      return nsnull;
  } else {
    iter->GetNumLines(&thisLine);
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
  nsILineIterator* iter;

  if (!mFirstChild)
    return nsnull;
  
  nsIFrame* parent = mFirstChild->GetParent();
  if (!parent)
    return aFrame ? GetPrevSiblingFor(aFrame) : mFirstChild;

  nsBidiLevel baseLevel = nsBidiPresUtils::GetFrameBaseLevel(mFirstChild);
  nsBidiPresUtils* bidiUtils = mFirstChild->GetPresContext()->GetBidiUtils();
  
  nsresult result = parent->QueryInterface(NS_GET_IID(nsILineIterator), (void**)&iter);
  if (NS_FAILED(result) || !iter) { 
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
    result = iter->FindLineContaining(aFrame, &thisLine);
    if (NS_FAILED(result) || thisLine < 0)
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
  
  PRInt32 numLines;
  iter->GetNumLines(&numLines);
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
