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
#include "nsSplittableFrame.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"

NS_IMETHODIMP
nsSplittableFrame::Init(nsPresContext*  aPresContext,
                        nsIContent*      aContent,
                        nsIFrame*        aParent,
                        nsStyleContext*  aContext,
                        nsIFrame*        aPrevInFlow)
{
  nsresult  rv;
  
  rv = nsFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  if (aPrevInFlow) {
    // Hook the frame into the flow
    SetPrevInFlow(aPrevInFlow);
    aPrevInFlow->SetNextInFlow(this);
  }

  return rv;
}

NS_IMETHODIMP
nsSplittableFrame::Destroy(nsPresContext* aPresContext)
{
  // Disconnect from the flow list
  if (mPrevContinuation || mNextContinuation) {
    RemoveFromFlow(this);
  }

  // Let the base class destroy the frame
  return nsFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsSplittableFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE;
  return NS_OK;
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
  nsSplittableFrame* firstContinuation = NS_CONST_CAST(nsSplittableFrame*, this);
  while (firstContinuation->mPrevContinuation)  {
    firstContinuation = NS_STATIC_CAST(nsSplittableFrame*, firstContinuation->mPrevContinuation);
  }
  NS_POSTCONDITION(firstContinuation, "illegal state in continuation chain.");
  return firstContinuation;
}

nsIFrame* nsSplittableFrame::GetLastContinuation() const
{
  nsSplittableFrame* lastContinuation = NS_CONST_CAST(nsSplittableFrame*, this);
  while (lastContinuation->mNextContinuation)  {
    lastContinuation = NS_STATIC_CAST(nsSplittableFrame*, lastContinuation->mNextContinuation);
  }
  NS_POSTCONDITION(lastContinuation, "illegal state in continuation chain.");
  return lastContinuation;
}

#ifdef DEBUG
PRBool nsSplittableFrame::IsInPrevContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  while (aFrame1) {
    if (aFrame1 == aFrame2)
      return PR_TRUE;
    aFrame1 = aFrame1->GetPrevContinuation();
  }
  return PR_FALSE;
}

PRBool nsSplittableFrame::IsInNextContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  while (aFrame1) {
    if (aFrame1 == aFrame2)
      return PR_TRUE;
    aFrame1 = aFrame1->GetNextContinuation();
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
  nsSplittableFrame* firstInFlow = NS_CONST_CAST(nsSplittableFrame*, this);
  while (nsIFrame *prev = firstInFlow->GetPrevInFlow())  {
    firstInFlow = NS_STATIC_CAST(nsSplittableFrame*, prev);
  }
  NS_POSTCONDITION(firstInFlow, "illegal state in flow chain.");
  return firstInFlow;
}

nsIFrame* nsSplittableFrame::GetLastInFlow() const
{
  nsSplittableFrame* lastInFlow = NS_CONST_CAST(nsSplittableFrame*, this);
  while (nsIFrame* next = lastInFlow->GetNextInFlow())  {
    lastInFlow = NS_STATIC_CAST(nsSplittableFrame*, next);
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

// Detach from previous frame in flow
void
nsSplittableFrame::BreakFromPrevFlow(nsIFrame* aFrame)
{
  nsIFrame* prevInFlow = aFrame->GetPrevInFlow();
  // If this frame has a non-fluid continuation, transfer it to its prevInFlow
  nsIFrame* nextNonFluid = nsnull;
  nsIFrame* nextContinuation = aFrame->GetNextContinuation();
  if (nextContinuation && !(nextContinuation->GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION)) {
    nextNonFluid = nextContinuation;
    aFrame->SetNextContinuation(nsnull);
  }
  if (prevInFlow) {
    if (nextNonFluid) {
      prevInFlow->SetNextContinuation(nextNonFluid);
      nextNonFluid->SetPrevContinuation(prevInFlow);
    } else {
      prevInFlow->SetNextInFlow(nsnull);
    }
    aFrame->SetPrevInFlow(nsnull);
  }
}

#ifdef DEBUG
void
nsSplittableFrame::DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData)
{
  nsFrame::DumpBaseRegressionData(aPresContext, out, aIndent, aIncludeStyleData);
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
