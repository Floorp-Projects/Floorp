/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsSplittableFrame.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsISizeOfHandler.h"

NS_IMETHODIMP
nsSplittableFrame::Init(nsIPresContext*  aPresContext,
                        nsIContent*      aContent,
                        nsIFrame*        aParent,
                        nsIStyleContext* aContext,
                        nsIFrame*        aPrevInFlow)
{
  nsresult  rv;
  
  rv = nsFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  if (aPrevInFlow) {
    // Hook the frame into the flow
    mPrevInFlow = aPrevInFlow;
    aPrevInFlow->SetNextInFlow(this);

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

// Remove this frame from the flow. Connects prev in flow and next in flow
void
nsSplittableFrame::RemoveFromFlow(nsIFrame* aFrame)
{
  nsIFrame* prevInFlow;
  nsIFrame* nextInFlow;

  aFrame->GetPrevInFlow(&prevInFlow);
  aFrame->GetNextInFlow(&nextInFlow);

  if (prevInFlow) {
    prevInFlow->SetNextInFlow(nextInFlow);
  }

  if (nextInFlow) {
    nextInFlow->SetPrevInFlow(prevInFlow);
  }

  aFrame->SetPrevInFlow(nsnull);
  aFrame->SetNextInFlow(nsnull);
}

// Detach from previous frame in flow
void
nsSplittableFrame::BreakFromPrevFlow(nsIFrame* aFrame)
{
  nsIFrame* prevInFlow;

  aFrame->GetPrevInFlow(&prevInFlow);
  if (prevInFlow) {
    prevInFlow->SetNextInFlow(nsnull);
    aFrame->SetPrevInFlow(nsnull);
  }
}

nsIFrame * nsSplittableFrame::GetPrevInFlow() 
{
  return mPrevInFlow;
}

nsIFrame * nsSplittableFrame::GetNextInFlow()
{
   return mNextInFlow;
} 

#ifdef DEBUG
void
nsSplittableFrame::DumpBaseRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData)
{
  nsFrame::DumpBaseRegressionData(aPresContext, out, aIndent, aIncludeStyleData);
  if (nsnull != mNextInFlow) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-in-flow va=\"%ld\"/>\n", PRUptrdiff(mNextInFlow));
  }
  if (nsnull != mPrevInFlow) {
    IndentBy(out, aIndent);
    fprintf(out, "<prev-in-flow va=\"%ld\"/>\n", PRUptrdiff(mPrevInFlow));
  }

}

NS_IMETHODIMP
nsSplittableFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
