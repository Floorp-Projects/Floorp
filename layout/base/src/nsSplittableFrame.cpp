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
#include "nsIContentDelegate.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"

nsSplittableFrame::nsSplittableFrame(nsIContent* aContent,
                                     PRInt32     aIndexInParent,
                                     nsIFrame*   aParent)
  : nsFrame(aContent, aIndexInParent, aParent)
{
}

nsSplittableFrame::~nsSplittableFrame()
{
  // XXX write me
}

// Flow member functions

PRBool nsSplittableFrame::IsSplittable() const
{
  return PR_TRUE;
}

/**
 * Creates a continuing frame. The continuing frame is appended to the flow.
 *
 * @param   aPresContext presentation context to use to get the content delegate
 * @param   aParent the geometric parent to use. Note that it maybe different than
 *            the receiver's geometric parent
 * @return  the continuing frame or null if unsuccessful
 */
nsIFrame* nsSplittableFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                                   nsIFrame*       aParent)
{
  nsIContentDelegate* contentDelegate = mContent->GetDelegate(aPresContext);
  nsIFrame*           continuingFrame =
    contentDelegate->CreateFrame(aPresContext, mContent, mIndexInParent, aParent);
  NS_RELEASE(contentDelegate);

  // Append the continuing frame to the flow
  continuingFrame->AppendToFlow(this);

  // Resolve style for the continuing frame and set its style context.
  nsIStyleContext* styleContext =
    aPresContext->ResolveStyleContextFor(mContent, aParent);
  continuingFrame->SetStyleContext(styleContext);
  NS_RELEASE(styleContext);

  return continuingFrame;
}

nsIFrame* nsSplittableFrame::GetPrevInFlow() const
{
  return mPrevInFlow;
}

void nsSplittableFrame::SetPrevInFlow(nsIFrame* aFrame)
{
  mPrevInFlow = aFrame;
}

nsIFrame* nsSplittableFrame::GetNextInFlow() const
{
  return mNextInFlow;
}

void nsSplittableFrame::SetNextInFlow(nsIFrame* aFrame)
{
  mNextInFlow = aFrame;
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
void nsSplittableFrame::AppendToFlow(nsIFrame* aAfterFrame)
{
  NS_PRECONDITION(aAfterFrame != nsnull, "null pointer");

  mPrevInFlow = aAfterFrame;
  mNextInFlow = aAfterFrame->GetNextInFlow();
  mPrevInFlow->SetNextInFlow(this);
  if (mNextInFlow) {
    mNextInFlow->SetPrevInFlow(this);
  }
}

// Prepend this frame to flow before aBeforeFrame
void nsSplittableFrame::PrependToFlow(nsIFrame* aBeforeFrame)
{
  NS_PRECONDITION(aBeforeFrame != nsnull, "null pointer");

  mPrevInFlow = aBeforeFrame->GetPrevInFlow();
  mNextInFlow = aBeforeFrame;
  mNextInFlow->SetPrevInFlow(this);
  if (mPrevInFlow) {
    mPrevInFlow->SetNextInFlow(this);
  }
}

// Remove this frame from the flow. Connects prev in flow and next in flow
void nsSplittableFrame::RemoveFromFlow()
{
  if (mPrevInFlow) {
    mPrevInFlow->SetNextInFlow(mNextInFlow);
  }

  if (mNextInFlow) {
    mNextInFlow->SetPrevInFlow(mPrevInFlow);
  }

  mPrevInFlow = mNextInFlow = nsnull;
}

// Detach from previous frame in flow
void nsSplittableFrame::BreakFromPrevFlow()
{
  if (mPrevInFlow) {
    mPrevInFlow->SetNextInFlow(nsnull);
    mPrevInFlow = nsnull;
  }
}

// Detach from next frame in flow
void nsSplittableFrame::BreakFromNextFlow()
{
  if (mNextInFlow) {
    mNextInFlow->SetPrevInFlow(nsnull);
    mNextInFlow = nsnull;
  }
}

