/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsFrameReflowState.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"

nsFrameReflowState::nsFrameReflowState(nsIPresContext& aPresContext,
                                       const nsReflowState& aReflowState,
                                       const nsReflowMetrics& aMetrics)
  : nsReflowState(aReflowState),
    mPresContext(aPresContext)
{
  // While we skip around the reflow state that our parent gave us so
  // that the parentReflowState is linked properly, we don't want to
  // skip over it's reason.
  reason = aReflowState.reason;
  mNextRCFrame = nsnull;

  // Initialize max-element-size
  mComputeMaxElementSize = nsnull != aMetrics.maxElementSize;
  mMaxElementSize.width = 0;
  mMaxElementSize.height = 0;

  // Get style data that we need
  frame->GetStyleData(eStyleStruct_Text,
                      (const nsStyleStruct*&) mStyleText);
  frame->GetStyleData(eStyleStruct_Display,
                      (const nsStyleStruct*&) mStyleDisplay);
  frame->GetStyleData(eStyleStruct_Spacing,
                      (const nsStyleStruct*&) mStyleSpacing);

  // Calculate our border and padding value
  mStyleSpacing->CalcBorderPaddingFor(frame, mBorderPadding);

  // Set mNoWrap flag
  switch (mStyleText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    mNoWrap = PR_TRUE;
    break;
  default:
    mNoWrap = PR_FALSE;
    break;
  }

  // Set mDirection value
  mDirection = mStyleDisplay->mDirection;

  // Initialize running margin value
  mRunningMargin = aMetrics.mCarriedInTopMargin;
}

nsFrameReflowState::~nsFrameReflowState()
{
}
