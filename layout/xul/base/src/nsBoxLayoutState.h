/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsBoxLayoutState_h___
#define nsBoxLayoutState_h___

#include "nsIFrame.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"

class nsRenderingContext;
class nsCalculatedBoxInfo;
struct nsHTMLReflowMetrics;
class nsString;
class nsHTMLReflowCommand;

class NS_STACK_CLASS nsBoxLayoutState
{
public:
  nsBoxLayoutState(nsPresContext* aPresContext,
                   nsRenderingContext* aRenderingContext = nsnull,
                   // see OuterReflowState() below
                   const nsHTMLReflowState* aOuterReflowState = nsnull,
                   PRUint16 aReflowDepth = 0) NS_HIDDEN;
  nsBoxLayoutState(const nsBoxLayoutState& aState) NS_HIDDEN;

  nsPresContext* PresContext() const { return mPresContext; }
  nsIPresShell* PresShell() const { return mPresContext->PresShell(); }

  PRUint32 LayoutFlags() const { return mLayoutFlags; }
  void SetLayoutFlags(PRUint32 aFlags) { mLayoutFlags = aFlags; }

  // if true no one under us will paint during reflow.
  void SetPaintingDisabled(bool aDisable) { mPaintingDisabled = aDisable; }
  bool PaintingDisabled() const { return mPaintingDisabled; }

  // The rendering context may be null for specialized uses of
  // nsBoxLayoutState and should be null-checked before it is used.
  // However, passing a null rendering context to the constructor when
  // doing box layout or intrinsic size calculation will cause bugs.
  nsRenderingContext* GetRenderingContext() const { return mRenderingContext; }

  struct AutoReflowDepth {
    AutoReflowDepth(nsBoxLayoutState& aState)
      : mState(aState) { ++mState.mReflowDepth; }
    ~AutoReflowDepth() { --mState.mReflowDepth; }
    nsBoxLayoutState& mState;
  };

  // The HTML reflow state that lives outside the box-block boundary.
  // May not be set reliably yet.
  const nsHTMLReflowState* OuterReflowState() { return mOuterReflowState; }

  PRUint16 GetReflowDepth() { return mReflowDepth; }
  
private:
  nsRefPtr<nsPresContext> mPresContext;
  nsRenderingContext *mRenderingContext;
  const nsHTMLReflowState *mOuterReflowState;
  PRUint32 mLayoutFlags;
  PRUint16 mReflowDepth; 
  bool mPaintingDisabled;
};

#endif

