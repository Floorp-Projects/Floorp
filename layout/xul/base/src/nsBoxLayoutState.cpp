/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsBoxLayoutState.h"

nsBoxLayoutState::nsBoxLayoutState(nsPresContext* aPresContext,
                                   nsRenderingContext* aRenderingContext,
                                   const nsHTMLReflowState* aOuterReflowState,
                                   PRUint16 aReflowDepth)
  : mPresContext(aPresContext)
  , mRenderingContext(aRenderingContext)
  , mOuterReflowState(aOuterReflowState)
  , mLayoutFlags(0)
  , mReflowDepth(aReflowDepth)
  , mPaintingDisabled(false)
{
  NS_ASSERTION(mPresContext, "PresContext must be non-null");
}

nsBoxLayoutState::nsBoxLayoutState(const nsBoxLayoutState& aState)
  : mPresContext(aState.mPresContext)
  , mRenderingContext(aState.mRenderingContext)
  , mOuterReflowState(aState.mOuterReflowState)
  , mLayoutFlags(aState.mLayoutFlags)
  , mReflowDepth(aState.mReflowDepth + 1)
  , mPaintingDisabled(aState.mPaintingDisabled)
{
  NS_ASSERTION(mPresContext, "PresContext must be non-null");
}
