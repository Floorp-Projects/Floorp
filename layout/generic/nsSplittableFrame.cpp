/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsSplittableFrame.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsISizeOfHandler.h"

NS_IMETHODIMP
nsSplittableFrame::Init(nsIPresContext&  aPresContext,
                        nsIContent*      aContent,
                        nsIFrame*        aParent,
                        nsIStyleContext* aContext,
                        nsIFrame*        aPrevInFlow)
{
  nsresult  rv;
  
  rv = nsFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  if (aPrevInFlow) {
    // Hook the frame into the flow
    AppendToFlow(aPrevInFlow);

    // Make sure the general flags bits are the same
    nsFrameState  state;
    aPrevInFlow->GetFrameState(&state);
    if ((state & NS_FRAME_SYNC_FRAME_AND_VIEW) == 0) {
      mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
    }
    if (state & NS_FRAME_REPLACED_ELEMENT) {
      mState |= NS_FRAME_REPLACED_ELEMENT;
    }
    if (state & NS_FRAME_SELECTED_CONTENT) {
      mState |= NS_FRAME_SELECTED_CONTENT;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsSplittableFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE;
  return NS_OK;
}

NS_METHOD nsSplittableFrame::GetPrevInFlow(nsIFrame** aPrevInFlow) const
{
  *aPrevInFlow = mPrevInFlow;
  return NS_OK;
}

NS_METHOD nsSplittableFrame::SetPrevInFlow(nsIFrame* aFrame)
{
  mPrevInFlow = aFrame;
  return NS_OK;
}

NS_METHOD nsSplittableFrame::GetNextInFlow(nsIFrame** aNextInFlow) const
{
  *aNextInFlow = mNextInFlow;
  return NS_OK;
}

NS_METHOD nsSplittableFrame::SetNextInFlow(nsIFrame* aFrame)
{
  mNextInFlow = aFrame;
  return NS_OK;
}

nsIFrame* nsSplittableFrame::GetFirstInFlow() const
{
  nsSplittableFrame* firstInFlow;
  nsSplittableFrame* prevInFlow = (nsSplittableFrame*)this;
  while (nsnull!=prevInFlow)  {
    firstInFlow = prevInFlow;
    prevInFlow = (nsSplittableFrame*)firstInFlow->mPrevInFlow;
  }
  NS_POSTCONDITION(nsnull!=firstInFlow, "illegal state in flow chain.");
  return firstInFlow;
}

nsIFrame* nsSplittableFrame::GetLastInFlow() const
{
  nsSplittableFrame* lastInFlow;
  nsSplittableFrame* nextInFlow = (nsSplittableFrame*)this;
  while (nsnull!=nextInFlow)  {
    lastInFlow = nextInFlow;
    nextInFlow = (nsSplittableFrame*)lastInFlow->mNextInFlow;
  }
  NS_POSTCONDITION(nsnull!=lastInFlow, "illegal state in flow chain.");
  return lastInFlow;
}

// Append this frame to flow after aAfterFrame
NS_METHOD nsSplittableFrame::AppendToFlow(nsIFrame* aAfterFrame)
{
  NS_PRECONDITION(aAfterFrame != nsnull, "null pointer");

  mPrevInFlow = aAfterFrame;
  aAfterFrame->GetNextInFlow(&mNextInFlow);
  mPrevInFlow->SetNextInFlow(this);
  if (mNextInFlow) {
    mNextInFlow->SetPrevInFlow(this);
  }
  return NS_OK;
}

// Prepend this frame to flow before aBeforeFrame
NS_METHOD nsSplittableFrame::PrependToFlow(nsIFrame* aBeforeFrame)
{
  NS_PRECONDITION(aBeforeFrame != nsnull, "null pointer");

  aBeforeFrame->GetPrevInFlow(&mPrevInFlow);
  mNextInFlow = aBeforeFrame;
  mNextInFlow->SetPrevInFlow(this);
  if (mPrevInFlow) {
    mPrevInFlow->SetNextInFlow(this);
  }
  return NS_OK;
}

// Remove this frame from the flow. Connects prev in flow and next in flow
NS_METHOD nsSplittableFrame::RemoveFromFlow()
{
  if (mPrevInFlow) {
    mPrevInFlow->SetNextInFlow(mNextInFlow);
  }

  if (mNextInFlow) {
    mNextInFlow->SetPrevInFlow(mPrevInFlow);
  }

  mPrevInFlow = mNextInFlow = nsnull;
  return NS_OK;
}

// Detach from previous frame in flow
NS_METHOD nsSplittableFrame::BreakFromPrevFlow()
{
  if (mPrevInFlow) {
    mPrevInFlow->SetNextInFlow(nsnull);
    mPrevInFlow = nsnull;
  }
  return NS_OK;
}

// Detach from next frame in flow
NS_METHOD nsSplittableFrame::BreakFromNextFlow()
{
  if (mNextInFlow) {
    mNextInFlow->SetPrevInFlow(nsnull);
    mNextInFlow = nsnull;
  }
  return NS_OK;
}

nsIFrame * nsSplittableFrame::GetPrevInFlow() 
{
  return mPrevInFlow;
}

nsIFrame * nsSplittableFrame::GetNextInFlow()
{
   return mNextInFlow;
} 

void
nsSplittableFrame::DumpBaseRegressionData(FILE* out, PRInt32 aIndent)
{
  nsFrame::DumpBaseRegressionData(out, aIndent);
  if (nsnull != mNextInFlow) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-in-flow va=\"%ld\"/>\n", PRUptrdiff(mNextInFlow));
  }
  if (nsnull != mPrevInFlow) {
    IndentBy(out, aIndent);
    fprintf(out, "<prev-in-flow va=\"%ld\"/>\n", PRUptrdiff(mPrevInFlow));
  }
}
