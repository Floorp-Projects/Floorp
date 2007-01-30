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

/* base class for rendering objects that do not have child lists */

#include "nsCOMPtr.h"
#include "nsLeafFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"

nsLeafFrame::~nsLeafFrame()
{
}

/* virtual */ nscoord
nsLeafFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  result = GetIntrinsicWidth();
  return result;
}

/* virtual */ nscoord
nsLeafFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  result = GetIntrinsicWidth();
  return result;
}

NS_IMETHODIMP
nsLeafFrame::Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsLeafFrame");
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsLeafFrame::Reflow: aMaxSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  // XXX add in code to check for width/height being set via css
  // and if set use them instead of calling GetDesiredSize.

  NS_ASSERTION(aReflowState.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
               "Shouldn't have unconstrained stuff here");

  aMetrics.width = aReflowState.ComputedWidth();
  if (NS_INTRINSICSIZE != aReflowState.mComputedHeight) {
    aMetrics.height = aReflowState.mComputedHeight;
  } else {
    aMetrics.height = GetIntrinsicHeight();
    // XXXbz using NS_CSS_MINMAX like this presupposes content-box sizing.
    aMetrics.height = NS_CSS_MINMAX(aMetrics.height,
                                    aReflowState.mComputedMinHeight,
                                    aReflowState.mComputedMaxHeight);
  }
  
  AddBordersAndPadding(aReflowState, aMetrics);
  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsLeafFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);

  aMetrics.mOverflowArea =
    nsRect(0, 0, aMetrics.width, aMetrics.height);
  FinishAndStoreOverflow(&aMetrics);
  return NS_OK;
}

PRBool
nsLeafFrame::IsFrameOfType(PRUint32 aFlags) const
{
  // We don't actually contain a block, but we do always want a
  // computed width, so tell a little white lie here.
  return !(aFlags & ~nsIFrame::eReplacedContainsBlock);
}

nscoord
nsLeafFrame::GetIntrinsicHeight()
{
  NS_NOTREACHED("Someone didn't override Reflow");
  return 0;
}

// XXX how should border&padding effect baseline alignment?
// => descent = borderPadding.bottom for example
void
nsLeafFrame::AddBordersAndPadding(const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics& aMetrics)
{
  aMetrics.width += aReflowState.mComputedBorderPadding.LeftRight();
  aMetrics.height += aReflowState.mComputedBorderPadding.TopBottom();
}

void
nsLeafFrame::SizeToAvailSize(const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width  = aReflowState.availableWidth; // FRAME
  aDesiredSize.height = aReflowState.availableHeight;
  aDesiredSize.mOverflowArea =
    nsRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  FinishAndStoreOverflow(&aDesiredSize);  
}
