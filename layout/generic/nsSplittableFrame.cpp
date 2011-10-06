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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * base class for rendering objects that can be split across lines,
 * columns, or pages
 */

#include "nsSplittableFrame.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"

NS_IMPL_FRAMEARENA_HELPERS(nsSplittableFrame)

NS_IMETHODIMP
nsSplittableFrame::Init(nsIContent*      aContent,
                        nsIFrame*        aParent,
                        nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsFrame::Init(aContent, aParent, aPrevInFlow);

  if (aPrevInFlow) {
    // Hook the frame into the flow
    SetPrevInFlow(aPrevInFlow);
    aPrevInFlow->SetNextInFlow(this);
  }

  return rv;
}

void
nsSplittableFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Disconnect from the flow list
  if (mPrevContinuation || mNextContinuation) {
    RemoveFromFlow(this);
  }

  // Let the base class destroy the frame
  nsFrame::DestroyFrom(aDestructRoot);
}

nsSplittableType
nsSplittableFrame::GetSplittableType() const
{
  return NS_FRAME_SPLITTABLE;
}

nsIFrame* nsSplittableFrame::GetPrevContinuation() const
{
  return mPrevContinuation;
}

NS_METHOD nsSplittableFrame::SetPrevContinuation(nsIFrame* aFrame)
{
  NS_ASSERTION (!aFrame || GetType() == aFrame->GetType(), "setting a prev continuation with incorrect type!");
  NS_ASSERTION (!IsInPrevContinuationChain(aFrame, this), "creating a loop in continuation chain!");
  mPrevContinuation = aFrame;
  RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  return NS_OK;
}

nsIFrame* nsSplittableFrame::GetNextContinuation() const
{
  return mNextContinuation;
}

NS_METHOD nsSplittableFrame::SetNextContinuation(nsIFrame* aFrame)
{
  NS_ASSERTION (!aFrame || GetType() == aFrame->GetType(),  "setting a next continuation with incorrect type!");
  NS_ASSERTION (!IsInNextContinuationChain(aFrame, this), "creating a loop in continuation chain!");
  mNextContinuation = aFrame;
  if (aFrame)
    aFrame->RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  return NS_OK;
}

nsIFrame* nsSplittableFrame::GetFirstContinuation() const
{
  nsSplittableFrame* firstContinuation = const_cast<nsSplittableFrame*>(this);
  while (firstContinuation->mPrevContinuation)  {
    firstContinuation = static_cast<nsSplittableFrame*>(firstContinuation->mPrevContinuation);
  }
  NS_POSTCONDITION(firstContinuation, "illegal state in continuation chain.");
  return firstContinuation;
}

nsIFrame* nsSplittableFrame::GetLastContinuation() const
{
  nsSplittableFrame* lastContinuation = const_cast<nsSplittableFrame*>(this);
  while (lastContinuation->mNextContinuation)  {
    lastContinuation = static_cast<nsSplittableFrame*>(lastContinuation->mNextContinuation);
  }
  NS_POSTCONDITION(lastContinuation, "illegal state in continuation chain.");
  return lastContinuation;
}

#ifdef DEBUG
bool nsSplittableFrame::IsInPrevContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  PRInt32 iterations = 0;
  while (aFrame1 && iterations < 10) {
    // Bail out after 10 iterations so we don't bog down debug builds too much
    if (aFrame1 == aFrame2)
      return PR_TRUE;
    aFrame1 = aFrame1->GetPrevContinuation();
    ++iterations;
  }
  return PR_FALSE;
}

bool nsSplittableFrame::IsInNextContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  PRInt32 iterations = 0;
  while (aFrame1 && iterations < 10) {
    // Bail out after 10 iterations so we don't bog down debug builds too much
    if (aFrame1 == aFrame2)
      return PR_TRUE;
    aFrame1 = aFrame1->GetNextContinuation();
    ++iterations;
  }
  return PR_FALSE;
}
#endif

nsIFrame* nsSplittableFrame::GetPrevInFlow() const
{
  return (GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? mPrevContinuation : nsnull;
}

NS_METHOD nsSplittableFrame::SetPrevInFlow(nsIFrame* aFrame)
{
  NS_ASSERTION (!aFrame || GetType() == aFrame->GetType(), "setting a prev in flow with incorrect type!");
  NS_ASSERTION (!IsInPrevContinuationChain(aFrame, this), "creating a loop in continuation chain!");
  mPrevContinuation = aFrame;
  AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  return NS_OK;
}

nsIFrame* nsSplittableFrame::GetNextInFlow() const
{
  return mNextContinuation && (mNextContinuation->GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? 
    mNextContinuation : nsnull;
}

NS_METHOD nsSplittableFrame::SetNextInFlow(nsIFrame* aFrame)
{
  NS_ASSERTION (!aFrame || GetType() == aFrame->GetType(),  "setting a next in flow with incorrect type!");
  NS_ASSERTION (!IsInNextContinuationChain(aFrame, this), "creating a loop in continuation chain!");
  mNextContinuation = aFrame;
  if (aFrame)
    aFrame->AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  return NS_OK;
}

nsIFrame* nsSplittableFrame::GetFirstInFlow() const
{
  nsSplittableFrame* firstInFlow = const_cast<nsSplittableFrame*>(this);
  while (nsIFrame *prev = firstInFlow->GetPrevInFlow())  {
    firstInFlow = static_cast<nsSplittableFrame*>(prev);
  }
  NS_POSTCONDITION(firstInFlow, "illegal state in flow chain.");
  return firstInFlow;
}

nsIFrame* nsSplittableFrame::GetLastInFlow() const
{
  nsSplittableFrame* lastInFlow = const_cast<nsSplittableFrame*>(this);
  while (nsIFrame* next = lastInFlow->GetNextInFlow())  {
    lastInFlow = static_cast<nsSplittableFrame*>(next);
  }
  NS_POSTCONDITION(lastInFlow, "illegal state in flow chain.");
  return lastInFlow;
}

// Remove this frame from the flow. Connects prev in flow and next in flow
void
nsSplittableFrame::RemoveFromFlow(nsIFrame* aFrame)
{
  nsIFrame* prevContinuation = aFrame->GetPrevContinuation();
  nsIFrame* nextContinuation = aFrame->GetNextContinuation();

  // The new continuation is fluid only if the continuation on both sides
  // of the removed frame was fluid
  if (aFrame->GetPrevInFlow() && aFrame->GetNextInFlow()) {
    if (prevContinuation) {
      prevContinuation->SetNextInFlow(nextContinuation);
    }
    if (nextContinuation) {
      nextContinuation->SetPrevInFlow(prevContinuation);
    }
  } else {
    if (prevContinuation) {
      prevContinuation->SetNextContinuation(nextContinuation);
    }
    if (nextContinuation) {
      nextContinuation->SetPrevContinuation(prevContinuation);
    }
  }

  aFrame->SetPrevInFlow(nsnull);
  aFrame->SetNextInFlow(nsnull);
}

#ifdef DEBUG
void
nsSplittableFrame::DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  nsFrame::DumpBaseRegressionData(aPresContext, out, aIndent);
  if (nsnull != mNextContinuation) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-continuation va=\"%ld\"/>\n", PRUptrdiff(mNextContinuation));
  }
  if (nsnull != mPrevContinuation) {
    IndentBy(out, aIndent);
    fprintf(out, "<prev-continuation va=\"%ld\"/>\n", PRUptrdiff(mPrevContinuation));
  }

}
#endif
