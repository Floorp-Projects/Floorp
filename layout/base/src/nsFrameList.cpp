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
#include "nsFrameList.h"
#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#endif

#ifdef IBMBIDI
#include "nsCOMPtr.h"
#include "nsLayoutAtoms.h"
#include "nsILineIterator.h"
#endif // IBMBIDI

void
nsFrameList::DestroyFrames(nsIPresContext* aPresContext)
{
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    nsIFrame* next;
    frame->GetNextSibling(&next);
    frame->Destroy(aPresContext);
    mFirstChild = frame = next;
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
    if (nsnull != aParent) {
      nsIFrame* frame = aFrameList;
      while (nsnull != frame) {
        frame->SetParent(aParent);
        frame->GetNextSibling(&frame);
      }
    }
  }
}

void
nsFrameList::AppendFrame(nsIFrame* aParent, nsIFrame* aFrame)
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (nsnull != aFrame) {
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
}

PRBool
nsFrameList::RemoveFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (nsnull != aFrame) {
    nsIFrame* nextFrame;
    aFrame->GetNextSibling(&nextFrame);
    aFrame->SetNextSibling(nsnull);
    if (aFrame == mFirstChild) {
      mFirstChild = nextFrame;
      return PR_TRUE;
    }
    else {
      nsIFrame* prevSibling = GetPrevSiblingFor(aFrame);
      if (nsnull != prevSibling) {
        prevSibling->SetNextSibling(nextFrame);
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

PRBool
nsFrameList::RemoveFirstChild()
{
  if (nsnull != mFirstChild) {
    nsIFrame* nextFrame;
    mFirstChild->GetNextSibling(&nextFrame);
    mFirstChild->SetNextSibling(nsnull);
    mFirstChild = nextFrame;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsFrameList::DestroyFrame(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (RemoveFrame(aFrame)) {
    aFrame->Destroy(aPresContext);
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
    if (nsnull == aPrevSibling) {
      aNewFrame->SetNextSibling(mFirstChild);
      mFirstChild = aNewFrame;
    }
    else {
      nsIFrame* nextFrame;
      aPrevSibling->GetNextSibling(&nextFrame);
      aPrevSibling->SetNextSibling(aNewFrame);
      aNewFrame->SetNextSibling(nextFrame);
    }
    if (nsnull != aParent) {
      aNewFrame->SetParent(aParent);
    }
  }
}

void
nsFrameList::InsertFrames(nsIFrame* aParent,
                          nsIFrame* aPrevSibling,
                          nsIFrame* aFrameList)
{
  NS_PRECONDITION(nsnull != aFrameList, "null ptr");
  if (nsnull != aFrameList) {
    nsIFrame* lastNewFrame = nsnull;
    if (nsnull != aParent) {
      nsIFrame* frame = aFrameList;
      while (nsnull != frame) {
        frame->SetParent(aParent);
        lastNewFrame = frame;
        frame->GetNextSibling(&frame);
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
      nsIFrame* nextFrame;
      aPrevSibling->GetNextSibling(&nextFrame);
      aPrevSibling->SetNextSibling(aFrameList);
      lastNewFrame->SetNextSibling(nextFrame);
    }
  }
}

PRBool
nsFrameList::ReplaceFrame(nsIFrame* aParent,
                          nsIFrame* aOldFrame,
                          nsIFrame* aNewFrame)
{
  NS_PRECONDITION(nsnull != aOldFrame, "null ptr");
  NS_PRECONDITION(nsnull != aNewFrame, "null ptr");
  if ((nsnull != aOldFrame) && (nsnull != aNewFrame)) {
    nsIFrame* nextFrame;
    aOldFrame->GetNextSibling(&nextFrame);
    if (aOldFrame == mFirstChild) {
      mFirstChild = aNewFrame;
      aNewFrame->SetNextSibling(nextFrame);
    }
    else {
      nsIFrame* prevSibling = GetPrevSiblingFor(aOldFrame);
      if (nsnull != prevSibling) {
        prevSibling->SetNextSibling(aNewFrame);
        aNewFrame->SetNextSibling(nextFrame);
      }
    }
    if (nsnull != aParent) {
      aNewFrame->SetParent(aParent);
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsFrameList::ReplaceAndDestroyFrame(nsIPresContext* aPresContext,
                                    nsIFrame* aParent,
                                    nsIFrame* aOldFrame,
                                    nsIFrame* aNewFrame)
{
  NS_PRECONDITION(nsnull != aOldFrame, "null ptr");
  NS_PRECONDITION(nsnull != aNewFrame, "null ptr");
  if (ReplaceFrame(aParent, aOldFrame, aNewFrame)) {
    aNewFrame->Destroy(aPresContext);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsFrameList::Split(nsIFrame* aAfterFrame, nsIFrame** aNextFrameResult)
{
  NS_PRECONDITION(nsnull != aAfterFrame, "null ptr");
  NS_PRECONDITION(nsnull != aNextFrameResult, "null ptr");
  NS_ASSERTION(ContainsFrame(aAfterFrame), "split after unknown frame");

  if ((nsnull != aNextFrameResult) && (nsnull != aAfterFrame)) {
    nsIFrame* nextFrame;
    aAfterFrame->GetNextSibling(&nextFrame);
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
  return pulledFrame;
}

nsIFrame*
nsFrameList::LastChild() const
{
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    nsIFrame* next;
    frame->GetNextSibling(&next);
    if (nsnull == next) {
      break;
    }
    frame = next;
  }
  return frame;
}

nsIFrame*
nsFrameList::FrameAt(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid arg");
  if (aIndex < 0) return nsnull;
  nsIFrame* frame = mFirstChild;
  while ((aIndex-- > 0) && (nsnull != frame)) {
    frame->GetNextSibling(&frame);
  }
  return frame;
}

PRBool
nsFrameList::ContainsFrame(const nsIFrame* aFrame) const
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    if (frame == aFrame) {
      return PR_TRUE;
    }
    frame->GetNextSibling(&frame);
  }
  return PR_FALSE;
}

PRInt32
nsFrameList::GetLength() const
{
  PRInt32 count = 0;
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    count++;
    frame->GetNextSibling(&frame);
  }
  return count;
}

nsIFrame*
nsFrameList::GetPrevSiblingFor(nsIFrame* aFrame) const
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  if (aFrame == mFirstChild) {
    return nsnull;
  }
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    nsIFrame* next;
    frame->GetNextSibling(&next);
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
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    nsIFrame* parent;
    frame->GetParent(&parent);
    NS_ASSERTION(parent == aParent, "bad parent");
    frame->GetNextSibling(&frame);
  }
#endif
}

#ifdef NS_DEBUG
void
nsFrameList::List(nsIPresContext* aPresContext, FILE* out) const
{
  fputs("<\n", out);
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->List(aPresContext, out, 1);
    }
    frame->GetNextSibling(&frame);
  }
  fputs(">\n", out);
}
#endif

#ifdef IBMBIDI
nsIFrame*
nsFrameList::GetPrevVisualFor(nsIFrame* aFrame) const
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  nsILineIterator* iter;
  nsCOMPtr<nsIAtom>atom;

  aFrame->GetFrameType(getter_AddRefs(atom));
  if (atom.get() == nsLayoutAtoms::blockFrame)
    return GetPrevSiblingFor(aFrame);

  nsRect tempRect;
  nsIFrame* blockFrame;
  nsIFrame* frame;
  nsIFrame* furthestFrame = nsnull;

  frame = mFirstChild;

  nsresult result = aFrame->GetParent(&blockFrame);
  if (NS_FAILED(result) || !blockFrame)
    return GetPrevSiblingFor(aFrame);
  result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator), (void**)&iter);
  if (NS_FAILED(result) || !iter) { // If the parent is not a block frame, just check all the siblings

    PRInt32 maxX, limX;
    maxX = -0x7fffffff;
    aFrame->GetRect(tempRect);
    limX = tempRect.x;
    while (frame) {
      frame->GetRect(tempRect);
      if (tempRect.x > maxX && tempRect.x < limX) { // we are looking for the highest value less than the current one
        maxX = tempRect.x;
        furthestFrame = frame;
      }
      frame->GetNextSibling(&frame);
    }
    return furthestFrame;

  }

  // Otherwise use the LineIterator to check the siblings on this line and the previous line
  if (!blockFrame || !iter)
    return nsnull;

  PRInt64 maxOrig, limOrig, testOrig;
  PRInt32 testLine, thisLine;

  maxOrig = LL_MININT;
  aFrame->GetRect(tempRect);
  result = iter->FindLineContaining(aFrame, &thisLine);
  if (NS_FAILED(result) || thisLine < 0)
    return nsnull;

  LL_SHL(limOrig, thisLine, 32);
  LL_OR2(limOrig, tempRect.x);

  while (frame) {
    if (NS_SUCCEEDED(iter->FindLineContaining(frame, &testLine))
        && testLine >= 0
        && (testLine == thisLine || testLine == thisLine - 1)) {
      frame->GetRect(tempRect);
      LL_SHL(testOrig, testLine, 32);
      LL_OR2(testOrig, tempRect.x);
      if (LL_CMP(testOrig, >, maxOrig) && LL_CMP(testOrig, <, limOrig)) { // we are looking for the highest value less than the current one
        maxOrig = testOrig;
        furthestFrame = frame;
      }
    }
    frame->GetNextSibling(&frame);
  }
  return furthestFrame;
}

nsIFrame*
nsFrameList::GetNextVisualFor(nsIFrame* aFrame) const
{
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  nsILineIterator* iter;
  nsCOMPtr<nsIAtom>atom;

  aFrame->GetFrameType(getter_AddRefs(atom));
  if (atom.get() == nsLayoutAtoms::blockFrame) {
    nsIFrame* frame;
    aFrame->GetNextSibling(&frame);
    return frame;
  }

  nsRect tempRect;
  nsIFrame* blockFrame;
  nsIFrame* frame;
  nsIFrame* nearestFrame = nsnull;

  frame = mFirstChild;

  nsresult result = aFrame->GetParent(&blockFrame);
  if (NS_FAILED(result) || !blockFrame)
    return GetPrevSiblingFor(aFrame);
  result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator), (void**)&iter);
  if (NS_FAILED(result) || !iter) { // If the parent is not a block frame, just check all the siblings

    PRInt32 minX, limX;
    minX = 0x7fffffff;
    aFrame->GetRect(tempRect);
    limX = tempRect.x;
    while (frame) {
      frame->GetRect(tempRect);
      if (tempRect.x < minX && tempRect.x > limX) { // we are looking for the lowest value greater than the current one
        minX = tempRect.x;
        nearestFrame = frame;
      }
      frame->GetNextSibling(&frame);
    }
    return nearestFrame;

  }

  // Otherwise use the LineIterator to check the siblings on this line and the previous line
  if (!blockFrame || !iter)
    return nsnull;

  PRInt64 minOrig, limOrig, testOrig;
  PRInt32 testLine, thisLine;

  minOrig = LL_MAXINT;
  aFrame->GetRect(tempRect);
  result = iter->FindLineContaining(aFrame, &thisLine);
  if (NS_FAILED(result) || thisLine < 0)
    return nsnull;

  LL_SHL(limOrig, thisLine, 32);
  LL_OR2(limOrig, tempRect.x);

  while (frame) {
    if (NS_SUCCEEDED(iter->FindLineContaining(frame, &testLine))
        && testLine >= 0
        && (testLine == thisLine || testLine == thisLine + 1)) {
      frame->GetRect(tempRect);
      LL_SHL(testOrig, testLine, 32);
      LL_OR2(testOrig, tempRect.x);
      if (LL_CMP(testOrig, <, minOrig) && LL_CMP(testOrig, >, limOrig)) { // we are looking for the lowest value greater than the current one
        minOrig = testOrig;
        nearestFrame = frame;
      }
    }
    frame->GetNextSibling(&frame);
  }
  return nearestFrame;
}
#endif
